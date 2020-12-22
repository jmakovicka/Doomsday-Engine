/** @file material.cpp  Material definition accessor.
 *
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/defs/material.h"
#include "doomsday/defs/ded.h"

#include <de/record.h>
#include <de/recordvalue.h>

using namespace de;

namespace defn {

void MaterialDecoration::resetToDefaults()
{
    Definition::resetToDefaults();

    // Add all expected fields with their default values.
    def().addArray("patternOffset", new ArrayValue(Vec2i()));
    def().addArray("patternSkip", new ArrayValue(Vec2i()));
    def().addArray("stage", new ArrayValue);
}

Record &MaterialDecoration::addStage()
{
    auto *stage = new Record;

    stage->addNumber("tics", 0);
    stage->addNumber("variance", 0);                              // Time.
    stage->addArray ("origin", new ArrayValue(Vec2f()));       // Surface-relative offset.
    stage->addNumber("elevation", 1);                             // Distance from the surface.
    stage->addArray ("color", new ArrayValue(Vec3f()));        // Light color. (0,0,0) means not visible during this stage.
    stage->addNumber("radius", 1);                                // Dynamic light radius (-1 = no light).
    stage->addArray ("lightLevels", new ArrayValue(Vec2f()));  // Fade by sector lightlevel.

    stage->addText  ("lightmapUp", "");                           // Uri. None.
    stage->addText  ("lightmapDown", "");                         // Uri. None.
    stage->addText  ("lightmapSide", "");                         // Uri. None.

    stage->addNumber("haloRadius", 0);                            // Halo radius (zero = no halo).
    stage->addText  ("haloTexture", "");                          // Uri. None.
    stage->addNumber("haloTextureIndex", 0);                      // Overrides haloTexture

    def()["stage"].array().add(new RecordValue(stage, RecordValue::OwnsRecord));

    return *stage;
}

int MaterialDecoration::stageCount() const
{
    return int(geta("stage").size());
}

bool MaterialDecoration::hasStage(int index) const
{
    return index >= 0 && index < stageCount();
}

Record &MaterialDecoration::stage(int index)
{
    return *def().geta("stage")[index].as<RecordValue>().record();
}

const Record &MaterialDecoration::stage(int index) const
{
    return *geta("stage")[index].as<RecordValue>().record();
}

// ------------------------------------------------------------------------------------

void MaterialLayer::resetToDefaults()
{
    Definition::resetToDefaults();

    // Add all expected fields with their default values.
    def().addArray("stage", new ArrayValue);
}

Record &MaterialLayer::addStage()
{
    auto *stage = new Record;

    stage->addText  ("texture", "");  // Uri. None.
    stage->addNumber("tics", 0);
    stage->addNumber("variance", 0);  // Time.
    stage->addNumber("glowStrength", 0);
    stage->addNumber("glowStrengthVariance", 0);
    stage->addArray ("texOrigin", new ArrayValue(Vec2f()));

    def()["stage"].array().add(new RecordValue(stage, RecordValue::OwnsRecord));

    return *stage;
}

int MaterialLayer::stageCount() const
{
    return int(geta("stage").size());
}

bool MaterialLayer::hasStage(int index) const
{
    return index >= 0 && index < stageCount();
}

Record &MaterialLayer::stage(int index)
{
    return *def().geta("stage")[index].as<RecordValue>().record();
}

const Record &MaterialLayer::stage(int index) const
{
    return *geta("stage")[index].as<RecordValue>().record();
}

// ------------------------------------------------------------------------------------

void Material::resetToDefaults()
{
    Definition::resetToDefaults();

    // Add all expected fields with their default values.
    def().addText   (VAR_ID, "");  // URI. Unknown.
    def().addBoolean("autoGenerated", false);
    def().addNumber ("flags", 0);
    def().addArray  ("dimensions", new ArrayValue(Vec2i()));
    def().addArray  ("decoration", new ArrayValue);
    def().addArray  ("layer", new ArrayValue);
}

Record &Material::addDecoration()
{
    auto *decor = new Record;
    MaterialDecoration(*decor).resetToDefaults();
    def()["decoration"].array().add(new RecordValue(decor, RecordValue::OwnsRecord));
    return *decor;
}

int Material::decorationCount() const
{
    return int(geta("decoration").size());
}

bool Material::hasDecoration(int index) const
{
    return index >= 0 && index < decorationCount();
}

Record &Material::decoration(int index)
{
    return *def().geta("decoration")[index].as<RecordValue>().record();
}

const Record &Material::decoration(int index) const
{
    return *geta("decoration")[index].as<RecordValue>().record();
}

Record &Material::addLayer()
{
    auto *layer = new Record;
    MaterialLayer(*layer).resetToDefaults();
    def()["layer"].array().add(new RecordValue(layer, RecordValue::OwnsRecord));
    return *layer;
}

int Material::layerCount() const
{
    return int(geta("layer").size());
}

bool Material::hasLayer(int index) const
{
    return index >= 0 && index < layerCount();
}

Record &Material::layer(int index)
{
    return *def().geta("layer")[index].as<RecordValue>().record();
}

const Record &Material::layer(int index) const
{
    return *geta("layer")[index].as<RecordValue>().record();
}

} // namespace defn
