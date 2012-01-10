/**\file p_saveio.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef LIBCOMMON_SAVESTATE_INPUT_OUTPUT_H
#define LIBCOMMON_SAVESTATE_INPUT_OUTPUT_H

#include "materialarchive.h"
#include "lzss.h"
#include "p_savedef.h"

typedef struct savegameparam_s {
    const ddstring_t* path;
    const char* name;
    int slot;
} savegameparam_t;

enum {
    SV_OK = 0,
    SV_INVALIDFILENAME
};

void SV_InitIO(void);
void SV_ShutdownIO(void);
void SV_ConfigureSavePaths(void);
const char* SV_SavePath(void);
#if !__JHEXEN__
const char* SV_ClientSavePath(void);
#endif

/*
 * File management
 */
LZFILE* SV_OpenFile(const char *fileName, const char* mode);
void SV_CloseFile(void);
LZFILE* SV_File(void);

/*
 * Save slots
 */
boolean SV_IsValidSlot(int slot);

/**
 * Force an update of the cached game-save info. To be called (sparingly)
 * at strategic points when an update is necessary (e.g., the game-save
 * paths have changed).
 *
 * \note It is not necessary to call this after a game-save is made,
 * this module will do so automatically.
 */
void SV_UpdateGameSaveInfo(void);

/**
 * Parse the given string and determine whether it references a logical
 * game-save slot.
 *
 * @param str  String to be parsed. Parse is divided into three passes.
 *   Pass 1: Check for a known game-save name which matches this.
 *      Search is in ascending logical slot order 0...N (where N is
 *      the number of available save slots in the current game).
 *   Pass 2: Check for keyword identifiers.
 *      <quick> = The currently nominated "quick save" slot.
 *   Pass 3: Check for a logical save slot identifier.
 *
 * @return  Save slot identifier of the slot else @c -1
 */
int SV_ParseGameSaveSlot(const char* str);
boolean SV_GetGameSavePathForSlot(int slot, ddstring_t* path);

#if __JHEXEN__

void SV_HxInitBaseSlot(void);
void SV_HxUpdateRebornSlot(void);
void SV_HxClearRebornSlot(void);
boolean SV_HxRebornSlotAvailable(void);
int SV_HxGetRebornSlot(void);
boolean SV_ExistingFile(char *name);

/**
 * Deletes all save game files associated with a slot number.
 */
void SV_ClearSaveSlot(int slot);

/**
 * Copies all the save game files from one slot to another.
 */
void SV_CopySaveSlot(int sourceSlot, int destSlot);

saveptr_t* SV_HxSavePtr(void);

#endif // __JHEXEN__

/*
 * Writing and reading values
 */
void SV_Write(const void* data, int len);
void SV_WriteByte(byte val);
#if __JHEXEN__
void SV_WriteShort(unsigned short val);
#else
void SV_WriteShort(short val);
#endif
#if __JHEXEN__
void SV_WriteLong(unsigned int val);
#else
void SV_WriteLong(long val);
#endif
void SV_WriteFloat(float val);
void SV_Read(void* data, int len);
byte SV_ReadByte(void);
short SV_ReadShort(void);
long SV_ReadLong(void);
float SV_ReadFloat(void);

void SV_MaterialArchive_Write(MaterialArchive* arc);
void SV_MaterialArchive_Read(MaterialArchive* arc, int version);

#endif /* LIBCOMMON_SAVESTATE_INPUT_OUTPUT_H */
