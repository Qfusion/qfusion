#ifndef AI_EDICTREF_H
#define AI_EDICTREF_H

#include "ai_local.h"
#include "vec3.h"

class EdictRef
{
public:
    edict_t * const self;
    EdictRef(edict_t *ent): self(ent) {}

    edict_t *Self() { return self; }
    const edict_t *Self() const { return self; }

    int EntNum() const { return ENTNUM(self); }
};

#endif
