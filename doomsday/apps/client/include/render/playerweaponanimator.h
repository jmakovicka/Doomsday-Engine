/** @file playerweaponanimator.h  Player weapon model animator.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef CLIENT_RENDER_PLAYERWEAPONANIMATOR_H
#define CLIENT_RENDER_PLAYERWEAPONANIMATOR_H

#include <de/ModelDrawable>
#include "mobjanimator.h"

class ClientPlayer;

/**
 * Animates the player weapon model.
 *
 * @ingroup render
 */
class PlayerWeaponAnimator
{
public:
    PlayerWeaponAnimator(ClientPlayer *plr);

    void setAsset(de::String const &identifier);

    void stateChanged(state_t const *state);

    MobjAnimator &animator();

    void advanceTime(de::TimeDelta const &elapsed);

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENT_RENDER_PLAYERWEAPONANIMATOR_H

