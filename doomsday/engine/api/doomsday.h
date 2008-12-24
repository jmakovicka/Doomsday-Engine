/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * doomsday.h: Doomsday Engine API (Routines exported from Doomsday.exe).
 *
 * Games and plugins need to include this to gain access to the engine's
 * features.
 */

#ifndef __DOOMSDAY_EXPORTS_H__
#define __DOOMSDAY_EXPORTS_H__

// The calling convention.
#if defined(WIN32)
#   define _DECALL  __cdecl
#elif defined(UNIX)
#   define _DECALL
#endif

#ifdef __cplusplus
extern          "C" {
#endif

/**
 * Public definitions of the internal map data pointers.  These can be
 * accessed externally, but only as identifiers to data instances.
 * For example, a game could use sector_t to identify to sector to
 * change with the Map Update API.
 *
 * Define __INTERNAL_MAP_DATA_ACCESS__ if access to the internal map data
 * structures is needed.
 */
#ifndef __INTERNAL_MAP_DATA_ACCESS__
    typedef struct node_s { int type; } node_t;
    typedef struct vertex_s {int type; } vertex_t;
    typedef struct linedef_s { int type; } linedef_t;
    typedef struct sidedef_s { int type; } sidedef_t;
    typedef struct seg_s { int type; } seg_t;
    typedef struct subsector_s { int type; } subsector_t;
    typedef struct sector_s { int type; } sector_t;
    typedef struct plane_s { int type; } plane_t;
#endif

#include "dd_share.h"
#include "dd_plugin.h"

    // Base.
    void            DD_AddIWAD(const char* path);
    void            DD_AddStartupWAD(const char* file);
    void            DD_SetConfigFile(filename_t filename);
    void            DD_SetDefsFile(filename_t file);
    int _DECALL     DD_GetInteger(int ddvalue);
    void            DD_SetInteger(int ddvalue, int parm);
    void            DD_SetVariable(int ddvalue, void* ptr);
    void*           DD_GetVariable(int ddvalue);
    ddplayer_t*     DD_GetPlayer(int number);

    // Base: Definitions.
    int             Def_Get(int type, char* id, void* out);
    int             Def_Set(int type, int index, int value, void* ptr);
    int             Def_EvalFlags(char* flags);

    // Base: Input.
    void            DD_ClearKeyRepeaters(void);
    int             DD_GetKeyCode(const char* name);

    // Base: WAD.
    lumpnum_t       W_CheckNumForName(const char* name);
    lumpnum_t       W_GetNumForName(const char* name);
    size_t          W_LumpLength(lumpnum_t lump);
    const char*     W_LumpName(lumpnum_t lump);
    void            W_ReadLump(lumpnum_t lump, void* dest);
    void            W_ReadLumpSection(lumpnum_t lump, void* dest,
                                      size_t startOffset, size_t length);
    void*           W_CacheLumpNum(lumpnum_t lump, int tag);
    void*           W_CacheLumpName(char* name, int tag);
    void            W_ChangeCacheTag(lumpnum_t lump, int tag);
    const char*     W_LumpSourceFile(lumpnum_t lump);
    uint            W_CRCNumber(void);
    boolean         W_IsFromIWAD(lumpnum_t lump);
    lumpnum_t       W_OpenAuxiliary(const char *fileName);

    // Base: Zone.
    void* _DECALL   Z_Malloc(size_t size, int tag, void* ptr);
    void* _DECALL   Z_Calloc(size_t size, int tag, void* user);
    void*           Z_Realloc(void* ptr, size_t n, int mallocTag);
    void*           Z_Recalloc(void* ptr, size_t n, int callocTag);
    void _DECALL    Z_Free(void* ptr);
    void            Z_FreeTags(int lowTag, int highTag);
    void            Z_ChangeTag2(void* ptr, int tag);
    void            Z_CheckHeap(void);

    // Console.
    int             Con_Busy(int flags, const char* taskName,
                             int (*workerFunc)(void*), void* workerData);
    void            Con_BusyWorkerEnd(void);
    boolean         Con_IsBusy(void);
    void            Con_Open(int yes);
    void            Con_SetFont(ddfont_t* cfont);
    void            Con_AddCommand(ccmd_t* cmd);
    void            Con_AddVariable(cvar_t* var);
    void            Con_AddCommandList(ccmd_t* cmdlist);
    void            Con_AddVariableList(cvar_t* varlist);
    cvar_t*         Con_GetVariable(char* name);
    byte            Con_GetByte(char* name);
    int             Con_GetInteger(char* name);
    float           Con_GetFloat(char* name);
    char*           Con_GetString(char* name);
    void            Con_SetInteger(char* name, int value, byte override);
    void            Con_SetFloat(char* name, float value, byte override);
    void            Con_SetString(char* name, char* text, byte override);
    void            Con_Printf(char* format, ...) PRINTF_F(1,2);
    void            Con_FPrintf(int flags, char* format, ...) PRINTF_F(2,3);
    int             DD_Execute(int silent, const char* command);
    int             DD_Executef(int silent, const char* command, ...);
    void            Con_Message(char* message, ...) PRINTF_F(1,2);
    void            Con_Error(char* error, ...) PRINTF_F(1,2);

    // Console: Bindings.
    void            B_FormEventString(char* buff, evtype_t type, evstate_t state,
                                      int data1);
    int             B_BindingsForCommand(const char* cmd, char* buf, int bufSize);
    int             B_BindingsForControl(int localPlayer, const char* controlName, int inverse,
                                         char* buf, int bufSize);

    // System.
    void            Sys_TicksPerSecond(float num);
    int             Sys_GetTime(void);
    double          Sys_GetSeconds(void);
    uint            Sys_GetRealTime(void);
    void            Sys_Sleep(int millisecs);
    int             Sys_CriticalMessage(char* msg);
    void            Sys_Quit(void);

    // System: Files.
    int             F_Access(const char* path);
    unsigned int    F_LastModified(const char* fileName);

    // Map building interface.
    boolean         MPE_Begin(const char* name);
    boolean         MPE_End(void);

    uint            MPE_VertexCreate(float x, float y);
    boolean         MPE_VertexCreatev(size_t num, float* values, uint* indices);
    uint            MPE_SidedefCreate(uint sector, short flags,
                                      materialnum_t topMaterial,
                                      float topOffsetX, float topOffsetY, float topRed,
                                      float topGreen, float topBlue,
                                      materialnum_t middleMaterial,
                                      float middleOffsetX, float middleOffsetY,
                                      float middleRed, float middleGreen,
                                      float middleBlue, float middleAlpha,
                                      materialnum_t bottomMaterial,
                                      float bottomOffsetX, float bottomOffsetY,
                                      float bottomRed, float bottomGreen,
                                      float bottomBlue);
    uint            MPE_LinedefCreate(uint v1, uint v2, uint frontSide,
                                      uint backSide, int flags);
    uint            MPE_SectorCreate(float lightlevel, float red, float green, float blue);
    uint            MPE_PlaneCreate(uint sector, float height,
                                    materialnum_t num,
                                    float matOffsetX, float matOffsetY,
                                    float r, float g, float b, float a,
                                    float normalX, float normalY, float normalZ);
    uint            MPE_PolyobjCreate(uint* lines, uint linecount,
                                      int tag, int sequenceType, float anchorX, float anchorY);
    boolean         MPE_GameObjProperty(const char* objName, uint idx,
                                        const char* propName, valuetype_t type,
                                        void* data);

    // Custom map object data types.
    boolean         P_RegisterMapObj(int identifier, const char* name);
    boolean         P_RegisterMapObjProperty(int identifier, int propIdentifier,
                                             const char* propName, valuetype_t type);

    // Network.
    void            Net_SendPacket(int to_player, int type, void* data,
                                   int length);
    int             Net_GetTicCmd(void* command, int player);
    const char*     Net_GetPlayerName(int player);
    ident_t         Net_GetPlayerID(int player);

    // Play.
    float           P_AccurateDistance(float dx, float dy);
    float           P_ApproxDistance(float dx, float dy);
    float           P_ApproxDistance3(float dx, float dy, float dz);
    int             P_PointOnLinedefSide(float x, float y,
                                      struct linedef_s* line);
    int             P_BoxOnLineSide(float* tmbox, struct linedef_s* ld);
    void            P_MakeDivline(struct linedef_s* li, divline_t* dl);
    int             P_PointOnDivlineSide(float x, float y,
                                         divline_t* line);
    float           P_InterceptVector(divline_t* v2, divline_t* v1);
    void            P_LineOpening(struct linedef_s* linedef);

    // Object in bounding box iterators.
    boolean         P_MobjsBoxIterator(const float box[4],
                                       boolean (*func) (struct mobj_s*, void*),
                                       void* data);
    boolean         P_LinesBoxIterator(const float box[4],
                                       boolean (*func) (struct linedef_s*, void*),
                                       void* data);
    boolean         P_AllLinesBoxIterator(const float box[4],
                                          boolean (*func) (struct linedef_s*, void*),
                                          void* data);
    boolean         P_SubsectorsBoxIterator(const float box[4], sector_t* sector,
                                           boolean (*func) (subsector_t*, void*),
                                           void* data);
    boolean         P_PolyobjsBoxIterator(const float box[4],
                                          boolean (*func) (struct polyobj_s*, void*),
                                          void* data);

    // Object type touching mobjs iterators.
    boolean         P_LineMobjsIterator(struct linedef_s* line,
                                        boolean (*func) (struct mobj_s*,
                                                         void *), void* data);
    boolean         P_SectorTouchingMobjsIterator
                        (sector_t* sector, boolean (*func) (struct mobj_s*, void*),
                         void* data);

    boolean         P_PathTraverse(float x1, float y1, float x2, float y2,
                                   int flags,
                                   boolean (*trav) (intercept_t*));
    boolean         P_CheckSight(struct mobj_s* t1, struct mobj_s* t2);

    // Play: Controls.
    void            P_NewPlayerControl(int id, controltype_t type, const char* name, const char* bindContext);
    void            P_GetControlState(int playerNum, int control, float* pos, float* relativeOffset);
    int             P_GetImpulseControlState(int playerNum, int control);

    // Play: Setup.
    boolean         P_LoadMap(const char* mapID);

    // Play: Map Data Updates and Information Access.
    unsigned int    P_ToIndex(const void* ptr);
    void*           P_ToPtr(int type, uint index);
    int             P_Callback(int type, uint index, void* context,
                               int (*callback)(void* p, void* ctx));
    int             P_Callbackp(int type, void* ptr, void* context,
                                int (*callback)(void* p, void* ctx));
    int             P_Iteratep(void *ptr, uint prop, void* context,
                               int (*callback) (void* p, void* ctx));

    /* dummy functions */
    void*           P_AllocDummy(int type, void* extraData);
    void            P_FreeDummy(void* dummy);
    int             P_DummyType(void* dummy);
    boolean         P_IsDummy(void* dummy);
    void*           P_DummyExtraData(void* dummy);

    /* index-based write functions */
    void            P_SetBool(int type, uint index, uint prop, boolean param);
    void            P_SetByte(int type, uint index, uint prop, byte param);
    void            P_SetInt(int type, uint index, uint prop, int param);
    void            P_SetFixed(int type, uint index, uint prop, fixed_t param);
    void            P_SetAngle(int type, uint index, uint prop, angle_t param);
    void            P_SetFloat(int type, uint index, uint prop, float param);
    void            P_SetPtr(int type, uint index, uint prop, void* param);

    void            P_SetBoolv(int type, uint index, uint prop, boolean* params);
    void            P_SetBytev(int type, uint index, uint prop, byte* params);
    void            P_SetIntv(int type, uint index, uint prop, int* params);
    void            P_SetFixedv(int type, uint index, uint prop, fixed_t* params);
    void            P_SetAnglev(int type, uint index, uint prop, angle_t* params);
    void            P_SetFloatv(int type, uint index, uint prop, float* params);
    void            P_SetPtrv(int type, uint index, uint prop, void* params);

    /* pointer-based write functions */
    void            P_SetBoolp(void* ptr, uint prop, boolean param);
    void            P_SetBytep(void* ptr, uint prop, byte param);
    void            P_SetIntp(void* ptr, uint prop, int param);
    void            P_SetFixedp(void* ptr, uint prop, fixed_t param);
    void            P_SetAnglep(void* ptr, uint prop, angle_t param);
    void            P_SetFloatp(void* ptr, uint prop, float param);
    void            P_SetPtrp(void* ptr, uint prop, void* param);

    void            P_SetBoolpv(void* ptr, uint prop, boolean* params);
    void            P_SetBytepv(void* ptr, uint prop, byte* params);
    void            P_SetIntpv(void* ptr, uint prop, int* params);
    void            P_SetFixedpv(void* ptr, uint prop, fixed_t* params);
    void            P_SetAnglepv(void* ptr, uint prop, angle_t* params);
    void            P_SetFloatpv(void* ptr, uint prop, float* params);
    void            P_SetPtrpv(void* ptr, uint prop, void* params);

    /* index-based read functions */
    boolean         P_GetBool(int type, uint index, uint prop);
    byte            P_GetByte(int type, uint index, uint prop);
    int             P_GetInt(int type, uint index, uint prop);
    fixed_t         P_GetFixed(int type, uint index, uint prop);
    angle_t         P_GetAngle(int type, uint index, uint prop);
    float           P_GetFloat(int type, uint index, uint prop);
    void*           P_GetPtr(int type, uint index, uint prop);

    void            P_GetBoolv(int type, uint index, uint prop, boolean* params);
    void            P_GetBytev(int type, uint index, uint prop, byte* params);
    void            P_GetIntv(int type, uint index, uint prop, int* params);
    void            P_GetFixedv(int type, uint index, uint prop, fixed_t* params);
    void            P_GetAnglev(int type, uint index, uint prop, angle_t* params);
    void            P_GetFloatv(int type, uint index, uint prop, float* params);
    void            P_GetPtrv(int type, uint index, uint prop, void* params);

    /* pointer-based read functions */
    boolean         P_GetBoolp(void* ptr, uint prop);
    byte            P_GetBytep(void* ptr, uint prop);
    int             P_GetIntp(void* ptr, uint prop);
    fixed_t         P_GetFixedp(void* ptr, uint prop);
    angle_t         P_GetAnglep(void* ptr, uint prop);
    float           P_GetFloatp(void* ptr, uint prop);
    void*           P_GetPtrp(void* ptr, uint prop);

    void            P_GetBoolpv(void* ptr, uint prop, boolean* params);
    void            P_GetBytepv(void* ptr, uint prop, byte* params);
    void            P_GetIntpv(void* ptr, uint prop, int* params);
    void            P_GetFixedpv(void* ptr, uint prop, fixed_t* params);
    void            P_GetAnglepv(void* ptr, uint prop, angle_t* params);
    void            P_GetFloatpv(void* ptr, uint prop, float* params);
    void            P_GetPtrpv(void* ptr, uint prop, void* params);

    uint            P_CountGameMapObjs(int identifier);
    byte            P_GetGMOByte(int identifier, uint elmIdx, int propIdentifier);
    short           P_GetGMOShort(int identifier, uint elmIdx, int propIdentifier);
    int             P_GetGMOInt(int identifier, uint elmIdx, int propIdentifier);
    fixed_t         P_GetGMOFixed(int identifier, uint elmIdx, int propIdentifier);
    angle_t         P_GetGMOAngle(int identifier, uint elmIdx, int propIdentifier);
    float           P_GetGMOFloat(int identifier, uint elmIdx, int propIdentifier);

    // Play: Misc.
    void            P_MergeCommand(ticcmd_t* dest, ticcmd_t* src); // temporary.
    void            P_SpawnDamageParticleGen(struct mobj_s* mo,
                                             struct mobj_s* inflictor,
                                             int amount);

    // Play: Mobjs.
    struct mobj_s*  P_MobjCreate(think_t function, float x, float y, float z,
                                 angle_t angle, float radius, float height, int ddflags);
    void            P_MobjDestroy(struct mobj_s* mo);
    void            P_MobjSetState(struct mobj_s* mo, int statenum);
    void            P_MobjLink(struct mobj_s* mo, byte flags);
    int             P_MobjUnlink(struct mobj_s* mo);

    // Mobj linked object iterators.
    boolean         P_MobjLinesIterator(struct mobj_s* mo,
                                        boolean (*func) (struct linedef_s*,
                                                          void*), void*);
    boolean         P_MobjSectorsIterator(struct mobj_s* mo,
                                          boolean (*func) (sector_t*, void*),
                                          void* data);

    // Play: Polyobjs.
    boolean         P_PolyobjMove(struct polyobj_s* po, float x, float y);
    boolean         P_PolyobjRotate(struct polyobj_s* po, angle_t angle);
    void            P_PolyobjLink(struct polyobj_s* po);
    void            P_PolyobjUnLink(struct polyobj_s* po);

    struct polyobj_s* P_GetPolyobj(uint num);
    void            P_SetPolyobjCallback(void (*func)(struct mobj_s*, void*, void*));

    // Play: Thinkers.
    void            P_ThinkerAdd(thinker_t* th);
    void            P_ThinkerRemove(thinker_t* th);
    void            P_ThinkerSetStasis(thinker_t* th, boolean on);

    boolean         P_IterateThinkers(think_t type, boolean (*func) (thinker_t *th, void*), void* data);
    void            P_RunThinkers(void);
    void            P_InitThinkers(void);

    // Refresh.
    int             DD_GetFrameRate(void);
    void            R_SetDataPath(const char* path);
    void            R_SetupMap(int mode, int flags);
    void            R_PrecacheMap(void);
    void            R_PrecachePatch(lumpnum_t lump);
    void            R_PrecacheSkinsForState(int stateIndex);
    void            R_RenderPlayerView(int num);
    void            R_SetViewWindow(int x, int y, int w, int h);
    int             R_GetViewPort(int player, int* x, int* y, int* w, int* h);
    void            R_SetBorderGfx(char* lumps[9]);
    void            R_GetSpriteInfo(int sprite, int frame,
                                    spriteinfo_t* sprinfo);
    void            R_GetPatchInfo(lumpnum_t lump, patchinfo_t* info);

    materialnum_t   R_MaterialCheckNumForName(const char* name, materialgroup_t group);
    materialnum_t   R_MaterialNumForName(const char* name, materialgroup_t group);
    materialnum_t   R_MaterialCheckNumForIndex(uint idx, materialgroup_t group);
    materialnum_t   R_MaterialNumForIndex(uint idx, materialgroup_t group);
    const char*     R_MaterialNameForNum(materialnum_t num);
    boolean         R_MaterialIsCustom(materialnum_t num);

    boolean         R_MaterialGetInfo(materialnum_t num, materialinfo_t* info);
    void            R_MaterialSetSkyMask(materialnum_t num, boolean yes);

    void            R_MaterialPrecache(materialnum_t num);

    int             R_CreateAnimGroup(int flags);
    void            R_AddToAnimGroup(int groupNum, materialnum_t num, int tics, int randomTics);

    angle_t         R_PointToAngle2(float x1, float y1, float x2,
                                    float y2);
    struct subsector_s* R_PointInSubsector(float x, float y);

    // DGL.
    void            DGL_DeleteTextures(int num, const DGLuint* names);
    int             DGL_Enable(int cap);
    void            DGL_Disable(int cap);
    void            DGL_EnableTexUnit(byte id);
    void            DGL_DisableTexUnit(byte id);
    boolean         DGL_GetIntegerv(int name, int* v);
    int             DGL_GetInteger(int name);
    boolean         DGL_SetInteger(int name, int value);
    float           DGL_GetFloat(int name);
    boolean         DGL_SetFloat(int name, float value);
    void            DGL_BlendFunc(int param1, int param2);
    void            DGL_BlendOp(int op);
    void            DGL_MatrixMode(int mode);
    void            DGL_PushMatrix(void);
    void            DGL_PopMatrix(void);
    void            DGL_LoadIdentity(void);
    void            DGL_Ortho(float left, float top, float right, float bottom, float znear,
                              float zfar);
    void            DGL_Scissor(int x, int y, int width, int height);
    void            DGL_Clear(int bufferbits);
    int             DGL_Bind(DGLuint texture);
    void            DGL_Begin(glprimtype_t type);
    void            DGL_End(void);
    boolean         DGL_NewList(DGLuint list, int mode);
    DGLuint         DGL_EndList(void);
    void            DGL_CallList(DGLuint list);
    void            DGL_DeleteLists(DGLuint list, int range);
    void            DGL_Translatef(float x, float y, float z);
    void            DGL_Rotatef(float angle, float x, float y, float z);
    void            DGL_Scalef(float x, float y, float z);
    void            DGL_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b);
    void            DGL_Color3ubv(const DGLubyte *data);
    void            DGL_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a);
    void            DGL_Color4ubv(const DGLubyte *data);
    void            DGL_Color3f(float r, float g, float b);
    void            DGL_Color3fv(const float* data);
    void            DGL_Color4f(float r, float g, float b, float a);
    void            DGL_Color4fv(const float* data);
    void            DGL_TexCoord2f(float s, float t);
    void            DGL_TexCoord2fv(const float* data);
    void            DGL_MultiTexCoord2f(byte target, float s, float t);
    void            DGL_MultiTexCoord2fv(byte target, float* data);
    void            DGL_Vertex2f(float x, float y);
    void            DGL_Vertex2fv(const float* data);
    void            DGL_Vertex3f(float x, float y, float z);
    void            DGL_Vertex3fv(const float* data);
    void            DGL_Vertices2ftv(int num, const gl_ft2vertex_t* data);
    void            DGL_Vertices3ftv(int num, const gl_ft3vertex_t* data);
    void            DGL_Vertices3fctv(int num, const gl_fct3vertex_t* data);

    // Renderer.
    void            Rend_Reset(void);
    void            Rend_SkyParams(int layer, int param, void* data);

    // Graphics.
    void            GL_UseFog(int yes);
    byte*           GL_GrabScreen(void);
    unsigned int    GL_NewTextureWithParams2(int format, int width, int height, void* pixels,
                                             int flags, int minFilter, int magFilter, int anisoFilter,
                                             int wrapS, int wrapT);
    void            GL_BlendMode(blendmode_t mode);
    void            GL_SetFilter(int filter_rgba);
    void            GL_PalIdxToRGB(int idx, float* rgb);
    void            GL_SetNoTexture(void);
    void            GL_SetPatch(lumpnum_t lump, int wrapS, int wrapT);
    void            GL_SetPSprite(materialnum_t mat);
    void            GL_SetTranslatedSprite(materialnum_t mat, int tmap, int tclass);
    void            GL_SetMaterial(materialnum_t mat);
    unsigned int    GL_SetRawImage(lumpnum_t lump, boolean part2, int wrapS, int wrapT);
    unsigned int    GL_LoadGraphics(const char* name, int mode);

    // Graphics: 2D drawing.
    void            GL_DrawPatch(int x, int y, lumpnum_t lump);
    void            GL_DrawPatch_CS(int x, int y, lumpnum_t lump);
    void            GL_DrawPatchLitAlpha(int x, int y, float light,
                                         float alpha, lumpnum_t lump);
    void            GL_DrawFuzzPatch(int x, int y, lumpnum_t lump);
    void            GL_DrawAltFuzzPatch(int x, int y, lumpnum_t lump);
    void            GL_DrawShadowedPatch(int x, int y, lumpnum_t lump);
    void            GL_DrawRawScreen(lumpnum_t lump, float offx, float offy);
    void            GL_DrawRawScreen_CS(lumpnum_t lump, float offx, float offy,
                                        float scalex, float scaley);
    void            GL_DrawLine(float x1, float y1, float x2, float y2,
                                float r, float g, float b, float a);
    void            GL_DrawRect(float x, float y, float w, float h, float r,
                                float g, float b, float a);
    void            GL_DrawRectTiled(int x, int y, int w, int h, int tw,
                                     int th);
    void            GL_DrawCutRectTiled(int x, int y, int w, int h, int tw,
                                        int th, int txoff, int tyoff, int cx,
                                        int cy, int cw, int ch);

    // Graphics: PCX.
    int             PCX_GetSize(const char* fn, int* w, int* h);
    void            PCX_Load(const char* fn, int bufWidth, int bufHeight,
                             byte* outBuffer);
    int             PCX_MemoryLoad(byte* imgdata, int len, int bufWidth,
                                   int bufHeight, byte* outBuffer);

    // Graphics: PNG.
    byte*           PNG_Load(const char* fileName, int* width, int* height,
                             int* pixelSize);

    // Graphics: TGA.
    int             TGA_Save24_rgb565(const char* filename, int w, int h,
                                      unsigned short* buffer);
    int             TGA_Save24_rgb888(const char* filename, int w, int h,
                                      byte* buffer);
    int             TGA_Save24_rgba8888(const char* filename, int w, int h,
                                        byte* buffer);
    int             TGA_Save16_rgb888(const char* filename, int w, int h,
                                      byte* buffer);
    int             TGA_GetSize(const char* filename, int *w, int *h);

    // Audio.
    void            S_MapChange(void);
    int             S_LocalSoundAtVolumeFrom(int sound_id,
                                             struct mobj_s* origin,
                                             float* pos, float volume);
    int             S_LocalSoundAtVolume(int soundID, struct mobj_s* origin,
                                         float volume);
    int             S_LocalSound(int soundID, struct mobj_s* origin);
    int             S_LocalSoundFrom(int soundID, float* fixedpos);
    int             S_StartSound(int soundId, struct mobj_s* origin);
    int             S_StartSoundEx(int soundId, struct mobj_s* origin);
    int             S_StartSoundAtVolume(int soundID, struct mobj_s* origin,
                                         float volume);
    int             S_ConsoleSound(int soundID, struct mobj_s* origin,
                                   int targetConsole);
    void            S_StopSound(int soundID, struct mobj_s* origin);
    int             S_IsPlaying(int soundID, struct mobj_s* origin);
    int             S_StartMusic(char* musicID, boolean looped);
    int             S_StartMusicNum(int id, boolean looped);
    void            S_StopMusic(void);

    // Miscellaneous.
    size_t          M_ReadFile(const char* name, byte** buffer);
    size_t          M_ReadFileCLib(const char* name, byte** buffer);
    boolean         M_WriteFile(const char* name, void* source, size_t length);
    void            M_ExtractFileBase(const char* path, char* dest);
    void            M_GetFileExt(const char* path, char* ext);
    boolean         M_CheckPath(const char* path);
    int             M_FileExists(const char* file);
    void            M_TranslatePath(const char* path, char* translated);
    const char*     M_PrettyPath(const char* path);
    char*           M_SkipWhite(char* str);
    char*           M_FindWhite(char* str);
    char*           M_StrCatQuoted(char* dest, const char* src);
    byte            RNG_RandByte(void);
    float           RNG_RandFloat(void);
    void            M_ClearBox(fixed_t* box);
    void            M_AddToBox(fixed_t* box, fixed_t x, fixed_t y);
    int             M_ScreenShot(const char* filename, int bits);
    int             M_CeilPow2(int num);

    // MiscellaneousL: Time utilities.
    boolean         M_RunTrigger(trigger_t* trigger, timespan_t advanceTime);
    boolean         M_CheckTrigger(const trigger_t* trigger, timespan_t advanceTime);

    // Miscellaneous: Math.
    void            V2_Rotate(float vec[2], float radians);
    binangle_t      bamsAtan2(int y, int x);

    // Miscellaneous: Command line.
    void _DECALL    ArgAbbreviate(char* longname, char* shortname);
    int _DECALL     Argc(void);
    char* _DECALL   Argv(int i);
    char** _DECALL  ArgvPtr(int i);
    char* _DECALL   ArgNext(void);
    int _DECALL     ArgCheck(char* check);
    int _DECALL     ArgCheckWith(char* check, int num);
    int _DECALL     ArgExists(char* check);
    int _DECALL     ArgIsOption(int i);

#ifdef __cplusplus
}
#endif
#endif
