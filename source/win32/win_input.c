/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// in_win.c -- windows mouse and joystick code
// 02/21/97 JCB Added extended DirectInput code to support external controllers.

#include "../client/client.h"
#include "winquake.h"

//#ifdef __GNUC__
#define DIRECTINPUT_VERSION 0x0700 // Could use dx9, but older is more frequently used
//#else
//#define	DIRECTINPUT_VERSION 0x0800
//#endif

#include <dinput.h>
#include <xinput.h>

#define DINPUT_BUFFERSIZE           64 // http://www.esreality.com/?a=post&id=905276#pid905330
#define iDirectInputCreate( a, b, c, d ) pDirectInputCreate( a, b, c, d )

static HRESULT( WINAPI * pDirectInputCreate )( HINSTANCE hinst, DWORD dwVersion,
											   LPDIRECTINPUT * lplpDirectInput, LPUNKNOWN punkOuter );

// raw input specific defines
#define MAX_RI_DEVICE_SIZE 128
#define INIT_RIBUFFER_SIZE ( sizeof( RAWINPUTHEADER ) + sizeof( RAWMOUSE ) )

#define RI_RAWBUTTON_MASK 0x000003E0
#define RI_INVALID_POS    0x80000000

// raw input dynamic functions
typedef int ( WINAPI * pGetRawInputDeviceList )( OUT PRAWINPUTDEVICELIST pRawInputDeviceList, IN OUT PINT puiNumDevices, IN UINT cbSize );
typedef int ( WINAPI * pGetRawInputData )( IN HRAWINPUT hRawInput, IN UINT uiCommand, OUT LPVOID pData, IN OUT PINT pcbSize, IN UINT cbSizeHeader );
typedef int ( WINAPI * pGetRawInputDeviceInfoA )( IN HANDLE hDevice, IN UINT uiCommand, OUT LPVOID pData, IN OUT PINT pcbSize );
typedef BOOL ( WINAPI * pRegisterRawInputDevices )( IN PCRAWINPUTDEVICE pRawInputDevices, IN UINT uiNumDevices, IN UINT cbSize );

pGetRawInputDeviceList _GRIDL;
pGetRawInputData _GRID;
pGetRawInputDeviceInfoA _GRIDIA;
pRegisterRawInputDevices _RRID;

// XInput
static bool in_xinput_initialized;
static HINSTANCE in_xinput_dll;
static DWORD( WINAPI * pXInputGetState )( DWORD dwUserIndex, XINPUT_STATE * pState );
static XINPUT_GAMEPAD in_xinput_gamepad, in_xinput_oldGamepad;

typedef struct {
	HANDLE rawinputhandle;          // raw input, identify particular mice

	int numbuttons;
	volatile int buttons;

	volatile int delta[2];
	int pos[2];
} rawmouse_t;

static rawmouse_t   *rawmice = NULL;
static int rawmicecount = 0;
static RAWINPUT     *raw = NULL;
static int ribuffersize = 0;
static bool rawinput_initialized = false;

static bool IN_RawInput_Init( void );
static void     IN_RawInput_Shutdown( void );
static int      IN_RawInput_Register( void );
static void     IN_RawInput_DeRegister( void );

extern int64_t sys_msg_time;

bool in_appactive, in_showcursor;

// forward-referenced functions
static void IN_XInput_Init( void );
static void IN_XInput_Shutdown( void );

/*
============================================================

MOUSE CONTROL

============================================================
*/

// used by win_vid.c
int mouse_buttons;
int mouse_wheel_type;

static int mouse_oldbuttonstate;
static POINT current_pos;
static int mx, my;
static bool mouseactive;    // false when not focus app
static bool restore_spi;
static bool mouseinitialized;
static int originalmouseparms[3], newmouseparms[3] = { 0, 0, 0 };
static bool mouseparmsvalid;
static unsigned int mstate_di;

static int window_center_x, window_center_y;
static RECT window_rect;

static LPDIRECTINPUT g_pdi;
static LPDIRECTINPUTDEVICE g_pMouse;

static HINSTANCE hInstDI;

static bool dinput_initialized;
static bool dinput_acquired;

// replacement for dxguid.lib which is not available in latest Windows SDKs for XP
static const GUID qGUID_XAxis =     { 0xa36d02e0, 0xc9f3, 0x11cf, { 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };
static const GUID qGUID_YAxis =     { 0xa36d02e1, 0xc9f3, 0x11cf, { 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };
static const GUID qGUID_ZAxis =     { 0xa36d02e2, 0xc9f3, 0x11cf, { 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };
static const GUID qGUID_SysMouse =  { 0x6f1d2b60, 0xd5a0, 0x11cf, { 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };

typedef struct MYDATA {
	LONG lX;                // X axis goes here
	LONG lY;                // Y axis goes here
	LONG lZ;                // Z axis goes here
	BYTE bButtonA;          // One button goes here
	BYTE bButtonB;          // Another button goes here
	BYTE bButtonC;          // Another button goes here
	BYTE bButtonD;          // Another button goes here
	BYTE bButtonE;          // Another button goes here
	BYTE bButtonF;          // Another button goes here
	BYTE bButtonG;          // Another button goes here
	BYTE bButtonH;          // Another button goes here
} MYDATA;

// This structure corresponds to c_dfDIMouse2 in dinput8.lib
// 0x80000000 is something undocumented but must be there, otherwise
// IDirectInputDevice_SetDataFormat may fail.
static DIOBJECTDATAFORMAT rgodf[] = {
	{ &qGUID_XAxis, FIELD_OFFSET( MYDATA, lX ), DIDFT_AXIS | DIDFT_ANYINSTANCE, 0, },
	{ &qGUID_YAxis, FIELD_OFFSET( MYDATA, lY ), DIDFT_AXIS | DIDFT_ANYINSTANCE, 0, },
	{ &qGUID_ZAxis, FIELD_OFFSET( MYDATA, lZ ), 0x80000000 | DIDFT_AXIS | DIDFT_ANYINSTANCE, 0, },
	{ 0, FIELD_OFFSET( MYDATA, bButtonA ), DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0, },
	{ 0, FIELD_OFFSET( MYDATA, bButtonB ), DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0, },
	{ 0, FIELD_OFFSET( MYDATA, bButtonC ), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0, },
	{ 0, FIELD_OFFSET( MYDATA, bButtonD ), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0, },
	{ 0, FIELD_OFFSET( MYDATA, bButtonE ), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0, },
	{ 0, FIELD_OFFSET( MYDATA, bButtonF ), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0, },
	{ 0, FIELD_OFFSET( MYDATA, bButtonG ), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0, },
	{ 0, FIELD_OFFSET( MYDATA, bButtonH ), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0, },
};

#define NUM_OBJECTS ( sizeof( rgodf ) / sizeof( rgodf[0] ) )

static DIDATAFORMAT df = {
	sizeof( DIDATAFORMAT ), // this structure
	sizeof( DIOBJECTDATAFORMAT ), // size of object data format
	DIDF_RELAXIS,           // absolute axis coordinates
	sizeof( MYDATA ),       // device data size
	NUM_OBJECTS,            // number of objects
	rgodf,                  // and here they are
};

static void IN_ShowCursor( void ) {
	if( in_showcursor ) {
		while( ShowCursor( TRUE ) < 0 ) ;
	} else {
		while( ShowCursor( FALSE ) >= 0 ) ;
	}
}

/*
* IN_ActivateMouse
*
* Called when the window gains focus or changes in some way
*/
static void IN_ActivateMouse( void ) {
	if( !mouseinitialized ) {
		return;
	}
	if( mouseactive ) {
		return;
	}

	mouseactive = true;

	if( dinput_initialized ) {
		mstate_di = 0;
		if( g_pMouse ) {
			if( cl_hwnd ) {
				if( FAILED( IDirectInputDevice_SetCooperativeLevel( g_pMouse, cl_hwnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND ) ) ) {
					Com_DPrintf( "Couldn't set DI coop level\n" );
					return;
				}
			}
			if( !dinput_acquired ) {
				IDirectInputDevice_Acquire( g_pMouse );
				dinput_acquired = true;
			}
		}
		return;
	}

	if( rawinput_initialized ) {
		if( IN_RawInput_Register() ) {
			Com_Printf( "Raw input: unable to register raw input, deinitializing\n" );
			IN_RawInput_Shutdown();
		}
	}

	mouse_oldbuttonstate = 0;

	if( mouseparmsvalid ) {
		restore_spi = SystemParametersInfo( SPI_SETMOUSE, 0, newmouseparms, 0 );
	}

	SetCursorPos( window_center_x, window_center_y );

	SetCapture( cl_hwnd );
	ClipCursor( &window_rect );
}


/*
* IN_DeactivateMouse
*
* Called when the window loses focus
*/
static void IN_DeactivateMouse( void ) {
	if( !mouseinitialized ) {
		return;
	}
	if( !mouseactive ) {
		return;
	}

	mouseactive = false;

	if( dinput_initialized ) {
		if( g_pMouse ) {
			if( dinput_acquired ) {
				IDirectInputDevice_Unacquire( g_pMouse );
				dinput_acquired = false;
			}
			if( cl_hwnd ) {
				if( FAILED( IDirectInputDevice_SetCooperativeLevel( g_pMouse, cl_hwnd, DISCL_EXCLUSIVE | DISCL_BACKGROUND ) ) ) {
					Com_DPrintf( "Couldn't set DI coop level\n" );
					return;
				}
			}
		}
		return;
	}

	if( rawinput_initialized ) {
		IN_RawInput_DeRegister();
	}

	if( restore_spi ) {
		SystemParametersInfo( SPI_SETMOUSE, 0, originalmouseparms, 0 );
	}

	SetCursorPos( window_center_x, window_center_y );

	ClipCursor( NULL );
	ReleaseCapture();
}


/*
* IN_InitDInput
*/
static bool IN_InitDInput( void ) {
	HRESULT hr;
	DIPROPDWORD dipdw = {
		{
			sizeof( DIPROPDWORD ), // diph.dwSize
			sizeof( DIPROPHEADER ),     // diph.dwHeaderSize
			0,                  // diph.dwObj
			DIPH_DEVICE,        // diph.dwHow
		},
		DINPUT_BUFFERSIZE,      // dwData
	};

	if( !hInstDI ) {
		hInstDI = LoadLibrary( "dinput.dll" );

		if( hInstDI == NULL ) {
			Com_Printf( "Couldn't load dinput.dll\n" );
			return false;
		}
	}

	if( !pDirectInputCreate ) {
		pDirectInputCreate = (void *)GetProcAddress( hInstDI, "DirectInputCreateA" );

		if( !pDirectInputCreate ) {
			Com_Printf( "Couldn't get DI proc addr\n" );
			return false;
		}
	}

	// register with DirectInput and get an IDirectInput to play with
	hr = iDirectInputCreate( global_hInstance, DIRECTINPUT_VERSION, &g_pdi, NULL );

	if( FAILED( hr ) ) {
		Com_Printf( "DirectInputCreate failed\n" );
		return false;
	}

	// obtain an interface to the system mouse device
	hr = IDirectInput_CreateDevice( g_pdi, &qGUID_SysMouse, &g_pMouse, NULL );

	if( FAILED( hr ) ) {
		Com_Printf( "Couldn't open DI mouse device\n" );
		return false;
	}

	// set the data format to "mouse format"
	hr = IDirectInputDevice_SetDataFormat( g_pMouse, &df );

	if( FAILED( hr ) ) {
		Com_Printf( "Couldn't set DI mouse format\n" );
		return false;
	}

	// set the cooperativity level
	hr = IDirectInputDevice_SetCooperativeLevel( g_pMouse, cl_hwnd,
												 DISCL_EXCLUSIVE | DISCL_FOREGROUND );

	if( FAILED( hr ) ) {
		Com_DPrintf( "Couldn't set DI coop level\n" );
		return false;
	}

	// set the buffer size to DINPUT_BUFFERSIZE elements
	// the buffer size is a DWORD property associated with the device
	hr = IDirectInputDevice_SetProperty( g_pMouse, DIPROP_BUFFERSIZE, &dipdw.diph );

	if( FAILED( hr ) ) {
		Com_DPrintf( "Couldn't set DI buffersize\n" );
		return false;
	}

	return true;
}

/*
* IN_ShutdownDInput
*/
static void IN_ShutdownDInput( void ) {
	if( g_pMouse ) {
		IDirectInputDevice_SetCooperativeLevel( g_pMouse, cl_hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND );
		IDirectInputDevice_Release( g_pMouse );
	}

	if( g_pdi ) {
		IDirectInput_Release( g_pdi );
	}

	if( hInstDI ) {
		FreeLibrary( hInstDI );
	}

	g_pMouse = NULL;
	g_pdi = NULL;
	hInstDI = NULL;
	pDirectInputCreate = NULL;
}

/*
=========================================================================

RAW INPUT

=========================================================================
*/

/*
* IN_RawInput_Register
*/
int IN_RawInput_Register( void ) {
	// This function registers to receive the WM_INPUT messages
	RAWINPUTDEVICE Rid; // Register only for mouse messages from WM_INPUT

	// register to get wm_input messages
	Rid.usUsagePage = 0x01;
	Rid.usUsage = 0x02;
	Rid.dwFlags = RIDEV_CAPTUREMOUSE | RIDEV_NOLEGACY; // adds HID mouse and also ignores clicks out of the window and legacy mouse messages
	Rid.hwndTarget = cl_hwnd;

	// Register to receive the WM_INPUT message for any change in mouse (buttons, wheel, and movement will all generate the same message)
	if( !( *_RRID )( &Rid, 1, sizeof( Rid ) ) ) {
		return 1;
	}
	return 0;
}

/*
* IN_RawInput_DeRegister
*/
static void IN_RawInput_DeRegister( void ) {
	RAWINPUTDEVICE Rid;

	// deregister raw input
	Rid.usUsagePage = 0x01;
	Rid.usUsage = 0x02;
	Rid.dwFlags = RIDEV_REMOVE;
	Rid.hwndTarget = NULL;

	( *_RRID )( &Rid, 1, sizeof( Rid ) );
}

/*
* IN_RawInput_IsRDPMouse
*/
int IN_RawInput_IsRDPMouse( const char *cDeviceString ) {
	const char cRDPString[] = "\\??\\Root#RDP_MOU#";
	int i;

	if( strlen( cDeviceString ) < strlen( cRDPString ) ) {
		return 0;
	}

	for( i = strlen( cRDPString ) - 1; i >= 0; i-- ) {
		if( cDeviceString[i] != cRDPString[i] ) {
			return 0;
		}
	}
	return 1; // is RDP mouse
}

/*
* IN_RawInput_Init
*
* Returns false if rawinput is not available
*/
bool IN_RawInput_Init( void ) {
	int inputdevices, i, j, mtemp;
	PRAWINPUTDEVICELIST pRawInputDeviceList;
	char dname[MAX_RI_DEVICE_SIZE];
	HMODULE user32 = LoadLibrary( "user32.dll" );

	_GRIDL          = NULL;
	_GRID           = NULL;
	_GRIDIA         = NULL;
	_RRID           = NULL;

	rawmice         = NULL;
	rawmicecount    = 0;
	raw             = NULL;
	ribuffersize    = 0;

	if( !user32 ) {
		Com_Printf( "Raw input: unable to load user32.dll\n" );
		return false;
	}

	if( !( _RRID = ( pRegisterRawInputDevices )GetProcAddress( user32, "RegisterRawInputDevices" ) ) ) {
		Com_Printf( "Raw input: function RegisterRawInputDevices could not be registered\n" );
		return false;
	}

	if( !( _GRIDL = ( pGetRawInputDeviceList )GetProcAddress( user32, "GetRawInputDeviceList" ) ) ) {
		Com_Printf( "Raw input: function GetRawInputDeviceList could not be registered\n" );
		return false;
	}

	if( !( _GRIDIA = ( pGetRawInputDeviceInfoA )GetProcAddress( user32, "GetRawInputDeviceInfoA" ) ) ) {
		Com_Printf( "Raw input: function GetRawInputDeviceInfoA could not be registered\n" );
		return false;
	}

	if( !( _GRID = ( pGetRawInputData )GetProcAddress( user32, "GetRawInputData" ) ) ) {
		Com_Printf( "Raw input: function GetRawInputData could not be registered\n" );
		return false;
	}

	// 1st call to GetRawInputDeviceList: Pass NULL to get the number of devices.
	if( ( *_GRIDL )( NULL, &inputdevices, sizeof( RAWINPUTDEVICELIST ) ) != 0 ) {
		Com_Printf( "Raw input: unable to count raw input devices\n" );
		return false;
	}

	// Allocate the array to hold the DeviceList
	pRawInputDeviceList = Mem_ZoneMalloc( sizeof( RAWINPUTDEVICELIST ) * inputdevices );

	// 2nd call to GetRawInputDeviceList: Pass the pointer to our DeviceList and GetRawInputDeviceList() will fill the array
	if( ( *_GRIDL )( pRawInputDeviceList, &inputdevices, sizeof( RAWINPUTDEVICELIST ) ) == -1 ) {
		Com_Printf( "Raw input: unable to get raw input device list\n" );
		return false;
	}

	// Loop through all devices and count the mice
	for( i = 0, mtemp = 0; i < inputdevices; i++ ) {
		if( pRawInputDeviceList[i].dwType == RIM_TYPEMOUSE ) {
			j = MAX_RI_DEVICE_SIZE;

			// Get the device name and use it to determine if it's the RDP Terminal Services virtual device.
			if( ( *_GRIDIA )( pRawInputDeviceList[i].hDevice, RIDI_DEVICENAME, dname, &j ) < 0 ) {
				dname[0] = 0;
			}

			if( IN_RawInput_IsRDPMouse( dname ) ) { // ignore rdp mouse
				continue;
			}

			// advance temp device count
			mtemp++;
		}
	}

	// exit out if no devices found
	if( !mtemp ) {
		Com_Printf( "Raw input: no usable device found\n" );
		return false;
	}

	// Loop again and bind devices
	rawmice = Mem_ZoneMalloc( sizeof( rawmouse_t ) * mtemp );
	for( i = 0; i < inputdevices; i++ ) {
		if( pRawInputDeviceList[i].dwType == RIM_TYPEMOUSE ) {
			j = MAX_RI_DEVICE_SIZE;

			// Get the device name and use it to determine if it's the RDP Terminal Services virtual device.
			if( ( *_GRIDIA )( pRawInputDeviceList[i].hDevice, RIDI_DEVICENAME, dname, &j ) < 0 ) {
				dname[0] = 0;
			}

			if( IN_RawInput_IsRDPMouse( dname ) ) { // ignore rdp mouse
				continue;
			}

			// print pretty message about the mouse
			dname[MAX_RI_DEVICE_SIZE - 1] = 0;
			for( mtemp = strlen( dname ); mtemp >= 0; mtemp-- ) {
				if( dname[mtemp] == '#' ) {
					dname[mtemp + 1] = 0;
					break;
				}
			}
			Com_Printf( "Raw input: [%i] %s\n", i, dname );

			// set handle
			rawmice[rawmicecount].rawinputhandle = pRawInputDeviceList[i].hDevice;
			rawmice[rawmicecount].numbuttons = 10;
			rawmice[rawmicecount].pos[0] = RI_INVALID_POS;
			rawmicecount++;
		}
	}

	// free the RAWINPUTDEVICELIST
	Mem_ZoneFree( pRawInputDeviceList );

	// alloc raw input buffer
	raw = Mem_ZoneMalloc( INIT_RIBUFFER_SIZE );
	ribuffersize = INIT_RIBUFFER_SIZE;

	return true;
}

/*
* IN_RawInput_Shutdown
*/
static void IN_RawInput_Shutdown( void ) {
	rawinput_initialized = false;

	if( rawmicecount < 1 ) {
		return;
	}

	IN_RawInput_DeRegister();

	Mem_ZoneFree( rawmice );
	Mem_ZoneFree( raw );

	// dealloc mouse structure
	rawmicecount = 0;
}

/*
* IN_RawInput_MouseRead
*/
void IN_RawInput_MouseRead( HANDLE in_device_handle ) {
	int i = 0, tbuttons, j;
	int dwSize;

	if( !raw || !rawmice || rawmicecount < 1 ) {
		return; // no thx

	}
	// get raw input
	if( ( *_GRID )( (HRAWINPUT)in_device_handle, RID_INPUT, NULL, &dwSize, sizeof( RAWINPUTHEADER ) ) == -1 ) {
		Com_Printf( "Raw input: unable to add to get size of raw input header.\n" );
		return;
	}

	if( dwSize > ribuffersize ) {
		ribuffersize = dwSize;
		raw = Mem_Realloc( raw, dwSize );
	}

	if( ( *_GRID )( (HRAWINPUT)in_device_handle, RID_INPUT, raw, &dwSize, sizeof( RAWINPUTHEADER ) ) != dwSize ) {
		Com_Printf( "Raw input: unable to add to get raw input header.\n" );
		return;
	}

	// find mouse in our mouse list
	for( ; i < rawmicecount; i++ ) {
		if( rawmice[i].rawinputhandle == raw->header.hDevice ) {
			break;
		}
	}

	if( i == rawmicecount ) { // we're not tracking this mouse
		return;
	}

	// movement
	if( raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE ) {
		if( rawmice[i].pos[0] != RI_INVALID_POS ) {
			rawmice[i].delta[0] += raw->data.mouse.lLastX - rawmice[i].pos[0];
			rawmice[i].delta[1] += raw->data.mouse.lLastY - rawmice[i].pos[1];
		}
		rawmice[i].pos[0] = raw->data.mouse.lLastX;
		rawmice[i].pos[1] = raw->data.mouse.lLastY;
	} else {   // RELATIVE
		rawmice[i].delta[0] += raw->data.mouse.lLastX;
		rawmice[i].delta[1] += raw->data.mouse.lLastY;
		rawmice[i].pos[0] = RI_INVALID_POS;
	}

	// buttons
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN ) {
		Key_MouseEvent( K_MOUSE1, true, sys_msg_time );
	}
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_UP ) {
		Key_MouseEvent( K_MOUSE1, false, sys_msg_time );
	}
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN ) {
		Key_MouseEvent( K_MOUSE2, true, sys_msg_time );
	}
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_UP ) {
		Key_MouseEvent( K_MOUSE2, false, sys_msg_time );
	}
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN ) {
		Key_MouseEvent( K_MOUSE3, true, sys_msg_time );
	}
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_UP ) {
		Key_MouseEvent( K_MOUSE3, false, sys_msg_time );
	}
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN ) {
		Key_MouseEvent( K_MOUSE4, true, sys_msg_time );
	}
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP ) {
		Key_MouseEvent( K_MOUSE4, false, sys_msg_time );
	}
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN ) {
		Key_MouseEvent( K_MOUSE5, true, sys_msg_time );
	}
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP ) {
		Key_MouseEvent( K_MOUSE5, false, sys_msg_time );
	}

	// mouse wheel
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL ) {
		// if the current message has a mouse_wheel message
		if( (SHORT)raw->data.mouse.usButtonData > 0 ) {
			Key_Event( K_MWHEELUP, true, sys_msg_time );
			Key_Event( K_MWHEELUP, false, sys_msg_time );
		}

		if( (SHORT)raw->data.mouse.usButtonData < 0 ) {
			Key_Event( K_MWHEELDOWN, true, sys_msg_time );
			Key_Event( K_MWHEELDOWN, false, sys_msg_time );
		}
	}

	// extra buttons
	tbuttons = raw->data.mouse.ulRawButtons & RI_RAWBUTTON_MASK;
	for( j = 6; j < rawmice[i].numbuttons; j++ ) {
		if( ( tbuttons & ( 1 << j ) ) && !( rawmice[i].buttons & ( 1 << j ) ) ) {
			Key_MouseEvent( K_MOUSE1 + j, true, sys_msg_time );
		}

		if( !( tbuttons & ( 1 << j ) ) && ( rawmice[i].buttons & ( 1 << j ) ) ) {
			Key_MouseEvent( K_MOUSE1 + j, false, sys_msg_time );
		}
	}

	rawmice[i].buttons &= ~RI_RAWBUTTON_MASK;
	rawmice[i].buttons |= tbuttons;
}

/*
* IN_StartupMouse
*/
static void IN_StartupMouse( void ) {
	cvar_t *cv;
	int width, height;

	cv = Cvar_Get( "in_initmouse", "1", CVAR_NOSET );
	if( !cv->integer ) {
		return;
	}

	in_showcursor = true;
	dinput_initialized = false;
	rawinput_initialized = false;

	cv = Cvar_Get( "m_raw", "1", CVAR_ARCHIVE );
	if( cv->integer ) {
		rawinput_initialized = IN_RawInput_Init();
	}

	if( !rawinput_initialized ) {
		cv = Cvar_Get( "in_dinput", "1", CVAR_ARCHIVE );
		if( cv->integer ) {
			dinput_initialized = IN_InitDInput();
		}
	}

	if( rawinput_initialized ) {
		Com_Printf( "Raw input initialized with %i mice\n", rawmicecount );
	} else if( dinput_initialized ) {
		Com_Printf( "DirectInput initialized\n" );
	} else {
		Com_Printf( "DirectInput not initialized, using standard input\n" );
		mouseparmsvalid = SystemParametersInfo( SPI_GETMOUSE, 0, originalmouseparms, 0 );
	}

	mouseinitialized = true;
	mouse_buttons = 8;
	mouse_wheel_type = MWHEEL_UNKNOWN;

	width = GetSystemMetrics( SM_CXSCREEN );
	height = GetSystemMetrics( SM_CYSCREEN );

	GetWindowRect( cl_hwnd, &window_rect );
	if( window_rect.left < 0 ) {
		window_rect.left = 0;
	}
	if( window_rect.top < 0 ) {
		window_rect.top = 0;
	}
	if( window_rect.right >= width ) {
		window_rect.right = width - 1;
	}
	if( window_rect.bottom >= height - 1 ) {
		window_rect.bottom = height - 1;
	}

	window_center_x = ( window_rect.right + window_rect.left ) / 2;
	window_center_y = ( window_rect.top + window_rect.bottom ) / 2;
}

/*
* IN_MouseEvent
*/
void IN_MouseEvent( int mstate ) {
	int i;

	if( !mouseinitialized || dinput_initialized ) {
		return;
	}
	if( cls.key_dest == key_console ) {
		return;
	}
	if( cls.key_dest == key_menu && !cls.show_cursor ) {
		return;
	}

	// perform button actions
	for( i = 0; i < mouse_buttons; i++ ) {
		if( ( mstate & ( 1 << i ) ) &&
			!( mouse_oldbuttonstate & ( 1 << i ) ) ) {
			Key_MouseEvent( K_MOUSE1 + i, true, sys_msg_time );
		}

		if( !( mstate & ( 1 << i ) ) &&
			( mouse_oldbuttonstate & ( 1 << i ) ) ) {
			Key_MouseEvent( K_MOUSE1 + i, false, sys_msg_time );
		}
	}

	mouse_oldbuttonstate = mstate;
}

/*
* IN_GetMouseMovement
*/
void IN_GetMouseMovement( int *dx, int *dy ) {
	DIDEVICEOBJECTDATA od;
	DWORD dwElements;
	HRESULT hr;

	*dx = *dy = 0;
	
	if( !mouseactive ) {
		return;
	}

	if( rawinput_initialized ) {
		// probably not the right way...
		int i;

		mx = my = 0;

		for( i = 0; i < rawmicecount; i++ ) {
			mx += rawmice[i].delta[0];
			my += rawmice[i].delta[1];
			rawmice[i].delta[0] = rawmice[i].delta[1] = 0;
		}
	} else if( dinput_initialized ) {
		mx = 0;
		my = 0;

		for(;; ) {
			dwElements = 1;

			hr = IDirectInputDevice_GetDeviceData( g_pMouse,
												   sizeof( DIDEVICEOBJECTDATA ), &od, &dwElements, 0 );

			if( ( hr == DIERR_INPUTLOST ) || ( hr == DIERR_NOTACQUIRED ) ) {
				dinput_acquired = true;
				IDirectInputDevice_Acquire( g_pMouse );
				break;
			}

			// unable to read data or no data available
			if( FAILED( hr ) || dwElements == 0 ) {
				break;
			}

			sys_msg_time = od.dwTimeStamp;

			// look at the element to see what happened
			switch( od.dwOfs ) {
				case DIMOFS_X:
					mx += (int)od.dwData;
					break;

				case DIMOFS_Y:
					my += (int)od.dwData;
					break;

				case DIMOFS_Z:
					if( mouse_wheel_type != MWHEEL_WM ) {
						mouse_wheel_type = MWHEEL_DINPUT;
						if( (int)od.dwData > 0 ) {
							Key_Event( K_MWHEELUP, true, sys_msg_time );
							Key_Event( K_MWHEELUP, false, sys_msg_time );
						} else {
							Key_Event( K_MWHEELDOWN, true, sys_msg_time );
							Key_Event( K_MWHEELDOWN, false, sys_msg_time );
						}
					}
					break;

				case DIMOFS_BUTTON0:
				case DIMOFS_BUTTON1:
				case DIMOFS_BUTTON2:
				case DIMOFS_BUTTON3:
				case DIMOFS_BUTTON0 + 4:
				case DIMOFS_BUTTON0 + 5:
				case DIMOFS_BUTTON0 + 6:
				case DIMOFS_BUTTON0 + 7:
					if( od.dwData & 0x80 ) {
						mstate_di |= ( 1 << ( od.dwOfs - DIMOFS_BUTTON0 ) );
					} else {
						mstate_di &= ~( 1 << ( od.dwOfs - DIMOFS_BUTTON0 ) );
					}
					break;
			}
		}

		dinput_initialized = false; // FIXME...
		IN_MouseEvent( mstate_di );
		dinput_initialized = true;
	} else {
		// find mouse movement
		if( !GetCursorPos( &current_pos ) ) {
			return;
		}

		mx = current_pos.x - window_center_x;
		my = current_pos.y - window_center_y;

		// force the mouse to the center, so there's room to move
		if( mx || my ) {
			SetCursorPos( window_center_x, window_center_y );
		}
	}

	*dx = mx;
	*dy = my;
}

/*
* IN_Init
*/
void IN_Init( void ) {
	Com_Printf( "\n------- input initialization -------\n" );

	IN_StartupMouse();
	IN_XInput_Init();

	Com_Printf( "------------------------------------\n" );
}

/*
* IN_Shutdown
*/
void IN_Shutdown( void ) {
	IN_DeactivateMouse();

	in_showcursor = true;
	IN_ShowCursor();

	if( rawinput_initialized ) {
		IN_RawInput_Shutdown();
	} else if( dinput_initialized ) {
		IN_ShutdownDInput();
	}

	IN_XInput_Shutdown();

	dinput_acquired = dinput_initialized = false;
	rawinput_initialized = false;
}

/*
* IN_Restart
*/
void IN_Restart( void ) {
	IN_Shutdown();
	IN_Init();
}

/*
* IN_Activate
*
* Called when the main window gains or loses focus.
* The window may have been destroyed and recreated
* between a deactivate and an activate.
*/
void IN_Activate( bool active ) {
	in_appactive = active;
	mouseactive = !active;  // force a new window check or turn off
	if( !active && in_xinput_initialized ) {
		memset( &in_xinput_gamepad, 0, sizeof( in_xinput_gamepad ) );
	}
}

/*
* IN_Frame
*
* Called every frame, even if not generating commands
*/
void IN_Frame( void ) {
	bool showcursor;
	bool console = cls.key_dest == key_console;
	bool menu = cls.key_dest == key_menu;

	extern cvar_t *vid_fullscreen;

	if( !mouseinitialized ) {
		return;
	}

	// TODO: clean up this mess
	if( vid_fullscreen ) {
		if( !vid_fullscreen->integer ) {
			 if( !console && !menu ) {
				if( !in_appactive && ActiveApp ) {
					IN_Activate( true );
				}
			} else {
				if( in_appactive ) {
					IN_Activate( false );
				}
			}
		} else {
			if( ( !ActiveApp || menu ) && in_appactive ) {
				IN_Activate( false );
			} else if( ActiveApp && !menu && !in_appactive ) {
				IN_Activate( true );
			}
		}
	}

	showcursor = !in_appactive;
	if( cls.key_dest == key_menu && !cls.show_cursor ) {
		showcursor = false;
	}

	if( !in_appactive ) {
		IN_DeactivateMouse();
	} else {
		IN_ActivateMouse();
	}

	if( showcursor != in_showcursor ) {
		in_showcursor = showcursor;
		IN_ShowCursor();
	}
}

/*
* IN_GetMousePosition
*/
void IN_GetMousePosition( int *x, int *y ) {
	POINT p;

	if( !mouseinitialized ) {
		return;
	}

	if( !GetCursorPos( &p ) ) {
		return;
	}
	if( ScreenToClient( cl_hwnd, &p ) ) {
		*x = p.x;
		*y = p.y;
	}
}

/*
=========================================================================

JOYSTICK

=========================================================================
*/

/*
* IN_XInput_Init
*/
static void IN_XInput_Init( void ) {
	in_xinput_dll = LoadLibrary( "xinput1_4.dll" );
	if( !in_xinput_dll ) {
		in_xinput_dll = LoadLibrary( "xinput1_3.dll" );
	}
	if( !in_xinput_dll ) {
		in_xinput_dll = LoadLibrary( "xinput9_1_0.dll" );
	}
	if( !in_xinput_dll ) {
		Com_Printf( "XInput: Couldn't load XInput DLL\n" );
		return;
	}

	pXInputGetState = ( void * )GetProcAddress( in_xinput_dll, "XInputGetState" );
	if( !pXInputGetState ) {
		Com_Printf( "XInput: Couldn't load symbol XInputGetState\n" );
		FreeLibrary( in_xinput_dll );
		return;
	}

	in_xinput_initialized = true;
}

/*
* IN_XInput_Shutdown
*/
static void IN_XInput_Shutdown( void ) {
	if( !in_xinput_initialized ) {
		return;
	}

	memset( &in_xinput_gamepad, 0, sizeof( in_xinput_gamepad ) );
	FreeLibrary( in_xinput_dll );
	in_xinput_initialized = false;
}

/*
* IN_Commands
*/
void IN_Commands( void ) {
	int i;
	int buttons, buttonsOld, buttonsDiff;
	bool trigger, triggerOld;
	static bool notConnected;
	static int64_t lastConnectedCheck;

	if( in_xinput_initialized && in_appactive ) {
		XINPUT_STATE state;

		if( notConnected && ( ( Sys_Milliseconds() - lastConnectedCheck ) < 2000 ) ) {
			// gamepad not connected, and the previous null state has been applied already
			return;
		}

		if( pXInputGetState( 0, &state ) == ERROR_SUCCESS ) {
			notConnected = false;
			memcpy( &in_xinput_gamepad, &( state.Gamepad ), sizeof( in_xinput_gamepad ) );
		} else {
			notConnected = true;
			lastConnectedCheck = Sys_Milliseconds();
			memset( &in_xinput_gamepad, 0, sizeof( in_xinput_gamepad ) );
		}
	}

	buttons = in_xinput_gamepad.wButtons;
	buttonsOld = in_xinput_oldGamepad.wButtons;
	buttonsDiff = buttons ^ buttonsOld;

	if( buttonsDiff ) {
		const int keys[] =
		{
			K_DPAD_UP, K_DPAD_DOWN, K_DPAD_LEFT, K_DPAD_RIGHT, 0, 0,
			K_LSTICK, K_RSTICK, K_LSHOULDER, K_RSHOULDER, 0, 0,
			K_A_BUTTON, K_B_BUTTON, K_X_BUTTON, K_Y_BUTTON
		};

		int64_t time = Sys_Milliseconds();

		for( i = 0; i < ( sizeof( keys ) / sizeof( keys[0] ) ); i++ ) {
			if( !keys[i] ) {
				continue;
			}

			if( buttonsDiff & ( 1 << i ) ) {
				Key_Event( keys[i], ( buttons & ( 1 << i ) ) ? true : false, time );
			}
		}

		if( buttonsDiff & ( XINPUT_GAMEPAD_START | XINPUT_GAMEPAD_BACK ) ) {
			if( !( buttonsOld & ( XINPUT_GAMEPAD_START | XINPUT_GAMEPAD_BACK ) ) ) {
				Key_Event( K_ESCAPE, true, time );
			} else if( !( buttons & ( XINPUT_GAMEPAD_START | XINPUT_GAMEPAD_BACK ) ) ) {
				Key_Event( K_ESCAPE, false, time );
			}
		}
	}

	trigger = ( in_xinput_gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD );
	triggerOld = ( in_xinput_oldGamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD );
	if( trigger != triggerOld ) {
		Key_Event( K_LTRIGGER, trigger, Sys_Milliseconds() );
	}

	trigger = ( in_xinput_gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD );
	triggerOld = ( in_xinput_oldGamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD );
	if( trigger != triggerOld ) {
		Key_Event( K_RTRIGGER, trigger, Sys_Milliseconds() );
	}

	memcpy( &in_xinput_oldGamepad, &in_xinput_gamepad, sizeof( in_xinput_gamepad ) );
}

/*
* IN_XInput_AxisValue
*/
static float IN_XInput_ThumbstickValue( int value ) {
	return value * ( ( value >= 0 ) ? ( 1.0f / 32767.0f ) : ( 1.0f / 32768.0f ) );
}

/*
* IN_GetThumbsticks
*/
void IN_GetThumbsticks( vec4_t sticks ) {
	sticks[0] = IN_XInput_ThumbstickValue( in_xinput_gamepad.sThumbLX );
	sticks[1] = -IN_XInput_ThumbstickValue( in_xinput_gamepad.sThumbLY );
	sticks[2] = IN_XInput_ThumbstickValue( in_xinput_gamepad.sThumbRX );
	sticks[3] = -IN_XInput_ThumbstickValue( in_xinput_gamepad.sThumbRY );
}

/*
* IN_SupportedDevices
*/
unsigned int IN_SupportedDevices( void ) {
	return IN_DEVICE_KEYBOARD | IN_DEVICE_MOUSE | IN_DEVICE_JOYSTICK;
}

/*
* IN_ShowSoftKeyboard
*/
void IN_ShowSoftKeyboard( bool show ) {
}

/*
=========================================================================

INPUT METHOD EDITORS

=========================================================================
*/

static HINSTANCE in_winime_dll;
static bool in_winime_initialized;

static HIMC in_winime_context;

static bool in_winime_enabled;

static CANDIDATELIST *in_winime_candList;
static size_t in_winime_candListSize;

static HIMC( WINAPI * qimmAssociateContext )( HWND hWnd, HIMC hIMC );
static HIMC( WINAPI * qimmCreateContext )( void );
static BOOL( WINAPI * qimmDestroyContext )( HIMC hIMC );
static DWORD( WINAPI * qimmGetCandidateList )( HIMC hIMC, DWORD dwIndex, LPCANDIDATELIST lpCandList, DWORD dwBufLen );
static LONG( WINAPI * qimmGetCompositionString )( HIMC hIMC, DWORD dwIndex, LPVOID lpBuf, DWORD dwBufLen );
static BOOL( WINAPI * qimmGetConversionStatus )( HIMC hIMC, LPDWORD lpfdwConversion, LPDWORD lpfdwSentence );
static DWORD( WINAPI * qimmGetProperty )( HKL hKL, DWORD fdwIndex );
static BOOL( WINAPI * qimmNotifyIME )( HIMC hIMC, DWORD dwAction, DWORD dwIndex, DWORD dwValue );

/*
* In_WinIME_GPA
*/
void *In_WinIME_GPA( const char *name ) {
	void *p = GetProcAddress( in_winime_dll, name );

	if( !p ) {
		Com_Printf( "IME: Couldn't load symbol: %s\n", name );
		in_winime_initialized = false;
	}

	return p;
}

/*
* IN_WinIME_Init
*/
void IN_WinIME_Init( void ) {
	in_winime_dll = LoadLibrary( "imm32.dll" );
	if( !in_winime_dll ) {
		Com_Printf( "IME: Couldn't load imm32.dll\n" );
		return;
	}

	in_winime_initialized = true;
	qimmAssociateContext = In_WinIME_GPA( "ImmAssociateContext" );
	qimmCreateContext = In_WinIME_GPA( "ImmCreateContext" );
	qimmDestroyContext = In_WinIME_GPA( "ImmDestroyContext" );
	qimmGetCandidateList = In_WinIME_GPA( "ImmGetCandidateListW" );
	qimmGetCompositionString = In_WinIME_GPA( "ImmGetCompositionStringW" );
	qimmGetConversionStatus = In_WinIME_GPA( "ImmGetConversionStatus" );
	qimmGetProperty = In_WinIME_GPA( "ImmGetProperty" );
	qimmNotifyIME = In_WinIME_GPA( "ImmNotifyIME" );
	if( !in_winime_initialized ) {
		IN_WinIME_Shutdown();
		return;
	}

	in_winime_context = qimmCreateContext();
	if( !in_winime_context ) {
		Com_Printf( "IME: Couldn't create an input context\n" );
		IN_WinIME_Shutdown();
		return;
	}
}

/*
* IN_WinIME_AssociateContext
*/
void IN_WinIME_AssociateContext( void ) {
	if( !in_winime_initialized || !cl_hwnd ) {
		return;
	}

	qimmAssociateContext( cl_hwnd, in_winime_enabled ? in_winime_context : NULL );
}

/*
* IN_WinIME_Shutdown
*/
void IN_WinIME_Shutdown( void ) {
	in_winime_enabled = false;

	if( in_winime_candList ) {
		Q_free( in_winime_candList );
		in_winime_candList = NULL;
		in_winime_candListSize = 0;
	}

	if( in_winime_context ) {
		qimmDestroyContext( in_winime_context );
		in_winime_context = NULL;
	}

	if( in_winime_dll ) {
		FreeLibrary( in_winime_dll );
		in_winime_dll = NULL;
	}

	in_winime_initialized = false;
}

/*
* IN_IME_Enable
*/
void IN_IME_Enable( bool enable ) {
	if( !in_winime_initialized || ( in_winime_enabled == enable ) ) {
		return;
	}

	in_winime_enabled = enable;
	qimmNotifyIME( in_winime_context, NI_COMPOSITIONSTR, CPS_CANCEL, 0 );
	IN_WinIME_AssociateContext();
}

/*
* IN_IME_GetComposition
*/
#define IN_WINIME_COMPSTR_LENGTH 100 // max length for the Japanese IME, but it's likely the same or less for Chinese
size_t IN_IME_GetComposition( char *str, size_t strSize, size_t *cursorPos, size_t *convStart, size_t *convLen ) {
	WCHAR compStr[IN_WINIME_COMPSTR_LENGTH + 1];
	char compAttr[IN_WINIME_COMPSTR_LENGTH + 1];
	int len, attrLen, i, cursor, attr, start = -1;
	size_t cursorutf = 0, startutf = 0, convutflen = 0, ret = 0;

	if( !strSize ) {
		str = NULL;
	}

	if( str ) {
		str[0] = '\0';
	}
	if( cursorPos ) {
		*cursorPos = 0;
	}
	if( convStart ) {
		*convStart = 0;
	}
	if( convLen ) {
		*convLen = 0;
	}

	if( !in_winime_enabled ) {
		return 0;
	}

	len = qimmGetCompositionString( in_winime_context, GCS_COMPSTR, compStr, sizeof( compStr ) ) / sizeof( WCHAR );
	if( len <= 0 ) {
		return 0;
	}

	compStr[len] = 0;
	if( str ) {
		ret = Q_WCharToUtf8String( compStr, str, strSize );
	} else {
		for( i = 0; i < len; i++ )
			ret += Q_WCharUtf8Length( compStr[i] );
	}

	if( cursorPos ) {
		cursor = LOWORD( qimmGetCompositionString( in_winime_context, GCS_CURSORPOS, NULL, 0 ) );
		for( i = 0; ( i < cursor ) && ( i < len ); i++ )
			cursorutf += Q_WCharUtf8Length( compStr[i] );
		clamp_high( cursorutf, ret );
		*cursorPos = cursorutf;
	}

	if( convStart || convLen ) {
		attrLen = qimmGetCompositionString( in_winime_context, GCS_COMPATTR, compAttr, sizeof( compAttr ) );
		if( attrLen == len ) {
			for( i = 0; i < attrLen; i++ ) {
				attr = compAttr[i];
				if( ( attr == ATTR_TARGET_CONVERTED ) || ( attr == ATTR_TARGET_NOTCONVERTED ) ) {
					if( start < 0 ) {
						start = startutf;
					}
					convutflen += Q_WCharUtf8Length( compStr[i] );
				} else {
					if( start >= 0 ) {
						break;
					}
					startutf += Q_WCharUtf8Length( compStr[i] );
				}
			}

			if( start >= 0 ) {
				if( start > ( int )ret ) {
					start = ret;
				}
				if( ( start + convutflen ) > ( int )ret ) {
					convutflen = ret - start;
				}
				if( convStart ) {
					*convStart = start;
				}
				if( convLen ) {
					*convLen = convutflen;
				}
			}
		}
	}

	return ret;
}

/*
* IN_IME_GetCandidates
*/
unsigned int IN_IME_GetCandidates( char * const *cands, size_t candSize, unsigned int maxCands, int *selected, int *firstKey ) {
	size_t candListSize;
	CANDIDATELIST *candList = in_winime_candList;
	unsigned int i;

	if( selected ) {
		*selected = -1;
	}
	if( firstKey ) {
		*firstKey = 1;
	}

	if( !in_winime_enabled ) {
		return 0;
	}

	candListSize = qimmGetCandidateList( in_winime_context, 0, NULL, 0 );
	if( !candListSize ) {
		return 0;
	}

	if( candListSize > in_winime_candListSize ) {
		candList = Q_realloc( candList, candListSize );
		if( !candList ) {
			return 0;
		}
		in_winime_candList = candList;
		in_winime_candListSize = candListSize;
	}

	if( qimmGetCandidateList( in_winime_context, 0, candList, candListSize ) != candListSize ) {
		return 0;
	}

	clamp_high( maxCands, candList->dwPageSize );
	if( ( candList->dwPageStart + maxCands ) > candList->dwCount ) {
		maxCands = candList->dwCount - candList->dwPageStart;
	}

	if( cands && candSize ) {
		for( i = 0; i < maxCands; i++ )
			Q_WCharToUtf8String( ( const WCHAR * )( ( const char * )candList + candList->dwOffset[candList->dwPageStart + i] ), cands[i], candSize );
	}

	if( selected && ( candList->dwSelection >= candList->dwPageStart ) && ( candList->dwSelection < ( candList->dwPageStart + maxCands ) ) ) {
		*selected = ( int )candList->dwSelection - ( int )candList->dwPageStart;
	}

	if( firstKey ) {
		if( !( qimmGetProperty( GetKeyboardLayout( 0 ), IGP_PROPERTY ) & IME_PROP_CANDLIST_START_FROM_1 ) ) {
			*firstKey = 0;
		}
	}

	return maxCands;
}

/*
* IN_GetInputLanguage
*/
void IN_GetInputLanguage( char *dest, size_t size ) {
	WORD langid = LOWORD( GetKeyboardLayout( 0 ) );
	char lang[16];

	lang[0] = '\0';

	GetLocaleInfo(
		MAKELCID( langid, SORT_DEFAULT ),
		LOCALE_SISO639LANGNAME,
		lang, sizeof( lang ) );
	Q_strupr( lang );

	if( ( ( langid & 0xff ) == LANG_JAPANESE ) && in_winime_enabled ) {
		DWORD mode;
		if( qimmGetConversionStatus( in_winime_context, &mode, NULL ) ) {
			const char *modeStr = " A";
			if( mode & IME_CMODE_NATIVE ) {
				if( mode & IME_CMODE_KATAKANA ) {
					if( mode & IME_CMODE_FULLSHAPE ) {
						modeStr = " \xe3\x82\xab";
					} else {
						modeStr = " \xef\xbd\xb6";
					}
				} else {
					modeStr = " \xe3\x81\x82";
				}
			} else if( mode & IME_CMODE_FULLSHAPE ) {
				modeStr = " \xef\xbc\xa1";
			}

			if( ( strlen( lang ) + 4 ) < sizeof( lang ) ) {
				strcat( lang, modeStr );
			}
		}
	}

	Q_strncpyz( dest, lang, size );
}
