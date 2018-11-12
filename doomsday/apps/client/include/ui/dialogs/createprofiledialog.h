/** @file
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_UI_CREATEPROFILEDIALOG_H
#define DENG_CLIENT_UI_CREATEPROFILEDIALOG_H

#include <doomsday/GameProfiles>
#include <de/InputDialog>
#include <de/IPersistent>

/**
 * Dialog for creating a game profile.
 */
class CreateProfileDialog
    : public de::InputDialog
    , public de::IPersistent
{
public:
    CreateProfileDialog(de::String const &gameFamily);

    /**
     * Creates a new profile based on the dialog's current selections.
     * @return New game profile. Caller gets ownership.
     */
    GameProfile *makeProfile() const;

    void fetchFrom(GameProfile const &profile);

    void applyTo(GameProfile &profile) const;

    de::String profileName() const;

    // Implements IPersistent.
    void operator>>(de::PersistentState &toState) const;
    void operator<<(de::PersistentState const &fromState);

public:
    static CreateProfileDialog *editProfile(de::String const &gameFamily,
                                            GameProfile &profile);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_UI_CREATEPROFILEDIALOG_H
