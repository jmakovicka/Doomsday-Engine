/** @file packagecompatibilitydialog.h
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

#ifndef DE_CLIENT_UI_PACKAGECOMPATIBILITYDIALOG_H
#define DE_CLIENT_UI_PACKAGECOMPATIBILITYDIALOG_H

#include <de/messagedialog.h>

/**
 * Dialog for resolving package incompatibilities.
 */
class PackageCompatibilityDialog : public de::MessageDialog
{
public:
    PackageCompatibilityDialog(const de::String &name = "");

    void setMessage(const de::String &msg);
    void setWantedPackages(const de::StringList& packages);

    bool isCompatible() const;

    // Events.
    bool handleEvent(const de::Event &event) override;

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_UI_PACKAGECOMPATIBILITYDIALOG_H
