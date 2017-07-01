#ifndef QFUSION_AI_GROUND_TRACE_CACHE_H
#define QFUSION_AI_GROUND_TRACE_CACHE_H

#include "../../gameshared/q_collision.h"

class AiGroundTraceCache
{
	// In order to prevent inclusion of g_local.h declare an untyped pointer
	void *data;
	AiGroundTraceCache();
	AiGroundTraceCache( const AiGroundTraceCache &that ) = delete;
	AiGroundTraceCache &operator=( const AiGroundTraceCache &that ) = delete;

public:
	AiGroundTraceCache( AiGroundTraceCache &&that ) {
		data = that.data;
		that.data = nullptr;
	}
	~AiGroundTraceCache();
	static AiGroundTraceCache *Instance();

	void GetGroundTrace( const struct edict_s *ent, float depth, trace_t *trace, uint64_t maxMillisAgo = 0 );
	bool TryDropToFloor( const struct edict_s *ent, float depth, vec3_t result, uint64_t maxMillisAgo = 0 );
};

#endif
