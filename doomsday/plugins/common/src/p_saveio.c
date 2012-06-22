/**\file p_saveio.c
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

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_saveg.h"
#include "p_saveio.h"
#include "p_savedef.h"
#include "materialarchive.h"

#define MAX_HUB_MAPS 99

static boolean inited;
static LZFILE* savefile;
static ddstring_t savePath; // e.g., "savegame/"
#if !__JHEXEN__
static ddstring_t clientSavePath; // e.g., "savegame/client/"
#endif
static gamesaveinfo_t* gameSaveInfo;
static gamesaveinfo_t autoGameSaveInfo;

#if __JHEXEN__
static saveptr_t saveptr;
#endif

static boolean readGameSaveHeader(gamesaveinfo_t* info);

static void errorIfNotInited(const char* callerName)
{
    if(inited) return;
    Con_Error("%s: Savegame I/O is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

static void initGameSaveInfo(gamesaveinfo_t* info)
{
    if(!info) return;
    Str_Init(&info->filePath);
    Str_Init(&info->name);
}

static void updateGameSaveInfo(gamesaveinfo_t* info, ddstring_t* savePath)
{
    if(!info) return;

    Str_CopyOrClear(&info->filePath, savePath);
    if(Str_IsEmpty(&info->filePath))
    {
        // The save path cannot be accessed for some reason. Perhaps its a
        // network path? Clear the info for this slot.
        Str_Clear(&info->name);
        return;
    }

    if(!readGameSaveHeader(info))
    {
        // Not a valid save file.
        Str_Clear(&info->filePath);
    }
}

static void clearGameSaveInfo(gamesaveinfo_t* info)
{
    if(!info) return;
    Str_Free(&info->filePath);
    Str_Free(&info->name);
}

static boolean existingFile(char* name)
{
    FILE* fp;
    if((fp = fopen(name, "rb")) != NULL)
    {
        fclose(fp);
        return true;
    }
    return false;
}

static int removeFile(const ddstring_t* path)
{
    if(!path) return 1;
    return remove(Str_Text(path));
}

static void copyFile(const ddstring_t* srcPath, const ddstring_t* destPath)
{
    size_t length;
    char* buffer;
    LZFILE* outf;

    if(!srcPath || !destPath) return;
    if(!existingFile(Str_Text(srcPath))) return;

    length = M_ReadFile(Str_Text(srcPath), &buffer);
    if(0 == length)
    {
        Con_Message("Warning: copyFile: Failed opening \"%s\" for reading.\n", Str_Text(srcPath));
        return;
    }

    outf = lzOpen((char*)Str_Text(destPath), "wp");
    if(outf)
    {
        lzWrite(buffer, length, outf);
        lzClose(outf);
    }
    Z_Free(buffer);
}

/// @return  Possibly relative saved game directory. Does not need to be free'd.
static AutoStr* composeSaveDir(void)
{
    AutoStr* dir = AutoStr_NewStd();

    if(CommandLine_CheckWith("-savedir", 1))
    {
        Str_Set(dir, CommandLine_Next());
        // Add a trailing backslash is necessary.
        if(Str_RAt(dir, 0) != '/')
            Str_AppendChar(dir, '/');
        return dir;
    }

    // Use the default path.
    { GameInfo gameInfo;
    if(DD_GameInfo(&gameInfo))
    {
        Str_Appendf(dir, SAVEGAME_DEFAULT_DIR "/%s/", gameInfo.identityKey);
        return dir;
    }}

    Con_Error("composeSaveDir: Error, failed retrieving GameInfo.");
    exit(1); // Unreachable.
}

/**
 * Compose the (possibly relative) path to the game-save associated
 * with the logical save @a slot.
 *
 * @param slot  Logical save slot identifier.
 * @param map   If @c >= 0 include this logical map index in the composed path.
 * @return  The composed path if reachable (else @c NULL). Does not need to be free'd.
 */
static AutoStr* composeGameSavePathForSlot2(int slot, int map)
{
    AutoStr* path;
    assert(inited);

    // A valid slot?
    if(!SV_IsValidSlot(slot)) return NULL;

    // Do we have a valid path?
    if(!F_MakePath(SV_SavePath())) return NULL;

    // Compose the full game-save path and filename.
    path = AutoStr_NewStd();
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

static AutoStr* composeGameSavePathForSlot(int slot)
{
    return composeGameSavePathForSlot2(slot, -1);
}

void SV_InitIO(void)
{
    Str_Init(&savePath);
#if !__JHEXEN__
    Str_Init(&clientSavePath);
#endif
    gameSaveInfo = NULL;
    inited = true;
    savefile = 0;
}

void SV_ShutdownIO(void)
{
    inited = false;

    SV_CloseFile();

    if(gameSaveInfo)
    {
        int i;
        for(i = 0; i < NUMSAVESLOTS; ++i)
        {
            gamesaveinfo_t* info = &gameSaveInfo[i];
            clearGameSaveInfo(info);
        }
        free(gameSaveInfo); gameSaveInfo = NULL;

        clearGameSaveInfo(&autoGameSaveInfo);
    }

    Str_Free(&savePath);
#if !__JHEXEN__
    Str_Free(&clientSavePath);
#endif
}

const char* SV_SavePath(void)
{
    return Str_Text(&savePath);
}

#ifdef __JHEXEN__
saveptr_t* SV_HxSavePtr(void)
{
    return &saveptr;
}
#endif

#if !__JHEXEN__
const char* SV_ClientSavePath(void)
{
    return Str_Text(&clientSavePath);
}
#endif

// Compose and create the saved game directories.
void SV_ConfigureSavePaths(void)
{
    assert(inited);
    {
    AutoStr* saveDir = composeSaveDir();
    boolean savePathExists;

    Str_Set(&savePath, Str_Text(saveDir));
#if !__JHEXEN__
    Str_Clear(&clientSavePath); Str_Appendf(&clientSavePath, "%sclient/", Str_Text(saveDir));
#endif

    // Ensure that these paths exist.
    savePathExists = F_MakePath(Str_Text(&savePath));
#if !__JHEXEN__
    if(!F_MakePath(Str_Text(&clientSavePath)))
        savePathExists = false;
#endif
    if(!savePathExists)
        Con_Message("Warning:configureSavePaths: Failed to locate \"%s\"\nPerhaps it could "
                    "not be created (insufficent permissions?). Saving will not be possible.\n",
                    Str_Text(&savePath));
    }
}

LZFILE* SV_File(void)
{
    return savefile;
}

LZFILE* SV_OpenFile(const char* fileName, const char* mode)
{
    assert(savefile == 0);
    savefile = lzOpen((char*)fileName, (char*)mode);
    return savefile;
}

void SV_CloseFile(void)
{
    if(savefile)
    {
        lzClose(savefile);
        savefile = 0;
    }
}

void SV_ClearSaveSlot(int slot)
{
    AutoStr* path;

    errorIfNotInited("SV_ClearSaveSlot");
    if(!SV_IsValidSlot(slot)) return;

    { int i;
    for(i = 0; i < MAX_HUB_MAPS; ++i)
    {
        path = composeGameSavePathForSlot2(slot, i);
        removeFile(path);
    }}

    path = composeGameSavePathForSlot(slot);
    removeFile(path);
}

boolean SV_IsValidSlot(int slot)
{
    if(slot == AUTO_SLOT) return true;
#if __JHEXEN__
    if(slot == BASE_SLOT) return true;
#endif
    return (slot >= 0  && slot < NUMSAVESLOTS);
}

boolean SV_IsUserWritableSlot(int slot)
{
    if(slot == AUTO_SLOT) return false;
#if __JHEXEN__
    if(slot == BASE_SLOT) return false;
#endif
    return SV_IsValidSlot(slot);
}

static boolean readGameSaveHeader(gamesaveinfo_t* info)
{
    boolean found = false;
#if __JHEXEN__
    LZFILE* fp;
#endif
    assert(inited && info);

    if(Str_IsEmpty(&info->filePath)) return false;

#if __JHEXEN__
    fp = lzOpen(Str_Text(&info->filePath), "rp");
    if(fp)
    {
        // Read the header.
        char versionText[HXS_VERSION_TEXT_LENGTH];
        char nameBuffer[SAVESTRINGSIZE];
        lzRead(nameBuffer, SAVESTRINGSIZE, fp);
        lzRead(versionText, HXS_VERSION_TEXT_LENGTH, fp);
        lzClose(fp); fp = NULL;
        if(!strncmp(versionText, HXS_VERSION_TEXT, 8))
        {
            const int ver = atoi(&versionText[8]);
            if(ver <= MY_SAVE_VERSION)
            {
                Str_Set(&info->name, nameBuffer);
                found = true;
            }
        }
    }
#else
    if(SV_OpenFile(Str_Text(&info->filePath), "rp"))
    {
        saveheader_t* hdr = SV_SaveHeader();

        SV_Header_Read(hdr);
        SV_CloseFile();

        if(MY_SAVE_MAGIC == hdr->magic)
        {
            Str_Set(&info->name, hdr->name);
            found = true;
        }
    }

    // If not found or not recognized try other supported formats.
#if !__JDOOM64__
    if(!found)
    {
        // Perhaps a DOOM(2).EXE v19 saved game?
        if(SV_OpenFile(Str_Text(&info->filePath), "r"))
        {
            char nameBuffer[SAVESTRINGSIZE];
            lzRead(nameBuffer, SAVESTRINGSIZE, SV_File());
            nameBuffer[SAVESTRINGSIZE - 1] = 0;
            Str_Set(&info->name, nameBuffer);
            SV_CloseFile();
            found = true;
        }
    }
# endif
#endif

    // Ensure we have a non-empty name.
    if(found && Str_IsEmpty(&info->name))
    {
        Str_Set(&info->name, "UNNAMED");
    }

    return found;
}

/// Re-build game-save info by re-scanning the save paths and populating the list.
static void buildGameSaveInfo(void)
{
    int i;
    assert(inited);

    if(!gameSaveInfo)
    {
        // Not yet been here. We need to allocate and initialize the game-save info list.
        gameSaveInfo = (gamesaveinfo_t*) malloc(NUMSAVESLOTS * sizeof(*gameSaveInfo));
        if(!gameSaveInfo)
            Con_Error("buildGameSaveInfo: Failed on allocation of %lu bytes for game-save info list.",
                      (unsigned long) (NUMSAVESLOTS * sizeof(*gameSaveInfo)));

        // Initialize.
        for(i = 0; i < NUMSAVESLOTS; ++i)
        {
            gamesaveinfo_t* info = &gameSaveInfo[i];
            initGameSaveInfo(info);
        }
        initGameSaveInfo(&autoGameSaveInfo);
    }

    /// Scan the save paths and populate the list.
    /// \todo We should look at all files on the save path and not just those
    /// which match the default game-save file naming convention.
    for(i = 0; i < NUMSAVESLOTS; ++i)
    {
        gamesaveinfo_t* info = &gameSaveInfo[i];
        updateGameSaveInfo(info, composeGameSavePathForSlot(i));
    }
    updateGameSaveInfo(&autoGameSaveInfo, composeGameSavePathForSlot(AUTO_SLOT));
}

/// Given a logical save slot identifier retrieve the assciated game-save info.
static gamesaveinfo_t* findGameSaveInfoForSlot(int slot)
{
    static gamesaveinfo_t invalidInfo = { { "" }, { "" } };
    assert(inited);

    if(slot == AUTO_SLOT)
    {
        // On first call - automatically build and populate game-save info.
        if(!gameSaveInfo)
            buildGameSaveInfo();
        // Retrieve the info for this slot.
        return &autoGameSaveInfo;
    }
    if(slot >= 0 && slot < NUMSAVESLOTS)
    {
        // On first call - automatically build and populate game-save info.
        if(!gameSaveInfo)
            buildGameSaveInfo();
        // Retrieve the info for this slot.
        return &gameSaveInfo[slot];
    }
    return &invalidInfo;
}

const gamesaveinfo_t* SV_GameSaveInfoForSlot(int slot)
{
    errorIfNotInited("SV_GameSaveInfoForSlot");
    return findGameSaveInfoForSlot(slot);
}

void SV_UpdateGameSaveInfo(void)
{
    errorIfNotInited("SV_UpdateGameSaveInfo");
    buildGameSaveInfo();
}

int SV_ParseGameSaveSlot(const char* str)
{
    int slot;

    // Try game-save name match.
    slot = SV_FindGameSaveSlotForName(str);
    if(slot >= 0)
    {
        return slot;
    }

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

int SV_FindGameSaveSlotForName(const char* name)
{
    int saveSlot = -1;

    errorIfNotInited("SV_FindGameSaveSlotForName");

    if(name && name[0])
    {
        int i = 0;
        // On first call - automatically build and populate game-save info.
        if(!gameSaveInfo)
        {
            buildGameSaveInfo();
        }

        do
        {
            const gamesaveinfo_t* info = &gameSaveInfo[i];
            if(!Str_CompareIgnoreCase(&info->name, name))
            {
                // This is the one!
                saveSlot = i;
            }
        } while(-1 == saveSlot && ++i < NUMSAVESLOTS);
    }
    return saveSlot;
}

boolean SV_GameSavePathForSlot(int slot, ddstring_t* path)
{
    errorIfNotInited("SV_GameSavePathForSlot");
    if(!path) return false;
    Str_CopyOrClear(path, composeGameSavePathForSlot(slot));
    return !Str_IsEmpty(path);
}

#if __JHEXEN__
boolean SV_GameSavePathForMapSlot(uint map, int slot, ddstring_t* path)
{
    errorIfNotInited("SV_GameSavePathForMapSlot");
    if(!path) return false;
    Str_CopyOrClear(path, composeGameSavePathForSlot2(slot, (int)map));
    return !Str_IsEmpty(path);
}
#else
/**
 * Compose the (possibly relative) path to the game-save associated
 * with @a gameId. If the game-save path is unreachable then @a path
 * will be made empty.
 *
 * @param gameId  Unique game identifier.
 * @param path  String buffer to populate with the game save path.
 * @return  @c true if @a path was set.
 */
static boolean composeClientGameSavePathForGameId(uint gameId, ddstring_t* path)
{
    assert(inited && NULL != path);
    // Do we have a valid path?
    if(!F_MakePath(SV_ClientSavePath())) return false;
    // Compose the full game-save path and filename.
    Str_Clear(path);
    Str_Appendf(path, "%s" CLIENTSAVEGAMENAME "%08X." SAVEGAMEEXTENSION, SV_ClientSavePath(), gameId);
    F_TranslatePath(path, path);
    return true;
}

boolean SV_ClientGameSavePathForGameId(uint gameId, ddstring_t* path)
{
    errorIfNotInited("SV_GameSavePathForSlot");
    if(!path) return false;
    Str_Clear(path);
    return composeClientGameSavePathForGameId(gameId, path);
}
#endif

boolean SV_IsGameSaveSlotUsed(int slot)
{
    const gamesaveinfo_t* info;
    errorIfNotInited("SV_IsGameSaveSlotUsed");

    info = SV_GameSaveInfoForSlot(slot);
    return !Str_IsEmpty(&info->filePath);
}

#if __JHEXEN__
boolean SV_HxGameSaveSlotHasMapState(int slot, uint map)
{
    AutoStr* path = composeGameSavePathForSlot2(slot, (int)map);
    if(!path || Str_IsEmpty(path)) return false;
    return existingFile(Str_Text(path));
}
#endif

void SV_CopySaveSlot(int sourceSlot, int destSlot)
{
    AutoStr* src, *dst;

    errorIfNotInited("SV_CopySaveSlot");

    if(!SV_IsValidSlot(sourceSlot))
    {
#if _DEBUG
        Con_Message("Warning: SV_CopySaveSlot: Source slot %i invalid, save game not copied.\n", sourceSlot);
#endif
        return;
    }

    if(!SV_IsValidSlot(destSlot))
    {
#if _DEBUG
        Con_Message("Warning: SV_CopySaveSlot: Dest slot %i invalid, save game not copied.\n", destSlot);
#endif
        return;
    }

    { int i;
    for(i = 0; i < MAX_HUB_MAPS; ++i)
    {
        src = composeGameSavePathForSlot2(sourceSlot, i);
        dst = composeGameSavePathForSlot2(destSlot, i);
        copyFile(src, dst);
    }}

    src = composeGameSavePathForSlot(sourceSlot);
    dst = composeGameSavePathForSlot(destSlot);
    copyFile(src, dst);
}

void SV_Write(const void* data, int len)
{
    errorIfNotInited("SV_Write");
    lzWrite((void*)data, len, savefile);
}

void SV_WriteByte(byte val)
{
    errorIfNotInited("SV_WriteByte");
    lzPutC(val, savefile);
}

#if __JHEXEN__
void SV_WriteShort(unsigned short val)
#else
void SV_WriteShort(short val)
#endif
{
    errorIfNotInited("SV_WriteShort");
    lzPutW(val, savefile);
}

#if __JHEXEN__
void SV_WriteLong(unsigned int val)
#else
void SV_WriteLong(long val)
#endif
{
    errorIfNotInited("SV_WriteLong");
    lzPutL(val, savefile);
}

void SV_WriteFloat(float val)
{
    int32_t temp = 0;
    assert(sizeof(val) == 4);
    errorIfNotInited("SV_WriteFloat");
    memcpy(&temp, &val, 4);
    lzPutL(temp, savefile);
}

void SV_Seek(uint offset)
{
    errorIfNotInited("SV_SetPos");
#if __JHEXEN__
    saveptr.b += offset;
#else
    lzSeek(savefile, offset);
#endif
}

void SV_Read(void *data, int len)
{
    errorIfNotInited("SV_Read");
#if __JHEXEN__
    memcpy(data, saveptr.b, len);
    saveptr.b += len;
#else
    lzRead(data, len, savefile);
#endif
}

byte SV_ReadByte(void)
{
    errorIfNotInited("SV_ReadByte");
#if __JHEXEN__
    return (*saveptr.b++);
#else
    return lzGetC(savefile);
#endif
}

short SV_ReadShort(void)
{
    errorIfNotInited("SV_ReadShort");
#if __JHEXEN__
    return (SHORT(*saveptr.w++));
#else
    return lzGetW(savefile);
#endif
}

long SV_ReadLong(void)
{
    errorIfNotInited("SV_ReadLong");
#if __JHEXEN__
    return (LONG(*saveptr.l++));
#else
    return lzGetL(savefile);
#endif
}

float SV_ReadFloat(void)
{
#if !__JHEXEN__
    float returnValue = 0;
    int32_t val;
#endif
    errorIfNotInited("SV_ReadFloat");
#if __JHEXEN__
    return (FLOAT(*saveptr.f++));
#else
    val = lzGetL(savefile);
    returnValue = 0;
    assert(sizeof(float) == 4);
    memcpy(&returnValue, &val, 4);
    return returnValue;
#endif
}

static void swi8(Writer* w, char i)
{
    if(!w) return;
    SV_WriteByte(i);
}

static void swi16(Writer* w, short i)
{
    if(!w) return;
    SV_WriteShort(i);
}

static void swi32(Writer* w, int i)
{
    if(!w) return;
    SV_WriteLong(i);
}

static void swf(Writer* w, float i)
{
    if(!w) return;
    SV_WriteFloat(i);
}

static void swd(Writer* w, const char* data, int len)
{
    if(!w) return;
    SV_Write(data, len);
}

#if !__JHEXEN__
void SV_Header_Write(saveheader_t* hdr)
{
    Writer* svWriter = Writer_NewWithCallbacks(swi8, swi16, swi32, swf, swd);
    ddstring_t name;

    Writer_WriteInt32(svWriter, hdr->magic);
    Writer_WriteInt32(svWriter, hdr->version);
    Writer_WriteInt32(svWriter, hdr->gameMode);

    Str_InitStatic(&name, hdr->name);
    Str_Write(&name, svWriter);

    Writer_WriteByte(svWriter, hdr->skill);
    Writer_WriteByte(svWriter, hdr->episode);
    Writer_WriteByte(svWriter, hdr->map);
    Writer_WriteByte(svWriter, hdr->deathmatch);
    Writer_WriteByte(svWriter, hdr->noMonsters);
    Writer_WriteByte(svWriter, hdr->respawnMonsters);
    Writer_WriteInt32(svWriter, hdr->mapTime);

    { int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        Writer_WriteByte(svWriter, hdr->players[i]);
    }}

    Writer_WriteInt32(svWriter, hdr->gameId);

    Writer_Delete(svWriter);
}
#endif

void SV_MaterialArchive_Write(MaterialArchive* arc)
{
    Writer* svWriter = Writer_NewWithCallbacks(swi8, swi16, swi32, swf, swd);
    MaterialArchive_Write(arc, svWriter);
    Writer_Delete(svWriter);
}

static char sri8(Reader* r)
{
    if(!r) return 0;
    return SV_ReadByte();
}

static short sri16(Reader* r)
{
    if(!r) return 0;
    return SV_ReadShort();
}

static int sri32(Reader* r)
{
    if(!r) return 0;
    return SV_ReadLong();
}

static float srf(Reader* r)
{
    if(!r) return 0;
    return SV_ReadFloat();
}

static void srd(Reader* r, char* data, int len)
{
    if(!r) return;
    SV_Read(data, len);
}

#if __JHEXEN__
void SV_Header_Read(saveheader_t* hdr)
{
    // Skip the name field.
    SV_HxSavePtr()->b += SAVESTRINGSIZE;

    memcpy(hdr->magic, SV_HxSavePtr()->b, 8);

    hdr->version = atoi((const char*) (SV_HxSavePtr()->b + 8));
    SV_HxSavePtr()->b += HXS_VERSION_TEXT_LENGTH;

    SV_AssertSegment(ASEG_GAME_HEADER);
    hdr->episode = 1;
    hdr->map = SV_ReadByte();
    hdr->skill = SV_ReadByte();
    hdr->deathmatch = SV_ReadByte();
    hdr->noMonsters = SV_ReadByte();
    hdr->randomClasses = SV_ReadByte();
}
#else
void SV_Header_Read(saveheader_t* hdr)
{
    Reader* svReader = Reader_NewWithCallbacks(sri8, sri16, sri32, srf, srd);

    hdr->magic = Reader_ReadInt32(svReader);
    hdr->version = Reader_ReadInt32(svReader);
    hdr->gameMode = Reader_ReadInt32(svReader);

    if(hdr->version >= 10)
    {
        ddstring_t buf;
        Str_InitStd(&buf);
        Str_Read(&buf, svReader);
        memcpy(hdr->name, Str_Text(&buf), SAVESTRINGSIZE);
        hdr->name[SAVESTRINGSIZE] = '\0';
        Str_Free(&buf);
    }
    else
    {
        // Older formats use a fixed-length name (24 characters).
        Reader_Read(svReader, hdr->name, SAVESTRINGSIZE);
    }
    hdr->skill = Reader_ReadByte(svReader);
    hdr->episode = Reader_ReadByte(svReader);
    hdr->map = Reader_ReadByte(svReader);
    hdr->deathmatch = Reader_ReadByte(svReader);
    hdr->noMonsters = Reader_ReadByte(svReader);
    hdr->respawnMonsters = Reader_ReadByte(svReader);

    // Older formats serialize the unpacked header struct; skip the junk values (alignment).
    if(hdr->version < 10) SV_Seek(2);

    hdr->mapTime = Reader_ReadInt32(svReader);

    { int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hdr->players[i] = Reader_ReadByte(svReader);
    }}
    hdr->gameId = Reader_ReadInt32(svReader);

    Reader_Delete(svReader);

    // Translate gameMode identifiers from older save versions.
#if __JDOOM__ || __JHERETIC__
# if __JDOOM__ //|| __JHEXEN__
    if(hdr->version < 9)
# elif __JHERETIC__
    if(hdr->version < 8)
# endif
    {
        static const gamemode_t oldGameModes[] = {
# if __JDOOM__
            doom_shareware,
            doom,
            doom2,
            doom_ultimate
# elif __JHERETIC__
            heretic_shareware,
            heretic,
            heretic_extended
# elif __JHEXEN__
            hexen_demo,
            hexen,
            hexen_deathkings
# endif
        };
        hdr->gameMode = oldGameModes[(int)hdr->gameMode];
#  if __JDOOM__
        /**
         * \kludge Older versions did not differentiate between versions of
         * Doom2 (i.e., Plutonia and TNT are marked as Doom2). If we detect
         * that this save is from some version of Doom2, replace the marked
         * gamemode with the current gamemode.
         */
        if(hdr->gameMode == doom2 && (gameModeBits & GM_ANY_DOOM2))
        {
            hdr->gameMode = gameMode;
        }
        /// kludge end.
#  endif
    }
#endif
}
#endif

void SV_MaterialArchive_Read(MaterialArchive* arc, int version)
{
    Reader* svReader = Reader_NewWithCallbacks(sri8, sri16, sri32, srf, srd);
    MaterialArchive_Read(arc, version, svReader);
    Reader_Delete(svReader);
}
