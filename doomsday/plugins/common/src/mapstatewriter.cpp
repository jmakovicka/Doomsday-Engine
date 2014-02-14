/** @file mapstatewriter.cpp  Saved map state writer.
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
#include "mapstatewriter.h"

#include "dmu_lib.h"
#include "p_saveg.h"     // SV_WriteSector, SV_WriteLine
#include "p_saveio.h"
#include "polyobjs.h"
#include "thinkerinfo.h"
#include <de/String>

namespace internal
{
    static bool useMaterialArchiveSegments() {
#if __JHEXEN__
        return true;
#else
        return false;
#endif
    }
}

using namespace internal;

DENG2_PIMPL(MapStateWriter)
{
    ThingArchive *thingArchive;
    MaterialArchive *materialArchive;
    Writer *writer;

    Instance(Public *i)
        : Base(i)
        , thingArchive(0)
        , materialArchive(0)
        , writer(0)
    {}

    void beginSegment(int segId)
    {
#if __JHEXEN__
        Writer_WriteInt32(writer, segId);
#else
        DENG_UNUSED(segId);
#endif
    }

    void endSegment()
    {
        beginSegment(ASEG_END);
    }

    void writeMapHeader()
    {
#if __JHEXEN__
        // Maps have their own version number.
        Writer_WriteByte(writer, MY_SAVE_VERSION);

        // Write the map timer
        Writer_WriteInt32(writer, mapTime);
#endif
    }

    void writeMaterialArchive()
    {
        MaterialArchive_Write(materialArchive, writer);
    }

    void writeElements()
    {
        beginSegment(ASEG_MAP_ELEMENTS);

        for(int i = 0; i < numsectors; ++i)
        {
            SV_WriteSector((Sector *)P_ToPtr(DMU_SECTOR, i), thisPublic);
        }

        for(int i = 0; i < numlines; ++i)
        {
            SV_WriteLine((Line *)P_ToPtr(DMU_LINE, i), thisPublic);
        }

        // endSegment();
    }

    void writePolyobjs()
    {
#if __JHEXEN__
        beginSegment(ASEG_POLYOBJS);

        Writer_WriteInt32(writer, numpolyobjs);
        for(int i = 0; i < numpolyobjs; ++i)
        {
            Polyobj *po = Polyobj_ById(i);
            DENG_ASSERT(po != 0);
            po->write(thisPublic);
        }

        // endSegment();
#endif
    }

    struct writethinkerworker_params_t
    {
        MapStateWriter *msw;
        bool excludePlayers;
    };

    /**
     * Serializes the specified thinker and writes it to save state.
     */
    static int writeThinkerWorker(thinker_t *th, void *context)
    {
        writethinkerworker_params_t &p = *static_cast<writethinkerworker_params_t *>(context);

        // We are only concerned with thinkers we have save info for.
        ThinkerClassInfo *thInfo = SV_ThinkerInfo(*th);
        if(!thInfo) return false;

        // Are we excluding players?
        if(p.excludePlayers)
        {
            if(th->function == (thinkfunc_t) P_MobjThinker && ((mobj_t *) th)->player)
                return false; // Continue iteration.
        }

        // Only the server saves this class of thinker?
        if((thInfo->flags & TSF_SERVERONLY) && IS_CLIENT)
            return false;

        // Write the header block for this thinker.
        Writer_WriteByte(p.msw->writer(), thInfo->thinkclass); // Thinker type byte.
        Writer_WriteByte(p.msw->writer(), th->inStasis? 1 : 0); // In stasis?

        // Write the thinker data.
        thInfo->writeFunc(th, p.msw);

        return false; // Continue iteration.
    }

    /**
     * Serializes thinkers for both client and server.
     *
     * @note Clients do not save data for all thinkers. In some cases the server will send it
     * anyway (so saving it would just bloat client save states).
     *
     * @note Some thinker classes are NEVER saved by clients.
     */
    void writeThinkers()
    {
        beginSegment(ASEG_THINKERS);

#if __JHEXEN__
        Writer_WriteInt32(writer, thingArchive->size()); // number of mobjs.
#endif

        // Serialize qualifying thinkers.
        writethinkerworker_params_t parm; de::zap(parm);
        parm.msw            = thisPublic;
        parm.excludePlayers = thingArchive->excludePlayers();
        Thinker_Iterate(0/*all thinkers*/, writeThinkerWorker, &parm);

        // Mark the end of the thinkers.
        // endSegment();
        Writer_WriteByte(writer, TC_END);
    }

    void writeACScriptData()
    {
#if __JHEXEN__
        beginSegment(ASEG_SCRIPTS);
        Game_ACScriptInterpreter().writeMapScriptData(thisPublic);
        // endSegment();
#endif
    }

    void writeSoundSequences()
    {
#if __JHEXEN__
        beginSegment(ASEG_SOUNDS);
        SN_WriteSequences(writer);
        // endSegment();
#endif
    }

    void writeMisc()
    {
#if __JHEXEN__
        beginSegment(ASEG_MISC);
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            Writer_WriteInt32(writer, localQuakeHappening[i]);
        }
#endif
#if __JDOOM__
        DENG_ASSERT(theBossBrain != 0);
        theBossBrain->write(thisPublic);
#endif
    }

    void writeSoundTargets()
    {
#if !__JHEXEN__
        // Not for us?
        if(!IS_SERVER) return;

        // Write the total number.
        int count = 0;
        for(int i = 0; i < numsectors; ++i)
        {
            xsector_t *xsec = P_ToXSector((Sector *)P_ToPtr(DMU_SECTOR, i));
            if(xsec->soundTarget)
            {
                count += 1;
            }
        }

        // beginSegment();
        Writer_WriteInt32(writer, count);

        // Write the mobj references using the mobj archive.
        for(int i = 0; i < numsectors; ++i)
        {
            xsector_t *xsec = P_ToXSector((Sector *)P_ToPtr(DMU_SECTOR, i));
            if(xsec->soundTarget)
            {
                Writer_WriteInt32(writer, i);
                Writer_WriteInt16(writer, thingArchive->serialIdFor(xsec->soundTarget));
            }
        }
        // endSegment();
#endif
    }
};

MapStateWriter::MapStateWriter(ThingArchive &thingArchive)
    : d(new Instance(this))
{
    d->thingArchive = &thingArchive;
}

void MapStateWriter::write(Writer *writer)
{
    DENG_ASSERT(writer != 0);
    d->writer = writer;

    // Prepare and populate the material archive.
    d->materialArchive = MaterialArchive_New(useMaterialArchiveSegments());

    // Serialize the map.
    d->beginSegment(ASEG_MAP_HEADER2);
    {
        d->writeMapHeader();
        d->writeMaterialArchive();

        d->writeElements();
        d->writePolyobjs();
        d->writeThinkers();
        d->writeACScriptData();
        d->writeSoundSequences();
        d->writeMisc();
        d->writeSoundTargets();
    }
    d->endSegment();

    // Cleanup.
    MaterialArchive_Delete(d->materialArchive); d->materialArchive = 0;
}

ThingArchive::SerialId MapStateWriter::serialIdFor(mobj_t *mobj)
{
    DENG_ASSERT(d->thingArchive != 0);
    return d->thingArchive->serialIdFor(mobj);
}

materialarchive_serialid_t MapStateWriter::serialIdFor(Material *material)
{
    DENG_ASSERT(d->materialArchive != 0);
    return MaterialArchive_FindUniqueSerialId(d->materialArchive, material);
}

Writer *MapStateWriter::writer()
{
    DENG_ASSERT(d->writer != 0);
    return d->writer;
}
