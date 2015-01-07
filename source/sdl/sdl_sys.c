#include <SDL.h>
#include "../client/client.h"

void Sys_Sleep( unsigned int millis )
{
	SDL_Delay( millis );
}

void Sys_Error( const char *format, ... )
{
	va_list argptr;
	char msg[1024];

	CL_Shutdown();

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, APPLICATION, msg, NULL );

	Qcommon_Shutdown();

	exit( 1 );
}

/*
* Sys_Init
*/
void Sys_Init( void )
{
	Sys_InitTime();
}

/*
 * Sys_InitDynvars
 */
void Sys_InitDynvars( void )
{
}
