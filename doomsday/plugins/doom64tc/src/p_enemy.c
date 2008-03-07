/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * p_enemy.c: Enemy thinking, AI.
 *
 * Action Pointer Functions that are associated with states/frames.
 *
 * Enemies are allways spawned with targetplayer = -1, threshold = 0
 * Most monsters are spawned unaware of all players,
 * but some can be made preaware
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef MSVC
#  pragma optimize("g", off)
#endif

#include "doom64tc.h"

#include "dmu_lib.h"
#include "p_mapspec.h"
#include "p_map.h"
#include "p_actor.h"

// MACROS ------------------------------------------------------------------

#define FATSPREAD               (ANG90/8)
#define FAT_DELTAANGLE          (85*ANGLE_1) // d64tc
#define FAT_ARM_EXTENSION_SHORT (32) // d64tc
#define FAT_ARM_EXTENSION_LONG  (16) // d64tc
#define FAT_ARM_HEIGHT          (64) // d64tc
#define SKULLSPEED              (20)

#define TRACEANGLE              (0xc000000)

// TYPES -------------------------------------------------------------------

typedef enum dirtype_s {
    DI_EAST,
    DI_NORTHEAST,
    DI_NORTH,
    DI_NORTHWEST,
    DI_WEST,
    DI_SOUTHWEST,
    DI_SOUTH,
    DI_SOUTHEAST,
    DI_NODIR,
    NUMDIRS
} dirtype_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void C_DECL A_ReFire(player_t *player, pspdef_t *psp);
void C_DECL A_Fall(mobj_t *actor);
void C_DECL A_Fire(mobj_t *actor);
void C_DECL A_SpawnFly(mobj_t *mo);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean bossKilled;

mobj_t **brainTargets;
int numBrainTargets;
int numBrainTargetsAlloc;
braindata_t brain; // Global state of boss brain.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static mobj_t *soundTarget;
static mobj_t *corpseHit;

static float dropoffDelta[2], floorZ;

// Eight directional movement speeds.
#define MOVESPEED_DIAGONAL      (0.71716309f)
static const float dirSpeed[8][2] =
{
    {1, 0},
    {MOVESPEED_DIAGONAL, MOVESPEED_DIAGONAL},
    {0, 1},
    {-MOVESPEED_DIAGONAL, MOVESPEED_DIAGONAL},
    {-1, 0},
    {-MOVESPEED_DIAGONAL, -MOVESPEED_DIAGONAL},
    {0, -1},
    {MOVESPEED_DIAGONAL, -MOVESPEED_DIAGONAL}
};
#undef MOVESPEED_DIAGONAL

// CODE --------------------------------------------------------------------

/**
 * Recursively traverse adjacent sectors, sound blocking lines cut off
 * traversal. Called by P_NoiseAlert.
 */
void P_RecursiveSound(sector_t *sec, int soundBlocks)
{
    int                 i;
    linedef_t          *check;
    xline_t            *xline;
    sector_t           *other;
    sector_t           *frontSec, *backSec;
    xsector_t          *xsec = P_ToXSector(sec);

    // Wake up all monsters in this sector.
    if(P_GetIntp(sec, DMU_VALID_COUNT) == VALIDCOUNT &&
       xsec->soundTraversed <= soundBlocks + 1)
        return; // Already flooded.

    P_SetIntp(sec, DMU_VALID_COUNT, VALIDCOUNT);

    xsec->soundTraversed = soundBlocks + 1;
    xsec->soundTarget = soundTarget;

    for(i = 0; i < P_GetIntp(sec, DMU_LINEDEF_COUNT); ++i)
    {
        check = P_GetPtrp(sec, DMU_LINEDEF_OF_SECTOR | i);

        frontSec = P_GetPtrp(check, DMU_FRONT_SECTOR);
        backSec = P_GetPtrp(check, DMU_BACK_SECTOR);

        if(!(P_GetIntp(check, DMU_FLAGS) & DDLF_TWOSIDED))
            continue;

        P_LineOpening(check);

        if(OPENRANGE <= 0)
            continue; // Closed door.

        if(frontSec == sec)
            other = backSec;
        else
            other = frontSec;

        xline = P_ToXLine(check);
        if(xline->flags & ML_SOUNDBLOCK)
        {
            if(!soundBlocks)
                P_RecursiveSound(other, 1);
        }
        else
        {
            P_RecursiveSound(other, soundBlocks);
        }
    }
}

/**
 * If a monster yells at a player, it will alert other monsters to the
 * player.
 */
void P_NoiseAlert(mobj_t *target, mobj_t *emitter)
{
    soundTarget = target;
    VALIDCOUNT++;
    P_RecursiveSound(P_GetPtrp(emitter->subsector, DMU_SECTOR), 0);
}

static boolean checkMeleeRange(mobj_t *actor)
{
    mobj_t             *pl;
    float               dist;
    float               range;

    if(!actor->target)
        return false;

    pl = actor->target;
    dist = P_ApproxDistance(pl->pos[VX] - actor->pos[VX],
                            pl->pos[VY] - actor->pos[VY]);
    if(!(cfg.netNoMaxZMonsterMeleeAttack))
        dist =
            P_ApproxDistance(dist, (pl->pos[VZ] + pl->height /2) -
                                   (actor->pos[VZ] + actor->height /2));

    //range = MELEERANGE - 20 + pl->info->radius;
    range = MELEERANGE - 14 + pl->info->radius; // Was 20, d64tc
    if(dist >= range)
        return false;

    if(!P_CheckSight(actor, actor->target))
        return false;

    return true;
}

static boolean checkMissileRange(mobj_t *actor)
{
    float               dist;

    if(!P_CheckSight(actor, actor->target))
        return false;

    if(actor->flags & MF_JUSTHIT)
    {   // The target just hit the enemy.
        // So fight back!
        actor->flags &= ~MF_JUSTHIT;
        return true;
    }

    if(actor->reactionTime)
        return false; // Do not attack yet.

    // OPTIMIZE: get this from a global checksight.
    dist =
        P_ApproxDistance(actor->pos[VX] - actor->target->pos[VX],
                         actor->pos[VY] - actor->target->pos[VY]) - 64;

    if(!actor->info->meleeState)
        dist -= 128; // No melee attack, so fire more.

    if(actor->type == MT_CYBORG || actor->type == MT_SPIDER ||
       actor->type == MT_SKULL)
    {
        dist /= 2;
    }

    if(dist > 200)
        dist = 200;

    if(actor->type == MT_CYBORG && dist > 160)
        dist = 160;

    if(P_Random() < dist)
        return false;

    return true;
}

/**
 * Move in the current direction. $dropoff_fix
 *
 * @return              @c false, if the move is blocked.
 */
static boolean moveMobj(mobj_t *actor, boolean dropoff)
{
    float               pos[3], step[3];
    linedef_t          *ld;
    boolean             good;

    if(actor->moveDir == DI_NODIR)
        return false;

    if((unsigned) actor->moveDir >= 8)
        Con_Error("Weird actor->moveDir!");

    step[VX] = actor->info->speed * dirSpeed[actor->moveDir][MX];
    step[VY] = actor->info->speed * dirSpeed[actor->moveDir][MY];
    pos[VX] = actor->pos[VX] + step[VX];
    pos[VY] = actor->pos[VY] + step[VY];

    // $dropoff_fix
    if(!P_TryMove(actor, pos[VX], pos[VY], dropoff, false))
    {
        // Open any specials.
        if((actor->flags & MF_FLOAT) && floatOk)
        {
            // Must adjust height.
            if(actor->pos[VZ] < tmFloorZ)
                actor->pos[VZ] += FLOATSPEED;
            else
                actor->pos[VZ] -= FLOATSPEED;

            actor->flags |= MF_INFLOAT;
            return true;
        }

        if(!P_IterListSize(spechit))
            return false;

        actor->moveDir = DI_NODIR;
        good = false;
        while((ld = P_PopIterList(spechit)) != NULL)
        {
            /**
             * If the special is not a door that can be opened, return false.
             *
             * $unstuck: This is what caused monsters to get stuck in
             * doortracks, because it thought that the monster freed itself
             * by opening a door, even if it was moving towards the
             * doortrack, and not the door itself.
             *
             * If a line blocking the monster is activated, return true 90%
             * of the time. If a line blocking the monster is not activated,
             * but some other line is, return false 90% of the time.
             * A bit of randomness is needed to ensure it's free from
             * lockups, but for most cases, it returns the correct result.
             *
             * Do NOT simply return false 1/4th of the time (causes monsters
             * to back out when they shouldn't, and creates secondary
             * stickiness).
             */

            if(P_ActivateLine(ld, actor, 0, SPAC_USE))
                good |= ld == blockLine ? 1 : 2;
        }

        if(!good || cfg.monstersStuckInDoors)
            return good;
        else
            return (P_Random() >= 230) || (good & 1);
    }
    else
    {
        P_MobjSetSRVO(actor, step[VX], step[VY]);
        actor->flags &= ~MF_INFLOAT;
    }

    // $dropoff_fix: fall more slowly, under gravity, if fellDown==true
    if(!(actor->flags & MF_FLOAT) && !fellDown)
    {
        if(actor->pos[VZ] > actor->floorZ)
            P_HitFloor(actor);

        actor->pos[VZ] = actor->floorZ;
    }

    return true;
}

/**
 * Attempts to move actor on in its current (ob->moveangle) direction.
 * If blocked by either a wall or an actor returns FALSE
 * If move is either clear or blocked only by a door, returns TRUE and sets...
 * If a door is in the way, an OpenDoor call is made to start it opening.
 */
static boolean tryMoveMobj(mobj_t *actor)
{
    // $dropoff_fix
    if(!moveMobj(actor, false))
    {
        return false;
    }

    actor->moveCount = P_Random() & 15;
    return true;
}

static void doNewChaseDir(mobj_t *actor, float deltaX, float deltaY)
{
    dirtype_t           xdir, ydir, tdir;
    dirtype_t           olddir = actor->moveDir;
    dirtype_t           turnaround = olddir;

    if(turnaround != DI_NODIR) // Find reverse direction.
        turnaround ^= 4;

    xdir = (deltaX > 10 ? DI_EAST : deltaX < -10 ? DI_WEST : DI_NODIR);
    ydir = (deltaY < -10 ? DI_SOUTH : deltaY > 10 ? DI_NORTH : DI_NODIR);

    // Try direct route.
    if(xdir != DI_NODIR && ydir != DI_NODIR &&
       turnaround != (actor->moveDir =
                      deltaY < 0 ? deltaX >
                      0 ? DI_SOUTHEAST : DI_SOUTHWEST : deltaX >
                      0 ? DI_NORTHEAST : DI_NORTHWEST) && tryMoveMobj(actor))
        return;

    // Try other directions.
    if(P_Random() > 200 || fabs(deltaY) > fabs(deltaX))
    {
        dirtype_t temp = xdir;

        xdir = ydir;
        ydir = temp;
    }

    if((xdir == turnaround ? xdir = DI_NODIR : xdir) != DI_NODIR &&
       (actor->moveDir = xdir, tryMoveMobj(actor)))
        return; // Either moved forward or attacked.

    if((ydir == turnaround ? ydir = DI_NODIR : ydir) != DI_NODIR &&
       (actor->moveDir = ydir, tryMoveMobj(actor)))
        return;

    // There is no direct path to the player, so pick another direction.
    if(olddir != DI_NODIR && (actor->moveDir = olddir, tryMoveMobj(actor)))
        return;

    // Randomly determine direction of search.
    if(P_Random() & 1)
    {
        for(tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++)
            if(tdir != turnaround &&
               (actor->moveDir = tdir, tryMoveMobj(actor)))
                return;
    }
    else
    {
        for(tdir = DI_SOUTHEAST; tdir != DI_EAST - 1; tdir--)
            if(tdir != turnaround &&
               (actor->moveDir = tdir, tryMoveMobj(actor)))
                return;
    }

    if((actor->moveDir = turnaround) != DI_NODIR && !tryMoveMobj(actor))
        actor->moveDir = DI_NODIR;
}

/**
 * Monsters try to move away from tall dropoffs.
 *
 * In Doom, they were never allowed to hang over dropoffs, and would remain
 * stuck if involuntarily forced over one. This logic, combined with
 * p_map.c::P_TryMove(), allows monsters to free themselves without making
 * them tend to hang over dropoffs.
 */
static boolean PIT_AvoidDropoff(linedef_t *line, void *data)
{
    sector_t           *backsector = P_GetPtrp(line, DMU_BACK_SECTOR);
    float              *bbox = P_GetPtrp(line, DMU_BOUNDING_BOX);

    if(backsector &&
       tmBBox[BOXRIGHT]  > bbox[BOXLEFT] &&
       tmBBox[BOXLEFT]   < bbox[BOXRIGHT]  &&
       tmBBox[BOXTOP]    > bbox[BOXBOTTOM] && // Linedef must be contacted
       tmBBox[BOXBOTTOM] < bbox[BOXTOP]    &&
       P_BoxOnLineSide(tmBBox, line) == -1)
    {
        sector_t   *frontsector = P_GetPtrp(line, DMU_FRONT_SECTOR);
        float       front = P_GetFloatp(frontsector, DMU_FLOOR_HEIGHT);
        float       back = P_GetFloatp(backsector, DMU_FLOOR_HEIGHT);
        float       dx = P_GetFloatp(line, DMU_DX);
        float       dy = P_GetFloatp(line, DMU_DY);
        angle_t     angle;

        // The monster must contact one of the two floors, and the other
        // must be a tall drop off (more than 24).
        if(back == floorZ && front < floorZ - 24)
        {
            angle = R_PointToAngle2(0, 0, dx, dy); // Front side drop off.
        }
        else
        {
            if(front == floorZ && back < floorZ - 24)
                angle = R_PointToAngle2(dx, dy, 0, 0); // Back side drop off.
            else
                return true;
        }

        // Move away from drop off at a standard speed.
        // Multiple contacted linedefs are cumulative (e.g. hanging over corner)
        dropoffDelta[VX] -= FIX2FLT(finesine[angle >> ANGLETOFINESHIFT] * 32);
        dropoffDelta[VY] += FIX2FLT(finecosine[angle >> ANGLETOFINESHIFT] * 32);
    }

    return true;
}

/**
 * Driver for above
 */
static boolean avoidDropoff(mobj_t *actor)
{
    floorZ = actor->pos[VZ]; // Remember floor height.

    dropoffDelta[VX] = dropoffDelta[VY] = 0;

    VALIDCOUNT++;

    // Check lines.
    P_MobjLinesIterator(actor, PIT_AvoidDropoff, 0);

    // Non-zero if movement prescribed.
    return !(dropoffDelta[VX] == 0 || dropoffDelta[VY] == 0);
}

static void newChaseDir(mobj_t *actor)
{
    mobj_t             *target = actor->target;
    float               deltaX = target->pos[VX] - actor->pos[VX];
    float               deltaY = target->pos[VY] - actor->pos[VY];

    if(actor->floorZ - actor->dropOffZ > 24 &&
       actor->pos[VZ] <= actor->floorZ &&
       !(actor->flags & (MF_DROPOFF | MF_FLOAT)) &&
       !cfg.avoidDropoffs && avoidDropoff(actor))
    {
        // Move away from dropoff.
        doNewChaseDir(actor, dropoffDelta[VX], dropoffDelta[VY]);

        // $dropoff_fix
        // If moving away from drop off, set movecount to 1 so that
        // small steps are taken to get monster away from drop off.

        actor->moveCount = 1;
        return;
    }

    doNewChaseDir(actor, deltaX, deltaY);
}

/**
 * If allaround is false, only look 180 degrees in front.
 *
 * @return              @c true, if a player is targeted.
 */
static boolean lookForPlayers(mobj_t *actor, boolean allAround)
{
    int                 c, stop, playerCount;
    player_t           *player;
    angle_t             an;
    float               dist;

    playerCount = 0;
    for(c = 0; c < MAXPLAYERS; ++c)
    {
        if(players[c].plr->inGame)
            playerCount++;
    }

    // Are there any players?
    if(!playerCount)
        return false;

    c = 0;
    stop = (actor->lastLook - 1) & 3;

    for(;; actor->lastLook = (actor->lastLook + 1) & 3)
    {
        if(!players[actor->lastLook].plr->inGame)
            continue;

        if(c++ == 2 || actor->lastLook == stop)
        {   // Done looking.
            return false;
        }

        player = &players[actor->lastLook];

        if(player->health <= 0)
            continue; // Player is already dead.

        if(!P_CheckSight(actor, player->plr->mo))
            continue; // Player is out of sight.

        if(!allAround)
        {
            an = R_PointToAngle2(actor->pos[VX],
                                 actor->pos[VY],
                                 player->plr->mo->pos[VX],
                                 player->plr->mo->pos[VY]);
            an -= actor->angle;

            if(an > ANG90 && an < ANG270)
            {
                dist =
                    P_ApproxDistance(player->plr->mo->pos[VX] - actor->pos[VX],
                                     player->plr->mo->pos[VY] - actor->pos[VY]);
                // If real close, react anyway.
                if(dist > MELEERANGE)
                    continue; // Behind back.
            }
        }

        actor->target = player->plr->mo;
        return true;
    }
}

int P_Massacre(void)
{
    int                 count = 0;
    mobj_t             *mo;
    thinker_t          *think;

    // Only massacre when in a level.
    if(G_GetGameState() != GS_LEVEL)
        return 0;

    for(think = thinkerCap.next; think != &thinkerCap; think = think->next)
    {
        if(think->function != P_MobjThinker)
            continue; // Not a mobj thinker.

        mo = (mobj_t *) think;
        if(mo->type == MT_SKULL ||
           ((mo->flags & MF_COUNTKILL) && mo->health > 0))
        {
            P_DamageMobj(mo, NULL, NULL, 10000);
            count++;
        }
    }

    return count;
}

/**
 * Initialize boss brain targets at level startup, rather than at boss
 * wakeup, to prevent savegame-related crashes.
 *
 * \todo Does not belong in this file, find it a better home.
 */
void P_SpawnBrainTargets(void)
{
    thinker_t          *thinker;
    mobj_t             *m;

    // Find all the target spots.
    for(thinker = thinkerCap.next; thinker != &thinkerCap;
        thinker = thinker->next)
    {
        if(thinker->function != P_MobjThinker)
            continue; // Not a mobj.

        m = (mobj_t *) thinker;

        if(m->type == MT_BOSSTARGET)
        {
            if(numBrainTargets >= numBrainTargetsAlloc)
            {
                // Do we need to alloc more targets?
                if(numBrainTargets == numBrainTargetsAlloc)
                {
                    numBrainTargetsAlloc *= 2;
                    brainTargets =
                        Z_Realloc(brainTargets,
                                  numBrainTargetsAlloc * sizeof(*brainTargets),
                                  PU_LEVEL);
                }
                else
                {
                    numBrainTargetsAlloc = 32;
                    brainTargets =
                        Z_Malloc(numBrainTargetsAlloc * sizeof(*brainTargets),
                                 PU_LEVEL, NULL);
                }
            }

            brainTargets[numBrainTargets++] = m;
        }
    }
}

static boolean countMobjsOfType(int type)
{
    thinker_t          *th;
    mobj_t             *mo;
    size_t              num = 0;

    // Scan the thinkers to count the number of mobjs of the given type.
    for(th = thinkerCap.next; th != &thinkerCap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo = (mobj_t *) th;
        if(mo->type == type && mo->health > 0)
        {
            num++;
        }
    }

    return num;
}

/**
 * DJS - Next up we have an obscene amount of repetion; 15(!) copies of
 * DOOM's A_KeenDie() with only very minor changes.
 *
 * \todo Replace this lot with XG (maybe need to add a new flag for
 * targeting "mobjs like me").
 */

/**
 * kaiser - Used for special stuff. works only per monster!!!
 */
void C_DECL A_BitchSpecial(mobj_t *mo)
{
    linedef_t          *dummyLine;

    A_Fall(mo);

    if(countMobjsOfType(mo->type) > 1) // +1 as this mobj is included.
    {   // There are others like us still alive.
        return;
    }

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 4459; // d64tc was 666.
    EV_DoDoor(dummyLine, lowerFloorToLowest); // d64tc was open.
    P_FreeDummyLine(dummyLine);
}

/**
 * kaiser - Used for special stuff. works only per monster!!!
 */
void C_DECL A_PossSpecial(mobj_t* mo)
{
    linedef_t          *dummyLine;

    A_Fall(mo);

    if(countMobjsOfType(mo->type) > 1) // +1 as this mobj is included.
    {   // There are others like us still alive.
        return;
    }

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 4444;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_SposSpecial(mobj_t* mo)
{
    linedef_t          *dummyLine;

    A_Fall(mo);

    if(countMobjsOfType(mo->type) > 1) // +1 as this mobj is included.
    {   // There are others like us still alive.
        return;
    }

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 4445;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_TrooSpecial(mobj_t* mo)
{
    linedef_t          *dummyLine;

    A_Fall(mo);

    if(countMobjsOfType(mo->type) > 1) // +1 as this mobj is included.
    {   // There are others like us still alive.
        return;
    }

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 4446;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_NtroSpecial(mobj_t* mo)
{
    linedef_t          *dummyLine;

    A_Fall(mo);

    if(countMobjsOfType(mo->type) > 1) // +1 as this mobj is included.
    {   // There are others like us still alive.
        return;
    }

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 4447;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_SargSpecial(mobj_t* mo)
{
    linedef_t          *dummyLine;

    A_Fall(mo);

    if(countMobjsOfType(mo->type) > 1) // +1 as this mobj is included.
    {   // There are others like us still alive.
        return;
    }

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 4448;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_Sar2Special(mobj_t* mo)
{
    linedef_t          *dummyLine;

    A_Fall(mo);

    if(countMobjsOfType(mo->type) > 1) // +1 as this mobj is included.
    {   // There are others like us still alive.
        return;
    }

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 4449;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_HeadSpecial(mobj_t* mo)
{
    linedef_t          *dummyLine;

    A_Fall(mo);

    if(countMobjsOfType(mo->type) > 1) // +1 as this mobj is included.
    {   // There are others like us still alive.
        return;
    }

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 4450;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_Hed2Special(mobj_t* mo)
{
    linedef_t          *dummyLine;

    A_Fall(mo);

    if(countMobjsOfType(mo->type) > 1) // +1 as this mobj is included.
    {   // There are others like us still alive.
        return;
    }

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 4451;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_SkulSpecial(mobj_t* mo)
{
    linedef_t          *dummyLine;

    A_Fall(mo);

    if(countMobjsOfType(mo->type) > 1) // +1 as this mobj is included.
    {   // There are others like us still alive.
        return;
    }

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 4452;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_Bos2Special(mobj_t* mo)
{
    linedef_t          *dummyLine;

    A_Fall(mo);

    if(countMobjsOfType(mo->type) > 1) // +1 as this mobj is included.
    {   // There are others like us still alive.
        return;
    }

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 4453;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_BossSpecial(mobj_t* mo)
{
    linedef_t          *dummyLine;

    A_Fall(mo);

    if(countMobjsOfType(mo->type) > 1) // +1 as this mobj is included.
    {   // There are others like us still alive.
        return;
    }

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 4454;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_PainSpecial(mobj_t* mo)
{
    linedef_t          *dummyLine;

    A_Fall(mo);

    if(countMobjsOfType(mo->type) > 1) // +1 as this mobj is included.
    {   // There are others like us still alive.
        return;
    }

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 4455;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_FattSpecial(mobj_t* mo)
{
    linedef_t          *dummyLine;

    A_Fall(mo);

    if(countMobjsOfType(mo->type) > 1) // +1 as this mobj is included.
    {   // There are others like us still alive.
        return;
    }

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 4456;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_BabySpecial(mobj_t* mo)
{
    linedef_t          *dummyLine;

    A_Fall(mo);

    if(countMobjsOfType(mo->type) > 1) // +1 as this mobj is included.
    {   // There are others like us still alive.
        return;
    }

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 4457;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_CybrSpecial(mobj_t* mo)
{
    linedef_t          *dummyLine;

    A_Fall(mo);

    if(countMobjsOfType(mo->type) > 1) // +1 as this mobj is included.
    {   // There are others like us still alive.
        return;
    }

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 4458;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/**
 * Stay in state until a player is sighted.
 */
void C_DECL A_Look(mobj_t *actor)
{
    sector_t           *sec = NULL;
    mobj_t             *targ;

    sec = P_GetPtrp(actor->subsector, DMU_SECTOR);

    if(!sec)
        return;

    actor->threshold = 0; // Any shot will wake us up.
    targ = P_ToXSector(sec)->soundTarget;

    if(targ && (targ->flags & MF_SHOOTABLE))
    {
        actor->target = targ;

        if(actor->flags & MF_AMBUSH)
        {
            if(P_CheckSight(actor, actor->target))
                goto seeyou;
        }
        else
            goto seeyou;
    }

    if(!lookForPlayers(actor, false))
        return;

    // Go into chase state.
  seeyou:
    if(actor->info->seeSound)
    {
        int                 sound;

        switch(actor->info->seeSound)
        {
        case sfx_posit1:
        case sfx_posit2:
        case sfx_posit3:
            sound = sfx_posit1 + P_Random() % 3;
            break;

        case sfx_bgsit1:
        case sfx_bgsit2:
            sound = sfx_bgsit1 + P_Random() % 2;
            break;

        default:
            sound = actor->info->seeSound;
            break;
        }

        if(actor->flags2 & MF2_BOSS)
        {   // Full volume.
            S_StartSound(sound | DDSF_NO_ATTENUATION, actor);
        }
        else
        {
            S_StartSound(sound, actor);
        }
    }

    P_MobjChangeState(actor, actor->info->seeState);
}

/**
 * Actor has a melee attack, so it tries to close as fast as possible.
 */
void C_DECL A_Chase(mobj_t *actor)
{
    int                 delta;

    // d64tc >
    if(actor->flags & MF_FLOAT)
    {
        int                 r = P_Random();

        if(r < 64)
            actor->mom[MZ] += 1;
        else if(r < 128)
            actor->mom[MZ] -= 1;
    }
    // < d64tc

    if(actor->reactionTime)
        actor->reactionTime--;

    // Modify target threshold.
    if(actor->threshold)
    {
        if(!actor->target || actor->target->health <= 0)
        {
            actor->threshold = 0;
        }
        else
            actor->threshold--;
    }

    // Turn towards movement direction if not there yet.
    if(actor->moveDir < 8)
    {
        actor->angle &= (7 << 29);
        delta = actor->angle - (actor->moveDir << 29);

        if(delta > 0)
            actor->angle -= ANG90 / 2;
        else if(delta < 0)
            actor->angle += ANG90 / 2;
    }

    if(!actor->target || !(actor->target->flags & MF_SHOOTABLE))
    {
        // Look for a new target.
        if(lookForPlayers(actor, true))
        {   // Got a new target.
        }
        else
        {
            P_MobjChangeState(actor, actor->info->spawnState);
        }

        return;
    }

    // Do not attack twice in a row.
    if(actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;
        if(gameSkill != SM_NIGHTMARE && !fastParm)
            newChaseDir(actor);

        return;
    }

    // Check for melee attack.
    if(actor->info->meleeState && checkMeleeRange(actor))
    {
        if(actor->info->attackSound)
            S_StartSound(actor->info->attackSound, actor);

        P_MobjChangeState(actor, actor->info->meleeState);
        return;
    }

    // Check for missile attack.
    if(actor->info->missileState)
    {
        if(!(gameSkill < SM_NIGHTMARE && !fastParm && actor->moveCount))
        {
            if(checkMissileRange(actor))
            {
                P_MobjChangeState(actor, actor->info->missileState);
                actor->flags |= MF_JUSTATTACKED;
                return;
            }
        }
    }

    // Possibly choose another target.
    if(IS_NETGAME && !actor->threshold &&
       !P_CheckSight(actor, actor->target))
    {
        if(lookForPlayers(actor, true))
            return; // Got a new target.
    }

    // Chase towards player.
    if(--actor->moveCount < 0 || !moveMobj(actor, false))
    {
        newChaseDir(actor);
    }

    // Make active sound.
    if(actor->info->activeSound && P_Random() < 3)
    {
        S_StartSound(actor->info->activeSound, actor);
    }
}

void C_DECL A_FaceTarget(mobj_t *actor)
{
    if(!actor->target)
        return;

    actor->turnTime = true; // $visangle-facetarget
    actor->flags &= ~MF_AMBUSH;
    actor->angle =
        R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                        actor->target->pos[VX], actor->target->pos[VY]);

    if(actor->target->flags & MF_SHADOW)
        actor->angle += (P_Random() - P_Random()) << 21;
}

void C_DECL A_PosAttack(mobj_t *actor)
{
    int                 damage;
    angle_t             angle;
    float               slope;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    angle = actor->angle;
    slope = P_AimLineAttack(actor, angle, MISSILERANGE);

    S_StartSound(sfx_pistol, actor);
    angle += (P_Random() - P_Random()) << 20;
    damage = ((P_Random() % 5) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void C_DECL A_SPosAttack(mobj_t *actor)
{
    int                 i, damage;
    angle_t             angle, bangle;
    float               slope;

    if(!actor->target)
        return;

    S_StartSound(sfx_shotgn, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    for(i = 0; i < 3; ++i)
    {
        angle = bangle + ((P_Random() - P_Random()) << 20);
        damage = ((P_Random() % 5) + 1) * 3;

        P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
    }
}

/**
 * d64tc
 */
void C_DECL A_CposPanLeft(mobj_t* actor)
{
    actor->angle = actor->angle + ANG90/4;
}

/**
 * d64tc
 */
void C_DECL A_CposPanRight(mobj_t* actor)
{
    actor->angle = actor->angle - ANG90/4;
}

void C_DECL A_CPosAttack(mobj_t *actor)
{
    int                 angle, bangle, damage, r;
    float               slope;

    if(!actor->target)
        return;

    S_StartSound(sfx_pistol, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    angle = bangle + ((P_Random() - P_Random()) << 20);
    damage = ((P_Random() % 5) + 1) * 3;

    r = P_Random();
    if(r < 64)
        A_CposPanLeft(actor);
    else if(r < 128)
        A_CposPanRight(actor);

    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void C_DECL A_CPosRefire(mobj_t *actor)
{
    if(!actor->target) // d64tc
        return;

    // Keep firing unless target got out of sight.
    A_FaceTarget(actor);

    if(P_Random() < 30) // d64tc: was "if(P_Random() < 40)"
        return;

    // d64tc: Added "|| P_Random() < 40"
    if(!actor->target || actor->target->health <= 0 ||
       !P_CheckSight(actor, actor->target) || P_Random() < 40)
    {
        P_MobjChangeState(actor, actor->info->seeState);
    }
}

void C_DECL A_SpidRefire(mobj_t *actor)
{
    // Keep firing unless target got out of sight.
    A_FaceTarget(actor);

    if(P_Random() < 10)
        return;

    if(!actor->target || actor->target->health <= 0 ||
       !P_CheckSight(actor, actor->target))
    {
        P_MobjChangeState(actor, actor->info->seeState);
    }
}

/**
 * d64tc: BspiAttack. Throw projectile.
 */
void BabyFire(mobj_t *actor, int type, boolean right)
{
#define BSPISPREAD                  (ANG90/8) //its cheap but it works
#define BABY_DELTAANGLE             (85*ANGLE_1)
#define BABY_ARM_EXTENSION_SHORT    (18)
#define BABY_ARM_HEIGHT             (24)

    mobj_t             *mo;
    angle_t             ang;
    float               pos[3];

    ang = actor->angle;
    if(right)
        ang += BABY_DELTAANGLE;
    else
        ang -= BABY_DELTAANGLE;
    ang >>= ANGLETOFINESHIFT;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += BABY_ARM_EXTENSION_SHORT * FIX2FLT(finecosine[ang]);
    pos[VY] += BABY_ARM_EXTENSION_SHORT * FIX2FLT(finesine[ang]);
    pos[VZ] -= actor->floorClip + BABY_ARM_HEIGHT;

    mo = P_SpawnMotherMissile(type, pos[VX], pos[VY], pos[VZ],
                              actor, actor->target);

    if(right)
        mo->angle += BSPISPREAD/6;
    else
        mo->angle -= BSPISPREAD/6;

    ang = mo->angle >> ANGLETOFINESHIFT;
    mo->mom[MX] = mo->info->speed * FIX2FLT(finecosine[ang]);
    mo->mom[MY] = mo->info->speed * FIX2FLT(finesine[ang]);

#undef BSPISPREAD
#undef BABY_DELTAANGLE
#undef BABY_ARM_EXTENSION_SHORT
#undef BABY_ARM_HEIGHT
}

/**
 * Shoot two plasmaballs while aligned to cannon (should of been like this
 * in Doom 2! - kaiser).
 */
void C_DECL A_BspiAttack(mobj_t *actor)
{
    int                 type = P_Random() % 2;

    switch(type)
    {
    case 0:
        if(actor->type == MT_BABY || actor->info->doomedNum == 234)
            type = MT_ARACHPLAZ;
        else if(actor->type == MT_NIGHTCRAWLER)
            type = MT_GRENADE;
        break;

    case 1:
        if(actor->type == MT_BABY || actor->info->doomedNum == 234)
            type = MT_ARACHPLAZ;
        else if(actor->type == MT_NIGHTCRAWLER)
            type = MT_GRENADE;
        break;
    }

    BabyFire(actor, type, false);
    BabyFire(actor, type, true);
}

/**
 * Formerly A_BspiAttack? - DJS
 */
void C_DECL A_TroopAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // Launch a missile.
    P_SpawnMissile(MT_TROOPSHOT, actor, actor->target);
}

/**
 * Formerly A_TroopAttack? - DJS
 */
void C_DECL A_TroopClaw(mobj_t *actor)
{
    int                 damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(checkMeleeRange(actor))
    {
        S_StartSound(sfx_claw, actor);
        damage = (P_Random() % 8 + 1) * 3;
        P_DamageMobj(actor->target, actor, actor, damage);
        return;
    }
}

void C_DECL A_NtroopAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // Launch a missile.
    P_SpawnMissile(MT_NTROSHOT, actor, actor->target);
}

/**
 * Mother Demon: Floorfire attack.
 */
void C_DECL A_MotherFloorFire(mobj_t *actor)
{
/*
#define FIRESPREAD      (ANG90 / 8 * 4)

    mobj_t             *mo;
    int                 an;
*/
    if(!actor->target)
        return;

    A_FaceTarget(actor);
    S_StartSound(sfx_mthatk, actor);
/*
    mo = P_SpawnMissile(MT_FIREEND, actor, actor->target);

    mo = P_SpawnMissile(MT_FIREEND, actor, actor->target);
    mo->angle -= FIRESPREAD;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->mom[MX] = mo->info->speed * FIX2FLT(finecosine[an]);
    mo->mom[MY] = mo->info->speed * FIX2FLT(finesine[an]);

    mo = P_SpawnMissile(MT_FIREEND, actor, actor->target);
    mo->angle += FIRESPREAD;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->mom[MX] = mo->info->speed * FIX2FLT(finecosine[an]);
    mo->mom[MY] = mo->info->speed * FIX2FLT(finesine[an]);

#undef FIRESPREAD
*/
}

static void motherFire(mobj_t *actor, int type, angle_t angle,
                       float distance, float height)
{
    angle_t             ang;
    float               pos[3];

    ang = actor->angle + angle;
    ang >>= ANGLETOFINESHIFT;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += distance * FIX2FLT(finecosine[ang]);
    pos[VY] += distance * FIX2FLT(finesine[ang]);
    pos[VZ] += -actor->floorClip + height;

    P_SpawnMotherMissile(type, pos[VX], pos[VY], pos[VZ],
                         actor, actor->target);
}

/**
 * d64tc: MotherDemon's Missle Attack code
 */
void C_DECL A_MotherMissle(mobj_t *actor)
{
#define MOTHER_DELTAANGLE           (85*ANGLE_1)
#define MOTHER_ARM_EXTENSION_SHORT  (40)
#define MOTHER_ARM_EXTENSION_LONG   (55)
#define MOTHER_ARM1_HEIGHT          (128)
#define MOTHER_ARM2_HEIGHT          (128)
#define MOTHER_ARM3_HEIGHT          (64)
#define MOTHER_ARM4_HEIGHT          (64)

    // Fire 4 missiles at once.
    motherFire(actor, MT_BITCHBALL, -MOTHER_DELTAANGLE,
               MOTHER_ARM_EXTENSION_SHORT, MOTHER_ARM1_HEIGHT);
    motherFire(actor, MT_BITCHBALL, MOTHER_DELTAANGLE,
               MOTHER_ARM_EXTENSION_SHORT, MOTHER_ARM2_HEIGHT);
    motherFire(actor, MT_BITCHBALL, -MOTHER_DELTAANGLE,
               MOTHER_ARM_EXTENSION_LONG, MOTHER_ARM3_HEIGHT);
    motherFire(actor, MT_BITCHBALL, MOTHER_DELTAANGLE,
               MOTHER_ARM_EXTENSION_LONG, MOTHER_ARM4_HEIGHT);

#undef MOTHER_DELTAANGLE
#undef MOTHER_ARM_EXTENSION_SHORT
#undef MOTHER_ARM_EXTENSION_LONG
#undef MOTHER_ARM1_HEIGHT
#undef MOTHER_ARM2_HEIGHT
#undef MOTHER_ARM3_HEIGHT
#undef MOTHER_ARM4_HEIGHT
}

/**
 * d64tc - Unused?
 */
void C_DECL A_SetFloorFire(mobj_t *actor)
{
/*
    mobj_t             *mo;
    float               pos[3];

    actor->pos[VZ] = actor->floorZ;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += FIX2FLT((P_Random() - P_Random()) << 10);
    pos[VY] += FIX2FLT((P_Random() - P_Random()) << 10);
    pos[VZ]  = ONFLOORZ;

    mo = P_SpawnMobj3fv(MT_SPAWNFIRE, pos);
    mo->target = actor->target;
*/
}

/**
 * d64tc
 */
void C_DECL A_MotherBallExplode(mobj_t *spread)
{
    int                 i, an;
    angle_t             angle;
    mobj_t             *shard;

    for(i = 0; i < 8; ++i)
    {
        shard = P_SpawnMobj3fv(MT_HEADSHOT, spread->pos);

        angle = i * ANG45;
        shard->target = spread->target;
        shard->angle  = angle;

        an = angle >> ANGLETOFINESHIFT;
        shard->mom[MX] = shard->info->speed * FIX2FLT(finecosine[an]);
        shard->mom[MY] = shard->info->speed * FIX2FLT(finesine[an]);
    }
}

/**
 * d64tc: Spawns a smoke sprite during the missle attack.
 */
void C_DECL A_BitchTracerPuff(mobj_t *smoke)
{
    if(!smoke)
        return;

    P_SpawnMobj3fv(MT_MOTHERPUFF, smoke->pos);
}

void C_DECL A_SargAttack(mobj_t *actor)
{
    int                 damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(checkMeleeRange(actor))
    {
        damage = ((P_Random() % 10) + 1) * 4;
        P_DamageMobj(actor->target, actor, actor, damage);
    }
}

void C_DECL A_HeadAttack(mobj_t *actor)
{
    int                 damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(checkMeleeRange(actor))
    {
        damage = (P_Random() % 6 + 1) * 10;
        P_DamageMobj(actor->target, actor, actor, damage);
        return;
    }

    // Launch a missile.
    P_SpawnMissile(MT_HEADSHOT, actor, actor->target);
}

/**
 * Cyber Demon: Missile Attack.
 *
 * Heavily modified for d64tc.
 */
void C_DECL A_CyberAttack(mobj_t *actor)
{
#define CYBER_DELTAANGLE            (85*ANGLE_1)
#define CYBER_ARM_EXTENSION_SHORT   (35)
#define CYBER_ARM1_HEIGHT           (68)

    angle_t             ang;
    float               pos[3];

    // This aligns the rocket to the d64tc cyberdemon's rocket launcher.
    ang = (actor->angle + CYBER_DELTAANGLE) >> ANGLETOFINESHIFT;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += CYBER_ARM_EXTENSION_SHORT * FIX2FLT(finecosine[ang]);
    pos[VY] += CYBER_ARM_EXTENSION_SHORT * FIX2FLT(finesine[ang]);
    pos[VZ] += -actor->floorClip + CYBER_ARM1_HEIGHT;

    P_SpawnMotherMissile(MT_CYBERROCKET, pos[VX], pos[VY], pos[VZ],
                         actor, actor->target);

#undef CYBER_DELTAANGLE
#undef CYBER_ARM_EXTENSION_SHORT
#undef CYBER_ARM1_HEIGHT
}

void C_DECL A_BruisAttack(mobj_t *actor)
{
    int                 damage;

    if(!actor->target)
        return;

    if(checkMeleeRange(actor))
    {
        S_StartSound(sfx_claw, actor);
        damage = (P_Random() % 8 + 1) * 10;
        P_DamageMobj(actor->target, actor, actor, damage);
        return;
    }

    // Launch a missile.
    P_SpawnMissile(MT_BRUISERSHOT, actor, actor->target);
}

/**
 * d64tc Special Bruiser shot for Baron.
 */
void C_DECL A_BruisredAttack(mobj_t *actor)
{
    int                 damage;

    if(!actor->target)
        return;

    if(checkMeleeRange(actor))
    {
        S_StartSound(sfx_claw, actor);
        damage = (P_Random() % 8 + 1) * 10;
        P_DamageMobj(actor->target, actor, actor, damage);
        return;
    }

    // Launch a missile.
    P_SpawnMissile(MT_BRUISERSHOTRED, actor, actor->target);
}

/**
 * kaiser - Too lazy to add a new action, instead I'll just borrow this one.
 * DJS - yup you are lazy :P
 *
 * \todo Implement this properly as two seperate actions.
 */
void C_DECL A_SkelMissile(mobj_t *actor)
{
    mobj_t             *mo;

    if(actor->type == MT_STALKER)
    {
        if(!((actor->flags & MF_SOLID) && (actor->flags & MF_SHOOTABLE)))
        {
            actor->flags |= MF_SOLID;
            actor->flags |= MF_SHOOTABLE;

            P_SpawnMobj3fv(MT_HFOG, actor->pos);
            S_StartSound(sfx_stlktp, actor);
            return;
        }

        if(!actor->target)
            return;

        if(P_Random() < 64)
        {
            P_SpawnMobj3fv(MT_HFOG, actor->pos);

            S_StartSound(sfx_stlktp, actor);
            P_MobjChangeState(actor, S_STALK_HIDE);
            actor->flags &= ~MF_SOLID;
            actor->flags &= ~MF_SHOOTABLE;

            memcpy(actor->pos, actor->target->pos, sizeof(actor->pos));
            actor->pos[VZ] += 32;
        }
        else
        {
            A_FaceTarget(actor);
            mo = P_SpawnMissile(MT_TRACER, actor, actor->target);

            mo->pos[VX] += mo->mom[MX];
            mo->pos[VY] += mo->mom[MY];
            mo->tracer = actor->target;
        }
    }
    else
    {
        if(!actor->target)
            return;

        A_FaceTarget(actor);
        mo = P_SpawnMissile(MT_TRACER, actor, actor->target);

        mo->pos[VX] += mo->mom[MX];
        mo->pos[VY] += mo->mom[MY];
        mo->tracer = actor->target;
    }
}

void C_DECL A_Tracer(mobj_t *actor)
{
    angle_t             exact;
    float               dist;
    float               slope;
    mobj_t             *dest;
    mobj_t             *th;

    if(GAMETIC & 3)
        return;

    // Spawn a puff of smoke behind the rocket.
    P_SpawnCustomPuff(MT_ROCKETPUFF, actor->pos[VX],
                      actor->pos[VY],
                      actor->pos[VZ]);

    th = P_SpawnMobj3f(MT_SMOKE,
                       actor->pos[VX] - actor->mom[MX],
                       actor->pos[VY] - actor->mom[MY],
                       actor->pos[VZ]);

    th->mom[MZ] = 1;
    th->tics -= P_Random() & 3;
    if(th->tics < 1)
        th->tics = 1;

    // Adjust direction.
    dest = actor->tracer;

    if(!dest || dest->health <= 0)
        return;

    // Change angle.
    exact = R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                            dest->pos[VX], dest->pos[VY]);

    if(exact != actor->angle)
    {
        if(exact - actor->angle > 0x80000000)
        {
            actor->angle -= TRACEANGLE;
            if(exact - actor->angle < 0x80000000)
                actor->angle = exact;
        }
        else
        {
            actor->angle += TRACEANGLE;
            if(exact - actor->angle > 0x80000000)
                actor->angle = exact;
        }
    }

    exact = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = FIX2FLT(FixedMul(actor->info->speed, finecosine[exact]));
    actor->mom[MY] = FIX2FLT(FixedMul(actor->info->speed, finesine[exact]));

    // Change slope.
    dist = P_ApproxDistance(dest->pos[VX] - actor->pos[VX],
                            dest->pos[VY] - actor->pos[VY]);

    dist /= FIX2FLT(actor->info->speed);

    if(dist < 1)
        dist = 1;
    slope = (dest->pos[VZ] + 40 - actor->pos[VZ]) / dist;

    if(slope < actor->mom[MZ])
        actor->mom[MZ] -= 1 / 8;
    else
        actor->mom[MZ] += 1 / 8;
}

void C_DECL A_SkelWhoosh(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);
    S_StartSound(sfx_skeswg, actor);
}

void C_DECL A_SkelFist(mobj_t *actor)
{
    int                 damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(checkMeleeRange(actor))
    {
        damage = ((P_Random() % 10) + 1) * 6;
        S_StartSound(sfx_skepch, actor);
        P_DamageMobj(actor->target, actor, actor, damage);
    }
}

void C_DECL A_FatRaise(mobj_t *actor)
{
    A_FaceTarget(actor);
    S_StartSound(sfx_manatk, actor);
}

/**
 * d64tc: Used for mancubus projectile.
 */
static void fatFire(mobj_t *actor, int type, angle_t spread, angle_t angle,
                    float distance, float height)
{
    mobj_t             *mo;
    angle_t             an;
    float               pos[3];

    an = (actor->angle + angle) >> ANGLETOFINESHIFT;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += distance * FIX2FLT(finecosine[an]);
    pos[VY] += distance * FIX2FLT(finesine[an]);
    pos[VZ] += -actor->floorClip + height;

    mo = P_SpawnMotherMissile(type, pos[VX], pos[VY], pos[VZ],
                              actor, actor->target);
    mo->angle += spread;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->mom[MX] = mo->info->speed * FIX2FLT(finecosine[an]);
    mo->mom[MY] = mo->info->speed * FIX2FLT(finesine[an]);
}

/**
 * d64tc
 */
void C_DECL A_FatAttack1(mobj_t *actor)
{
    fatFire(actor, MT_FATSHOT, -(FATSPREAD / 4), -FAT_DELTAANGLE,
            FAT_ARM_EXTENSION_SHORT, FAT_ARM_HEIGHT);
    fatFire(actor, MT_FATSHOT, FATSPREAD * 1.5, FAT_DELTAANGLE,
            FAT_ARM_EXTENSION_LONG, FAT_ARM_HEIGHT);
}

/**
 * d64tc
 */
void C_DECL A_FatAttack2(mobj_t *actor)
{
    fatFire(actor, MT_FATSHOT, -(FATSPREAD * 1.5), -FAT_DELTAANGLE,
            FAT_ARM_EXTENSION_LONG, FAT_ARM_HEIGHT);
    fatFire(actor, MT_FATSHOT, FATSPREAD / 4, FAT_DELTAANGLE,
            FAT_ARM_EXTENSION_SHORT, FAT_ARM_HEIGHT);
}

/**
 * d64tc
 */
void C_DECL A_FatAttack3(mobj_t *actor)
{
    fatFire(actor, MT_FATSHOT, FATSPREAD / 4, FAT_DELTAANGLE,
            FAT_ARM_EXTENSION_SHORT, FAT_ARM_HEIGHT);
    fatFire(actor, MT_FATSHOT, -(FATSPREAD / 4), -FAT_DELTAANGLE,
            FAT_ARM_EXTENSION_SHORT, FAT_ARM_HEIGHT);
}

/**
 * LostSoul Attack: Fly at the player like a missile.
 */
void C_DECL A_SkullAttack(mobj_t *actor)
{
    mobj_t             *dest;
    uint                an;
    float               dist;

    if(!actor->target)
        return;

    dest = actor->target;
    actor->flags |= MF_SKULLFLY;

    S_StartSound(actor->info->attackSound, actor);
    A_FaceTarget(actor);

    an = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = SKULLSPEED * FIX2FLT(finecosine[an]);
    actor->mom[MY] = SKULLSPEED * FIX2FLT(finesine[an]);

    dist = P_ApproxDistance(dest->pos[VX] - actor->pos[VX],
                            dest->pos[VY] - actor->pos[VY]);
    dist /= SKULLSPEED;

    if(dist < 1)
        dist = 1;
    actor->mom[MZ] =
        (dest->pos[VZ] + (dest->height / 2) - actor->pos[VZ]) / dist;
}

/**
 * PainElemental Attack: Spawn a lost soul and launch it at the target.
 */
void C_DECL A_PainShootSkull(mobj_t *actor, angle_t angle)
{
    float               pos[3];
    mobj_t             *newmobj;
    uint                an;
    float               prestep;
    int                 count;
    sector_t           *sec;
    thinker_t          *currentthinker;

    // DJS - Compat option for unlimited lost soul spawns
    if(cfg.maxSkulls)
    {
        // Count total number of skull currently on the level.
        count = 0;

        currentthinker = thinkerCap.next;
        while(currentthinker != &thinkerCap)
        {
            if((currentthinker->function == P_MobjThinker) &&
               ((mobj_t *) currentthinker)->type == MT_SKULL)
                count++;

            currentthinker = currentthinker->next;
        }

        // If there are already 20 skulls on the level, don't spit another.
        if(count > 20)
            return;
    }

    an = angle >> ANGLETOFINESHIFT;

    prestep = 4 +
        3 * ((actor->info->radius + mobjInfo[MT_SKULL].radius) / 2);

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += prestep * FIX2FLT(finecosine[an]);
    pos[VY] += prestep * FIX2FLT(finesine[an]);
    pos[VZ] += 8;

    // Compat option to prevent spawning lost souls inside walls.
    if(cfg.allowSkullsInWalls)
    {
        newmobj = P_SpawnMobj3fv(MT_SKULL, pos);
    }
    else
    {
       /**
         * Check whether the Lost Soul is being fired through a 1-sided
         * wall or an impassible line, or a "monsters can't cross" line.
         * If it is, then we don't allow the spawn.
         */

        if(P_CheckSides(actor, pos[VX], pos[VY]))
            return;

        newmobj = P_SpawnMobj3fv(MT_SKULL, pos);
        sec = P_GetPtrp(newmobj->subsector, DMU_SECTOR);

        // Check to see if the new Lost Soul's z value is above the
        // ceiling of its new sector, or below the floor. If so, kill it.
        if((newmobj->pos[VZ] >
              (P_GetFloatp(sec, DMU_CEILING_HEIGHT) - newmobj->height)) ||
           (newmobj->pos[VZ] < P_GetFloatp(sec, DMU_FLOOR_HEIGHT)))
        {
            // Kill it immediately.
            P_DamageMobj(newmobj, actor, actor, 10000);
            return;
        }
    }

    // Check for movements $dropoff_fix.
    if(!P_TryMove(newmobj, newmobj->pos[VX], newmobj->pos[VY], false, false))
    {
        // Kill it immediately.
        P_DamageMobj(newmobj, actor, actor, 10000);
        return;
    }

    newmobj->target = actor->target;
    A_SkullAttack(newmobj);
}

/**
 * PainElemental Attack: Spawn a lost soul and launch it at the target.
 */
void C_DECL A_PainAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // d64tc - Shoots two lost souls from left and right side.
    A_PainShootSkull(actor, actor->angle + ANG270);
    A_PainShootSkull(actor, actor->angle + ANG90);
}

void C_DECL A_PainDie(mobj_t *actor)
{
    A_Fall(actor);
    A_PainShootSkull(actor, actor->angle + ANG90);
    A_PainShootSkull(actor, actor->angle + ANG180);
    A_PainShootSkull(actor, actor->angle + ANG270);
}

/**
 * d64tc: Rocket Trail Puff
 * kaiser - Current Rocket Puff code unknown because I know squat.
 *          A fixed version of the pain attack code.
 *
 * DJS - This looks to be doing something similar to the pain elemental
 *       above in that it could possibily spawn mobjs in the void. In this
 *       instance its of little consequence as they are just for fx.
 */
void A_Rocketshootpuff(mobj_t *actor, angle_t angle)
{
    uint                an;
    float               prestep;
    float               pos[3];
    mobj_t             *mo;

    an = angle >> ANGLETOFINESHIFT;

    prestep = 4 + 3 *
              (actor->info->radius + mobjInfo[MT_ROCKETPUFF].radius) / 2;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += prestep * FIX2FLT(finecosine[an]);
    pos[VY] += prestep * FIX2FLT(finesine[an]);
    pos[VZ] += 8;

    mo = P_SpawnMobj3fv(MT_ROCKETPUFF, pos);

    // Check for movements $dropoff_fix.
    if(!P_TryMove(mo, mo->pos[VX], mo->pos[VY], false, false))
    {
        // Kill it immediately.
        P_DamageMobj(mo, actor, actor, 10000);
        return;
    }
}

void C_DECL A_Scream(mobj_t *actor)
{
    int                 sound;

    switch(actor->info->deathSound)
    {
    case 0:
        return;

    case sfx_podth1:
    case sfx_podth2:
    case sfx_podth3:
        sound = sfx_podth1 + P_Random() % 3;
        break;

    case sfx_bgdth1:
    case sfx_bgdth2:
        sound = sfx_bgdth1 + P_Random() % 2;
        break;

    default:
        sound = actor->info->deathSound;
        break;
    }

    // Check for bosses.
    if(actor->type == MT_SPIDER || actor->type == MT_CYBORG ||
       actor->type == MT_BITCH) // d64tc added "|| actor->type == MT_BITCH"
    {
        // Full volume.
        S_StartSound(sound | DDSF_NO_ATTENUATION, NULL);
        actor->reactionTime += 30;  // d64tc
    }
    else
        S_StartSound(sound, actor);

    // d64tc >
    // \todo This absolutely does not belong here. Seperate this out into a
    // new action.
    if(actor->type == MT_ACID)
    {
        int                 i;
        mobj_t             *mo = NULL;

        for(i = 0; i < 16; ++i)
        {
            mo = P_SpawnMissile(MT_ACIDMISSILE, actor, actor);
            if(mo)
            {
                mo->mom[MX] = FIX2FLT((P_Random() - 128) << 11);
                mo->mom[MY] = FIX2FLT((P_Random() - 128) << 11);
                mo->mom[MZ] = FIX2FLT(10 + (P_Random() << 10));
                mo->target = actor;
            }
        }
    }
    // < d64tc
}

/**
 * d64tc
 */
void C_DECL A_BossExplode(mobj_t *actor)
{
    mobj_t             *mo;
    float               pos[3];

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += FIX2FLT((P_Random() - 128) << 11);
    pos[VY] += FIX2FLT((P_Random() - 128) << 11);
    pos[VZ] += actor->height / 2;

    mo = P_SpawnMobj3fv(MT_KABOOM, pos);
    if(mo)
    {
        S_StartSound(sfx_barexp, mo);
        mo->mom[MX] = FIX2FLT((P_Random() - 128) << 11);
        mo->mom[MY] = FIX2FLT((P_Random() - 128) << 11);
        mo->target = actor;
    }

    actor->reactionTime--;
    if(actor->reactionTime <= 0)
    {
        P_MobjChangeState(actor, actor->info->deathState + 2);
    }
}

/**
 * d64tc
 */
boolean P_CheckAcidRange(mobj_t *actor)
{
    float               dist;

    if(!actor->target)
        return false;

    dist = P_ApproxDistance(actor->target->pos[VX] - actor->pos[VX],
                            actor->target->pos[VY] - actor->pos[VY]);

    dist = P_ApproxDistance(dist, (actor->target->pos[VZ] + actor->target->height /2) -
                                  (actor->pos[VZ] + actor->height /2));

    if(dist >= ACIDRANGE - 14 + actor->target->info->radius)
        return false;

    if(!P_CheckSight(actor, actor->target))
        return false;

    return true;
}

/**
 * d64tc
 */
void C_DECL A_SpitAcid(mobj_t *actor)
{
    if(!actor->target)
        return;

    if(P_CheckAcidRange(actor))
    {
        int                 i;
        angle_t             an;
        mobj_t             *mo;

        A_FaceTarget(actor);
        S_StartSound(sfx_sgtatk, actor);

        for(i = 0; i < 16; ++i)
        {
            mo = P_SpawnMissile(MT_ACIDMISSILE, actor, actor->target);

            if(mo)
            {
                mo->angle = actor->angle;
                an = mo->angle >> ANGLETOFINESHIFT;

                mo->mom[MX] = mo->info->speed *
                    FIX2FLT(finecosine[an] + P_Random() % 3);
                mo->mom[MY] = mo->info->speed *
                    FIX2FLT(finesine[an] + P_Random() % 3);
                mo->mom[MZ] = FIX2FLT(4 + (P_Random() << 10));

                mo->target = actor;
            }
        }

        // kludge >
        actor->info->speed = 7;
        for(i= S_ACID_RUN1; i <= S_ACID_RUN8; ++i)
            states[i].tics = 3;
        // < kludge
    }
    else
    {
        P_MobjChangeState(actor, actor->info->seeState);
    }
}

/**
 * d64tc
 */
void C_DECL A_AcidCharge(mobj_t *actor)
{
    int                 i;

    if(!actor->target)
        return;

    if(!(P_CheckAcidRange(actor)))
    {
        A_FaceTarget(actor);
        A_Chase(actor);

        // kludge >
        for(i = S_ACID_RUN1; i <= S_ACID_RUN8; ++i)
            states[i].tics = 1;
        actor->info->speed = 15;
        // < kludge
    }
    else
    {
        P_MobjChangeState(actor, actor->info->missileState + 1);
    }
}

/**
 * d64tc: Spawns a smoke sprite during the missle attack
 */
void C_DECL A_Rocketpuff(mobj_t *actor)
{
    if(!actor)
        return;

    P_SpawnMobj3fv(MT_ROCKETPUFF, actor->pos);

    if(actor->type == MT_GRENADE)
    {
        actor->reactionTime -= 8;
        if(actor->reactionTime <= 0)
        {
            actor->mom[MX] = actor->mom[MY] = actor->mom[MZ] = 0;

            P_MobjChangeState(actor, actor->info->deathState);
            S_StartSound(actor->info->deathSound, actor);
        }
    }
}

/**
 * d64tc
 */
void C_DECL A_Lasersmoke(mobj_t *mo)
{
    if(!mo)
        return;

    P_SpawnMobj3fv(MT_LASERDUST, mo->pos);
}

/**
 * d64tc
 */
void C_DECL A_RevealFloater(mobj_t *mo)
{
    mobj_t             *mo2;
    float               pos[3];

    memcpy(pos, mo->pos, sizeof(pos));
    pos[VX] += (float) ((P_Random() % 63) - 32);
    pos[VY] += (float) ((P_Random() % 63) - 32);

    mo2 = P_SpawnMobj3fv(MT_LASERDUST, pos);

    mo2->flags &= ~MF_NOGRAVITY;
    mo->reactionTime--;
    // DJS - Is this ever true? We've just spawned it so it will be -1...
    // FIXME?
    if(mo->reactionTime == 0)
    {
        P_MobjChangeState(mo, S_TBALLX1);
    }
}

void C_DECL A_XScream(mobj_t *actor)
{
    S_StartSound(sfx_slop, actor);
}

void C_DECL A_Pain(mobj_t *actor)
{
    if(actor->info->painSound)
        S_StartSound(actor->info->painSound, actor);
}

void C_DECL A_Fall(mobj_t *actor)
{
    // Actor is on ground, it can be walked over.
    actor->flags &= ~MF_SOLID;
}

void C_DECL A_Explode(mobj_t *mo)
{
    // d64tc >
    int                 radius;

    if(mo->type == MT_GRENADE)
        radius = 48;
    else
        radius = 128;
    // < d64tc

    //P_RadiusAttack(mo, mo->target, 128, 127); // d64tc
    P_RadiusAttack(mo, mo->target, radius, radius - 1);
}

/**
 * Possibly trigger special effects if on first boss level
 *
 * kaiser - Removed exit special at end to allow MT_FATSO to properly
 *          work in Map33 for d64tc.
 */
void C_DECL A_BossDeath(mobj_t *mo)
{
    int                 i;
    linedef_t          *dummyLine;

    // Has the boss already been killed?
    if(bossKilled)
        return;

    //if(gameMode == commercial) // d64tc
    {
        /* d64tc
        if(gameMap != 7)
            return;
        if((mo->type != MT_FATSO) && (mo->type != MT_BABY))
            return;
        */

        // d64tc >
        if((gameMap != 1) && (gameMap != 30) && (gameMap != 32) &&
           (gameMap != 33) && (gameMap != 35))
            return;

        if((mo->type != MT_BITCH) && (mo->type != MT_CYBORG) &&
           (mo->type != MT_BARREL) && (mo->type != MT_FATSO))
            return;
        // < d64tc
    }
/* d64tc >
    else
    {
        switch (gameEpisode)
        {
        case 1:
            if(gameMap != 8)
                return;

            // Ultimate DOOM behavioral change
            // This test was added so that the (variable) effects of the
            // 666 special would only take effect when the last Baron
            // died and not ANY monster.
            // Many classic PWADS such as "Doomsday of UAC" (UAC_DEAD.wad)
            // relied on the old behaviour and cannot be completed.

            // Added compatibility option.
            if(!cfg.anyBossDeath)
                if(mo->type != MT_BRUISER)
                    return;
            break;

        case 2:
            if(gameMap != 8)
                return;

            if(mo->type != MT_CYBORG)
                return;
            break;

        case 3:
            if(gameMap != 8)
                return;

            if(mo->type != MT_SPIDER)
                return;

            break;

        case 4:
            switch (gameMap)
            {
            case 6:
                if(mo->type != MT_CYBORG)
                    return;
                break;

            case 8:
                if(mo->type != MT_SPIDER)
                    return;
                break;

            default:
                return;
                break;
            }
            break;

        default:
            if(gameMap != 8)
                return;
            break;
        }

    }
< d64tc */

    // Make sure there is a player alive for victory.
    for(i = 0; i < MAXPLAYERS; ++i)
        if(players[i].plr->inGame && players[i].health > 0)
            break;

    if(i == MAXPLAYERS)
        return; // No one left alive, so do not end game.

    // Scan the remaining thinkers to see if all bosses are dead.
    if(countMobjsOfType(mo->type) > 1) // +1 as this mobj is counted.
    {   // Other boss not dead.
        return;
    }

    // d64tc >
    if(gameMap == 1)
    {
        if(mo->type == MT_BARREL)
        {
            dummyLine = P_AllocDummyLine();
            P_ToXLine(dummyLine)->tag = 666;
            EV_DoDoor(dummyLine, blazeRaise);

            P_FreeDummyLine(dummyLine);
            return;
        }
    }
    else if(gameMap == 30)
    {
        if(mo->type == MT_BITCH)
        {
            G_LeaveLevel(G_GetLevelNumber(gameEpisode, gameMap), 0, false);
        }
    }
    else if(gameMap == 32 || gameMap == 33)
    {
        if(mo->type == MT_CYBORG)
        {
            dummyLine = P_AllocDummyLine();
            P_ToXLine(dummyLine)->tag = 666;
            EV_DoDoor(dummyLine, blazeRaise);

            P_FreeDummyLine(dummyLine);
            return;
        }

        if(mo->type == MT_FATSO)
        {
            G_LeaveLevel(G_GetLevelNumber(gameEpisode, gameMap), 0, false);
        }
    }
    else if(gameMap == 35)
    {
        if(mo->type == MT_CYBORG)
        {
            G_LeaveLevel(G_GetLevelNumber(gameEpisode, gameMap), 0, false);
        }
    }
    // < d64tc

/* d64tc >
    // victory!
    if(gameMode == commercial)
    {
        if(gameMap == 7)
        {
            if(mo->type == MT_FATSO)
            {
                dummyLine = P_AllocDummyLine();
                P_ToXLine(dummyLine)->tag = 666;
                EV_DoFloor(dummyLine, lowerFloorToLowest);
                P_FreeDummyLine(dummyLine);
                return;
            }

            if(mo->type == MT_BABY)
            {
                dummyLine = P_AllocDummyLine();
                P_ToXLine(dummyLine)->tag = 667;
                EV_DoFloor(dummyLine, raiseToTexture);
                P_FreeDummyLine(dummyLine);

                // Only activate once (rare Dead simple bug)
                bossKilled = true;
                return;
            }
        }
    }
    else
    {
        switch (gameEpisode)
        {
        case 1:
            dummyLine = P_AllocDummyLine();
            P_ToXLine(dummyLine)->tag = 666;
            EV_DoFloor(dummyLine, lowerFloorToLowest);
            P_FreeDummyLine(dummyLine);
            bossKilled = true;
            return;
            break;

        case 4:
            switch (gameMap)
            {
            case 6:
                dummyLine = P_AllocDummyLine();
                P_ToXLine(dummyLine)->tag = 666;
                EV_DoDoor(dummyLine, blazeOpen);
                P_FreeDummyLine(dummyLine);
                bossKilled = true;
                return;
                break;

            case 8:
                dummyLine = P_AllocDummyLine();
                P_ToXLine(dummyLine)->tag = 666;
                EV_DoFloor(dummyLine, lowerFloorToLowest);
                P_FreeDummyLine(dummyLine);
                bossKilled = true;
                return;
                break;
            }
        }
    }

    G_LeaveLevel(G_GetLevelNumber(gameEpisode, gameMap), 0, false);
*/
}

void C_DECL A_Hoof(mobj_t *mo)
{
    /**
     * \kludge Only play very loud sounds in map 8.
     * \todo: Implement a MAPINFO option for this.
     */
    S_StartSound(sfx_hoof |
                 (gameMode != commercial &&
                  gameMap == 8 ? DDSF_NO_ATTENUATION : 0), mo);
    A_Chase(mo);
}

void C_DECL A_Metal(mobj_t *mo)
{
    /**
     * \kludge Only play very loud sounds in map 8.
     * \todo: Implement a MAPINFO option for this.
     */
    S_StartSound(sfx_metal |
                 (gameMode != commercial &&
                  gameMap == 8 ? DDSF_NO_ATTENUATION : 0), mo);
    A_Chase(mo);
}

void C_DECL A_BabyMetal(mobj_t *mo)
{
    S_StartSound(sfx_bspwlk, mo);
    A_Chase(mo);
}

void C_DECL A_BrainAwake(mobj_t *mo)
{
    S_StartSound(sfx_bossit, NULL);
}

void C_DECL A_BrainPain(mobj_t *mo)
{
    S_StartSound(sfx_bospn, NULL);
}

void C_DECL A_BrainScream(mobj_t *mo)
{
    float               pos[3];
    mobj_t             *th;

    for(pos[VX] = mo->pos[VX] - 196; pos[VX] < mo->pos[VX] + 320;
        pos[VX] += 8)
    {
        pos[VY] = mo->pos[VY] - 320;
        pos[VZ] = 128 + (P_Random() * 2);

        th = P_SpawnMobj3fv(MT_ROCKET, pos);
        th->mom[MZ] = FIX2FLT(P_Random() * 512);

        P_MobjChangeState(th, S_BRAINEXPLODE1);

        th->tics -= P_Random() & 7;
        if(th->tics < 1)
            th->tics = 1;
    }

    S_StartSound(sfx_bosdth, NULL);
}

void C_DECL A_BrainExplode(mobj_t *mo)
{
    float               pos[3];
    mobj_t             *th;

    pos[VX] = mo->pos[VX] + ((P_Random() - P_Random()) * 2048);
    pos[VY] = mo->pos[VY];
    pos[VZ] = 128 + (P_Random() * 2);

    th = P_SpawnMobj3fv(MT_ROCKET, pos);
    th->mom[MZ] = FIX2FLT(P_Random() * 512);

    P_MobjChangeState(th, S_BRAINEXPLODE1);

    th->tics -= P_Random() & 7;
    if(th->tics < 1)
        th->tics = 1;
}

void C_DECL A_BrainDie(mobj_t *mo)
{
    G_LeaveLevel(G_GetLevelNumber(gameEpisode, gameMap), 0, false);
}

/**
 * Unused?
 */
void C_DECL A_BrainSpit(mobj_t *mo)
{
/*
    mobj_t             *targ;
    mobj_t             *newmobj;

    if(!numBrainTargets)
        return; // Ignore if no targets.

    brain.easy ^= 1;
    if(gameSkill <= SM_EASY && (!brain.easy))
        return;

    // Shoot a cube at current target.
    targ = brainTargets[brain.targetOn++];
    brain.targetOn %= numBrainTargets;

    // Spawn brain missile.
    newmobj = P_SpawnMissile(MT_SPAWNSHOT, mo, targ);
    if(newmobj)
    {
        newmobj->target = targ;
        newmobj->reactionTime =
            ((targ->pos[VY] - mo->pos[VY]) / newmobj->mom[MY]) / newmobj->state->tics;
    }

    S_StartSound(sfx_bospit, NULL);
*/
}

/**
 * Travelling cube sound.
 */
void C_DECL A_SpawnSound(mobj_t *mo)
{
    S_StartSound(sfx_boscub, mo);
    A_SpawnFly(mo);
}

void C_DECL A_SpawnFly(mobj_t *mo)
{
    int                 r;
    mobj_t             *newmobj;
    mobj_t             *targ;
    mobjtype_t          type;

    if(--mo->reactionTime)
        return; // Still flying.

    targ = mo->target;

    // Randomly select monster to spawn.
    r = P_Random();

    // Probability distribution (kind of :), decreasing likelihood.
    if(r < 50)
        type = MT_TROOP;
    else if(r < 90)
        type = MT_SERGEANT;
    else if(r < 120)
        type = MT_SHADOWS;
    else if(r < 130)
        type = MT_PAIN;
    else if(r < 160)
        type = MT_HEAD;
    else if(r < 172)
        type = MT_NIGHTMARECACO; // d64tc was "MT_UNDEAD"
    else if(r < 192)
        type = MT_BABY;
    else if(r < 222)
        type = MT_FATSO;
    else if(r < 246)
        type = MT_KNIGHT;
    else
        type = MT_BRUISER;

    newmobj = P_SpawnMobj3fv(type, targ->pos);

    if(lookForPlayers(newmobj, true))
        P_MobjChangeState(newmobj, newmobj->info->seeState);

    // Telefrag anything in this spot.
    P_TeleportMove(newmobj, newmobj->pos[VX], newmobj->pos[VY], false);

    // Remove self (i.e., cube).
    P_MobjRemove(mo);
}

void C_DECL A_PlayerScream(mobj_t *mo)
{
    int                 sound = sfx_pldeth; // Default death sound.

    if((gameMode == commercial) && (mo->health < -50))
    {
        // If the player dies less with less than -50% without gibbing.
        sound = sfx_pdiehi;
    }

    S_StartSound(sound, mo);
}
