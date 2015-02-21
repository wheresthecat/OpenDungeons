/*
 *  Copyright (C) 2011-2015  OpenDungeons Team
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "game/Player.h"

#include "game/Seat.h"

#include "entities/Creature.h"
#include "entities/RenderedMovableEntity.h"
#include "entities/ResearchEntity.h"
#include "entities/Tile.h"

#include "game/Research.h"

#include "gamemap/GameMap.h"

#include "modes/ModeManager.h"

#include "network/ODServer.h"
#include "network/ServerNotification.h"
#include "network/ODClient.h"

#include "render/RenderManager.h"
#include "render/ODFrameListener.h"

#include "rooms/Room.h"

#include "spell/Spell.h"

#include "traps/Trap.h"

#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

#include <cmath>

//! \brief The number of seconds the local player must stay out of danger to trigger the calm music again.
const float BATTLE_TIME_COUNT = 10.0f;

//! \brief The number of seconds the local player will not be notified again if no treasury is available
const float NO_TREASURY_TIME_COUNT = 30.0f;

Player::Player(GameMap* gameMap, int32_t id) :
    mId(id),
    mNewRoomType(RoomType::nullRoomType),
    mNewTrapType(TrapType::nullTrapType),
    mCurrentAction(SelectedAction::none),
    mGameMap(gameMap),
    mSeat(nullptr),
    mIsHuman(false),
    mFightingTime(0.0f),
    mNoTreasuryAvailableTime(0.0f),
    mIsPlayerLostSent(false),
    mResearchPoints(0),
    mCurrentResearch(nullptr),
    mNeedRefreshGuiResearchDone(false),
    mNeedRefreshGuiResearchPending(false)
{
}

unsigned int Player::numCreaturesInHand(const Seat* seat) const
{
    unsigned int cpt = 0;
    for(GameEntity* entity : mObjectsInHand)
    {
        if(entity->getObjectType() != GameEntityType::creature)
            continue;

        if(seat != nullptr && entity->getSeat() != seat)
            continue;

        ++cpt;
    }
    return cpt;
}

unsigned int Player::numObjectsInHand() const
{
    return mObjectsInHand.size();
}

void Player::addEntityToHand(GameEntity *entity)
{
    if (mObjectsInHand.empty())
    {
        mObjectsInHand.push_back(entity);
        return;
    }

    // creaturesInHand.push_front(c);
    // Since vectors have no push_front method,
    // we need to move all of the elements in the vector back one
    // and then add this one to the beginning.
    mObjectsInHand.push_back(nullptr);
    for (unsigned int j = mObjectsInHand.size() - 1; j > 0; --j)
        mObjectsInHand[j] = mObjectsInHand[j - 1];

    mObjectsInHand[0] = entity;
}

void Player::pickUpEntity(GameEntity *entity)
{
    if (!ODServer::getSingleton().isConnected() && !ODClient::getSingleton().isConnected())
        return;

    if(!entity->tryPickup(getSeat()))
           return;

    entity->pickup();

    // Start tracking this creature as being in this player's hand
    addEntityToHand(entity);

    if (mGameMap->isServerGameMap())
    {
        entity->firePickupEntity(this);
        return;
    }

    OD_ASSERT_TRUE(this == mGameMap->getLocalPlayer());
    if (this == mGameMap->getLocalPlayer())
    {
        // Send a render request to move the crature into the "hand"
        RenderManager::getSingleton().rrPickUpEntity(entity, this);
    }
}

void Player::clearObjectsInHand()
{
    mObjectsInHand.clear();
}

bool Player::isDropHandPossible(Tile *t, unsigned int index)
{
    // if we have a creature to drop
    if (mObjectsInHand.empty())
        return false;

    GameEntity* entity = mObjectsInHand[index];
    return entity->tryDrop(getSeat(), t);
}

GameEntity* Player::dropHand(Tile *t, unsigned int index)
{
    // Add the creature to the map
    OD_ASSERT_TRUE_MSG(index < mObjectsInHand.size(), "playerNick=" + getNick() + ", index=" + Ogre::StringConverter::toString(index));
    if(index >= mObjectsInHand.size())
        return nullptr;

    GameEntity *entity = mObjectsInHand[index];
    mObjectsInHand.erase(mObjectsInHand.begin() + index);

    entity->drop(Ogre::Vector3(static_cast<Ogre::Real>(t->getX()),
            static_cast<Ogre::Real>(t->getY()), entity->getPosition().z));

    if(mGameMap->isServerGameMap())
    {
        entity->fireDropEntity(this, t);
        return entity;
    }

    // If this is the result of another player dropping the creature it is currently not visible so we need to create a mesh for it
    //cout << "\nthis:  " << this << "\nme:  " << gameMap->getLocalPlayer() << endl;
    //cout.flush();
    OD_ASSERT_TRUE(this == mGameMap->getLocalPlayer());
    if(this == mGameMap->getLocalPlayer())
    {
        // Send a render request to rearrange the creatures in the hand to move them all forward 1 place
        RenderManager::getSingleton().rrDropHand(entity, this);
    }

    return entity;
}

void Player::rotateHand(Direction d)
{
    if(mObjectsInHand.size() > 1)
    {
        if(d == Direction::left)
        {
            std::rotate(mObjectsInHand.begin(), mObjectsInHand.begin() + 1, mObjectsInHand.end());
        }
        else
        {
            std::rotate(mObjectsInHand.begin(), mObjectsInHand.end() - 1, mObjectsInHand.end());
        }
        // Send a render request to move the entity into the "hand"
        RenderManager::getSingleton().rrRotateHand(this);
    }
}

void Player::notifyNoMoreDungeonTemple()
{
    if(mIsPlayerLostSent)
        return;

    mIsPlayerLostSent = true;
    // We check if there is still a player in the team with a dungeon temple. If yes, we notify the player he lost his dungeon
    // if no, we notify the team they lost
    std::vector<Room*> dungeonTemples = mGameMap->getRoomsByType(RoomType::dungeonTemple);
    bool hasTeamLost = true;
    for(Room* dungeonTemple : dungeonTemples)
    {
        if(getSeat()->isAlliedSeat(dungeonTemple->getSeat()))
        {
            hasTeamLost = false;
            break;
        }
    }

    if(hasTeamLost)
    {
        // This message will be sent in 1v1 or multiplayer so it should not talk about team. If we want to be
        // more precise, we shall handle the case
        for(Seat* seat : mGameMap->getSeats())
        {
            if(seat->getPlayer() == nullptr)
                continue;
            if(!seat->getPlayer()->getIsHuman())
                continue;
            if(!getSeat()->isAlliedSeat(seat))
                continue;

            ServerNotification *serverNotification = new ServerNotification(
            ServerNotificationType::chatServer, seat->getPlayer());
            serverNotification->mPacket << "You lost the game";
            ODServer::getSingleton().queueServerNotification(serverNotification);
        }
    }
    else
    {
        for(Seat* seat : mGameMap->getSeats())
        {
            if(seat->getPlayer() == nullptr)
                continue;
            if(!seat->getPlayer()->getIsHuman())
                continue;
            if(!getSeat()->isAlliedSeat(seat))
                continue;

            if(this == seat->getPlayer())
            {
                ServerNotification *serverNotification = new ServerNotification(
                ServerNotificationType::chatServer, seat->getPlayer());
                serverNotification->mPacket << "You lost";
                ODServer::getSingleton().queueServerNotification(serverNotification);
                continue;
            }

            ServerNotification *serverNotification = new ServerNotification(
            ServerNotificationType::chatServer, seat->getPlayer());
            serverNotification->mPacket << "An ally has lost";
            ODServer::getSingleton().queueServerNotification(serverNotification);
        }
    }
}

void Player::updateTime(Ogre::Real timeSinceLastUpdate)
{
    // Handle fighting time
    if(mFightingTime > 0.0f)
    {
        if(mFightingTime > timeSinceLastUpdate)
        {
            mFightingTime -= timeSinceLastUpdate;
        }
        else
        {
            mFightingTime = 0.0f;
            // Notify the player he is no longer under attack.
            ServerNotification *serverNotification = new ServerNotification(
                ServerNotificationType::playerNoMoreFighting, this);
            ODServer::getSingleton().queueServerNotification(serverNotification);
        }
    }

    if(mNoTreasuryAvailableTime > 0.0f)
    {
        if(mNoTreasuryAvailableTime > timeSinceLastUpdate)
            mNoTreasuryAvailableTime -= timeSinceLastUpdate;
        else
            mNoTreasuryAvailableTime = 0.0f;
    }
}

void Player::notifyFighting()
{
    if(mFightingTime == 0.0f)
    {
        ServerNotification *serverNotification = new ServerNotification(
            ServerNotificationType::playerFighting, this);
        ODServer::getSingleton().queueServerNotification(serverNotification);
    }

    mFightingTime = BATTLE_TIME_COUNT;
}

void Player::notifyNoTreasuryAvailable()
{
    if(mNoTreasuryAvailableTime == 0.0f)
    {
        mNoTreasuryAvailableTime = NO_TREASURY_TIME_COUNT;

        std::string chatMsg = "No treasury available. You should build a bigger one";
        ServerNotification *serverNotification = new ServerNotification(
            ServerNotificationType::chatServer, this);
        serverNotification->mPacket << chatMsg;
        ODServer::getSingleton().queueServerNotification(serverNotification);
    }
}

void Player::setCurrentAction(SelectedAction action)
{
    mCurrentAction = action;
    mNewTrapType = TrapType::nullTrapType;
    mNewRoomType = RoomType::nullRoomType;
    mNewSpellType = SpellType::nullSpellType;
}

void Player::initPlayer()
{
    // TODO: If we want to change the available rooms/traps/spells at game start, we can do that here
    // We add the rooms available from start
    std::vector<ResearchType> researchesDone;
    researchesDone.push_back(ResearchType::roomTreasury);
    researchesDone.push_back(ResearchType::roomHatchery);
    researchesDone.push_back(ResearchType::roomDormitory);
    researchesDone.push_back(ResearchType::roomLibrary);
    researchesDone.push_back(ResearchType::trapCannon);
    researchesDone.push_back(ResearchType::spellSummonWorker);
    setResearchesDone(researchesDone);

    // TODO: fill researches from the server depending on the player input
    std::vector<ResearchType> researches;

    researches.push_back(ResearchType::spellCallToWar);
    researches.push_back(ResearchType::roomTrainingHall);
    researches.push_back(ResearchType::roomForge);
    researches.push_back(ResearchType::trapSpike);
    researches.push_back(ResearchType::roomCrypt);
    researches.push_back(ResearchType::trapBoulder);
    setResearchTree(researches);
}

bool Player::isSpellAvailableForPlayer(SpellType type) const
{
    switch(type)
    {
        case SpellType::summonWorker:
            return isResearchDone(ResearchType::spellSummonWorker);
        case SpellType::callToWar:
            return isResearchDone(ResearchType::spellCallToWar);
        default:
            OD_ASSERT_TRUE_MSG(false, "Unknown enum value : " + Helper::toString(
                static_cast<int>(type)) + " for player " + getNick());
            return false;
    }
    return false;
}

bool Player::isRoomAvailableForPlayer(RoomType type) const
{
    switch(type)
    {
        case RoomType::treasury:
            return isResearchDone(ResearchType::roomTreasury);
        case RoomType::dormitory:
            return isResearchDone(ResearchType::roomDormitory);
        case RoomType::hatchery:
            return isResearchDone(ResearchType::roomHatchery);
        case RoomType::trainingHall:
            return isResearchDone(ResearchType::roomTrainingHall);
        case RoomType::library:
            return isResearchDone(ResearchType::roomLibrary);
        case RoomType::forge:
            return isResearchDone(ResearchType::roomForge);
        case RoomType::crypt:
            return isResearchDone(ResearchType::roomCrypt);
        default:
            OD_ASSERT_TRUE_MSG(false, "Unknown enum value : " + Helper::toString(
                static_cast<int>(type)) + " for player " + getNick());
            return false;
    }
    return false;
}

bool Player::isTrapAvailableForPlayer(TrapType type) const
{
    switch(type)
    {
        case TrapType::boulder:
            return isResearchDone(ResearchType::trapBoulder);
        case TrapType::cannon:
            return isResearchDone(ResearchType::trapCannon);
        case TrapType::spike:
            return isResearchDone(ResearchType::trapSpike);
        default:
            OD_ASSERT_TRUE_MSG(false, "Unknown enum value : " + Helper::toString(
                static_cast<int>(type)) + " for player " + getNick());
            return false;
    }
    return false;
}

bool Player::addResearch(ResearchType type)
{
    if(std::find(mResearchDone.begin(), mResearchDone.end(), type) != mResearchDone.end())
        return false;

    std::vector<ResearchType> researchDone = mResearchDone;
    researchDone.push_back(type);
    setResearchesDone(researchDone);

    return true;
}

bool Player::isResearchDone(ResearchType type) const
{
    for(ResearchType researchDone : mResearchDone)
    {
        if(researchDone != type)
            continue;

        return true;
    }

    return false;
}

const Research* Player::addResearchPoints(int32_t points)
{
    if(mCurrentResearch == nullptr)
        return nullptr;

    mResearchPoints += points;
    if(mResearchPoints < mCurrentResearch->getNeededResearchPoints())
        return nullptr;

    const Research* ret = mCurrentResearch;
    mResearchPoints -= mCurrentResearch->getNeededResearchPoints();

    // The current research is complete. The library that completed it will release
    // a ResearchEntity. Once it will reach its destination, the research will be
    // added to the done list

    setNextResearch(mCurrentResearch->getType());
    return ret;
}

void Player::setNextResearch(ResearchType researchedType)
{
    mCurrentResearch = nullptr;
    if(!mResearchPending.empty())
    {
        // We search for the first pending research we don't own a corresponding ResearchEntity
        const std::vector<RenderedMovableEntity*>& renderables = mGameMap->getRenderedMovableEntities();
        ResearchType researchType = ResearchType::nullResearchType;
        for(ResearchType pending : mResearchPending)
        {
            if(pending == researchedType)
                continue;

            researchType = pending;
            for(RenderedMovableEntity* renderable : renderables)
            {
                if(renderable->getObjectType() != GameEntityType::researchEntity)
                    continue;

                if(renderable->getSeat() != getSeat())
                    continue;

                ResearchEntity* researchEntity = static_cast<ResearchEntity*>(renderable);
                if(researchEntity->getResearchType() != pending)
                    continue;

                // We found a ResearchEntity of the same Research. We should work on
                // something else
                researchType = ResearchType::nullResearchType;
                break;
            }

            if(researchType != ResearchType::nullResearchType)
                break;
        }

        if(researchType == ResearchType::nullResearchType)
            return;

        // We have found a fitting research. We retrieve the corresponding Research
        // object and start working on that
        const std::vector<const Research*>& researches = ConfigManager::getSingleton().getResearches();
        for(const Research* research : researches)
        {
            if(research->getType() != researchType)
                continue;

            mCurrentResearch = research;
            break;
        }
    }
}

void Player::setResearchesDone(const std::vector<ResearchType>& researches)
{
    mResearchDone = researches;
    // We remove the researches done from the pending researches (if it was there,
    // which may not be true if the research list changed after creating the
    // researchEntity for example)
    for(ResearchType type : researches)
    {
        auto research = std::find(mResearchPending.begin(), mResearchPending.end(), type);
        if(research == mResearchPending.end())
            continue;

        mResearchPending.erase(research);
    }

    if(mGameMap->isServerGameMap())
    {
        if(getIsHuman())
        {
            // We notify the client
            ServerNotification *serverNotification = new ServerNotification(
                ServerNotificationType::researchesDone, this);

            uint32_t nbItems = mResearchDone.size();
            serverNotification->mPacket << nbItems;
            for(ResearchType research : mResearchDone)
                serverNotification->mPacket << research;

            ODServer::getSingleton().queueServerNotification(serverNotification);
        }
    }
    else
    {
        // We notify the mode that the available researches changed. This way, it will
        // be able to update the UI as needed
        mNeedRefreshGuiResearchDone = true;
    }
}

void Player::setResearchTree(const std::vector<ResearchType>& researches)
{
    if(mGameMap->isServerGameMap())
    {
        // We check if all the researches in the vector are allowed. If not, we don't update the list
        const std::vector<const Research*>& researchList = ConfigManager::getSingleton().getResearches();
        std::vector<ResearchType> researchesDoneInTree = mResearchDone;
        for(ResearchType researchType : researches)
        {
            const Research* research = nullptr;
            for(const Research* researchTmp : researchList)
            {
                if(researchTmp->getType() != researchType)
                    continue;

                research = researchTmp;
                break;
            }

            if(research == nullptr)
            {
                // We found an unknow research
                OD_ASSERT_TRUE_MSG(false, "Unknow research: " + Research::researchTypeToString(researchType));
                return;
            }

            if(!research->canBeResearched(researches))
            {
                // Invalid research. This might be allowed in the gui to enter invalid
                // values. In this case, we should remove the assert
                OD_ASSERT_TRUE_MSG(false, "Unallowed research: " + Research::researchTypeToString(researchType));
                return;
            }

            // This research is valid. We add it in the list and we check if the next one also is
            researchesDoneInTree.push_back(researchType);
        }

        mResearchPending = researches;
        if(getIsHuman())
        {
            // We notify the client
            ServerNotification *serverNotification = new ServerNotification(
                ServerNotificationType::researchTree, this);

            uint32_t nbItems = mResearchPending.size();
            serverNotification->mPacket << nbItems;
            for(ResearchType research : mResearchPending)
                serverNotification->mPacket << research;

            ODServer::getSingleton().queueServerNotification(serverNotification);
        }

        // We start working on the research tree
        setNextResearch(ResearchType::nullResearchType);
    }
    else
    {
        // On client side, no need to check if the research tree is allowed
        mResearchPending = researches;
        mNeedRefreshGuiResearchPending = true;
    }
}
