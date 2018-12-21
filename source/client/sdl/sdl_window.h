#pragma once

#include <stddef.h>

typedef enum FullScreenMode {
	FullScreenMode_Windowed,
	FullScreenMode_Fullscreen,
	FullScreenMode_FullscreenBorderless,
} FullScreenMode;

typedef struct VideoMode {
	int width, height;
	int frequency;
	FullScreenMode fullscreen;
} VideoMode;

typedef enum VsyncEnabled {
	VsyncEnabled_Disabled,
	VsyncEnabled_Enabled,
} VsyncEnabled;

int VID_GetNumVideoModes();
VideoMode VID_GetVideoMode( int i );
VideoMode VID_GetDefaultVideoMode();

void VID_WindowInit( VideoMode mode, int stencilbits );
void VID_WindowShutdown();

void VID_EnableVsync( VsyncEnabled enabled );

bool VID_SetVideoMode( VideoMode mode );
VideoMode VID_GetCurrentVideoMode();

bool VID_GetGammaRamp( size_t stride, unsigned short *psize, unsigned short *ramp );
void VID_SetGammaRamp( size_t stride, unsigned short size, unsigned short *ramp );

void VID_Swap();
