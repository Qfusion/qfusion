#pragma once

#include <stddef.h>

#include "qcommon/ggformat.h"

enum FullScreenMode {
	FullScreenMode_Windowed,
	FullScreenMode_Fullscreen,
	FullScreenMode_FullscreenBorderless,
};

struct VideoMode {
	int width, height;
	int frequency;
};

struct WindowMode {
	VideoMode video_mode;
	int monitor;
	int x, y;
	FullScreenMode fullscreen;
};

enum VsyncEnabled {
	VsyncEnabled_Disabled,
	VsyncEnabled_Enabled,
};

int VID_GetNumVideoModes();
VideoMode VID_GetVideoMode( int i );
VideoMode VID_GetCurrentVideoMode();
void VID_SetVideoMode( VideoMode mode );

void VID_WindowInit( WindowMode mode );
void VID_WindowShutdown();

void VID_EnableVsync( VsyncEnabled enabled );

WindowMode VID_GetWindowMode();
void VID_SetWindowMode( WindowMode mode );

bool VID_GetGammaRamp( size_t stride, unsigned short *psize, unsigned short *ramp );
void VID_SetGammaRamp( size_t stride, unsigned short size, unsigned short *ramp );

void VID_Swap();

void format( FormatBuffer * fb, VideoMode mode, const FormatOpts & opts );
void format( FormatBuffer * fb, WindowMode mode, const FormatOpts & opts );
