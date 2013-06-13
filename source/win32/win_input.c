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
#define	DIRECTINPUT_VERSION 0x0700 // Could use dx9, but older is more frequently used
//#else
//#define	DIRECTINPUT_VERSION 0x0800
//#endif

#include <dinput.h>

#define DINPUT_BUFFERSIZE           64 // http://www.esreality.com/?a=post&id=905276#pid905330
#define iDirectInputCreate( a, b, c, d ) pDirectInputCreate( a, b, c, d )

static HRESULT ( WINAPI *pDirectInputCreate )( HINSTANCE hinst, DWORD dwVersion,
									   LPDIRECTINPUT * lplpDirectInput, LPUNKNOWN punkOuter );

// raw input specific defines
#define MAX_RI_DEVICE_SIZE 128
#define INIT_RIBUFFER_SIZE (sizeof(RAWINPUTHEADER)+sizeof(RAWMOUSE))

#define RI_RAWBUTTON_MASK 0x000003E0
#define RI_INVALID_POS    0x80000000

// raw input dynamic functions
typedef int (WINAPI *pGetRawInputDeviceList)		(OUT PRAWINPUTDEVICELIST pRawInputDeviceList, IN OUT PINT puiNumDevices, IN UINT cbSize);
typedef int (WINAPI *pGetRawInputData)				(IN HRAWINPUT hRawInput, IN UINT uiCommand, OUT LPVOID pData, IN OUT PINT pcbSize, IN UINT cbSizeHeader);
typedef int (WINAPI *pGetRawInputDeviceInfoA)		(IN HANDLE hDevice, IN UINT uiCommand, OUT LPVOID pData, IN OUT PINT pcbSize);
typedef BOOL (WINAPI *pRegisterRawInputDevices)		(IN PCRAWINPUTDEVICE pRawInputDevices, IN UINT uiNumDevices, IN UINT cbSize);

pGetRawInputDeviceList		_GRIDL;
pGetRawInputData			_GRID;
pGetRawInputDeviceInfoA		_GRIDIA;
pRegisterRawInputDevices	_RRID;

typedef struct
{
	HANDLE			rawinputhandle; // raw input, identify particular mice

	int				numbuttons;
	volatile int	buttons;

	volatile int	delta[2];
	int				pos[2];
} rawmouse_t;

static rawmouse_t	*rawmice = NULL;
static int			rawmicecount = 0;
static RAWINPUT		*raw = NULL;
static int			ribuffersize = 0;
static qboolean		rawinput_initialized = qfalse;

static qboolean	IN_RawInput_Init( void );
static void		IN_RawInput_Shutdown( void );
static int		IN_RawInput_Register( void );
static void		IN_RawInput_DeRegister( void );

extern unsigned	sys_msg_time;

// joystick defines and variables
// where should defines be moved?
#define JOY_ABSOLUTE_AXIS   0x00000000      // control like a joystick
#define JOY_RELATIVE_AXIS   0x00000010      // control like a mouse, spinner, trackball
#define	JOY_MAX_AXES	    6               // X, Y, Z, R, U, V
#define JOY_AXIS_X	    0
#define JOY_AXIS_Y	    1
#define JOY_AXIS_Z	    2
#define JOY_AXIS_R	    3
#define JOY_AXIS_U	    4
#define JOY_AXIS_V	    5

enum _ControlList
{
	AxisNada = 0, AxisForward, AxisLook, AxisSide, AxisTurn, AxisUp
};

DWORD dwAxisFlags[JOY_MAX_AXES] =
{
	JOY_RETURNX, JOY_RETURNY, JOY_RETURNZ, JOY_RETURNR, JOY_RETURNU, JOY_RETURNV
};

DWORD dwAxisMap[JOY_MAX_AXES];
DWORD dwControlMap[JOY_MAX_AXES];
PDWORD pdwRawValue[JOY_MAX_AXES];

cvar_t *in_mouse;
cvar_t *in_grabinconsole;
cvar_t *in_joystick;


// none of these cvars are saved over a session
// this means that advanced controller configuration needs to be executed
// each time.  this avoids any problems with getting back to a default usage
// or when changing from one controller to another.  this way at least something
// works.
cvar_t *joy_name;
cvar_t *joy_advanced;
cvar_t *joy_advaxisx;
cvar_t *joy_advaxisy;
cvar_t *joy_advaxisz;
cvar_t *joy_advaxisr;
cvar_t *joy_advaxisu;
cvar_t *joy_advaxisv;
cvar_t *joy_forwardthreshold;
cvar_t *joy_sidethreshold;
cvar_t *joy_pitchthreshold;
cvar_t *joy_yawthreshold;
cvar_t *joy_forwardsensitivity;
cvar_t *joy_sidesensitivity;
cvar_t *joy_pitchsensitivity;
cvar_t *joy_yawsensitivity;
cvar_t *joy_upthreshold;
cvar_t *joy_upsensitivity;
cvar_t *joy_freelook;
cvar_t *joy_lookspring;
cvar_t *joy_lookstrafe;

static qboolean	jlooking = qfalse;
qboolean joy_avail, joy_advancedinit, joy_haspov;
DWORD joy_oldbuttonstate, joy_oldpovstate;

int joy_id;
DWORD joy_flags;
DWORD joy_numbuttons;

static JOYINFOEX ji;

qboolean in_appactive;

// forward-referenced functions
static void IN_StartupJoystick( void );
static void Joy_AdvancedUpdate_f( void );

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
static qboolean	mouseactive;    // qfalse when not focus app
static qboolean	restore_spi;
static qboolean	mouseinitialized;
static int originalmouseparms[3], newmouseparms[3] = { 0, 0, 0 };
static qboolean	mouseparmsvalid;
static unsigned int mstate_di;

static int window_center_x, window_center_y;
static RECT window_rect;

static LPDIRECTINPUT g_pdi;
static LPDIRECTINPUTDEVICE g_pMouse;

static HINSTANCE hInstDI;

static qboolean	dinput_initialized;
static qboolean	dinput_acquired;

typedef struct MYDATA
{
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
	{ &GUID_XAxis, FIELD_OFFSET( MYDATA, lX ), DIDFT_AXIS | DIDFT_ANYINSTANCE, 0, },
	{ &GUID_YAxis, FIELD_OFFSET( MYDATA, lY ), DIDFT_AXIS | DIDFT_ANYINSTANCE, 0, },
	{ &GUID_ZAxis, FIELD_OFFSET( MYDATA, lZ ), 0x80000000 | DIDFT_AXIS | DIDFT_ANYINSTANCE, 0, },
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

/*
* IN_ActivateMouse
* 
* Called when the window gains focus or changes in some way
*/
static void IN_ActivateMouse( void )
{
	int width, height;

	if( !mouseinitialized )
		return;
	if( !in_mouse->integer )
	{
		mouseactive = qfalse;
		return;
	}
	if( mouseactive )
		return;

	mouseactive = qtrue;

	if( dinput_initialized )
	{
		mstate_di = 0;
		if( g_pMouse )
		{
			if( cl_hwnd )
				if( FAILED( IDirectInputDevice_SetCooperativeLevel( g_pMouse, cl_hwnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND ) ) )
				{
					Com_DPrintf( "Couldn't set DI coop level\n" );
					return;
				}
				if( !dinput_acquired )
				{
					IDirectInputDevice_Acquire( g_pMouse );
					dinput_acquired = qtrue;
				}
		}
		return;
	}

	if( rawinput_initialized )
	{
		if( IN_RawInput_Register() )
		{
			Com_Printf( "Raw input: unable to register raw input, deinitializing\n" );
			IN_RawInput_Shutdown();
		}
	}

	mouse_oldbuttonstate = 0;

	if( mouseparmsvalid )
		restore_spi = SystemParametersInfo( SPI_SETMOUSE, 0, newmouseparms, 0 );

	width = GetSystemMetrics( SM_CXSCREEN );
	height = GetSystemMetrics( SM_CYSCREEN );

	GetWindowRect( cl_hwnd, &window_rect );
	if( window_rect.left < 0 )
		window_rect.left = 0;
	if( window_rect.top < 0 )
		window_rect.top = 0;
	if( window_rect.right >= width )
		window_rect.right = width-1;
	if( window_rect.bottom >= height-1 )
		window_rect.bottom = height-1;

	window_center_x = ( window_rect.right + window_rect.left )/2;
	window_center_y = ( window_rect.top + window_rect.bottom )/2;

	SetCursorPos( window_center_x, window_center_y );

	SetCapture( cl_hwnd );
	ClipCursor( &window_rect );
	while( ShowCursor( FALSE ) >= 0 ) ;
}


/*
* IN_DeactivateMouse
* 
* Called when the window loses focus
*/
static void IN_DeactivateMouse( void )
{
	if( !mouseinitialized )
		return;
	if( !mouseactive )
		return;

	mouseactive = qfalse;

	if( dinput_initialized )
	{
		if( g_pMouse )
		{
			if( dinput_acquired )
			{
				IDirectInputDevice_Unacquire( g_pMouse );
				dinput_acquired = qfalse;
			}
			if( cl_hwnd )
				if( FAILED( IDirectInputDevice_SetCooperativeLevel( g_pMouse, cl_hwnd, DISCL_EXCLUSIVE | DISCL_BACKGROUND ) ) )
				{
					Com_DPrintf( "Couldn't set DI coop level\n" );
					return;
				}
		}
		return;
	}

	if( rawinput_initialized > 0 )
		IN_RawInput_DeRegister();

	if( restore_spi )
		SystemParametersInfo( SPI_SETMOUSE, 0, originalmouseparms, 0 );

	ClipCursor( NULL );
	ReleaseCapture();
	while( ShowCursor( TRUE ) < 0 ) ;
}


/*
* IN_InitDInput
*/
static qboolean IN_InitDInput( void )
{
	HRESULT	hr;
	DIPROPDWORD dipdw = {
		{
			sizeof( DIPROPDWORD ), // diph.dwSize
				sizeof( DIPROPHEADER ), // diph.dwHeaderSize
				0,              // diph.dwObj
				DIPH_DEVICE,    // diph.dwHow
		},
		DINPUT_BUFFERSIZE,      // dwData
	};

	if( !hInstDI )
	{
		hInstDI = LoadLibrary( "dinput.dll" );

		if( hInstDI == NULL )
		{
			Com_Printf( "Couldn't load dinput.dll\n" );
			return qfalse;
		}
	}

	if( !pDirectInputCreate )
	{
		pDirectInputCreate = (void *)GetProcAddress( hInstDI, "DirectInputCreateA" );

		if( !pDirectInputCreate )
		{
			Com_Printf( "Couldn't get DI proc addr\n" );
			return qfalse;
		}
	}

	// register with DirectInput and get an IDirectInput to play with
	hr = iDirectInputCreate( global_hInstance, DIRECTINPUT_VERSION, &g_pdi, NULL );

	if( FAILED( hr ) )
	{
		Com_Printf( "DirectInputCreate failed\n" );
		return qfalse;
	}

	// obtain an interface to the system mouse device
	hr = IDirectInput_CreateDevice( g_pdi, &GUID_SysMouse, &g_pMouse, NULL );

	if( FAILED( hr ) )
	{
		Com_Printf( "Couldn't open DI mouse device\n" );
		return qfalse;
	}

	// set the data format to "mouse format"
	hr = IDirectInputDevice_SetDataFormat( g_pMouse, &df );

	if( FAILED( hr ) )
	{
		Com_Printf( "Couldn't set DI mouse format\n" );
		return qfalse;
	}

	// set the cooperativity level
	hr = IDirectInputDevice_SetCooperativeLevel( g_pMouse, cl_hwnd,
		DISCL_EXCLUSIVE | DISCL_FOREGROUND );

	if( FAILED( hr ) )
	{
		Com_DPrintf( "Couldn't set DI coop level\n" );
		return qfalse;
	}

	// set the buffer size to DINPUT_BUFFERSIZE elements
	// the buffer size is a DWORD property associated with the device
	hr = IDirectInputDevice_SetProperty( g_pMouse, DIPROP_BUFFERSIZE, &dipdw.diph );

	if( FAILED( hr ) )
	{
		Com_DPrintf( "Couldn't set DI buffersize\n" );
		return qfalse;
	}

	return qtrue;
}

/*
* IN_ShutdownDInput
*/
static void IN_ShutdownDInput( void )
{
	if( g_pMouse )
	{
		IDirectInputDevice_SetCooperativeLevel( g_pMouse, cl_hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND );
		IDirectInputDevice_Release( g_pMouse );
	}

	if( g_pdi )
		IDirectInput_Release( g_pdi );

	if( hInstDI )
		FreeLibrary( hInstDI );

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
int IN_RawInput_Register(void)
{
	// This function registers to receive the WM_INPUT messages
	RAWINPUTDEVICE Rid; // Register only for mouse messages from WM_INPUT

	// register to get wm_input messages
	Rid.usUsagePage = 0x01;
	Rid.usUsage = 0x02;
	Rid.dwFlags = RIDEV_NOLEGACY; // adds HID mouse and also ignores legacy mouse messages
	Rid.hwndTarget = NULL;

	// Register to receive the WM_INPUT message for any change in mouse (buttons, wheel, and movement will all generate the same message)
	if( !(*_RRID)(&Rid, 1, sizeof( Rid ) ) )
		return 1;
	return 0;
}

/*
* IN_RawInput_DeRegister
*/
static void IN_RawInput_DeRegister( void )
{
	RAWINPUTDEVICE Rid;

	// deregister raw input
	Rid.usUsagePage = 0x01;
	Rid.usUsage = 0x02;
	Rid.dwFlags = RIDEV_REMOVE;
	Rid.hwndTarget = NULL;

	(*_RRID)( &Rid, 1, sizeof( Rid ) );
}

/*
* IN_RawInput_IsRDPMouse
*/
int IN_RawInput_IsRDPMouse( const char *cDeviceString )
{
	const char cRDPString[] = "\\??\\Root#RDP_MOU#";
	int i;

	if( strlen( cDeviceString ) < strlen( cRDPString ) )
		return 0;

	for( i = strlen(cRDPString) - 1; i >= 0; i-- )
	{
		if( cDeviceString[i] != cRDPString[i] )
			return 0;
	}
	return 1; // is RDP mouse
}

/*
* IN_RawInput_Init
*
* Returns qfalse if rawinput is not available
*/
qboolean IN_RawInput_Init( void )
{
	int inputdevices, i, j, mtemp;
	PRAWINPUTDEVICELIST pRawInputDeviceList;
	char dname[MAX_RI_DEVICE_SIZE];
	HMODULE user32 = LoadLibrary( "user32.dll" );

	_GRIDL			= NULL;
	_GRID			= NULL;
	_GRIDIA			= NULL;
	_RRID			= NULL;

	rawmice			= NULL;
	rawmicecount	= 0;
	raw				= NULL;
	ribuffersize	= 0;

	if( !user32 )
	{
		Com_Printf( "Raw input: unable to load user32.dll\n" );
		return qfalse;
	}

	if( !(_RRID = ( pRegisterRawInputDevices )GetProcAddress( user32, "RegisterRawInputDevices" )) )
	{
		Com_Printf( "Raw input: function RegisterRawInputDevices could not be registered\n" );
		return qfalse;
	}

	if( !(_GRIDL = ( pGetRawInputDeviceList )GetProcAddress( user32, "GetRawInputDeviceList" )) )
	{
		Com_Printf( "Raw input: function GetRawInputDeviceList could not be registered\n" );
		return qfalse;
	}

	if( !(_GRIDIA = ( pGetRawInputDeviceInfoA )GetProcAddress( user32, "GetRawInputDeviceInfoA" )) )
	{
		Com_Printf( "Raw input: function GetRawInputDeviceInfoA could not be registered\n" );
		return qfalse;
	}

	if( !(_GRID = ( pGetRawInputData )GetProcAddress( user32, "GetRawInputData" )) )
	{
		Com_Printf( "Raw input: function GetRawInputData could not be registered\n" );
		return qfalse;
	}

	// 1st call to GetRawInputDeviceList: Pass NULL to get the number of devices.
	if( (*_GRIDL)( NULL, &inputdevices, sizeof( RAWINPUTDEVICELIST ) ) != 0 )
	{
		Com_Printf( "Raw input: unable to count raw input devices\n" );
		return qfalse;
	}

	// Allocate the array to hold the DeviceList
	pRawInputDeviceList = Mem_ZoneMalloc( sizeof( RAWINPUTDEVICELIST ) * inputdevices );

	// 2nd call to GetRawInputDeviceList: Pass the pointer to our DeviceList and GetRawInputDeviceList() will fill the array
	if( (*_GRIDL)( pRawInputDeviceList, &inputdevices, sizeof( RAWINPUTDEVICELIST ) ) == -1 )
	{
		Com_Printf( "Raw input: unable to get raw input device list\n" );
		return qfalse;
	}

	// Loop through all devices and count the mice
	for( i = 0, mtemp = 0; i < inputdevices; i++ )
	{
		if( pRawInputDeviceList[i].dwType == RIM_TYPEMOUSE )
		{
			j = MAX_RI_DEVICE_SIZE;

			// Get the device name and use it to determine if it's the RDP Terminal Services virtual device.
			if( (*_GRIDIA)( pRawInputDeviceList[i].hDevice, RIDI_DEVICENAME, dname, &j ) < 0 )
				dname[0] = 0;

			if( IN_RawInput_IsRDPMouse(dname) ) // ignore rdp mouse
				continue;

			// advance temp device count
			mtemp++;
		}
	}

	// exit out if no devices found
	if( !mtemp )
	{
		Com_Printf( "Raw input: no usable device found\n" );
		return qfalse;
	}

	// Loop again and bind devices
	rawmice = Mem_ZoneMalloc( sizeof( rawmouse_t ) * mtemp );
	for( i = 0; i < inputdevices; i++ )
	{
		if( pRawInputDeviceList[i].dwType == RIM_TYPEMOUSE )
		{
			j = MAX_RI_DEVICE_SIZE;

			// Get the device name and use it to determine if it's the RDP Terminal Services virtual device.
			if( (*_GRIDIA)( pRawInputDeviceList[i].hDevice, RIDI_DEVICENAME, dname, &j ) < 0 )
				dname[0] = 0;

			if( IN_RawInput_IsRDPMouse( dname ) ) // ignore rdp mouse
				continue;

			// print pretty message about the mouse
			dname[MAX_RI_DEVICE_SIZE - 1] = 0;
			for (mtemp = strlen(dname); mtemp >= 0; mtemp--)
			{
				if (dname[mtemp] == '#')
				{
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

	return qtrue;
}

/*
* IN_RawInput_Shutdown
*/
static void IN_RawInput_Shutdown( void )
{
	if( rawmicecount < 1 )
		return;

	IN_RawInput_DeRegister();

	Mem_ZoneFree( rawmice );
	Mem_ZoneFree( raw );

	// dealloc mouse structure
	rawmicecount = 0;
}

/*
* IN_RawInput_MouseRead
*/
void IN_RawInput_MouseRead( HANDLE in_device_handle )
{
	int i = 0, tbuttons, j;
	int dwSize;

	if( !raw || !rawmice || rawmicecount < 1 )
		return; // no thx

	// get raw input
	if( (*_GRID)((HRAWINPUT)in_device_handle, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER ) ) == -1 )
	{
		Com_Printf( "Raw input: unable to add to get size of raw input header.\n" );
		return;
	}

	if( dwSize > ribuffersize )
	{
		ribuffersize = dwSize;
		raw = Mem_Realloc( raw, dwSize );
	}

	if( (*_GRID)( (HRAWINPUT)in_device_handle, RID_INPUT, raw, &dwSize, sizeof( RAWINPUTHEADER ) ) != dwSize )
	{
		Com_Printf( "Raw input: unable to add to get raw input header.\n" );
		return;
	}

	// find mouse in our mouse list
	for( ; i < rawmicecount; i++ )
	{
		if( rawmice[i].rawinputhandle == raw->header.hDevice )
			break;
	}

	if( i == rawmicecount ) // we're not tracking this mouse
		return;

	// movement
	if( raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE )
	{
		if( rawmice[i].pos[0] != RI_INVALID_POS )
		{
			rawmice[i].delta[0] += raw->data.mouse.lLastX - rawmice[i].pos[0];
			rawmice[i].delta[1] += raw->data.mouse.lLastY - rawmice[i].pos[1];
		}
		rawmice[i].pos[0] = raw->data.mouse.lLastX;
		rawmice[i].pos[1] = raw->data.mouse.lLastY;
	}
	else // RELATIVE
	{
		rawmice[i].delta[0] += raw->data.mouse.lLastX;
		rawmice[i].delta[1] += raw->data.mouse.lLastY;
		rawmice[i].pos[0] = RI_INVALID_POS;
	}

	// buttons
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN )
		Key_MouseEvent( K_MOUSE1, qtrue, sys_msg_time );
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_UP )
		Key_MouseEvent( K_MOUSE1, qfalse, sys_msg_time );
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN )
		Key_MouseEvent( K_MOUSE2, qtrue, sys_msg_time );
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_UP )
		Key_MouseEvent( K_MOUSE2, qfalse, sys_msg_time );
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN )
		Key_MouseEvent( K_MOUSE3, qtrue, sys_msg_time );
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_UP )
		Key_MouseEvent( K_MOUSE3, qfalse, sys_msg_time );
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN )
		Key_MouseEvent( K_MOUSE4, qtrue, sys_msg_time );
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP )
		Key_MouseEvent( K_MOUSE4, qfalse, sys_msg_time );
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN )
		Key_MouseEvent( K_MOUSE5, qtrue, sys_msg_time );
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP )
		Key_MouseEvent( K_MOUSE5, qfalse, sys_msg_time );

	// mouse wheel
	if( raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL )
	{
		// if the current message has a mouse_wheel message
		if( (SHORT)raw->data.mouse.usButtonData > 0 )
		{
			Key_Event(K_MWHEELUP, qtrue, sys_msg_time);
			Key_Event(K_MWHEELUP, qfalse, sys_msg_time);
		}

		if( (SHORT)raw->data.mouse.usButtonData < 0 )
		{
			Key_Event(K_MWHEELDOWN, qtrue, sys_msg_time);
			Key_Event(K_MWHEELDOWN, qfalse, sys_msg_time);
		}
	}

	// extra buttons
	tbuttons = raw->data.mouse.ulRawButtons & RI_RAWBUTTON_MASK;
	for( j = 6; j < rawmice[i].numbuttons; j++ )
	{
		if ( (tbuttons & (1<<j)) && !(rawmice[i].buttons & (1<<j)) )
			Key_MouseEvent (K_MOUSE1 + j, qtrue, sys_msg_time);

		if ( !(tbuttons & (1<<j)) && (rawmice[i].buttons & (1<<j)) )
			Key_MouseEvent (K_MOUSE1 + j, qfalse, sys_msg_time);
	}

	rawmice[i].buttons &= ~RI_RAWBUTTON_MASK;
	rawmice[i].buttons |= tbuttons;
}

/*
* IN_StartupMouse
*/
static void IN_StartupMouse( void )
{
	cvar_t *cv;

	cv = Cvar_Get( "in_initmouse", "1", CVAR_NOSET );
	if( !cv->integer )
		return;

	dinput_initialized = qfalse;
	rawinput_initialized = qfalse;

	cv = Cvar_Get( "m_raw", "0", CVAR_ARCHIVE );
	if( cv->integer )
		rawinput_initialized = IN_RawInput_Init();

	if( !rawinput_initialized )
	{
		cv = Cvar_Get( "in_dinput", "1", CVAR_ARCHIVE );
		if( cv->integer )
			dinput_initialized = IN_InitDInput();
	}

	if( rawinput_initialized )
		Com_Printf( "Raw input initialized with %i mice\n", rawmicecount );
	else if( dinput_initialized )
		Com_Printf( "DirectInput initialized\n" );
	else
		Com_Printf( "DirectInput not initialized, using standard input\n" );

	mouseinitialized = qtrue;
	mouseparmsvalid = SystemParametersInfo( SPI_GETMOUSE, 0, originalmouseparms, 0 );
	mouse_buttons = 8;
	mouse_wheel_type = MWHEEL_UNKNOWN;
}

/*
* IN_MouseEvent
*/
void IN_MouseEvent( int mstate )
{
	int i;

	if( !mouseinitialized || dinput_initialized )
		return;
	if( ( cls.key_dest == key_console ) && !in_grabinconsole->integer )
		return;

	// perform button actions
	for( i = 0; i < mouse_buttons; i++ )
	{
		if( ( mstate & ( 1<<i ) ) &&
			!( mouse_oldbuttonstate & ( 1<<i ) ) )
			Key_MouseEvent( K_MOUSE1 + i, qtrue, sys_msg_time );

		if( !( mstate & ( 1<<i ) ) &&
			( mouse_oldbuttonstate & ( 1<<i ) ) )
			Key_MouseEvent( K_MOUSE1 + i, qfalse, sys_msg_time );
	}

	mouse_oldbuttonstate = mstate;
}

/*
* IN_MouseMove
*/
void IN_MouseMove( usercmd_t *cmd )
{
	DIDEVICEOBJECTDATA od;
	DWORD dwElements;
	HRESULT	hr;

	if( !mouseactive )
		return;

	if (rawinput_initialized)
	{
		// probably not the right way...
		int i;

		mx = my = 0;

		for( i = 0; i < rawmicecount; i++ )
		{
			mx += rawmice[i].delta[0];
			my += rawmice[i].delta[1];
			rawmice[i].delta[0] = rawmice[i].delta[1] = 0;
		}
	}
	else if( dinput_initialized )
	{
		mx = 0;
		my = 0;

		for(;; )
		{
			dwElements = 1;

			hr = IDirectInputDevice_GetDeviceData( g_pMouse,
				sizeof( DIDEVICEOBJECTDATA ), &od, &dwElements, 0 );

			if( ( hr == DIERR_INPUTLOST ) || ( hr == DIERR_NOTACQUIRED ) )
			{
				dinput_acquired = qtrue;
				IDirectInputDevice_Acquire( g_pMouse );
				break;
			}

			// unable to read data or no data available
			if( FAILED( hr ) || dwElements == 0 )
				break;

			sys_msg_time = od.dwTimeStamp;

			// look at the element to see what happened
			switch( od.dwOfs )
			{
			case DIMOFS_X:
				mx += (int)od.dwData;
				break;

			case DIMOFS_Y:
				my += (int)od.dwData;
				break;

			case DIMOFS_Z:
				if( mouse_wheel_type != MWHEEL_WM )
				{
					mouse_wheel_type = MWHEEL_DINPUT;
					if( (int)od.dwData > 0 )
					{
						Key_Event( K_MWHEELUP, qtrue, sys_msg_time );
						Key_Event( K_MWHEELUP, qfalse, sys_msg_time );
					}
					else
					{
						Key_Event( K_MWHEELDOWN, qtrue, sys_msg_time );
						Key_Event( K_MWHEELDOWN, qfalse, sys_msg_time );
					}
				}
				break;

			case DIMOFS_BUTTON0:
			case DIMOFS_BUTTON1:
			case DIMOFS_BUTTON2:
			case DIMOFS_BUTTON3:
			case DIMOFS_BUTTON0+4:
			case DIMOFS_BUTTON0+5:
			case DIMOFS_BUTTON0+6:
			case DIMOFS_BUTTON0+7:
				if( od.dwData & 0x80 )
					mstate_di |= ( 1<<( od.dwOfs-DIMOFS_BUTTON0 ) );
				else
					mstate_di &= ~( 1<<( od.dwOfs-DIMOFS_BUTTON0 ) );
				break;
			}
		}

		dinput_initialized = qfalse; // FIXME...
		IN_MouseEvent( mstate_di );
		dinput_initialized = qtrue;
	}
	else
	{
		// find mouse movement
		if( !GetCursorPos( &current_pos ) )
			return;

		mx = current_pos.x - window_center_x;
		my = current_pos.y - window_center_y;

		// force the mouse to the center, so there's room to move
		if( mx || my )
			SetCursorPos( window_center_x, window_center_y );
	}

	CL_MouseMove( cmd, mx, my );
}


/*
=========================================================================

VIEW CENTERING

=========================================================================
*/

cvar_t *v_centermove;
cvar_t *v_centerspeed;

/*
* Joystick looking
*/
static void Joy_JLookDown( void )
{
	jlooking = qtrue;
}

static void Joy_JLookUp( void )
{
	jlooking = qfalse;
	if( !joy_freelook->integer && joy_lookspring->integer )
		IN_CenterView();
}


/*
* IN_Init
*/
void IN_Init( void )
{
	Com_Printf( "\n------- input initialization -------\n" );

	// mouse variables
	in_mouse	    = Cvar_Get( "in_mouse", "1", CVAR_ARCHIVE );
	in_grabinconsole	= Cvar_Get( "in_grabinconsole",	"0", CVAR_ARCHIVE );

	// joystick variables
	in_joystick		= Cvar_Get( "in_joystick", "0",	CVAR_ARCHIVE );
	joy_name		= Cvar_Get( "joy_name",	"joystick", 0 );
	joy_advanced		= Cvar_Get( "joy_advanced", "0", 0 );
	joy_advaxisx		= Cvar_Get( "joy_advaxisx", "0", 0 );
	joy_advaxisy		= Cvar_Get( "joy_advaxisy", "0", 0 );
	joy_advaxisz		= Cvar_Get( "joy_advaxisz", "0", 0 );
	joy_advaxisr		= Cvar_Get( "joy_advaxisr", "0", 0 );
	joy_advaxisu		= Cvar_Get( "joy_advaxisu", "0", 0 );
	joy_advaxisv		= Cvar_Get( "joy_advaxisv", "0", 0 );
	joy_forwardthreshold	= Cvar_Get( "joy_forwardthreshold", "0.15", 0 );
	joy_sidethreshold	= Cvar_Get( "joy_sidethreshold", "0.15", 0 );
	joy_upthreshold		= Cvar_Get( "joy_upthreshold", "0.15", 0 );
	joy_pitchthreshold	= Cvar_Get( "joy_pitchthreshold", "0.15", 0 );
	joy_yawthreshold	= Cvar_Get( "joy_yawthreshold",	"0.15",	0 );
	joy_forwardsensitivity	= Cvar_Get( "joy_forwardsensitivity", "-1", 0 );
	joy_sidesensitivity	= Cvar_Get( "joy_sidesensitivity", "-1", 0 );
	joy_upsensitivity	= Cvar_Get( "joy_upsensitivity", "-1", 0 );
	joy_pitchsensitivity	= Cvar_Get( "joy_pitchsensitivity", "1", 0 );
	joy_yawsensitivity	= Cvar_Get( "joy_yawsensitivity", "-1",	0 );
	joy_freelook = Cvar_Get( "joy_freelook", "1", 0 );
	joy_lookstrafe = Cvar_Get( "joy_lookstrafe", "0", 0 );
	joy_lookspring = Cvar_Get( "joy_lookspring", "0", 0 );

	Cmd_AddCommand( "+jlook", Joy_JLookDown );
	Cmd_AddCommand( "-jlook", Joy_JLookUp );
	Cmd_AddCommand( "joy_advancedupdate", Joy_AdvancedUpdate_f );

	IN_StartupMouse();
	IN_StartupJoystick();

	Com_Printf( "------------------------------------\n" );
}

/*
* IN_Shutdown
*/
void IN_Shutdown( void )
{
	IN_DeactivateMouse();

	if( rawinput_initialized )
		IN_RawInput_Shutdown();
	else if( dinput_initialized )
		IN_ShutdownDInput();

	Cmd_RemoveCommand( "+jlook" );
	Cmd_RemoveCommand( "-jlook" );
	Cmd_RemoveCommand( "joy_advancedupdate" );

	dinput_acquired = dinput_initialized = qfalse;
	rawinput_initialized = qfalse;
}

/*
* IN_Restart
*/
void IN_Restart( void )
{
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
void IN_Activate( qboolean active )
{
	in_appactive = active;
	mouseactive = !active;  // force a new window check or turn off
}


/*
* IN_Frame
* 
* Called every frame, even if not generating commands
*/
void IN_Frame( void )
{
	extern cvar_t *vid_fullscreen;

	if( !mouseinitialized )
		return;

	if( vid_fullscreen && (!vid_fullscreen->integer || cl_parent_hwnd) )
	{
		extern cvar_t *in_grabinconsole;

		// if we have a parent window (say, a browser plugin window) and
		// the window is not focused, deactivate the input
		if( cl_parent_hwnd && !AppFocused )
		{
			if( in_appactive )
				IN_Activate( qfalse );
		}
		else if( in_grabinconsole->integer || cls.key_dest != key_console )
		{
			if( !in_appactive && ActiveApp )
				IN_Activate( qtrue );
		}
		else
		{
			if( in_appactive )
				IN_Activate( qfalse );
		}
	}

	if( !in_mouse || !in_appactive )
	{
		IN_DeactivateMouse();
		return;
	}

	IN_ActivateMouse();
}

/*
=========================================================================

JOYSTICK

=========================================================================
*/

/*
* IN_StartupJoystick
*/
static void IN_StartupJoystick( void )
{
	int numdevs;
	JOYCAPS	jc;
	MMRESULT mmr = 0;
	cvar_t *cv;

	// assume no joystick
	joy_avail = qfalse;

	// abort startup if user requests no joystick
	cv = Cvar_Get( "in_initjoy", "1", CVAR_NOSET );
	if( !cv->integer )
		return;

	// verify joystick driver is present
	if( ( numdevs = joyGetNumDevs() ) == 0 )
	{
		//		Com_Printf ("joystick not found -- driver not present\n");
		return;
	}

	// cycle through the joystick ids for the first valid one
	for( joy_id = 0; joy_id < numdevs; joy_id++ )
	{
		memset( &ji, 0, sizeof( ji ) );
		ji.dwSize = sizeof( ji );
		ji.dwFlags = JOY_RETURNCENTERED;

		if( ( mmr = joyGetPosEx( joy_id, &ji ) ) == JOYERR_NOERROR )
			break;
	}

	// abort startup if we didn't find a valid joystick
	if( mmr != JOYERR_NOERROR )
	{
		Com_Printf( "joystick not found -- no valid joysticks (%x)\n", mmr );
		return;
	}

	// get the capabilities of the selected joystick
	// abort startup if command fails
	memset( &jc, 0, sizeof( jc ) );
	if( ( mmr = joyGetDevCaps( joy_id, &jc, sizeof( jc ) ) ) != JOYERR_NOERROR )
	{
		Com_Printf( "joystick not found -- invalid joystick capabilities (%x)\n", mmr );
		return;
	}

	// save the joystick's number of buttons and POV status
	joy_numbuttons = jc.wNumButtons;
	joy_haspov = jc.wCaps & JOYCAPS_HASPOV;

	// old button and POV states default to no buttons pressed
	joy_oldbuttonstate = joy_oldpovstate = 0;

	// mark the joystick as available and advanced initialization not completed
	// this is needed as cvars are not available during initialization

	joy_avail = qtrue;
	joy_advancedinit = qfalse;

	Com_Printf( "joystick detected\n" );
}


/*
* RawValuePointer
*/
static PDWORD RawValuePointer( int axis )
{
	switch( axis )
	{
	case JOY_AXIS_X:
		return &ji.dwXpos;
	case JOY_AXIS_Y:
		return &ji.dwYpos;
	case JOY_AXIS_Z:
		return &ji.dwZpos;
	case JOY_AXIS_R:
		return &ji.dwRpos;
	case JOY_AXIS_U:
		return &ji.dwUpos;
	case JOY_AXIS_V:
		return &ji.dwVpos;
	}

	return NULL;
}


/*
* Joy_AdvancedUpdate_f
*/
static void Joy_AdvancedUpdate_f( void )
{
	// called once by IN_ReadJoystick and by user whenever an update is needed
	// cvars are now available
	int i;
	DWORD dwTemp;

	// initialize all the maps
	for( i = 0; i < JOY_MAX_AXES; i++ )
	{
		dwAxisMap[i] = AxisNada;
		dwControlMap[i] = JOY_ABSOLUTE_AXIS;
		pdwRawValue[i] = RawValuePointer( i );
	}

	if( joy_advanced->integer == 0 )
	{
		// default joystick initialization
		// 2 axes only with joystick control
		dwAxisMap[JOY_AXIS_X] = AxisTurn;
		// dwControlMap[JOY_AXIS_X] = JOY_ABSOLUTE_AXIS;
		dwAxisMap[JOY_AXIS_Y] = AxisForward;
		// dwControlMap[JOY_AXIS_Y] = JOY_ABSOLUTE_AXIS;
	}
	else
	{
		if( strcmp( joy_name->string, "joystick" ) != 0 )
		{
			// notify user of advanced controller
			Com_Printf( "\n%s configured\n\n", joy_name->string );
		}

		// advanced initialization here
		// data supplied by user via joy_axisn cvars
		dwTemp = (DWORD) joy_advaxisx->integer;
		dwAxisMap[JOY_AXIS_X] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_X] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisy->integer;
		dwAxisMap[JOY_AXIS_Y] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Y] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisz->integer;
		dwAxisMap[JOY_AXIS_Z] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Z] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisr->integer;
		dwAxisMap[JOY_AXIS_R] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_R] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisu->integer;
		dwAxisMap[JOY_AXIS_U] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_U] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisv->integer;
		dwAxisMap[JOY_AXIS_V] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_V] = dwTemp & JOY_RELATIVE_AXIS;
	}

	// compute the axes to collect from DirectInput
	joy_flags = JOY_RETURNCENTERED | JOY_RETURNBUTTONS | JOY_RETURNPOV;
	for( i = 0; i < JOY_MAX_AXES; i++ )
	{
		if( dwAxisMap[i] != AxisNada )
			joy_flags |= dwAxisFlags[i];
	}
}


/*
* IN_Commands
*/
void IN_Commands( void )
{
	unsigned i;
	int key_index;
	DWORD buttonstate, povstate;

	if( !joy_avail )
		return;

	// loop through the joystick buttons
	// key a joystick event or auxillary event for higher number buttons for each state change
	buttonstate = ji.dwButtons;
	for( i = 0; i < joy_numbuttons; i++ )
	{
		if( ( buttonstate & ( 1<<i ) ) && !( joy_oldbuttonstate & ( 1<<i ) ) )
		{
			key_index = ( i < 4 ) ? K_JOY1 : K_AUX1;
			Key_MouseEvent( key_index + i, qtrue, 0 );
		}

		if( !( buttonstate & ( 1<<i ) ) && ( joy_oldbuttonstate & ( 1<<i ) ) )
		{
			key_index = ( i < 4 ) ? K_JOY1 : K_AUX1;
			Key_MouseEvent( key_index + i, qfalse, 0 );
		}
	}
	joy_oldbuttonstate = buttonstate;

	if( joy_haspov )
	{
		// convert POV information into 4 bits of state information
		// this avoids any potential problems related to moving from one
		// direction to another without going through the center position
		povstate = 0;
		if( ji.dwPOV != JOY_POVCENTERED )
		{
			if( ji.dwPOV == JOY_POVFORWARD )
				povstate |= 0x01;
			if( ji.dwPOV == JOY_POVRIGHT )
				povstate |= 0x02;
			if( ji.dwPOV == JOY_POVBACKWARD )
				povstate |= 0x04;
			if( ji.dwPOV == JOY_POVLEFT )
				povstate |= 0x08;
		}
		// determine which bits have changed and key an auxillary event for each change
		for( i = 0; i < 4; i++ )
		{
			if( ( povstate & ( 1<<i ) ) && !( joy_oldpovstate & ( 1<<i ) ) )
				Key_Event( K_AUX29 + i, qtrue, 0 );

			if( !( povstate & ( 1<<i ) ) && ( joy_oldpovstate & ( 1<<i ) ) )
				Key_Event( K_AUX29 + i, qfalse, 0 );
		}
		joy_oldpovstate = povstate;
	}
}


/*
* IN_ReadJoystick
*/
static qboolean IN_ReadJoystick( void )
{
	memset( &ji, 0, sizeof( ji ) );
	ji.dwSize = sizeof( ji );
	ji.dwFlags = joy_flags;

	if( joyGetPosEx( joy_id, &ji ) == JOYERR_NOERROR )
	{
		return qtrue;
	}
	else
	{
		// read error occurred
		// turning off the joystick seems too harsh for 1 read error,
		// but what should be done?
		// Com_Printf ("IN_ReadJoystick: no response\n");
		// joy_avail = qfalse;
		return qfalse;
	}
}

#define JOY_SPEED_KEY 400

/*
* IN_JoyMove
*/
void IN_JoyMove( usercmd_t *cmd )
{
	float speed, aspeed;
	float fAxisValue;
	int i;

	if( !ActiveApp )
		return;

	// complete initialization if first time in
	// this is needed as cvars are not available at initialization time
	if( joy_advancedinit != qtrue )
	{
		Joy_AdvancedUpdate_f();
		joy_advancedinit = qtrue;
	}

	// verify joystick is available and that the user wants to use it
	if( !joy_avail || !in_joystick->integer )
		return;

	// collect the joystick data, if possible
	if( IN_ReadJoystick() != qtrue )
		return;

	// wsw : jal : decide walk in server side
	//if ( (in_speed.state & 1) ^ cl_run->integer)
	//	speed = 2;
	//else
	speed = 1;
	//aspeed = speed * cls.frametime;
	aspeed =  cls.frametime;

	// loop through the axes
	for( i = 0; i < JOY_MAX_AXES; i++ )
	{
		// get the floating point zero-centered, potentially-inverted data for the current axis
		fAxisValue = (float) *pdwRawValue[i];
		// move centerpoint to zero
		fAxisValue -= 32768.0;

		// convert range from -32768..32767 to -1..1
		fAxisValue /= 32768.0;

		switch( dwAxisMap[i] )
		{
		case AxisForward:
			if( ( joy_advanced->integer == 0 ) && jlooking )
			{
				// user wants forward control to become look control
				if( fabs( fAxisValue ) > joy_pitchthreshold->value )
				{
					// if mouse invert is on, invert the joystick pitch value
					// only absolute control support here (joy_advanced is qfalse)
					if( m_pitch->value < 0.0 )
						cl.viewangles[PITCH] -= ( fAxisValue * joy_pitchsensitivity->value ) * aspeed * cl_pitchspeed->value;
					else
						cl.viewangles[PITCH] += ( fAxisValue * joy_pitchsensitivity->value ) * aspeed * cl_pitchspeed->value;
				}
			}
			else
			{
				// user wants forward control to be forward control
				if( fabs( fAxisValue ) > joy_forwardthreshold->value )
					cmd->forwardmove += ( fAxisValue * joy_forwardsensitivity->value ) * speed * JOY_SPEED_KEY;
			}
			break;

		case AxisSide:
			if( fabs( fAxisValue ) > joy_sidethreshold->value )
				cmd->sidemove += ( fAxisValue * joy_sidesensitivity->value ) * speed * JOY_SPEED_KEY;
			break;

		case AxisUp:
			if( fabs( fAxisValue ) > joy_upthreshold->value )
				cmd->upmove += ( fAxisValue * joy_upsensitivity->value ) * speed * JOY_SPEED_KEY;
			break;

		case AxisTurn:
			if( ( in_strafe.state & 1 ) || ( joy_lookstrafe->value && jlooking ) )
			{
				// user wants turn control to become side control
				if( fabs( fAxisValue ) > joy_sidethreshold->value )
					cmd->sidemove -= ( fAxisValue * joy_sidesensitivity->value ) * speed * JOY_SPEED_KEY;
			}
			else
			{
				// user wants turn control to be turn control
				if( fabs( fAxisValue ) > joy_yawthreshold->value )
				{
					if( dwControlMap[i] == JOY_ABSOLUTE_AXIS )
						cl.viewangles[YAW] += ( fAxisValue * joy_yawsensitivity->value ) * aspeed * cl_yawspeed->value;
					else
						cl.viewangles[YAW] += ( fAxisValue * joy_yawsensitivity->value ) * speed * 180.0;
				}
			}
			break;

		case AxisLook:
			if( jlooking )
			{
				if( fabs( fAxisValue ) > joy_pitchthreshold->value )
				{
					// pitch movement detected and pitch movement desired by user
					if( dwControlMap[i] == JOY_ABSOLUTE_AXIS )
						cl.viewangles[PITCH] += ( fAxisValue * joy_pitchsensitivity->value ) * aspeed * cl_pitchspeed->value;
					else
						cl.viewangles[PITCH] += ( fAxisValue * joy_pitchsensitivity->value ) * speed * 180.0;
				}
			}
			break;

		default:
			break;
		}
	}
}
