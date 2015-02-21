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

#ifndef RESEARCHENTITY_H
#define RESEARCHENTITY_H

#include "entities/RenderedMovableEntity.h"

#include <string>
#include <istream>
#include <ostream>

class Creature;
class Room;
class GameMap;
class Tile;
class ODPacket;

enum class ResearchType;

class ResearchEntity: public RenderedMovableEntity
{
public:
    ResearchEntity(GameMap* gameMap, const std::string& libraryName, ResearchType researchType);
    ResearchEntity(GameMap* gameMap);

    virtual GameEntityType getObjectType() const override
    { return GameEntityType::researchEntity; }

    virtual const Ogre::Vector3& getScale() const;

    ResearchType getResearchType() const
    { return mResearchType; }

    virtual EntityCarryType getEntityCarryType()
    { return EntityCarryType::researchEntity; }

    virtual void notifyEntityCarryOn(Creature* carrier);
    virtual void notifyEntityCarryOff(const Ogre::Vector3& position);

    static ResearchEntity* getResearchEntityFromStream(GameMap* gameMap, std::istream& is);
    static ResearchEntity* getResearchEntityFromPacket(GameMap* gameMap, ODPacket& is);
    static const char* getFormat();
private:
    ResearchType mResearchType;
};

#endif // RESEARCHENTITY_H
