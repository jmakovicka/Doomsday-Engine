/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * def_read.c: Doomsday Engine Definition File Reader
 */

/**
 * A GHASTLY MESS!!! This should be rewritten.
 *
 * You have been warned!
 *
 * At the moment the idea is that a lot of macros are used to read a more
 * or less fixed structure of definitions, and if an error occurs then
 * "goto out_of_here;". It leads to a lot more code than an elegant parser
 * would require.
 *
 * The current implementation of the reader is a "structural" approach:
 * the definition file is parsed based on the structure implied by
 * the read tokens. A true parser would have syntax definitions for
 * a bunch of tokens, and the same parsing rules would be applied for
 * everything.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"
#include "de_refresh.h"
#include "de_defs.h"

// XGClass.h is actually a part of the engine.
#include "../../../plugins/common/include/xgclass.h"

// MACROS ------------------------------------------------------------------

#define MAX_RECUR_DEPTH     30

// Some macros.
#define STOPCHAR(x) (isspace(x) || x == ';' || x == '#' || x == '{' \
                    || x == '}' || x == '=' || x == '"' || x == '*' \
                    || x == '|')

#define ISTOKEN(X)  (!stricmp(token, X))

#define READSTR(X)  if(!ReadString(X, sizeof(X))) { \
                    SetError("Syntax error in string value."); \
                    retval = false; goto ded_end_read; }

#define MISSING_SC_ERROR    SetError("Missing semicolon."); \
                            retval = false; goto ded_end_read;

#define CHECKSC     if(source->version <= 5) { ReadToken(); if(!ISTOKEN(";")) { MISSING_SC_ERROR; } }

#define FINDBEGIN   while(!ISTOKEN("{") && !source->atEnd) ReadToken();
#define FINDEND     while(!ISTOKEN("}") && !source->atEnd) ReadToken();

#define ISLABEL(X)  (!stricmp(label, X))

#define READLABEL   if(!ReadLabel(label)) { retval = false; goto ded_end_read; } \
                    if(ISLABEL("}")) break;

#define READLABEL_NOBREAK   if(!ReadLabel(label)) { retval = false; goto ded_end_read; }

#define FAILURE     retval = false; goto ded_end_read;
#define READBYTE(X) if(!ReadByte(&X)) { FAILURE }
#define READINT(X)  if(!ReadInt(&X, false)) { FAILURE }
#define READUINT(X) if(!ReadInt(&X, true)) { FAILURE }
#define READFLT(X)  if(!ReadFloat(&X)) { FAILURE }
#define READNBYTEVEC(X,N) if(!ReadNByteVector(X, N)) { FAILURE }
#define READFLAGS(X,P) if(!ReadFlags(&X, P)) { FAILURE }

#define RV_BYTE(lab, X) if(ISLABEL(lab)) { READBYTE(X); } else
#define RV_INT(lab, X)  if(ISLABEL(lab)) { READINT(X); } else
#define RV_UINT(lab, X) if(ISLABEL(lab)) { READUINT(X); } else
#define RV_FLT(lab, X)  if(ISLABEL(lab)) { READFLT(X); } else
#define RV_VEC(lab, X, N)   if(ISLABEL(lab)) { int b; FINDBEGIN; \
                        for(b=0; b<N; ++b) {READFLT(X[b])} ReadToken(); } else
#define RV_IVEC(lab, X, N)  if(ISLABEL(lab)) { int b; FINDBEGIN; \
                        for(b=0; b<N; ++b) {READINT(X[b])} ReadToken(); } else
#define RV_NBVEC(lab, X, N) if(ISLABEL(lab)) { READNBYTEVEC(X,N); } else
#define RV_STR(lab, X)  if(ISLABEL(lab)) { READSTR(X); } else
#define RV_STR_INT(lab, S, I)   if(ISLABEL(lab)) { if(!ReadString(S,sizeof(S))) \
                                I = strtol(token,0,0); } else
#define RV_FLAGS(lab, X, P) if(ISLABEL(lab)) { READFLAGS(X, P); } else
#define RV_ANYSTR(lab, X)   if(ISLABEL(lab)) { if(!ReadAnyString(&X)) { FAILURE } } else
#define RV_END          { SetError2("Unknown label.", label); retval = false; goto ded_end_read; }

#define RV_XGIPARM(lab, S, I, IP) if(ISLABEL(lab)) { \
                   if(xgClassLinks[l->line_class].iparm[IP].flagprefix) { \
                   if(!ReadFlags(&I, xgClassLinks[l->line_class].iparm[IP].flagprefix)) { \
                     FAILURE } } \
                   else { if(!ReadString(S,sizeof(S))) { \
                     I = strtol(token,0,0); } } } else

// TYPES -------------------------------------------------------------------

typedef struct dedsource_s {
    char   *buffer;
    char   *pos;
    boolean atEnd;
    int     lineNumber;
    const char *fileName;
    int     version;            // v6 does not require semicolons.
} dedsource_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// the actual classes are game-side
extern xgclass_t *xgClassLinks;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char    dedReadError[512];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static dedsource_t sourceStack[MAX_RECUR_DEPTH];
static dedsource_t *source;     // Points to the current source.

static char    token[128];
static char    unreadToken[128];

// CODE --------------------------------------------------------------------

static char *sdup(const char *str)
{
    char   *newstr;

    if(!str)
        return NULL;
    newstr = M_Malloc(strlen(str) + 1);
    strcpy(newstr, str);
    return newstr;
}

static void SetError(char *str)
{
    sprintf(dedReadError, "Error in %s:\n  Line %i: %s",
            source ? source->fileName : "?", source ? source->lineNumber : 0,
            str);
}

static void SetError2(char *str, char *more)
{
    sprintf(dedReadError, "Error in %s:\n  Line %i: %s (%s)",
            source ? source->fileName : "?", source ? source->lineNumber : 0,
            str, more);
}

/**
 * Reads a single character from the input file. Increments the line
 * number counter if necessary.
 */
static int FGetC(void)
{
    int     ch = (unsigned char) *source->pos;

    if(ch)
        source->pos++;
    else
        source->atEnd = true;
    if(ch == '\n')
        source->lineNumber++;
    if(ch == '\r')
        return FGetC();
    return ch;
}

/**
 * Undoes an FGetC.
 */
static int FUngetC(int ch)
{
    if(source->atEnd)
        return 0;
    if(ch == '\n')
        source->lineNumber--;
    if(source->pos > source->buffer)
        source->pos--;
    return ch;
}

/**
 * Reads stuff until a newline is found.
 */
static void SkipComment(void)
{
    int     ch = FGetC();
    boolean seq = false;

    if(ch == '\n')
        return;                 // Comment ends right away.
    if(ch != '>')               // Single-line comment?
    {
        while(FGetC() != '\n' && !source->atEnd);
    }
    else                        // Multiline comment?
    {
        while(!source->atEnd)
        {
            ch = FGetC();
            if(seq)
            {
                if(ch == '#')
                    break;
                seq = false;
            }
            if(ch == '<')
                seq = true;
        }
    }
}

static int ReadToken(void)
{
    int     ch;
    char   *out = token;

    // Has a token been unread?
    if(unreadToken[0])
    {
        strcpy(token, unreadToken);
        strcpy(unreadToken, "");
        return true;
    }

    ch = FGetC();
    if(source->atEnd)
        return false;

    // Skip whitespace and comments in the beginning.
    while((ch == '#' || isspace(ch)))
    {
        if(ch == '#')
            SkipComment();
        ch = FGetC();
        if(source->atEnd)
            return false;
    }

    // Always store the first character.
    *out++ = ch;
    if(STOPCHAR(ch))
    {
        // Stop here.
        *out = 0;
        return true;
    }

    while(!STOPCHAR(ch) && !source->atEnd)
    {
        // Store the character in the buffer.
        ch = FGetC();
        *out++ = ch;
    }
    *(out - 1) = 0;             // End token.

    // Put the last read character back in the stream.
    FUngetC(ch);
    return true;
}

static void UnreadToken(const char *token)
{
    // The next time ReadToken() is called, this is returned.
    strcpy(unreadToken, token);
}

/**
 * Current pos in the file is at the first ".
 * Does not expand escape sequences, only checks for \".
 *
 * @return          <code>true</code> if successful.
 */
static int ReadStringEx(char *dest, int maxlen, boolean inside,
                        boolean doubleq)
{
    char   *ptr = dest;
    int     ch, esc = false, newl = false;

    if(!inside)
    {
        ReadToken();
        if(!ISTOKEN("\""))
            return false;
    }
    // Start reading the characters.
    ch = FGetC();
    while(esc || ch != '"')     // The string-end-character.
    {
        if(source->atEnd)
            return false;

        // If a newline is found, skip all whitespace that follows.
        if(newl)
        {
            if(isspace(ch))
            {
                ch = FGetC();
                continue;
            }
            else
            {
                // End the skip.
                newl = false;
            }
        }

        // An escape character?
        if(!esc && ch == '\\')
            esc = true;
        else
        {
            // In case it's something other than \" or \\, just insert
            // the whole sequence as-is.
            if(esc && ch != '"' && ch != '\\')
                *ptr++ = '\\';
            esc = false;
        }
        if(ch == '\n')
            newl = true;

        // Store the character in the buffer.
        if(ptr - dest < maxlen && !esc && !newl)
        {
            *ptr++ = ch;
            if(doubleq && ch == '"')
                *ptr++ = '"';
        }

        // Read the next character, please.
        ch = FGetC();
    }
    // End the string in a null.
    *ptr = 0;
    return true;
}

static int ReadString(char *dest, int maxlen)
{
    return ReadStringEx(dest, maxlen, false, false);
}

/**
 * Read a string of (pretty much) any length.
 */
static int ReadAnyString(char **dest)
{
    char    buffer[0x20000];

    if(!ReadString(buffer, sizeof(buffer)))
        return false;

    // Get rid of the old string.
    if(*dest)
        M_Free(*dest);

    // Make sure it doesn't overflow.
    buffer[sizeof(buffer) - 1] = 0;

    // Make a copy.
    *dest = M_Malloc(strlen(buffer) + 1);
    strcpy(*dest, buffer);
    return true;
}

static int ReadNByteVector(unsigned char *dest, int max)
{
    int     i;

    FINDBEGIN;
    for(i = 0; i < max; ++i)
    {
        ReadToken();
        if(ISTOKEN("}"))
            return true;
        dest[i] = strtoul(token, 0, 0);
    }
    FINDEND;
    return true;
}

/**
 * @return          <code>true</code> if successful.
 */
static int ReadByte(unsigned char *dest)
{
    ReadToken();
    if(ISTOKEN(";"))
    {
        SetError("Missing integer value.");
        return false;
    }
    *dest = strtoul(token, 0, 0);
    return true;
}

/**
 * @return          <code>true</code> if successful.
 */
static int ReadInt(int *dest, int unsign)
{
    ReadToken();
    if(ISTOKEN(";"))
    {
        SetError("Missing integer value.");
        return false;
    }
    *dest = unsign ? strtoul(token, 0, 0) : strtol(token, 0, 0);
    return true;
}

/**
 * @return          <code>true</code> if successful.
 */
static int ReadFloat(float *dest)
{
    ReadToken();
    if(ISTOKEN(";"))
    {
        SetError("Missing float value.");
        return false;
    }
    *dest = (float) strtod(token, 0);
    return true;
}

static int ReadFlags(unsigned int *dest, const char *prefix)
{
    char    flag[1024];

    // By default, no flags are set.
    *dest = 0;

    ReadToken();
    UnreadToken(token);
    if(ISTOKEN("\""))
    {
        // The old format.
        if(!ReadString(flag, sizeof(flag)))
            return false;

        *dest = Def_EvalFlags(flag);
        return true;
    }

    for(;;)
    {
        // Read the flag.
        ReadToken();
        if(prefix)
        {
            strcpy(flag, prefix);
            strcat(flag, token);
        }
        else
        {
            strcpy(flag, token);
        }

        *dest |= Def_EvalFlags(flag);

        if(!ReadToken())
            break;

        if(!ISTOKEN("|"))       // | is required for multiple flags.
        {
            UnreadToken(token);
            break;
        }
    }
    return true;
}

/**
 * @return          <code>true</code> if successful.
 */
static int ReadLabel(char *label)
{
    strcpy(label, "");
    for(;;)
    {
        ReadToken();
        if(source->atEnd)
        {
            SetError("Unexpected end of file.");
            return false;
        }
        if(ISTOKEN("}"))        // End block.
        {
            strcpy(label, token);
            return true;
        }
        if(ISTOKEN(";"))
        {
            if(source->version <= 5)
            {
                SetError("Label without value.");
                return false;
            }
            continue;           // Semicolons are optional in v6.
        }
        if(ISTOKEN("=") || ISTOKEN("{"))
            break;

        if(label[0])
            strcat(label, " ");
        strcat(label, token);
    }
    return true;
}

static void DED_Include(char *fileName, directory_t * dir)
{
    char    tmp[256], path[256];

    M_TranslatePath(fileName, tmp);
    if(!Dir_IsAbsolute(tmp))
    {
        sprintf(path, "%s%s", dir->path, tmp);
    }
    else
    {
        strcpy(path, tmp);
    }
    Def_ReadProcessDED(path);
    strcpy(token, "");
}

static void DED_InitReader(char *buffer, const char *fileName)
{
    if(source && source - sourceStack >= MAX_RECUR_DEPTH)
    {
        Con_Error("DED_InitReader: Include recursion is too deep.\n");
    }

    if(!source)
    {
        // This'll be the first source.
        source = sourceStack;
    }
    else
    {
        // Take the next entry in the stack.
        source++;
    }

    source->pos = source->buffer = buffer;
    source->atEnd = false;
    source->lineNumber = 1;
    source->fileName = fileName;
    source->version = DED_VERSION;
}

static void DED_CloseReader(void)
{
    if(source == sourceStack)
    {
        source = NULL;
    }
    else
    {
        memset(source, 0, sizeof(*source));
        source--;
    }
}

/**
 * Return true if the condition passes. The condition token can be a
 * command line option or a game mode.
 */
static boolean DED_CheckCondition(const char *cond, boolean expected)
{
    boolean value = false;

    if(cond[0] == '-')
    {
        // It's a command line option.
        value = (ArgCheck(token) != 0);
    }
    else if(isalnum(cond[0]))
    {
        // Then it must be a game mode.
        value = !stricmp(cond, (char *) gx.GetVariable(DD_GAME_MODE));
    }

    return value == expected;
}

/**
 * Reads definitions from the given buffer.
 * The definition is being loaded from @sourcefile (DED or WAD).
 *
 * @param buffer        The data to be read, must be null-terminated.
 * @param sourceFile    Just FYI.
 */
/* *INDENT-OFF* */
static int DED_ReadData(ded_t *ded, char *buffer, const char *sourceFile)
{
    char dummy[128];
    int dummyInt;
    int idx, retval = true;
    char label[128], tmp[256];
    ded_mobj_t *mo;
    int prev_mo_idx = -1; // For "Copy".
    ded_state_t *st;
    int prev_state_idx = -1; // For "Copy"
    ded_light_t *lig;
    int prev_ligdef_idx = -1; // For "Copy".
    ded_model_t *mdl, *prevmdl = 0;
    int prev_modef_idx = -1; // For "Copy".
    ded_sound_t *snd;
    ded_mapinfo_t *mi;
    int prev_mapinfo_idx = -1; // For "Copy".
    ded_str_t *tn;
    ded_value_t *val;
    ded_detailtexture_t *dtl;
    int prev_dtldef_idx = -1; // For "Copy".
    ded_ptcgen_t *gen;
    int prev_gendef_idx = -1; // For "Copy".
    int prev_decordef_idx = -1; // For "Copy".
    int prev_refdef_idx = -1; // For "Copy".
    ded_finale_t *fin;
    ded_linetype_t *l;
    int prev_linetype_idx = -1; // For "Copy".
    ded_sectortype_t *sec;
    int prev_sectortype_idx = -1; // For "Copy".
    ded_lumpformat_t *lmpf;
    int prev_lumpformat_idx = -1; // For "Copy".
//    ded_xgclass_t *xgc;

    int sub;
    int depth;
    char *rootstr = 0, *ptr;
    int bCopyNext = false;
    directory_t fileDir;

    // For including other files -- we must know where we are.
    Dir_FileDir(sourceFile, &fileDir);

    // Get the next entry from the source stack.
    DED_InitReader(buffer, sourceFile);

    while(ReadToken())
    {
        if(ISTOKEN("Copy") || ISTOKEN("*"))
        {
            bCopyNext = true;
            continue; // Read the next token.
        }
        if(ISTOKEN(";"))
        {
            // Unnecessary semicolon? Just skip it.
            continue;
        }
        if(ISTOKEN("SkipIf"))
        {
            sub = 1;
            ReadToken();
            if(ISTOKEN("Not"))
            {
                sub = 0;
                ReadToken();
            }
            if(DED_CheckCondition(token, sub))
            {
                // Ah, we're done. Get out of here.
                goto ded_end_read;
            }
            CHECKSC;
        }
        if(ISTOKEN("Include"))
        {
            // A new include.
            READSTR(tmp);
            CHECKSC;

            DED_Include(tmp, &fileDir);
            strcpy(label, "");
        }
        if(ISTOKEN("IncludeIf")) // An optional include.
        {
            sub = 1;
            ReadToken();
            if(ISTOKEN("Not"))
            {
                sub = 0;
                ReadToken();
            }
            if(DED_CheckCondition(token, sub))
            {
                READSTR(tmp);
                CHECKSC;

                DED_Include(tmp, &fileDir);
                strcpy(label, "");
            }
            else
            {
                // Arg not found; just skip it.
                READSTR(tmp);
                CHECKSC;
            }
        }
        if(ISTOKEN("ModelPath"))
        {
            // A new model path. Append to the list.
            READSTR(label);
            CHECKSC;
            R_AddModelPath(label, true);
        }
        if(ISTOKEN("Header"))
        {
            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                if(ISLABEL("Version"))
                {
                    READINT(ded->version);
                    source->version = ded->version;
                }
                else RV_STR("Thing prefix", dummy)
                RV_STR("State prefix", dummy)
                RV_STR("Sprite prefix", dummy)
                RV_STR("Sfx prefix", dummy)
                RV_STR("Mus prefix", dummy)
                RV_STR("Text prefix", dummy)
                RV_STR("Model path", ded->model_path)
                RV_FLAGS("Common model flags", ded->model_flags, "df_")
                RV_FLT("Default model scale", ded->model_scale)
                RV_FLT("Default model offset", ded->model_offset)
                RV_END
                CHECKSC;
            }
        }
        if(ISTOKEN("Flag"))
        {
            // A new flag.
            idx = DED_AddFlag(ded, "", "", 0);
            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", ded->flags[idx].id)
                RV_UINT("Value", ded->flags[idx].value)
                RV_STR("Info", ded->flags[idx].text)
                RV_END
                CHECKSC;
            }
        }
        if(ISTOKEN("Thing"))
        {
            // A new thing.
            idx = DED_AddMobj(ded, "");
            mo = ded->mobjs + idx;

            if(prev_mo_idx >= 0 && bCopyNext)
            {
                // Should we copy the previous definition?
                memcpy(mo, ded->mobjs + prev_mo_idx, sizeof(*mo));
            }

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", mo->id)
                RV_INT("DoomEd number", mo->doomednum)
                RV_STR("Name", mo->name)
                RV_STR("Spawn state", mo->spawnstate)
                RV_STR("See state", mo->seestate)
                RV_STR("Pain state", mo->painstate)
                RV_STR("Melee state", mo->meleestate)
                RV_STR("Missile state", mo->missilestate)
                RV_STR("Crash state", mo->crashstate)
                RV_STR("Death state", mo->deathstate)
                RV_STR("Xdeath state", mo->xdeathstate)
                RV_STR("Raise state", mo->raisestate)
                RV_STR("See sound", mo->seesound)
                RV_STR("Attack sound", mo->attacksound)
                RV_STR("Pain sound", mo->painsound)
                RV_STR("Death sound", mo->deathsound)
                RV_STR("Active sound", mo->activesound)
                RV_INT("Reaction time", mo->reactiontime)
                RV_INT("Pain chance", mo->painchance)
                RV_INT("Spawn health", mo->spawnhealth)
                RV_FLT("Speed", mo->speed)
                RV_FLT("Radius", mo->radius)
                RV_FLT("Height", mo->height)
                RV_INT("Mass", mo->mass)
                RV_INT("Damage", mo->damage)
                RV_FLAGS("Flags", mo->flags[0], "mf_")
                RV_FLAGS("Flags2", mo->flags[1], "mf2_")
                RV_FLAGS("Flags3", mo->flags[2], "mf3_")
                RV_INT("Misc1", mo->misc[0])
                RV_INT("Misc2", mo->misc[1])
                RV_INT("Misc3", mo->misc[2])
                RV_INT("Misc4", mo->misc[3])
                RV_END
                CHECKSC;
            }
            prev_mo_idx = idx;
        }
        if(ISTOKEN("State"))
        {
            // A new state.
            idx = DED_AddState(ded, "");
            st = ded->states + idx;

            if(prev_state_idx >= 0 && bCopyNext)
            {
                // Should we copy the previous definition?
                memcpy(st, ded->states + prev_state_idx, sizeof(*st));
            }

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", st->id)
                RV_FLAGS("Flags", st->flags, "statef_")
                RV_STR("Sprite", st->sprite.id)
                RV_INT("Frame", st->frame)
                RV_INT("Tics", st->tics)
                RV_STR("Action", st->action)
                RV_STR("Next state", st->nextstate)
                RV_INT("Misc1", st->misc[0])
                RV_INT("Misc2", st->misc[1])
                RV_INT("Misc3", st->misc[2])
                RV_ANYSTR("Execute", st->execute)
                RV_END
                CHECKSC;
            }
            prev_state_idx = idx;
        }
        if(ISTOKEN("Sprite"))
        {
            // A new sprite.
            idx = DED_AddSprite(ded, "");
            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", ded->sprites[idx].id)
                RV_END
                CHECKSC;
            }
        }
        if(ISTOKEN("Light"))
        {
            // A new light.
            idx = DED_AddLight(ded, "");
            lig = ded->lights + idx;
            if(prev_ligdef_idx >= 0 && bCopyNext)
            {
                // Should we copy the previous definition?
                memcpy(lig, ded->lights + prev_ligdef_idx, sizeof(*lig));
            }
            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("State", lig->state)
                RV_STR("Map", lig->level)
                RV_FLT("X Offset", lig->offset[VX])
                RV_FLT("Y Offset", lig->offset[VY])
                RV_VEC("Origin", lig->offset, 3)
                RV_FLT("Size", lig->size)
                RV_FLT("Intensity", lig->size)
                RV_FLT("Red", lig->color[0])
                RV_FLT("Green", lig->color[1])
                RV_FLT("Blue", lig->color[2])
                RV_VEC("Color", lig->color, 3)
                if(ISLABEL("Sector levels"))
                {
                    int     b;
                    FINDBEGIN;
                    for(b = 0; b < 2; ++b)
                    {
                        READFLT(lig->lightlevel[b])
                        lig->lightlevel[b] /= 255.0f;
                        if(lig->lightlevel[b] < 0)
                            lig->lightlevel[b] = 0;
                        else if(lig->lightlevel[b] > 1)
                            lig->lightlevel[b] = 1;
                    }
                    ReadToken();
                }
                else
                RV_FLAGS("Flags", lig->flags, "lgf_")
                RV_STR("Top map", lig->up.id)
                RV_STR("Bottom map", lig->down.id)
                RV_STR("Side map", lig->sides.id)
                RV_STR("Flare map", lig->flare.id)
                RV_FLT("Halo radius", lig->halo_radius)
                RV_END
                CHECKSC;
            }
            prev_ligdef_idx = idx;
        }
        if(ISTOKEN("Model"))
        {
            // A new model.
            idx = DED_AddModel(ded, "");
            mdl = ded->models + idx;
            sub = 0;
            if(prev_modef_idx >= 0)
            {
                prevmdl = ded->models + prev_modef_idx;
                // Should we copy the previous definition?
                if(bCopyNext) memcpy(mdl, prevmdl, sizeof(*mdl));
            }
            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", mdl->id)
                RV_STR("State", mdl->state)
                RV_INT("Off", mdl->off)
                RV_STR("Sprite", mdl->sprite.id)
                RV_INT("Sprite frame", mdl->spriteframe)
                RV_FLAGS("Group", mdl->group, "mg_")
                RV_INT("Selector", mdl->selector)
                RV_FLAGS("Flags", mdl->flags, "df_")
                RV_FLT("Inter", mdl->intermark)
                RV_INT("Skin tics", mdl->skintics)
                RV_FLT("Resize", mdl->resize)
                if(ISLABEL("Scale"))
                {
                    READFLT(mdl->scale[1]);
                    mdl->scale[0] = mdl->scale[2] = mdl->scale[1];
                }
                else RV_VEC("Scale XYZ", mdl->scale, 3)
                RV_FLT("Offset", mdl->offset[1])
                RV_VEC("Offset XYZ", mdl->offset, 3)
                RV_VEC("Interpolate", mdl->interrange, 2)
                RV_FLT("Shadow radius", mdl->shadowradius)
                if(ISLABEL("Md2") || ISLABEL("Sub"))
                {
                    if(sub >= DED_MAX_SUB_MODELS)
                        Con_Error("DED_ReadData: Too many submodels (%s).\n",
                        mdl->state);
                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        RV_STR("File", mdl->sub[sub].filename.path)
                        RV_STR("Frame", mdl->sub[sub].frame)
                        RV_INT("Frame range", mdl->sub[sub].framerange)
                        RV_FLAGS("Blending mode", mdl->sub[sub].blendmode, "bm_")
                        RV_INT("Skin", mdl->sub[sub].skin)
                        RV_STR("Skin file", mdl->sub[sub].skinfilename.path)
                        RV_INT("Skin range", mdl->sub[sub].skinrange)
                        RV_VEC("Offset XYZ", mdl->sub[sub].offset, 3)
                        RV_FLAGS("Flags", mdl->sub[sub].flags, "df_")
                        RV_FLT("Transparent", mdl->sub[sub].alpha)
                        RV_FLT("Parm", mdl->sub[sub].parm)
                        RV_BYTE("Selskin mask", mdl->sub[sub].selskinbits[0])
                        RV_BYTE("Selskin shift", mdl->sub[sub].selskinbits[1])
                        RV_NBVEC("Selskins", mdl->sub[sub].selskins, 8)
                        RV_STR("Shiny skin", mdl->sub[sub].shinyskin)
                        RV_FLT("Shiny", mdl->sub[sub].shiny)
                        RV_VEC("Shiny color", mdl->sub[sub].shinycolor, 3)
                        RV_FLT("Shiny reaction", mdl->sub[sub].shinyreact)
                        RV_END
                        CHECKSC;
                    }
                    sub++;
                }
                else RV_END
                CHECKSC;
            }
            // Some post-processing. No point in doing this in a fancy way,
            // the whole reader will be rewritten sooner or later...
            if(prevmdl)
            {
                int i;
                if(!strcmp(mdl->state, "-"))        strcpy(mdl->state,      prevmdl->state);
                if(!strcmp(mdl->sprite.id, "-"))    strcpy(mdl->sprite.id,  prevmdl->sprite.id);
                //if(!strcmp(mdl->group, "-"))      strcpy(mdl->group,      prevmdl->group);
                //if(!strcmp(mdl->flags, "-"))      strcpy(mdl->flags,      prevmdl->flags);
                for(i = 0; i < DED_MAX_SUB_MODELS; ++i)
                {
                    if(!strcmp(mdl->sub[i].filename.path, "-")) strcpy(mdl->sub[i].filename.path,   prevmdl->sub[i].filename.path);
                    if(!strcmp(mdl->sub[i].frame, "-"))         strcpy(mdl->sub[i].frame,           prevmdl->sub[i].frame);
                    //if(!strcmp(mdl->sub[i].flags, "-"))           strcpy(mdl->sub[i].flags,           prevmdl->sub[i].flags);
                }
            }
            prev_modef_idx = idx;
        }
        if(ISTOKEN("Sound"))
        {
            // A new sound.
            idx = DED_AddSound(ded, "");
            snd = ded->sounds + idx;
            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", snd->id)
                RV_STR("Lump", snd->lumpname)
                RV_STR("Name", snd->name)
                RV_STR("Link", snd->link)
                RV_INT("Link pitch", snd->link_pitch)
                RV_INT("Link volume", snd->link_volume)
                RV_INT("Priority", snd->priority)
                RV_INT("Max channels", snd->channels)
                RV_INT("Group", snd->group)
                RV_FLAGS("Flags", snd->flags, "sf_")
                RV_STR("Ext", snd->ext.path)
                RV_STR("File", snd->ext.path)
                RV_STR("File name", snd->ext.path)
                RV_END
                CHECKSC;
            }
        }
        if(ISTOKEN("Music"))
        {
            // A new music.
            idx = DED_AddMusic(ded, "");
            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", ded->music[idx].id)
                RV_STR("Lump", ded->music[idx].lumpname)
                RV_STR("File name", ded->music[idx].path.path)
                RV_STR("File", ded->music[idx].path.path)
                RV_STR("Ext", ded->music[idx].path.path) // Both work.
                RV_INT("CD track", ded->music[idx].cdtrack)
                RV_END
                CHECKSC;
            }
        }
        if(ISTOKEN("Map")) // Info
        {
            // A new map info.
            idx = DED_AddMapInfo(ded, "");
            mi = ded->mapinfo + idx;
            if(prev_mapinfo_idx >= 0 && bCopyNext)
            {
                int m;
                // Should we copy the previous definition?
                memcpy(mi, ded->mapinfo + prev_mapinfo_idx, sizeof(*mi));
                mi->execute = sdup(mi->execute);
                for(m = 0; m < NUM_SKY_MODELS; ++m)
                {
                    mi->sky_models[m].execute
                        = sdup(mi->sky_models[m].execute);
                }
            }
            prev_mapinfo_idx = idx;
            sub = 0;
            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", mi->id)
                RV_STR("Name", mi->name)
                RV_STR("Author", mi->author)
                RV_FLAGS("Flags", mi->flags, "mif_")
                RV_STR("Music", mi->music)
                RV_FLT("Par time", mi->partime)
                RV_FLT("Fog color R", mi->fog_color[0])
                RV_FLT("Fog color G", mi->fog_color[1])
                RV_FLT("Fog color B", mi->fog_color[2])
                RV_FLT("Fog start", mi->fog_start)
                RV_FLT("Fog end", mi->fog_end)
                RV_FLT("Fog density", mi->fog_density)
                RV_FLT("Ambient light", mi->ambient)
                RV_FLT("Gravity", mi->gravity)
                RV_FLT("Sky height", mi->sky_height)
                RV_FLT("Horizon offset", mi->horizon_offset)
                RV_ANYSTR("Execute", mi->execute)
                RV_VEC("Sky light color", mi->sky_color, 3)
                if(ISLABEL("Sky Layer 1") || ISLABEL("Sky Layer 2"))
                {
                    ded_skylayer_t *sl = mi->sky_layers + atoi(label+10) - 1;
                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        RV_FLAGS("Flags", sl->flags, "slf_")
                        RV_STR("Texture", sl->texture)
                        RV_FLT("Offset", sl->offset)
                        RV_FLT("Color limit", sl->color_limit)
                        RV_END
                        CHECKSC;
                    }
                }
                else if(ISLABEL("Sky Model"))
                {
                    ded_skymodel_t *sm = mi->sky_models + sub;
                    if(sub == NUM_SKY_MODELS)
                    {
                        // Too many!
                        SetError("Too many sky models.");
                        retval = false;
                        goto ded_end_read;
                    }
                    sub++;
                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        RV_STR("ID", sm->id)
                        RV_INT("Layer", sm->layer)
                        RV_FLT("Frame interval", sm->frame_interval)
                        RV_FLT("Yaw", sm->yaw)
                        RV_FLT("Yaw speed", sm->yaw_speed)
                        RV_VEC("Rotate", sm->rotate, 2)
                        RV_VEC("Offset factor", sm->coord_factor, 3)
                        RV_VEC("Color", sm->color, 4)
                        RV_ANYSTR("Execute", sm->execute)
                        RV_END
                        CHECKSC;
                    }
                }
                else RV_END
                CHECKSC;
            }
        }
        if(ISTOKEN("Text"))
        {
            // A new text.
            idx = DED_AddText(ded, "");
            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", ded->text[idx].id)
                if(ISLABEL("Text"))
                {
                    // Allocate a 'huge' buffer.
                    char *temp = M_Malloc(0x10000);

                    if(ReadString(temp, 0xffff))
                    {
                        int len = strlen(temp) + 1;

                        ded->text[idx].text = M_Realloc(temp, len);
                        //memcpy(ded->text[idx].text, temp, len);
                    }
                    else
                    {
                        M_Free(temp);
                        SetError("Syntax error in text value.");
                        retval = false;
                        goto ded_end_read;
                    }
                }
                else RV_END
                CHECKSC;
            }
        }
        if(ISTOKEN("Texture")) // Environment
        {
            // A new texenv.
            idx = DED_AddTexEnviron(ded, "");
            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", ded->tenviron[idx].id)
                if(ISLABEL("Texture"))
                {
                    // A new texture name.
                    tn = DED_NewEntry((void**)&ded->tenviron[idx].textures,
                                      &ded->tenviron[idx].texCount, sizeof(*tn));
                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        RV_STR("ID", tn->str)
                        RV_END
                        CHECKSC;
                    }
                }
                else if(ISLABEL("Flat"))
                {
                    // A new flat name.
                    tn = DED_NewEntry((void**)&ded->tenviron[idx].flats,
                                      &ded->tenviron[idx].flatCount, sizeof(*tn));
                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        RV_STR("ID", tn->str)
                        RV_END
                        CHECKSC;
                    }
                }
                else RV_END
                CHECKSC;
            }
        }
        if(ISTOKEN("Values")) // String Values
        {
            depth = 0;
            rootstr = M_Calloc(1); // A null string.
            FINDBEGIN;
            for(;;)
            {
                // Get the next label but don't stop on }.
                READLABEL_NOBREAK;
                if(strchr(label, '|'))
                {
                    SetError("Value labels can't include | characters (ASCII 124).");
                    retval = false;
                    goto ded_end_read;
                }
                if(ISTOKEN("="))
                {
                    // Define a new string.
                    char *temp = M_Malloc(0x1000);    // A 'huge' buffer.

                    if(ReadString(temp, 0xffff))
                    {
                        // Reallocate the buffer down to actual string length.
                        temp = M_Realloc(temp, strlen(temp) + 1);

                        // Get a new value entry.
                        idx = DED_AddValue(ded, 0);
                        val = ded->values + idx;
                        val->text = temp;

                        // Compose the identifier.
                        val->id = M_Malloc(strlen(rootstr) + strlen(label) + 1);
                        strcpy(val->id, rootstr);
                        strcat(val->id, label);
                    }
                    else
                    {
                        M_Free(temp);
                        SetError("Syntax error in string value.");
                        retval = false;
                        goto ded_end_read;
                    }
                }
                else if(ISTOKEN("{"))
                {
                    // Begin a new group.
                    rootstr = M_Realloc(rootstr, strlen(rootstr)
                        + strlen(label) + 2);
                    strcat(rootstr, label);
                    strcat(rootstr, "|");   // The separator.
                    // Increase group level.
                    depth++;
                    continue;
                }
                else if(ISTOKEN("}"))
                {
                    // End group.
                    if(!depth)
                        break;   // End root depth.

                    // Decrease level and modify roostr.
                    depth--;
                    sub = strlen(rootstr);
                    rootstr[sub-1] = 0; // Remove last |.
                    ptr = strrchr(rootstr, '|');
                    if(ptr)
                    {
                        ptr[1] = 0;
                        rootstr = M_Realloc(rootstr, strlen(rootstr) + 1);
                    }
                    else
                    {
                        // Back to level zero.
                        rootstr = M_Realloc(rootstr, 1);
                        *rootstr = 0;
                    }
                }
                else
                {
                    // Only the above characters are allowed.
                    SetError("Illegal token.");
                    retval = false;
                    goto ded_end_read;
                }
                CHECKSC;
            }
            M_Free(rootstr);
            rootstr = 0;
        }
        if(ISTOKEN("Detail")) // Detail Texture
        {
            idx = DED_AddDetail(ded, "");
            dtl = ded->details + idx;
            if(prev_dtldef_idx >= 0 && bCopyNext)
            {
                // Should we copy the previous definition?
                memcpy(dtl, ded->details + prev_dtldef_idx, sizeof(*dtl));
            }
            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("Wall", dtl->wall)
                RV_STR("Texture", dtl->wall) // alias
                RV_STR("Flat", dtl->flat)
                if(ISLABEL("Lump"))
                {
                    READSTR(dtl->detail_lump.path)
                    dtl->is_external = false;
                }
                else if(ISLABEL("File"))
                {
                    READSTR(dtl->detail_lump.path)
                    dtl->is_external = true;
                }
                else
                RV_FLT("Scale", dtl->scale)
                RV_FLT("Strength", dtl->strength)
                RV_FLT("Distance", dtl->maxdist)
                RV_END
                CHECKSC;
            }
            prev_dtldef_idx = idx;
        }
        if(ISTOKEN("Reflection")) // Surface reflection
        {
            ded_reflection_t *ref = NULL;

            idx = DED_AddReflection(ded);
            ref = &ded->reflections[idx];

            if(prev_refdef_idx >= 0 && bCopyNext)
            {
                // Should we copy the previous definition?
                memcpy(ref, ded->reflections + prev_refdef_idx, sizeof(*ref));
            }

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_FLT("Shininess", ref->shininess)
                RV_VEC("Min color", ref->min_color, 3)
                RV_FLAGS("Blending mode", ref->blend_mode, "bm_")
                RV_STR("Shiny map", ref->shiny_map.path)
                RV_STR("Mask map", ref->mask_map.path)
                RV_FLT("Mask width", ref->mask_width)
                RV_FLT("Mask height", ref->mask_height)
                if(ISLABEL("Texture"))
                {
                    READSTR(ref->surface)
                    ref->is_texture = true;
                }
                else if(ISLABEL("Flat"))
                {
                    READSTR(ref->surface)
                    ref->is_texture = false;
                }
                else RV_END
                CHECKSC;
            }
            prev_refdef_idx = idx;
        }
        if(ISTOKEN("Generator")) // Particle Generator
        {
            idx = DED_AddPtcGen(ded, "");
            gen = ded->ptcgens + idx;
            sub = 0;
            if(prev_gendef_idx >= 0 && bCopyNext)
            {
                // Should we copy the previous definition?
                memcpy(gen, ded->ptcgens + prev_gendef_idx, sizeof(*gen));

                // Duplicate the stages array.
                if(ded->ptcgens[prev_gendef_idx].stages)
                {
                    gen->stages = M_Malloc(sizeof(ded_ptcstage_t) *
                                           gen->stage_count.max);
                    memcpy(gen->stages, ded->ptcgens[prev_gendef_idx].stages,
                           sizeof(ded_ptcstage_t) * gen->stage_count.num);
                }
            }
            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("State", gen->state)
                if(ISLABEL("Flat"))
                {
                    READSTR(gen->surface)
                    gen->is_texture = false;
                }
                else
                RV_STR("Mobj", gen->type)
                RV_STR("Alt mobj", gen->type2)
                RV_STR("Damage mobj", gen->damage)
                RV_STR("Map", gen->map)
                RV_FLAGS("Flags", gen->flags, "gnf_")
                RV_FLT("Speed", gen->speed)
                RV_FLT("Speed Rnd", gen->spd_variance)
                RV_VEC("Vector", gen->vector, 3)
                RV_FLT("Vector Rnd", gen->vec_variance)
                RV_FLT("Init vector Rnd", gen->init_vec_variance)
                RV_VEC("Center", gen->center, 3)
                RV_INT("Submodel", gen->submodel)
                RV_FLT("Spawn radius", gen->spawn_radius)
                RV_FLT("Min spawn radius", gen->min_spawn_radius)
                RV_FLT("Distance", gen->maxdist)
                RV_INT("Spawn age", gen->spawn_age)
                RV_INT("Max age", gen->max_age)
                RV_INT("Particles", gen->particles)
                RV_FLT("Spawn rate", gen->spawn_rate)
                RV_FLT("Spawn Rnd", gen->spawn_variance)
                RV_INT("Presim", gen->presim)
                RV_INT("Alt start", gen->alt_start)
                RV_FLT("Alt Rnd", gen->alt_variance)
                RV_VEC("Force axis", gen->force_axis, 3)
                RV_FLT("Force radius", gen->force_radius)
                RV_FLT("Force", gen->force)
                RV_VEC("Force origin", gen->force_origin, 3)
                if(ISLABEL("Stage"))
                {
                    ded_ptcstage_t *st = NULL;

                    if(sub >= gen->stage_count.num)
                    {
                        // Allocate new stage.
                        sub = DED_AddPtcGenStage(gen);
                    }

                    st = &gen->stages[sub];

                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        RV_FLAGS("Type", st->type, "pt_")
                        RV_INT("Tics", st->tics)
                        RV_FLT("Rnd", st->variance)
                        RV_VEC("Color", st->color, 4)
                        RV_FLT("Radius", st->radius)
                        RV_FLT("Radius rnd", st->radius_variance)
                        RV_FLAGS("Flags", st->flags, "ptf_")
                        RV_FLT("Bounce", st->bounce)
                        RV_FLT("Gravity", st->gravity)
                        RV_FLT("Resistance", st->resistance)
                        RV_STR("Frame", st->frame_name)
                        RV_STR("End frame", st->end_frame_name)
                        RV_VEC("Spin", st->spin, 2)
                        RV_VEC("Spin resistance", st->spin_resistance, 2)
                        RV_STR("Sound", st->sound.name)
                        RV_FLT("Volume", st->sound.volume)
                        RV_STR("Hit sound", st->hit_sound.name)
                        RV_FLT("Hit volume", st->hit_sound.volume)
                        RV_VEC("Force", st->vector_force, 3)
                        RV_END
                        CHECKSC;
                    }
                    sub++;
                }
                else RV_END
                CHECKSC;
            }
            prev_gendef_idx = idx;
        }
        if(ISTOKEN("Finale") || ISTOKEN("InFine"))
        {
            idx = DED_AddFinale(ded);
            fin = ded->finales + idx;
            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", fin->id)
                RV_STR("Before", fin->before)
                RV_STR("After", fin->after)
                RV_INT("Game", dummyInt)
                if(ISLABEL("Script"))
                {
                    // Allocate an "enormous" 64K buffer.
                    char *temp = M_Calloc(0x10000), *ptr;

                    if(fin->script)
                        M_Free(fin->script);

                    FINDBEGIN;
                    ptr = temp;
                    ReadToken();
                    while(!ISTOKEN("}") && !source->atEnd)
                    {
                        if(ptr != temp)
                            *ptr++ = ' ';

                        strcpy(ptr, token);
                        ptr += strlen(token);
                        if(ISTOKEN("\""))
                        {
                            ReadStringEx(ptr, 0x10000 - (ptr-temp), true, true);
                            ptr += strlen(ptr); // Continue from the null.
                            *ptr++ = '"';
                        }
                        ReadToken();
                    }
                    fin->script = M_Realloc(temp, strlen(temp) + 1);
                }
                else RV_END
                CHECKSC;
            }
        }
        if(ISTOKEN("Decoration"))
        {
            ded_decor_t *decor;

            idx = DED_AddDecoration(ded);
            decor = ded->decorations + idx;
            sub = 0;
            if(prev_decordef_idx >= 0 && bCopyNext)
            {
                // Should we copy the previous definition?
                memcpy(decor, ded->decorations + prev_decordef_idx,
                       sizeof(*decor));
            }
            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_FLAGS("Flags", decor->flags, "dcf_")
                if(ISLABEL("Texture"))
                {
                    READSTR(decor->surface)
                    decor->is_texture = true;
                }
                else if(ISLABEL("Flat"))
                {
                    READSTR(decor->surface)
                    decor->is_texture = false;
                }
                else if(ISLABEL("Glow"))
                {
                    decor->glow = true;
                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        // No paramaters yet. 
                        CHECKSC;
                    }
                    sub++;
                }
                else if(ISLABEL("Light"))
                {
                    ded_decorlight_t *dl = decor->lights + sub;
                    if(sub == DED_DECOR_NUM_LIGHTS)
                    {
                        SetError("Too many lights in decoration.");
                        retval = false;
                        goto ded_end_read;
                    }
                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        RV_VEC("Offset", dl->pos, 2)
                        RV_FLT("Distance", dl->elevation)
                        RV_VEC("Color", dl->color, 3)
                        RV_FLT("Radius", dl->radius)
                        RV_FLT("Halo radius", dl->halo_radius)
                        RV_IVEC("Pattern offset", dl->pattern_offset, 2)
                        RV_IVEC("Pattern skip", dl->pattern_skip, 2)
                        if(ISLABEL("Levels"))
                        {
                            int     b;
                            FINDBEGIN;
                            for(b = 0; b < 2; ++b)
                            {
                                READFLT(dl->lightlevels[b])
                                dl->lightlevels[b] /= 255.0f;
                                if(dl->lightlevels[b] < 0)
                                    dl->lightlevels[b] = 0;
                                else if(dl->lightlevels[b] > 1)
                                    dl->lightlevels[b] = 1;
                            }
                            ReadToken();
                        }
                        else
                        RV_INT("Flare texture", dl->flare_texture)
                        RV_STR("Flare map", dl->flare.id)
                        RV_STR("Top map", dl->up.id)
                        RV_STR("Bottom map", dl->down.id)
                        RV_STR("Side map", dl->sides.id)
                        RV_END
                        CHECKSC;
                    }
                    sub++;
                }
                else RV_END
                CHECKSC;
            }
            prev_decordef_idx = idx;
        }
        if(ISTOKEN("Group"))
        {
            ded_group_t *grp;
            idx = DED_AddGroup(ded);
            grp = ded->groups + idx;
            sub = 0;
            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                if(ISLABEL("Texture") || ISLABEL("Flat"))
                {
                    ded_group_member_t *memb;

                    grp->is_texture = ISLABEL("Texture");

                    if(sub >= grp->count.num)
                    {
                        // Allocate new stage.
                        sub = DED_AddGroupMember(grp);
                    }

                    memb = &grp->members[sub];

                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        RV_STR("ID", memb->name)
                        RV_FLT("Tics", memb->tics)
                        RV_FLT("Random", memb->random_tics)
                        RV_END
                        CHECKSC;
                    }
                    ++sub;
                }
                else RV_FLAGS("Flags", grp->flags, "tgf_")
                RV_END
                CHECKSC;
            }
        }
        if(ISTOKEN("LumpFormat"))     // Binary Lump Format
        {
            // A new binary lump format.
            idx = DED_AddLumpFormat(ded);
            lmpf = ded->lumpformats + idx;
            sub = 0;
            if(prev_lumpformat_idx >= 0 && bCopyNext)
            {
                // Should we copy the previous definition?
                memcpy(lmpf, ded->lumpformats + prev_lumpformat_idx, sizeof(*lmpf));

                // Duplicate the properties array.
                if(ded->lumpformats[prev_lumpformat_idx].properties)
                {
                    lmpf->properties =
                        M_Malloc(sizeof(ded_lumpformat_property_t) * lmpf->property_count.max);
                    memcpy(lmpf->properties, ded->lumpformats[prev_lumpformat_idx].properties,
                           sizeof(ded_lumpformat_property_t) * lmpf->property_count.num);
                }
            }
            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", lmpf->id)
                RV_FLAGS("Class", lmpf->type, "damlmpc_")
                RV_INT("ElementSize", lmpf->elmsize)
                if(ISLABEL("Property"))
                {
                    ded_lumpformat_property_t *lfm = NULL;

                    if(sub >= lmpf->property_count.num)
                    {
                        // Allocate a new property.
                        sub = DED_AddLumpFormatMember(lmpf);
                    }

                    lfm = &lmpf->properties[sub];
                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        RV_STR("ID", lfm->id)
                        RV_FLAGS("Flags", lfm->flags, "dampf_")
                        RV_INT("Size", lfm->size)
                        RV_INT("Offset", lfm->offset)
                        RV_END
                        CHECKSC;
                    }
                    sub++;
                }
                else RV_END
                CHECKSC;
            }
            prev_lumpformat_idx = idx;
        }
        /*if(ISTOKEN("XGClass"))     // XG Class
        {
            // A new XG Class definition
            idx = DED_AddXGClass(ded);
            xgc = ded->xgclasses + idx;
            sub = 0;

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", xgc->id)
                RV_STR("Name", xgc->name)
                if(ISLABEL("Property"))
                {
                    ded_xgclass_property_t *xgcp = NULL;

                    if(sub >= xgc->properties_count.num)
                    {
                        // Allocate new property
                        sub = DED_AddXGClassProperty(xgc);
                    }

                    xgcp = &xgc->properties[sub];

                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        RV_FLAGS("ID", xgcp->id, "xgcp_")
                        RV_FLAGS("Flags", xgcp->flags, "xgcpf_")
                        RV_STR("Name", xgcp->name)
                        RV_STR("Flag Group", xgcp->flagprefix)
                        RV_END
                        CHECKSC;
                    }
                    sub++;
                }
                else RV_END
                CHECKSC;
            }
        }*/
        if(ISTOKEN("Line"))     // Line Type
        {
            // A new line type.
            idx = DED_AddLine(ded, 0);
            l = ded->lines + idx;

            if(prev_linetype_idx >= 0 && bCopyNext)
            {
                // Should we copy the previous definition?
                memcpy(l, ded->lines + prev_linetype_idx, sizeof(*l));
            }

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_INT("ID", l->id)
                RV_STR("Comment", l->comment)
                RV_FLAGS("Flags", l->flags[0], "ltf_")
                RV_FLAGS("Flags2", l->flags[1], "ltf2_")
                RV_FLAGS("Flags3", l->flags[2], "ltf3_")
                RV_FLAGS("Class", l->line_class, "ltc_")
                RV_FLAGS("Type", l->act_type, "lat_")
                RV_INT("Count", l->act_count)
                RV_FLT("Time", l->act_time)
                RV_INT("Act tag", l->act_tag)
                RV_INT("Ap0", l->aparm[0])
                RV_INT("Ap1", l->aparm[1])
                RV_INT("Ap2", l->aparm[2])
                RV_INT("Ap3", l->aparm[3])
                RV_FLAGS("Ap4", l->aparm[4], "lref_")
                RV_INT("Ap5", l->aparm[5])
                RV_FLAGS("Ap6", l->aparm[6], "lref_")
                RV_INT("Ap7", l->aparm[7])
                RV_INT("Ap8", l->aparm[8])
                RV_STR("Ap9", l->aparm9)
                RV_INT("Health above", l->aparm[0])
                RV_INT("Health below", l->aparm[1])
                RV_INT("Power above", l->aparm[2])
                RV_INT("Power below", l->aparm[3])
                RV_FLAGS("Line act lref", l->aparm[4], "lref_")
                RV_INT("Line act lrefd", l->aparm[5])
                RV_FLAGS("Line inact lref", l->aparm[6], "lref_")
                RV_INT("Line inact lrefd", l->aparm[7])
                RV_INT("Color", l->aparm[8])
                RV_STR("Thing type", l->aparm9)
                RV_FLT("Ticker start time", l->ticker_start)
                RV_FLT("Ticker end time", l->ticker_end)
                RV_INT("Ticker tics", l->ticker_interval)
                RV_STR("Act sound", l->act_sound)
                RV_STR("Deact sound", l->deact_sound)
                RV_INT("Event chain", l->ev_chain)
                RV_INT("Act chain", l->act_chain)
                RV_INT("Deact chain", l->deact_chain)
                RV_FLAGS("Wall section", l->wallsection, "lws_")
                RV_STR("Act texture", l->act_tex)
                RV_STR("Deact texture", l->deact_tex)
                RV_INT("Act type", l->act_linetype)
                RV_INT("Deact type", l->deact_linetype)
                RV_STR("Act message", l->act_msg)
                RV_STR("Deact message", l->deact_msg)
                RV_FLT("Texmove angle", l->texmove_angle)
                RV_FLT("Texmove speed", l->texmove_speed)
                RV_STR_INT("Ip0", l->iparm_str[0], l->iparm[0])
                RV_STR_INT("Ip1", l->iparm_str[1], l->iparm[1])
                RV_STR_INT("Ip2", l->iparm_str[2], l->iparm[2])
                RV_STR_INT("Ip3", l->iparm_str[3], l->iparm[3])
                RV_STR_INT("Ip4", l->iparm_str[4], l->iparm[4])
                RV_STR_INT("Ip5", l->iparm_str[5], l->iparm[5])
                RV_STR_INT("Ip6", l->iparm_str[6], l->iparm[6])
                RV_STR_INT("Ip7", l->iparm_str[7], l->iparm[7])
                RV_STR_INT("Ip8", l->iparm_str[8], l->iparm[8])
                RV_STR_INT("Ip9", l->iparm_str[9], l->iparm[9])
                RV_STR_INT("Ip10", l->iparm_str[10], l->iparm[10])
                RV_STR_INT("Ip11", l->iparm_str[11], l->iparm[11])
                RV_STR_INT("Ip12", l->iparm_str[12], l->iparm[12])
                RV_STR_INT("Ip13", l->iparm_str[13], l->iparm[13])
                RV_STR_INT("Ip14", l->iparm_str[14], l->iparm[14])
                RV_STR_INT("Ip15", l->iparm_str[15], l->iparm[15])
                RV_STR_INT("Ip16", l->iparm_str[16], l->iparm[16])
                RV_STR_INT("Ip17", l->iparm_str[17], l->iparm[17])
                RV_STR_INT("Ip18", l->iparm_str[18], l->iparm[18])
                RV_STR_INT("Ip19", l->iparm_str[19], l->iparm[19])
                RV_FLT("Fp0", l->fparm[0])
                RV_FLT("Fp1", l->fparm[1])
                RV_FLT("Fp2", l->fparm[2])
                RV_FLT("Fp3", l->fparm[3])
                RV_FLT("Fp4", l->fparm[4])
                RV_FLT("Fp5", l->fparm[5])
                RV_FLT("Fp6", l->fparm[6])
                RV_FLT("Fp7", l->fparm[7])
                RV_FLT("Fp8", l->fparm[8])
                RV_FLT("Fp9", l->fparm[9])
                RV_FLT("Fp10", l->fparm[10])
                RV_FLT("Fp11", l->fparm[11])
                RV_FLT("Fp12", l->fparm[12])
                RV_FLT("Fp13", l->fparm[13])
                RV_FLT("Fp14", l->fparm[14])
                RV_FLT("Fp15", l->fparm[15])
                RV_FLT("Fp16", l->fparm[16])
                RV_FLT("Fp17", l->fparm[17])
                RV_FLT("Fp18", l->fparm[18])
                RV_FLT("Fp19", l->fparm[19])
                RV_STR("Sp0", l->sparm[0])
                RV_STR("Sp1", l->sparm[1])
                RV_STR("Sp2", l->sparm[2])
                RV_STR("Sp3", l->sparm[3])
                RV_STR("Sp4", l->sparm[4])
                if(l->line_class)
                {
                    // IpX Alt names can only be used if the class is defined first!
                    // they also support the DED v6 flags format
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[0].name,
                               l->iparm_str[0], l->iparm[0], 0)
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[1].name,
                               l->iparm_str[1], l->iparm[1], 1)
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[2].name,
                               l->iparm_str[2], l->iparm[2], 2)
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[3].name,
                               l->iparm_str[3], l->iparm[3], 3)
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[4].name,
                               l->iparm_str[4], l->iparm[4], 4)
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[5].name,
                               l->iparm_str[5], l->iparm[5], 5)
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[6].name,
                               l->iparm_str[6], l->iparm[6], 6)
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[7].name,
                               l->iparm_str[7], l->iparm[7], 7)
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[8].name,
                               l->iparm_str[8], l->iparm[8], 8)
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[9].name,
                               l->iparm_str[9], l->iparm[9], 9)
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[10].name,
                               l->iparm_str[10], l->iparm[10], 10)
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[11].name,
                               l->iparm_str[11], l->iparm[11], 11)
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[12].name,
                               l->iparm_str[12], l->iparm[12], 12)
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[13].name,
                               l->iparm_str[13], l->iparm[13], 13)
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[14].name,
                               l->iparm_str[14], l->iparm[14], 14)
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[15].name,
                               l->iparm_str[15], l->iparm[15], 15)
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[16].name,
                               l->iparm_str[16], l->iparm[16], 16)
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[17].name,
                               l->iparm_str[17], l->iparm[17], 17)
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[18].name,
                               l->iparm_str[18], l->iparm[18], 18)
                    RV_XGIPARM(xgClassLinks[l->line_class].iparm[19].name,
                               l->iparm_str[19], l->iparm[19], 19)
                    RV_END

                } else
                RV_END
                CHECKSC;
            }
            prev_linetype_idx = idx;
        }
        if(ISTOKEN("Sector"))   // Sector Type
        {
            // A new sector type.
            idx = DED_AddSector(ded, 0);
            sec = ded->sectors + idx;

            if(prev_sectortype_idx >= 0 && bCopyNext)
            {
                // Should we copy the previous definition?
                memcpy(sec, ded->sectors + prev_sectortype_idx,
                       sizeof(*sec));
            }

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_INT("ID", sec->id)
                RV_STR("Comment", sec->comment)
                RV_FLAGS("Flags", sec->flags, "stf_")
                RV_INT("Act tag", sec->act_tag)
                RV_INT("Floor chain", sec->chain[0])
                RV_INT("Ceiling chain", sec->chain[1])
                RV_INT("Inside chain", sec->chain[2])
                RV_INT("Ticker chain", sec->chain[3])
                RV_FLAGS("Floor chain flags", sec->chain_flags[0], "scef_")
                RV_FLAGS("Ceiling chain flags", sec->chain_flags[1], "scef_")
                RV_FLAGS("Inside chain flags", sec->chain_flags[2], "scef_")
                RV_FLAGS("Ticker chain flags", sec->chain_flags[3], "scef_")
                RV_FLT("Floor chain start time", sec->start[0])
                RV_FLT("Ceiling chain start time", sec->start[1])
                RV_FLT("Inside chain start time", sec->start[2])
                RV_FLT("Ticker chain start time", sec->start[3])
                RV_FLT("Floor chain end time", sec->end[0])
                RV_FLT("Ceiling chain end time", sec->end[1])
                RV_FLT("Inside chain end time", sec->end[2])
                RV_FLT("Ticker chain end time", sec->end[3])
                RV_FLT("Floor chain min interval", sec->interval[0][0])
                RV_FLT("Ceiling chain min interval", sec->interval[1][0])
                RV_FLT("Inside chain min interval", sec->interval[2][0])
                RV_FLT("Ticker chain min interval", sec->interval[3][0])
                RV_FLT("Floor chain max interval", sec->interval[0][1])
                RV_FLT("Ceiling chain max interval", sec->interval[1][1])
                RV_FLT("Inside chain max interval", sec->interval[2][1])
                RV_FLT("Ticker chain max interval", sec->interval[3][1])
                RV_INT("Floor chain count", sec->count[0])
                RV_INT("Ceiling chain count", sec->count[1])
                RV_INT("Inside chain count", sec->count[2])
                RV_INT("Ticker chain count", sec->count[3])
                RV_STR("Ambient sound", sec->ambient_sound)
                RV_FLT("Ambient min interval", sec->sound_interval[0])
                RV_FLT("Ambient max interval", sec->sound_interval[1])
                RV_FLT("Floor texmove angle", sec->texmove_angle[0])
                RV_FLT("Ceiling texmove angle", sec->texmove_angle[1])
                RV_FLT("Floor texmove speed", sec->texmove_speed[0])
                RV_FLT("Ceiling texmove speed", sec->texmove_speed[1])
                RV_FLT("Wind angle", sec->wind_angle)
                RV_FLT("Wind speed", sec->wind_speed)
                RV_FLT("Vertical wind", sec->vertical_wind)
                RV_FLT("Gravity", sec->gravity)
                RV_FLT("Friction", sec->friction)
                RV_STR("Light fn", sec->lightfunc)
                RV_INT("Light fn min tics", sec->light_interval[0])
                RV_INT("Light fn max tics", sec->light_interval[1])
                RV_STR("Red fn", sec->colfunc[0])
                RV_STR("Green fn", sec->colfunc[1])
                RV_STR("Blue fn", sec->colfunc[2])
                RV_INT("Red fn min tics", sec->col_interval[0][0])
                RV_INT("Red fn max tics", sec->col_interval[0][1])
                RV_INT("Green fn min tics", sec->col_interval[1][0])
                RV_INT("Green fn max tics", sec->col_interval[1][1])
                RV_INT("Blue fn min tics", sec->col_interval[2][0])
                RV_INT("Blue fn max tics", sec->col_interval[2][1])
                RV_STR("Floor fn", sec->floorfunc)
                RV_FLT("Floor fn scale", sec->floormul)
                RV_FLT("Floor fn offset", sec->flooroff)
                RV_INT("Floor fn min tics", sec->floor_interval[0])
                RV_INT("Floor fn max tics", sec->floor_interval[1])
                RV_STR("Ceiling fn", sec->ceilfunc)
                RV_FLT("Ceiling fn scale", sec->ceilmul)
                RV_FLT("Ceiling fn offset", sec->ceiloff)
                RV_INT("Ceiling fn min tics", sec->ceil_interval[0])
                RV_INT("Ceiling fn max tics", sec->ceil_interval[1])
                RV_END
                CHECKSC;
            }
            prev_sectortype_idx = idx;
        }
        bCopyNext = false;
    }

ded_end_read:
    M_Free(rootstr);

    // Free the source stack entry we were using.
    DED_CloseReader();

    return retval;
}
/* *INDENT-ON* */

/**
 * @return          <code>true</code> if the file was successfully loaded.
 */
int DED_Read(ded_t *ded, const char *sPathName)
{
    DFILE  *file;
    char   *defData;
    int     len;
    int     result;
    char    translated[256];

    M_TranslatePath(sPathName, translated);

    if((file = F_Open(translated, "rb")) == NULL)
    {
        SetError("File can't be opened for reading.");
        return false;
    }

    // Allocate a large enough buffer and read the file.
    F_Seek(file, 0, SEEK_END);
    len = F_Tell(file);
    F_Rewind(file);

    defData = M_Calloc(len + 1);
    F_Read(defData, len, file);
    F_Close(file);

    // Parse the definitions.
    result = DED_ReadData(ded, defData, translated);

    // Now we can free the buffer.
    M_Free(defData);

    return result;
}

/**
 * Reads definitions from the given lump.
 */
int DED_ReadLump(ded_t * ded, int lump)
{
    int     result;

    if(lump < 0 || lump >= numlumps)
    {
        SetError("Bad lump number.");
        return false;
    }
    result = DED_ReadData(ded, W_CacheLumpNum(lump, PU_STATIC),
                          W_LumpSourceFile(lump));
    W_ChangeCacheTag(lump, PU_CACHE);
    return result;
}
