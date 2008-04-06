/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2008 Daniel Swanson <danij@dengine.net>
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
 * rend_fakeradio.h: Faked Radiosity Lighting
 */

#ifndef __DOOMSDAY_RENDER_FAKERADIO_H__
#define __DOOMSDAY_RENDER_FAKERADIO_H__

void            Rend_RadioRegister(void);
void            Rend_RadioInitForFrame(void);
void            Rend_RadioUpdateLinedef(linedef_t *line, boolean backSide);
void            Rend_RadioInitForSubsector(subsector_t *sector);
void            Rend_RadioSegSection(const rvertex_t *origVertices,
                                     const walldiv_t* origDivs,
                                     rendpolytype_t origType,
                                     linedef_t* line, byte side,
                                     float xOffset, float segLength);
void            Rend_RadioSubsectorEdges(subsector_t *subsector);

#endif
