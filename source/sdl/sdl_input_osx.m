#import <AppKit/AppKit.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/hidsystem/IOHIDLib.h>
#import <IOKit/hidsystem/IOHIDParameter.h>
#import <IOKit/hidsystem/event_status_driver.h>

#include "../client/client.h"

extern cvar_t *in_disablemacosxmouseaccel;

io_connect_t IN_GetIOHandle( void )
{
	io_connect_t myHandle = MACH_PORT_NULL;
	kern_return_t myStatus;
	io_service_t myService = MACH_PORT_NULL;
	mach_port_t myMasterPort;

	myStatus = IOMasterPort( MACH_PORT_NULL, &myMasterPort );

	if( myStatus != KERN_SUCCESS ) {
		return ( 0 );
	}

	myService = IORegistryEntryFromPath( myMasterPort, kIOServicePlane ":/IOResources/IOHIDSystem" );

	if( myService == 0 ) {
		return ( 0 );
	}

	myStatus = IOServiceOpen( myService, mach_task_self(), kIOHIDParamConnectType, &myHandle );
	IOObjectRelease( myService );

	return ( myHandle );
}

void IN_SetMouseScalingEnabled( bool isRestore )
{
	static double myOldAcceleration = 0.0;

	if( in_disablemacosxmouseaccel->integer ) {
		io_connect_t mouseDev = IN_GetIOHandle();
		if( mouseDev != 0 ) {
			// if isRestore YES, restore old (set by system control panel) acceleration.
			if( isRestore ) {
				IOHIDSetAccelerationWithKey( mouseDev, CFSTR( kIOHIDMouseAccelerationType ), myOldAcceleration );
			} else // otherwise, disable mouse acceleration. we won't disable trackpad acceleration.
			{
				if( IOHIDGetAccelerationWithKey( mouseDev, CFSTR( kIOHIDMouseAccelerationType ), &myOldAcceleration ) == kIOReturnSuccess ) {
					//					Com_Printf("previous mouse acceleration: %f\n", myOldAcceleration);
					if( IOHIDSetAccelerationWithKey( mouseDev, CFSTR( kIOHIDMouseAccelerationType ), -1.0 ) != kIOReturnSuccess ) {
						Com_Printf( "Could not disable mouse acceleration (failed at IOHIDSetAccelerationWithKey).\n" );
						Cvar_Set( "in_disablemacosxmouseaccel", "0" );
					}
				} else {
					Com_Printf( "Could not disable mouse acceleration (failed at IOHIDGetAccelerationWithKey).\n" );
					Cvar_Set( "in_disablemacosxmouseaccel", "0" );
				}
			}
			IOServiceClose( mouseDev );
		} else {
			Com_Printf( "Could not disable mouse acceleration (failed at IO_GetIOHandle).\n" );
			Cvar_Set( "in_disablemacosxmouseaccel", "0" );
		}
	}
}

/**
 * IN_GetInputLanguage
 */
void IN_GetInputLanguage( char *dest, size_t size )
{
	if( size )
		dest[0] = '\0';
}

