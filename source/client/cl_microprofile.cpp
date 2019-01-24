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

#if MICROPROFILE_ENABLED

void CL_Profiler_Init() {
	MicroProfileOnThreadCreate( "Main" );
        MicroProfileSetForceEnable( true );
        MicroProfileSetEnableAllGroups( true );
        MicroProfileSetForceMetaCounters( true );

	MicroProfileInitUI();

	Cmd_AddCommand( "toggleprofiler", MicroProfileToggleDisplayMode );
}

void CL_Profiler_Shutdown() {
	MicroProfileShutdown();

	Cmd_RemoveCommand( "toggleprofiler" );
}

void CL_Profiler_InitGL() {
	MicroProfileGpuInitGL();
	MicroProfileDrawInitGL();
}

void CL_Profiler_ShutdownGL() {
	MicroProfileGpuShutdownGL();
}

void CL_Profiler_Flip() {
	MicroProfileFlip();
}

#else

void CL_Profiler_Init() { }
void CL_Profiler_Shutdown() { }
void CL_Profiler_InitGL() { }
void CL_Profiler_ShutdownGL() { }
void CL_Profiler_Flip() { }

#endif
