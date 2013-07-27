/**\file g_game.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

/**
 * Top-level (common) game routines jHexen - specific.
 */

#ifndef LIBJHEXEN_G_GAME_H
#define LIBJHEXEN_G_GAME_H

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "p_mobj.h"
#include "x_player.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int gaSaveGameSaveSlot;
extern int gaLoadGameSaveSlot;

extern player_t players[MAXPLAYERS];

extern uint gameEpisode;
extern uint gameMap;
extern uint gameMapEntryPoint;
extern skillmode_t gameSkill;

extern boolean deathmatch;
extern boolean paused;
extern boolean precache;
extern boolean userGame;
extern boolean customPal;

extern skillmode_t dSkill;

extern uint nextMap;
extern uint nextMapEntryPoint;
extern boolean briefDisabled;

extern int gsvMapMusic;

void            G_CommonShutdown(void);

void            R_InitRefresh(void);
void            R_GetTranslation(int plrClass, int plrColor, int* tclass, int* tmap);
void            Mobj_UpdateTranslationClassAndMap(mobj_t* mo);

void            G_PrintMapList(void);

int             G_BriefingEnabled(uint episode, uint map, ddfinale_t* fin);
int             G_DebriefingEnabled(uint episode, uint map, ddfinale_t* fin);

void            G_QuitGame(void);

/// @return  @c true = loading is presently possible.
boolean G_IsLoadGamePossible(void);

/**
 * To be called to schedule a load game-save action.
 * @param slot  Logical identifier of the save slot to use.
 * @return  @c true iff @a saveSlot is in use and loading is presently possible.
 */
boolean G_LoadGame(int slot);

/// @return  @c true = saving is presently possible.
boolean G_IsSaveGamePossible(void);

/**
 * To be called to schedule a save game-save action.
 * @param slot  Logical identifier of the save slot to use.
 * @param name  New name for the game-save. Can be @c NULL in which case
 *      the name will not change if the slot has already been used.
 *      If an empty string a new name will be generated automatically.
 * @return  @c true iff @a saveSlot is valid and saving is presently possible.
 */
boolean G_SaveGame2(int slot, const char* name);
boolean G_SaveGame(int slot);

void            G_CommonPreInit(void);
void            G_CommonPostInit(void);

int             G_GetInteger(int id);
void*           G_GetVariable(int id);

void            G_PlayerReborn(int player);
void            G_DeathMatchSpawnPlayer(int playernum);
void            G_DeferredPlayDemo(char* demo);
void            G_DoPlayDemo(void);

void            G_PlayDemo(char* name);
void            G_TimeDemo(char* name);
void            G_IntermissionDone(void);
void            G_ScreenShot(void);
void            G_DoReborn(int playernum);
void            G_StopDemo(void);

void            G_Ticker(timespan_t ticLength);

/// @return  @c true if the input event @a ev was eaten.
int G_PrivilegedResponder(event_t* ev);

/// @return  @c true if the input event @a ev was eaten.
int G_Responder(event_t* ev);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBJHEXEN_G_GAME_H */
