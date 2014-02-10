/** @file saveslots.cpp  Map of logical game save slots.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "common.h"
#include "saveslots.h"

#include "p_saveg.h" /// @todo remove me
#include <de/memory.h>

#define MAX_HUB_MAPS 99

DENG2_PIMPL(SaveSlots)
{
    SaveInfo **saveInfo;
    SaveInfo *autoSaveInfo;
#if __JHEXEN__
    SaveInfo *baseSaveInfo;
#endif
    SaveInfo *nullSaveInfo;

    Instance(Public *i)
        : Base(i)
        , saveInfo(0)
        , autoSaveInfo(0)
#if __JHEXEN__
        , baseSaveInfo(0)
#endif
        , nullSaveInfo(0)
    {}

    /**
     * Determines whether to announce when the specified @a slot is cleared.
     */
    bool announceOnClearingSlot(int slot)
    {
#if _DEBUG
        return true; // Always.
#endif
#if __JHEXEN__
        return (slot != AUTO_SLOT && slot != BASE_SLOT);
#else
        return (slot != AUTO_SLOT);
#endif
    }

    void updateSaveInfo(Str const *path, SaveInfo *info)
    {
        if(!info) return;

        if(!path || Str_IsEmpty(path))
        {
            // The save path cannot be accessed for some reason. Perhaps its a
            // network path? Clear the info for this slot.
            info->setDescription(0);
            info->setGameId(0);
            return;
        }

        // Is this a recognisable save state?
        if(!SV_RecogniseGameState(path, info))
        {
            // Clear the info for this slot.
            info->setDescription(0);
            info->setGameId(0);
            return;
        }

        // Ensure we have a valid name.
        if(Str_IsEmpty(info->description()))
        {
            info->setDescription(AutoStr_FromText("UNNAMED"));
        }
    }
};

SaveSlots::SaveSlots() : d(new Instance(this))
{}

void SaveSlots::clearSaveInfo()
{
    if(d->saveInfo)
    {
        for(int i = 0; i < NUMSAVESLOTS; ++i)
        {
            delete d->saveInfo[i];
        }
        M_Free(d->saveInfo); d->saveInfo = 0;
    }

    if(d->autoSaveInfo)
    {
        delete d->autoSaveInfo; d->autoSaveInfo = 0;
    }
#if __JHEXEN__
    if(d->baseSaveInfo)
    {
        delete d->baseSaveInfo; d->baseSaveInfo = 0;
    }
#endif
    if(d->nullSaveInfo)
    {
        delete d->nullSaveInfo; d->nullSaveInfo = 0;
    }
}

void SaveSlots::buildSaveInfo()
{
    if(!d->saveInfo)
    {
        // Not yet been here. We need to allocate and initialize the game-save info list.
        d->saveInfo = (SaveInfo **)M_Malloc(NUMSAVESLOTS * sizeof(*d->saveInfo));

        // Initialize.
        for(int i = 0; i < NUMSAVESLOTS; ++i)
        {
            d->saveInfo[i] = new SaveInfo;
        }
        d->autoSaveInfo = new SaveInfo;
#if __JHEXEN__
        d->baseSaveInfo = new SaveInfo;
#endif
        d->nullSaveInfo = new SaveInfo;
    }

    /// Scan the save paths and populate the list.
    /// @todo We should look at all files on the save path and not just those
    /// which match the default game-save file naming convention.
    for(int i = 0; i < NUMSAVESLOTS; ++i)
    {
        SaveInfo *info = d->saveInfo[i];
        d->updateSaveInfo(composeGameSavePathForSlot(i), info);
    }
    d->updateSaveInfo(composeGameSavePathForSlot(AUTO_SLOT), d->autoSaveInfo);
#if __JHEXEN__
    d->updateSaveInfo(composeGameSavePathForSlot(BASE_SLOT), d->baseSaveInfo);
#endif
}

void SaveSlots::updateAllSaveInfo()
{
    buildSaveInfo();
}

AutoStr *SaveSlots::composeSlotIdentifier(int slot)
{
    AutoStr *str = AutoStr_NewStd();
    if(slot < 0) return Str_Set(str, "(invalid slot)");
    if(slot == AUTO_SLOT) return Str_Set(str, "<auto>");
#if __JHEXEN__
    if(slot == BASE_SLOT) return Str_Set(str, "<base>");
#endif
    return Str_Appendf(str, "%i", slot);
}

int SaveSlots::parseSlotIdentifier(char const *str)
{
    // Try game-save name match.
    int slot = SV_SlotForSaveName(str);
    if(slot >= 0) return slot;

    // Try keyword identifiers.
    if(!stricmp(str, "last") || !stricmp(str, "<last>"))
    {
        return Con_GetInteger("game-save-last-slot");
    }
    if(!stricmp(str, "quick") || !stricmp(str, "<quick>"))
    {
        return Con_GetInteger("game-save-quick-slot");
    }
    if(!stricmp(str, "auto") || !stricmp(str, "<auto>"))
    {
        return AUTO_SLOT;
    }

    // Try logical slot identifier.
    if(M_IsStringValidInt(str))
    {
        return atoi(str);
    }

    // Unknown/not found.
    return -1;
}

int SaveSlots::slotForSaveName(char const *description)
{
    DENG_ASSERT(description != 0);

    int slot = -1;
    if(description && description[0])
    {
        // On first call - automatically build and populate game-save info.
        if(!d->saveInfo)
        {
            buildSaveInfo();
        }

        int i = 0;
        do
        {
            SaveInfo *info = d->saveInfo[i];
            if(!Str_CompareIgnoreCase(info->description(), description))
            {
                // This is the one!
                slot = i;
            }
        } while(-1 == slot && ++i < NUMSAVESLOTS);
    }

    return slot;
}

bool SaveSlots::slotInUse(int slot)
{
    if(SV_ExistingFile(composeGameSavePathForSlot(slot)))
    {
        return findSaveInfoForSlot(slot)->isLoadable();
    }
    return false;
}

bool SaveSlots::isValidSlot(int slot)
{
    if(slot == AUTO_SLOT) return true;
#if __JHEXEN__
    if(slot == BASE_SLOT) return true;
#endif
    return (slot >= 0  && slot < NUMSAVESLOTS);
}

bool SaveSlots::isUserWritableSlot(int slot)
{
    if(slot == AUTO_SLOT) return false;
#if __JHEXEN__
    if(slot == BASE_SLOT) return false;
#endif
    return isValidSlot(slot);
}

SaveInfo *SaveSlots::findSaveInfoForSlot(int slot)
{
    if(!isValidSlot(slot)) return d->nullSaveInfo;

    // On first call - automatically build and populate game-save info.
    if(!d->saveInfo)
    {
        buildSaveInfo();
    }

    // Retrieve the info for this slot.
    if(slot == AUTO_SLOT) return d->autoSaveInfo;
#if __JHEXEN__
    if(slot == BASE_SLOT) return d->baseSaveInfo;
#endif
    return d->saveInfo[slot];
}

void SaveSlots::replaceSaveInfo(int slot, SaveInfo *newInfo)
{
    DENG_ASSERT(isValidSlot(slot));

    SaveInfo **destAdr;
    if(slot == AUTO_SLOT)
    {
        destAdr = &d->autoSaveInfo;
    }
#if __JHEXEN__
    else if(slot == BASE_SLOT)
    {
        destAdr = &d->baseSaveInfo;
    }
#endif
    else
    {
        destAdr = &d->saveInfo[slot];
    }

    if(*destAdr) delete (*destAdr);
    *destAdr = newInfo;
}

void SaveSlots::clearSlot(int slot)
{
    if(!isValidSlot(slot)) return;

    if(d->announceOnClearingSlot(slot))
    {
        AutoStr *ident = SV_ComposeSlotIdentifier(slot);
        App_Log(DE2_RES_MSG, "Clearing save slot %s", Str_Text(ident));
    }

    for(int i = 0; i < MAX_HUB_MAPS; ++i)
    {
        AutoStr *path = composeGameSavePathForSlot(slot, i);
        SV_RemoveFile(path);
    }

    AutoStr *path = composeGameSavePathForSlot(slot);
    SV_RemoveFile(path);

    d->updateSaveInfo(path, findSaveInfoForSlot(slot));
}

void SaveSlots::copySlot(int sourceSlot, int destSlot)
{
    if(!isValidSlot(sourceSlot))
    {
        DENG_ASSERT(!"SaveSlots::copySlot: Source slot invalid");
        return;
    }

    if(!isValidSlot(destSlot))
    {
        DENG_ASSERT(!"SaveSlots::copySlot: Dest slot invalid");
        return;
    }

    // Clear all save files at destination slot.
    clearSlot(destSlot);

    AutoStr *src, *dst;
    for(int i = 0; i < MAX_HUB_MAPS; ++i)
    {
        src = composeGameSavePathForSlot(sourceSlot, i);
        dst = composeGameSavePathForSlot(destSlot, i);
        SV_CopyFile(src, dst);
    }

    src = composeGameSavePathForSlot(sourceSlot);
    dst = composeGameSavePathForSlot(destSlot);
    SV_CopyFile(src, dst);

    // Copy saveinfo too.
    SaveInfo *info = findSaveInfoForSlot(sourceSlot);
    DENG_ASSERT(info != 0);
    replaceSaveInfo(destSlot, new SaveInfo(*info));
}

AutoStr *SaveSlots::composeGameSavePathForSlot(int slot, int map)
{
    AutoStr *path = AutoStr_NewStd();

    // A valid slot?
    if(!isValidSlot(slot)) return path;

    // Do we have a valid path?
    /// @todo Do not do alter the file system until necessary.
    if(!F_MakePath(SV_SavePath())) return path;

    // Compose the full game-save path and filename.
    if(map >= 0)
    {
        Str_Appendf(path, "%s" SAVEGAMENAME "%i%02i." SAVEGAMEEXTENSION, SV_SavePath(), slot, map);
    }
    else
    {
        Str_Appendf(path, "%s" SAVEGAMENAME "%i." SAVEGAMEEXTENSION, SV_SavePath(), slot);
    }
    F_TranslatePath(path, path);
    return path;
}
