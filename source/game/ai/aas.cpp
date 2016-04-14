#include "aas.h"
#include "../g_local.h"

#include <stdarg.h>

// Based on Q3 BotImport_Print() from code/server/sv_bot.c
void AAS_QF_Print(int type, char *format, ...)
{
    char str[1024];
    va_list ap;

    va_start(ap, format);
    Q_vsnprintf(str, sizeof(str), format, ap);
    va_end(ap);

    switch(type) {
        case PRT_MESSAGE: {
            Com_Printf("%s", str);
            break;
        }
        case PRT_WARNING: {
            Com_Printf(S_COLOR_YELLOW "Warning: %s", str);
            break;
        }
        case PRT_ERROR: {
            Com_Printf(S_COLOR_RED "Error: %s", str);
            break;
        }
        case PRT_FATAL: {
            Com_Printf(S_COLOR_RED "Fatal: %s", str);
            break;
        }
        case PRT_EXIT: {
            Com_Error(ERR_DROP, S_COLOR_RED "Exit: %s", str);
            break;
        }
        default: {
            Com_Printf("unknown print type\n");
            break;
        }
    }
}

// Based on Q3 BotImport_Trace() from code/server/sv_bot.c
void AAS_QF_Trace(bsp_trace_t *bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask)
{
    trace_t trace;
    G_Trace(&trace, start, mins, maxs, end, game.edicts + passent, contentmask);
    //copy the trace information
    bsptrace->allsolid = trace.allsolid;
    bsptrace->startsolid = trace.startsolid;
    bsptrace->fraction = trace.fraction;
    VectorCopy(trace.endpos, bsptrace->endpos);
    bsptrace->plane.dist = trace.plane.dist;
    VectorCopy(trace.plane.normal, bsptrace->plane.normal);
    bsptrace->plane.signbits = trace.plane.signbits;
    bsptrace->plane.type = trace.plane.type;
    bsptrace->surface.value = 0;
    bsptrace->surface.flags = trace.surfFlags;
    bsptrace->ent = trace.ent;
    bsptrace->exp_dist = 0;
    bsptrace->sidenum = 0;
    bsptrace->contents = 0;
}

// Based on SV_ClipHandleForEntity() from Q3 code/server/sv_world.c
static cmodel_s *AAS_QF_CModelForEntity(edict_t *ent)
{
    if (ent->r.solid & SOLID_BMODEL)
    {
        // explicit hulls in the BSP model
        return trap_CM_InlineModel(ent->s.modelindex);
    }
    if (ent->s.type == ET_PLAYER || ent->s.type == ET_CORPSE)
    {
        // create a temp s/capsule/octagon from bounding box sizes
        return trap_CM_OctagonModelForBBox(ent->r.mins, ent->r.maxs);
    }
    // create a temp tree from bounding box sizes
    return trap_CM_ModelForBBox(ent->r.mins, ent->r.maxs);
}

// Based on SV_ClipToEntity() from Q3 code/server/sv_world.c
static void AAS_QF_ClipToEntity(trace_t *trace, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int entityNum, int contentmask)
{
    edict_t *touch = game.edicts + entityNum;

    Com_Memset(trace, 0, sizeof(trace_t));

    // if it doesn't have any brushes of a type we are looking for, ignore it
    if (!( contentmask & touch->r.clipmask))
    {
        trace->fraction = 1.0;
        return;
    }

    // might intersect, so do an exact clip
    cmodel_s *cmodel = AAS_QF_CModelForEntity(touch);

    float *origin = touch->s.origin;
    float *angles = touch->s.angles;

    if (!touch->r.solid & SOLID_BMODEL)
        angles = vec3_origin;	// boxes don't rotate

    trap_CM_TransformedBoxTrace(
        trace, (float *)start, (float *)end, (float *)mins, (float *)maxs, cmodel, contentmask, origin, angles);

    if (trace->fraction < 1)
        trace->ent = touch->s.number;
}

// Based on Q3 BotImport_EntityTrace() from code/server/sv_bot.c
void AAS_QF_EntityTrace(bsp_trace_t *bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int entnum, int contentmask)
{
    trace_t trace;
    AAS_QF_ClipToEntity(&trace, start, mins, maxs, end, entnum, contentmask);
    //copy the trace information
    bsptrace->allsolid = trace.allsolid;
    bsptrace->startsolid = trace.startsolid;
    bsptrace->fraction = trace.fraction;
    VectorCopy(trace.endpos, bsptrace->endpos);
    bsptrace->plane.dist = trace.plane.dist;
    VectorCopy(trace.plane.normal, bsptrace->plane.normal);
    bsptrace->plane.signbits = trace.plane.signbits;
    bsptrace->plane.type = trace.plane.type;
    bsptrace->surface.value = 0;
    bsptrace->surface.flags = trace.surfFlags;
    bsptrace->ent = trace.ent;
    bsptrace->exp_dist = 0;
    bsptrace->sidenum = 0;
    bsptrace->contents = 0;
}

int AAS_QF_inPVS(vec3_t p1, vec3_t p2)
{
    return trap_inPVS(p1, p2);
}

char *AAS_QF_BSPEntityData()
{
    if (!level.mapString)
    {
        G_Error("AAS_QF_BSPEntityData(): level.mapString is null\n");
    }
    return level.mapString;
}

// Based on Q3 code/server/sv_bot.c
void AAS_QF_BSPModelMinsMaxsOrigin(int modelnum, vec3_t angles, vec3_t outmins, vec3_t outmaxs, vec3_t origin)
{
    vec3_t mins, maxs;
    cmodel_s *model = trap_CM_InlineModel(modelnum);
    trap_CM_InlineModelBounds(model, mins, maxs);
    //if the model is rotated
    if ((angles[0] || angles[1] || angles[2])) {
        // expand for rotation

        float max = RadiusFromBounds(mins, maxs);
        for (int i = 0; i < 3; i++) {
            mins[i] = -max;
            maxs[i] = max;
        }
    }
    if (outmins) VectorCopy(mins, outmins);
    if (outmaxs) VectorCopy(maxs, outmaxs);
    if (origin) VectorClear(origin);
}

void *AAS_QF_GetMemory(int size)
{
    return G_Malloc(size);
}

void AAS_QF_FreeMemory(void *ptr)
{
    G_Free(ptr);
}

int AAS_QF_AvailableMemory()
{
    return 128 * 1024 * 1024;
}

void *AAS_QF_HunkAlloc(int size)
{
    return G_LevelMalloc(size);
}

int AAS_QF_FS_FOpenFile( const char *qpath, fileHandle_t *file, fsMode_t mode)
{
    return trap_FS_FOpenFile(qpath, file, mode);
}

int	AAS_QF_FS_Read(void *buffer, int len, fileHandle_t f)
{
    return trap_FS_Read(buffer, len, f);
}

int	AAS_QF_FS_Write(const void *buffer, int len, fileHandle_t f)
{
    return trap_FS_Write(buffer, len, f);
}

void AAS_QF_FS_FCloseFile(fileHandle_t f)
{
    return trap_FS_FCloseFile(f);
}

int AAS_QF_FS_Seek(fileHandle_t f, long offset, int origin)
{
    return trap_FS_Seek(f, offset, origin);
}

int	AAS_QF_DebugLineCreate() { return 0; }

void AAS_QF_DebugLineDelete(int line) {}

void AAS_QF_DebugLineShow(int line, vec3_t start, vec3_t end, int color) {}

int	AAS_QF_DebugPolygonCreate(int color, int numPoints, vec3_t *points) { return 0; }

void AAS_QF_DebugPolygonDelete(int id) {}

// Since we link AAS code statically, use this "botimport" instead of one previously defined in be_interface.c
// to reduce amount of boilerplate code and aid optimization
extern "C" const botlib_import_t botimport = {
    //void (*Print)(int type, char *fmt, ...);
    AAS_QF_Print,
    //void (*Trace)(bsp_trace_t *trace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask);
    AAS_QF_Trace,
    //void (*EntityTrace)(bsp_trace_t *trace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int entnum, int contentmask);
    AAS_QF_EntityTrace,
    //int (*PointContents)(vec3_t point);
    G_PointContents,
    //int (*inPVS)(vec3_t p1, vec3_t p2);
    AAS_QF_inPVS,
    //char *(*BSPEntityData)(void);
    AAS_QF_BSPEntityData,
    //void (*BSPModelMinsMaxsOrigin)(int modelnum, vec3_t angles, vec3_t mins, vec3_t maxs, vec3_t origin);
    AAS_QF_BSPModelMinsMaxsOrigin,
    //void (*BotClientCommand)(int client, char *command);
    nullptr, // Unused
    //void *(*GetMemory)(int size);        // allocate from Zone
    AAS_QF_GetMemory,
    //void (*FreeMemory)(void *ptr);        // free memory from Zone
    AAS_QF_FreeMemory,
    //int (*AvailableMemory)(void);        // available Zone memory
    AAS_QF_AvailableMemory,
    //void *(*HunkAlloc)(int size);        // allocate from hunk
    AAS_QF_HunkAlloc,
    //int (*FS_FOpenFile)(const char *qpath, fileHandle_t *file, fsMode_t mode);
    AAS_QF_FS_FOpenFile,
    //int (*FS_Read)(void *buffer, int len, fileHandle_t f);
    AAS_QF_FS_Read,
    //int (*FS_Write)(const void *buffer, int len, fileHandle_t f);
    AAS_QF_FS_Write,
    //void (*FS_FCloseFile)(fileHandle_t f);
    AAS_QF_FS_FCloseFile,
    //int (*FS_Seek)(fileHandle_t f, long offset, int origin);
    AAS_QF_FS_Seek,
    //int (*DebugLineCreate)(void);
    AAS_QF_DebugLineCreate,
    //void (*DebugLineDelete)(int line);
    AAS_QF_DebugLineDelete,
    //void (*DebugLineShow)(int line, vec3_t start, vec3_t end, int color);
    AAS_QF_DebugLineShow,
    //int (*DebugPolygonCreate)(int color, int numPoints, vec3_t *points);
    AAS_QF_DebugPolygonCreate,
    //void (*DebugPolygonDelete)(int id);
    AAS_QF_DebugPolygonDelete
};

// Wtf? AAS code can't find this symbol while linking...
extern "C" void Com_Error(com_error_code_t code, const char *format, ...) {}

// Use it instead of calling AAS_QF_BotLibVarSet to avoid casting a string literal to a (non-const) char * warning
inline void LibVarSet(const char *name, const char *value)
{
    AAS_QF_BotLibVarSet(const_cast<char*>(name), const_cast<char*>(value));
}

bool AI_InitAAS()
{
    if (AAS_QF_BotLibSetup() != BLERR_NOERROR)
        return false;

    // These values will be read by AAS_InitSettings() during map loading
    // This is a "complementary" (get->set) copy of AAS_InitSettings() stuff with obvious fixes for the QF game

    LibVarSet("phys_friction", "8"); // wsw
    LibVarSet("phys_stopspeed", "100");
    LibVarSet("phys_gravity", va("%f", (float)GRAVITY)); // wsw
    LibVarSet("phys_waterfriction", "1");
    LibVarSet("phys_watergravity", "400");
    LibVarSet("phys_maxvelocity", "320");
    LibVarSet("phys_maxwalkvelocity", "320");
    LibVarSet("phys_maxcrouchvelocity", "100");
    LibVarSet("phys_maxswimvelocity", "150");
    LibVarSet("phys_walkaccelerate", "12"); // wsw
    LibVarSet("phys_airaccelerate", "1");
    LibVarSet("phys_swimaccelerate", "10"); // wsw
    LibVarSet("phys_maxstep", "19");
    LibVarSet("phys_maxsteepness", "0.7");
    LibVarSet("phys_maxwaterjump", "18");
    LibVarSet("phys_maxbarrier", "33");
    LibVarSet("phys_jumpvel", va("%f", (float)DEFAULT_JUMPSPEED)); // wsw
    LibVarSet("phys_falldelta5", "40");
    LibVarSet("phys_falldelta10", "60");
    LibVarSet("rs_waterjump", "400");
    LibVarSet("rs_teleport", "50");
    LibVarSet("rs_barrierjump", "100");
    LibVarSet("rs_startcrouch", "300");
    LibVarSet("rs_startgrapple", "500");
    LibVarSet("rs_startwalkoffledge", "70");
    LibVarSet("rs_startjump", "300");
    LibVarSet("rs_rocketjump", "500");
    LibVarSet("rs_bfgjump", "500");
    LibVarSet("rs_jumppad", "250");
    LibVarSet("rs_aircontrolledjumppad", "300");
    LibVarSet("rs_funcbob", "300");
    LibVarSet("rs_startelevator", "50");
    LibVarSet("rs_falldamage5", "300");
    LibVarSet("rs_falldamage10", "500");
    LibVarSet("rs_maxfallheight", "0");
    LibVarSet("rs_maxjumpfallheight", "450");

    return true;
}

void AI_ShutdownAAS()
{
    AAS_QF_BotLibShutdown();
}

bool AI_LoadLevelAAS(const char *mapname)
{
    return AAS_QF_BotLibLoadMap(mapname) == BLERR_NOERROR;
}

void AI_AASFrame()
{
    if (AAS_Loaded())
    {
        AAS_QF_BotLibStartFrame(level.time);
    }
}

