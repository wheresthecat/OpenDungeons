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

#ifndef PLAYER_H
#define PLAYER_H

#include <OgrePrerequisites.h>

#include <string>
#include <vector>

class Creature;
class GameMap;
class GameEntity;
class Research;
class Seat;
class Tile;

enum class SpellType;
enum class RoomType;
enum class TrapType;
enum class ResearchType;

/*! \brief The player class contains information about a human, or computer, player in the game.
 *
 * When a new player joins a game being hosted on a server the server will
 * allocate a new Player structure and fill it in with the appropriate values.
 * Its relevant information will then be sent to the other players in the game
 * so they are aware of its presence.
 */
class Player
{
    friend class Seat;
public:
    enum SelectedAction
    {
        none,
        buildRoom,
        buildTrap,
        castSpell,
        changeTile,
        selectTile,
        destroyRoom,
        destroyTrap
    };

    enum class Direction
    {
        left = -1,
        right = 1
    };

    Player(GameMap* gameMap, int32_t id);

    inline int32_t getId() const
    { return mId; }

    const std::string& getNick() const
    { return mNickname; }

    Seat* getSeat()
    { return mSeat; }

    const Seat* getSeat() const
    { return mSeat; }

    void setNick (const std::string& nick)
    { mNickname = nick; }

    //! \brief A simple accessor function to return the number of creatures
    //! this player is holding in his/her hand that belongs to seat seat.
    //! If seat is nullptr, then returns the total number of creatures
    unsigned int numCreaturesInHand(const Seat* seat = nullptr) const;
    unsigned int numObjectsInHand() const;

    /*! \brief Check to see if it is the user or another player picking up the creature and act accordingly.
     *
     * This function takes care of all of the operations required for a player to
     * pick up an object (creature, treasury, ...).  If the player is the user we need to move the creature
     * oncreen to the "hand" as well as add the creature to the list of creatures
     * in our own hand, this is done by setting moveToHand to true.  If move to
     * hand is false we just hide the creature (and stop its AI, etc.), rather than
     * making it follow the cursor.
     */
    void pickUpEntity(GameEntity *entity);

    //! \brief Check to see the first object in hand can be dropped on Tile t and do so if possible.
    bool isDropHandPossible(Tile *t, unsigned int index = 0);

    //! \brief Drops the creature on tile t. Returns the dropped creature
    GameEntity* dropHand(Tile *t, unsigned int index = 0);

    void rotateHand(Direction d);

    //! \brief Clears all creatures that a player might have in his hand
    void clearObjectsInHand();

    //! \brief Clears all creatures that a player might have in his hand
    void notifyNoMoreDungeonTemple();

    inline bool getIsHuman() const
    { return mIsHuman; }

    inline void setIsHuman(bool isHuman)
    { mIsHuman = isHuman; }

    inline const std::vector<GameEntity*>& getObjectsInHand()
    { return mObjectsInHand; }

    inline const RoomType getNewRoomType()
    { return mNewRoomType; }

    inline const SelectedAction getCurrentAction()
    { return mCurrentAction; }

    inline void setNewRoomType(RoomType newRoomType)
    { mNewRoomType = newRoomType; }

    inline const TrapType getNewTrapType() const
    { return mNewTrapType; }

    inline void setNewTrapType(TrapType newTrapType)
    { mNewTrapType = newTrapType; }

    inline const SpellType getNewSpellType() const
    { return mNewSpellType; }

    inline void setNewSpellType(SpellType newSpellType)
    { mNewSpellType = newSpellType; }

    void setCurrentAction(SelectedAction action);

    void initPlayer();

    //! \brief Notify the player is fighting
    //! Should be called on the server game map for human players only
    void notifyFighting();

    //! \brief Notify the player is fighting
    //! Should be called on the server game map for human players only
    void notifyNoTreasuryAvailable();

    //! \brief Allows to handle timed events like fighting music
    //! Should be called on the server game map for human players only
    void updateTime(Ogre::Real timeSinceLastUpdate);

    //! \brief Checks if the given spell is available for the Player. This check
    //! should be done on server side to avoid cheating
    bool isSpellAvailableForPlayer(SpellType type) const;

    //! \brief Gets the current research to do
    bool isResearching() const
    { return mCurrentResearch != nullptr; }

    //! \brief Checks if the given room is available for the Player. This check
    //! should be done on server side to avoid cheating
    bool isRoomAvailableForPlayer(RoomType type) const;

    //! \brief Checks if the given trap is available for the Player. This check
    //! should be done on server side to avoid cheating
    bool isTrapAvailableForPlayer(TrapType type) const;

    //! Returns true if the given ResearchType is already done for this player. False
    //! otherwise
    bool isResearchDone(ResearchType type) const;

    //! Called when the research entity reaches its destination. From there, the researched
    //! thing is available
    //! Returns true if the type was inserted and false otherwise
    bool addResearch(ResearchType type);

    //! Called from the library as creatures are researching. When enough points are gathered,
    //! the corresponding research will become available.
    //! Returns the research done if enough points have been gathered and nullptr otherwise
    const Research* addResearchPoints(int32_t points);

    //! Used on both client and server side. On server side, the research tree's validity will be
    //! checked. If ok, it will be sent to the client. If not, the research tree will not be
    //! changed. Note that the order of the research matters as the first researches in the given
    //! vector can needed for the next ones
    void setResearchTree(const std::vector<ResearchType>& researches);

    //! Used on both client and server side
    void setResearchesDone(const std::vector<ResearchType>& researches);

    inline bool getNeedRefreshGuiResearchDone() const
    { return mNeedRefreshGuiResearchDone; }

    inline void guiResearchRefreshedDone()
    { mNeedRefreshGuiResearchDone = false; }

    inline bool getNeedRefreshGuiResearchPending() const
    { return mNeedRefreshGuiResearchPending; }

    inline void guiResearchRefreshedPending()
    { mNeedRefreshGuiResearchPending = false; }

private:
    //! \brief Player ID is only used during seat configuration phase
    //! During the game, one should use the seat ID to identify a player because
    //! every AI player has an id = 0.
    //! ID is unique only for human players
    int32_t mId;
    //! \brief Room, trap or Spell tile type the player is currently willing to place on map.
    RoomType mNewRoomType;
    TrapType mNewTrapType;
    SpellType mNewSpellType;
    SelectedAction mCurrentAction;

    GameMap* mGameMap;
    Seat *mSeat;

    //! \brief The nickname used in chat, etc.
    std::string mNickname;

    //! \brief The creature the player has got in hand.
    std::vector<GameEntity*> mObjectsInHand;

    //! True: player is human. False: player is a computer/inactive.
    bool mIsHuman;

    //! \brief This counter tells for how much time is left before considering
    //! the player to be out of struggle.
    //! When > 0, the player is considered attacking or being attacked.
    //! This member is used to trigger the calm or fighting music when incarnating
    //! the local player.
    float mFightingTime;

    //! \brief This counter tells for how much time is left before considering
    //! the player should be notified again that he has no free space to store gold.
    float mNoTreasuryAvailableTime;

    bool mIsPlayerLostSent;

    //! \brief Counter for research points
    int32_t mResearchPoints;

    //! \brief Currently researched Research. This pointer is external and should not be deleted
    const Research* mCurrentResearch;

    //! \brief true if the available list of research changed. False otherwise. This will be pulled
    //! by the GameMode to know if it should refresh GUI or not.
    bool mNeedRefreshGuiResearchDone;

    //! true if the pending researches have changed. False otherwise. This will be pulled
    //! by the GameMode to know if it should refresh GUI or not.
    bool mNeedRefreshGuiResearchPending;

    //! \brief Researches already done. This is used on both client and server side and should be updated
    std::vector<ResearchType> mResearchDone;

    //! \brief Researches pending. Used on server side only
    std::vector<ResearchType> mResearchPending;

    //! \brief A simple mutator function to put the given entity into the player's hand,
    //! note this should NOT be called directly for creatures on the map,
    //! for that you should use the correct function like pickUpEntity() instead.
    void addEntityToHand(GameEntity *entity);

    //! \brief Sets mCurrentResearch to the first entry in mResearchPending. If the pending
    //! list in empty, mCurrentResearch will be set to null
    //! researchedType is the currently researched type if any (nullResearchType if none)
    void setNextResearch(ResearchType researchedType);
};

#endif // PLAYER_H
