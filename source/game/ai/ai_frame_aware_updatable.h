#ifndef QFUSION_AI_FRAME_AWARE_UPDATABLE_H
#define QFUSION_AI_FRAME_AWARE_UPDATABLE_H

#include "ai_local.h"
#include <stdarg.h>

class AiFrameAwareUpdatable
{
protected:
	unsigned frameAffinityModulo;
	unsigned frameAffinityOffset;

	bool ShouldSkipThinkFrame() {
		// Check whether the modulo has not been set yet
		return frameAffinityModulo == 0 || level.framenum % frameAffinityModulo != frameAffinityOffset;
	}

	// This method group is called on each frame. See Update() for calls order.
	// It is not recommended (but not forbidden) to do CPU-intensive updates in these methods.
	virtual void Frame() {}
	virtual void PreFrame() {}
	virtual void PostFrame() {}

	inline void CheckIsInThinkFrame( const char *function ) {
#ifdef _DEBUG
		if( ShouldSkipThinkFrame() ) {
			const char *format = "%s has been called not in think frame: frame#=%d, modulo=%d, offset=%d\n";
			G_Printf( format, function, frameAffinityModulo, frameAffinityOffset );
			printf( format, function, frameAffinityModulo, frameAffinityOffset );
			abort();
		}
#endif
	}

	// This method group is called only when frame affinity allows to do it. See Update() for calls order.
	// It is recommended (but not mandatory) to do all CPU-intensive updates in these methods instead of Frame();
	virtual void Think() {}
	virtual void PreThink() {}
	virtual void PostThink() {}

	// May be overridden if some actions should be performed when a frame affinity is set
	virtual void SetFrameAffinity( unsigned modulo, unsigned offset ) {
		frameAffinityModulo = modulo;
		frameAffinityOffset = offset;
	}

#ifndef _MSC_VER
	inline void Debug( const char *format, ... ) const __attribute__( ( format( printf, 2, 3 ) ) )
#else
	inline void Debug( _Printf_format_string_ const char *format, ... ) const
#endif
	{
		va_list va;
		va_start( va, format );
		AI_Debugv( tag, format, va );
		va_end( va );
	}

#ifndef _MSC_VER
	inline void FailWith( const char *format, ... ) const __attribute__( ( format( printf, 2, 3 ) ) ) __attribute__( ( noreturn ) )
#else
	__declspec( noreturn ) inline void FailWith( _Printf_format_string_ const char *format, ... ) const
#endif
	{
		va_list va;
		va_start( va, format );
		AI_FailWithv( tag, format, va );
		va_end( va );
	}

	static constexpr unsigned MAX_TAG_LEN = 128;
	char tag[MAX_TAG_LEN];

	void SetTag( const char *tag_ ) {
		Q_strncpyz( this->tag, tag_, MAX_TAG_LEN );
	}

public:
	AiFrameAwareUpdatable() : frameAffinityModulo( 0 ), frameAffinityOffset( 0 ) {
		tag[0] = '\0';
	}
	virtual ~AiFrameAwareUpdatable() {}

	inline const char *Tag() const { return tag; }

	// Call this method to update an instance state.
	void Update();
};

#endif
