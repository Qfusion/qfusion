#ifndef AI_EDICTREF_H
#define AI_EDICTREF_H

#include "vec3.h"
#include "../g_local.h"

class EdictRef
{
public:
    edict_t * const self;
    EdictRef(edict_t *ent): self(ent) {}
};

#endif
