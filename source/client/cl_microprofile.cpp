#include "renderer/glad.h"

#define MICROPROFILE_MAX_FRAME_HISTORY 2048
#define MICROPROFILE_GPU_TIMERS_GL 1

#define MICROPROFILE_IMPL
#define MICROPROFILEUI_IMPL
#define MICROPROFILEDRAW_IMPL
#include "microprofile/microprofile.h"
#include "microprofile/microprofileui.h"
#include "microprofile/microprofiledraw.h"

#include "qcommon/qcommon.h"

void CL_Profiler_Init() {
	MicroProfileOnThreadCreate( "Main" );
        MicroProfileSetForceEnable( true );
        MicroProfileSetEnableAllGroups( true );
        MicroProfileSetForceMetaCounters( true );

#if MICROPROFILE_ENABLED
	MicroProfileInitUI();
#endif

#if !PUBLIC_BUILD
	Cmd_AddCommand( "toggleprofiler", MicroProfileToggleDisplayMode );
#endif
}

void CL_Profiler_Shutdown() {
	MicroProfileShutdown();

#if !PUBLIC_BUILD
	Cmd_RemoveCommand( "toggleprofiler" );
#endif
}

void CL_Profiler_InitGL() {
#if MICROPROFILE_ENABLED
	MicroProfileGpuInitGL();
	MicroProfileDrawInitGL();
#endif
}

void CL_Profiler_ShutdownGL() {
#if MICROPROFILE_ENABLED
	MicroProfileGpuShutdownGL();
#endif
}

void CL_Profiler_Flip() {
	MicroProfileFlip();
}
