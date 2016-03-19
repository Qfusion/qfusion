#ifndef QFUSION_Q3COMPAT_H
#define QFUSION_Q3COMPAT_H

#include <stdbool.h>
#include "../../../gameshared/q_shared.h"
#include "../../../gameshared/q_math.h"
#include "../../../gameshared/q_collision.h"
#include "../../../gameshared/q_comref.h"

typedef bool qboolean;
#define qfalse false
#define qtrue true

typedef unsigned char byte;

#ifdef __GNUC__
#define QDECL // avoid "warning: ‘__cdecl__’ attribute ignored [-Wattributes]"
#endif

#ifdef _MSC_VER
#define QDECL __cdecl
#endif

#define ID_INLINE inline

#define PAD(base, alignment)	(((base)+(alignment)-1) & ~((alignment)-1))
#define PADLEN(base, alignment)	(PAD((base), (alignment)) - (base))

#define PADP(base, alignment)	((void *) PAD((intptr_t) (base), (alignment)))

#define Com_Memset memset
#define Com_Memcpy memcpy

#define Q_vsnprintf Q_vsnprintfz
#define Com_sprintf Q_snprintfz
#define Q_strcat(dest, size, src) Q_strncatz(dest, src, size)

typedef int fileHandle_t;
typedef int fsMode_t;

#define ENTITYNUM_WORLD 0
#define MAX_OSPATH MAX_PATH
#define BASEGAME "basewsw"
#define PATH_SEP "/"
#define ARRAY_LEN(arr) sizeof(arr) / sizeof(*arr)

#define MAX_TOKENLENGTH 1024

typedef struct pc_token_s {
    int type;
    int subtype;
    int intvalue;
    float floatvalue;
    char string[MAX_TOKENLENGTH];
} pc_token_t;

#endif
