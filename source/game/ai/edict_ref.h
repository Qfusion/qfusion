#ifndef AI_EDICTREF_H
#define AI_EDICTREF_H

#include "ai_local.h"
#include "vec3.h"

class EdictRef
{
public:
    edict_t * const self;
    EdictRef(edict_t *ent): self(ent) {}
};

#endif
