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

#include "p_saveio.h"
#include "p_saveg.h" /// SV_RecogniseGameState, @todo remove me
#include <de/String>
#include <de/memory.h>

#define MAX_HUB_MAPS 99

static int cvarLastSlot  = -1; ///< @c -1= Not yet loaded/saved in this game session.
static int cvarQuickSlot = -1; ///< @c -1= Not yet chosen/determined.

DENG2_PIMPL(SaveSlots)
{
    int slotCount;
    SaveInfo **saveInfo;
    SaveInfo *autoSaveInfo;
#if __JHEXEN__
    SaveInfo *baseSaveInfo;
#endif

    Instance(Public *i, int slotCount)
        : Base(i)
        , slotCount(de::max(1, slotCount))
        , saveInfo(0)
        , autoSaveInfo(0)
#if __JHEXEN__
        , baseSaveInfo(0)
#endif
    {}

    ~Instance()
    {
        self.clearSaveInfo();
    }

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

    void updateSaveInfo(Str const *path, SaveInfo &info)
    {
        if(!path || Str_IsEmpty(path))
        {
            // The save path cannot be accessed for some reason. Perhaps its a
            // network path? Clear the info for this slot.
            info.setDescription(0);
            info.setSessionId(0);
            return;
        }

        // Is this a recognisable save state?
        if(!SV_RecogniseGameState(path, &info))
        {
            // Clear the info for this slot.
            info.setDescription(0);
            info.setSessionId(0);
            return;
        }

        // Ensure we have a valid description.
        if(Str_IsEmpty(info.description()))
        {
            info.setDescription(AutoStr_FromText("UNNAMED"));
        }
    }

    /// Re-build save info by re-scanning the save paths and populating the list.
    void buildSaveInfo()
    {
        if(!saveInfo)
        {
            // Not yet been here. We need to allocate and initialize the game-save info list.
            saveInfo = (SaveInfo **)M_Malloc(slotCount * sizeof(*saveInfo));

            for(int i = 0; i < slotCount; ++i)
            {
                saveInfo[i] = new SaveInfo;
            }
            autoSaveInfo = new SaveInfo;
#if __JHEXEN__
            baseSaveInfo = new SaveInfo;
#endif
        }

        /// Scan the save paths and populate the list.
        /// @todo We should look at all files on the save path and not just those
        /// which match the default game-save file naming convention.
        for(int i = 0; i < slotCount; ++i)
        {
            updateSaveInfo(self.composeSavePathForSlot(i), *saveInfo[i]);
        }
        updateSaveInfo(self.composeSavePathForSlot(AUTO_SLOT), *autoSaveInfo);
#if __JHEXEN__
        updateSaveInfo(self.composeSavePathForSlot(BASE_SLOT), *baseSaveInfo);
#endif
    }
};

SaveSlots::SaveSlots(int slotCount) : d(new Instance(this, slotCount))
{}

void SaveSlots::clearSaveInfo()
{
    if(d->saveInfo)
    {
        for(int i = 0; i < d->slotCount; ++i)
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

    // Reset last-used and quick-save slot tracking.
    Con_SetInteger2("game-save-last-slot", -1, SVF_WRITE_OVERRIDE);
    Con_SetInteger("game-save-quick-slot", -1);
}

void SaveSlots::updateAllSaveInfo()
{
    d->buildSaveInfo();
}

AutoStr *SaveSlots::composeSlotIdentifier(int slot) const
{
    AutoStr *str = AutoStr_NewStd();
    if(slot < 0) return Str_Set(str, "(invalid slot)");
    if(slot == AUTO_SLOT) return Str_Set(str, "<auto>");
#if __JHEXEN__
    if(slot == BASE_SLOT) return Str_Set(str, "<base>");
#endif
    return Str_Appendf(str, "%i", slot);
}

int SaveSlots::parseSlotIdentifier(char const *str) const
{
    // Try game-save name match.
    int slot = findSlotWithSaveDescription(str);
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

int SaveSlots::findSlotWithSaveDescription(char const *description) const
{
    int slot = -1;
    if(description && description[0])
    {
        // On first call - automatically build and populate game-save info.
        if(!d->saveInfo)
        {
            d->buildSaveInfo();
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
        } while(-1 == slot && ++i < d->slotCount);
    }

    return slot;
}

bool SaveSlots::slotInUse(int slot) const
{
    if(SV_ExistingFile(composeSavePathForSlot(slot)))
    {
        return saveInfo(slot).isLoadable();
    }
    return false;
}

int SaveSlots::slotCount() const
{
    return d->slotCount;
}

bool SaveSlots::isValidSlot(int slot) const
{
    if(slot == AUTO_SLOT) return true;
#if __JHEXEN__
    if(slot == BASE_SLOT) return true;
#endif
    return (slot >= 0  && slot < d->slotCount);
}

bool SaveSlots::slotIsUserWritable(int slot) const
{
    if(slot == AUTO_SLOT) return false;
#if __JHEXEN__
    if(slot == BASE_SLOT) return false;
#endif
    return isValidSlot(slot);
}

SaveInfo &SaveSlots::saveInfo(int slot) const
{
    if(!isValidSlot(slot))
    {
        /// @throw InvalidSlotError An invalid slot was specified.
        throw InvalidSlotError("SaveSlots::saveInfo", "Invalid slot " + de::String::number(slot));
    }

    // On first call - automatically build and populate game-save info.
    if(!d->saveInfo)
    {
        d->buildSaveInfo();
    }

    // Retrieve the info for this slot.
    if(slot == AUTO_SLOT) return *d->autoSaveInfo;
#if __JHEXEN__
    if(slot == BASE_SLOT) return *d->baseSaveInfo;
#endif
    return *d->saveInfo[slot];
}

void SaveSlots::replaceSaveInfo(int slot, SaveInfo *newInfo)
{
    if(!isValidSlot(slot))
    {
        /// @throw InvalidSlotError An invalid slot was specified.
        throw InvalidSlotError("SaveSlots::replaceSaveInfo", "Invalid slot " + de::String::number(slot));
    }

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
    if(!isValidSlot(slot))
    {
        /// @throw InvalidSlotError An invalid slot was specified.
        throw InvalidSlotError("SaveSlots::clearSlot", "Invalid slot " + de::String::number(slot));
    }

    if(d->announceOnClearingSlot(slot))
    {
        AutoStr *ident = composeSlotIdentifier(slot);
        App_Log(DE2_RES_MSG, "Clearing save slot %s", Str_Text(ident));
    }

    for(int i = 0; i < MAX_HUB_MAPS; ++i)
    {
        AutoStr *path = composeSavePathForSlot(slot, i);
        SV_RemoveFile(path);
    }

    AutoStr *path = composeSavePathForSlot(slot);
    SV_RemoveFile(path);

    d->updateSaveInfo(path, saveInfo(slot));
}

void SaveSlots::copySlot(int sourceSlot, int destSlot)
{
    if(!isValidSlot(sourceSlot))
    {
        /// @throw InvalidSlotError An invalid slot was specified.
        throw InvalidSlotError("SaveSlots::copySlot", "Invalid source slot " + de::String::number(sourceSlot));
    }

    if(!isValidSlot(destSlot))
    {
        /// @throw InvalidSlotError An invalid slot was specified.
        throw InvalidSlotError("SaveSlots::saveInfo", "Invalid dest slot " + de::String::number(destSlot));
    }

    // Clear all save files at destination slot.
    clearSlot(destSlot);

    AutoStr *src, *dst;
    for(int i = 0; i < MAX_HUB_MAPS; ++i)
    {
        src = composeSavePathForSlot(sourceSlot, i);
        dst = composeSavePathForSlot(destSlot, i);
        SV_CopyFile(src, dst);
    }

    src = composeSavePathForSlot(sourceSlot);
    dst = composeSavePathForSlot(destSlot);
    SV_CopyFile(src, dst);

    // Copy save info too.
    replaceSaveInfo(destSlot, new SaveInfo(saveInfo(sourceSlot)));
}

AutoStr *SaveSlots::composeSavePathForSlot(int slot, int map) const
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

void SaveSlots::consoleRegister() // static
{
#if !__JHEXEN__
    C_VAR_BYTE("game-save-auto-loadonreborn",    &cfg.loadAutoSaveOnReborn,  0, 0, 1);
#endif
    C_VAR_BYTE("game-save-confirm",              &cfg.confirmQuickGameSave,  0, 0, 1);
    C_VAR_BYTE("game-save-confirm-loadonreborn", &cfg.confirmRebornLoad,     0, 0, 1);
    C_VAR_BYTE("game-save-last-loadonreborn",    &cfg.loadLastSaveOnReborn,  0, 0, 1);
    C_VAR_INT ("game-save-last-slot",            &cvarLastSlot, CVF_NO_MIN|CVF_NO_MAX|CVF_NO_ARCHIVE|CVF_READ_ONLY, 0, 0);
    C_VAR_INT ("game-save-quick-slot",           &cvarQuickSlot, CVF_NO_MAX|CVF_NO_ARCHIVE, -1, 0);

    // Aliases for obsolete cvars:
    C_VAR_BYTE("menu-quick-ask",                 &cfg.confirmQuickGameSave, 0, 0, 1);
}

// C wrapper API ---------------------------------------------------------------

SaveSlots *SaveSlots_New(int slotCount)
{
    return new SaveSlots(slotCount);
}

void SaveSlots_Delete(SaveSlots *sslots)
{
    delete sslots;
}

void SaveSlots_ClearSaveInfo(SaveSlots *sslots)
{
    DENG_ASSERT(sslots != 0);
    sslots->clearSaveInfo();
}

void SaveSlots_UpdateAllSaveInfo(SaveSlots *sslots)
{
    DENG_ASSERT(sslots != 0);
    sslots->updateAllSaveInfo();
}

int SaveSlots_SlotCount(SaveSlots const *sslots)
{
    DENG_ASSERT(sslots != 0);
    return sslots->slotCount();
}

dd_bool SaveSlots_IsValidSlot(SaveSlots const *sslots, int slot)
{
    DENG_ASSERT(sslots != 0);
    return sslots->isValidSlot(slot);
}

AutoStr *SaveSlots_ComposeSlotIdentifier(SaveSlots const *sslots, int slot)
{
    DENG_ASSERT(sslots != 0);
    return sslots->composeSlotIdentifier(slot);
}

int SaveSlots_ParseSlotIdentifier(SaveSlots const *sslots, char const *str)
{
    DENG_ASSERT(sslots != 0);
    return sslots->parseSlotIdentifier(str);
}

int SaveSlots_FindSlotWithSaveDescription(SaveSlots const *sslots, char const *description)
{
    DENG_ASSERT(sslots != 0);
    return sslots->findSlotWithSaveDescription(description);
}

dd_bool SaveSlots_SlotInUse(SaveSlots const *sslots, int slot)
{
    DENG_ASSERT(sslots != 0);
    return sslots->slotInUse(slot);
}

dd_bool SaveSlots_SlotIsUserWritable(SaveSlots const *sslots, int slot)
{
    DENG_ASSERT(sslots != 0);
    return sslots->slotIsUserWritable(slot);
}

SaveInfo *SaveSlots_SaveInfo(SaveSlots *sslots, int slot)
{
    DENG_ASSERT(sslots != 0);
    return sslots->saveInfoPtr(slot);
}

void SaveSlots_ReplaceSaveInfo(SaveSlots *sslots, int slot, SaveInfo *newInfo)
{
    DENG_ASSERT(sslots != 0);
    sslots->replaceSaveInfo(slot, newInfo);
}

void SaveSlots_ClearSlot(SaveSlots *sslots, int slot)
{
    DENG_ASSERT(sslots != 0);
    sslots->clearSlot(slot);
}

void SaveSlots_CopySlot(SaveSlots *sslots, int sourceSlot, int destSlot)
{
    DENG_ASSERT(sslots != 0);
    sslots->copySlot(sourceSlot, destSlot);
}

AutoStr *SaveSlots_ComposeSavePathForSlot(SaveSlots const *sslots, int slot, int map)
{
    DENG_ASSERT(sslots != 0);
    return sslots->composeSavePathForSlot(slot, map);
}

void SaveSlots_ConsoleRegister()
{
    SaveSlots::consoleRegister();
}