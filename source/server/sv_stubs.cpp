#include "qcommon/qcommon.h"

void CL_Init() { }
void CL_Shutdown() { }
void CL_Frame( int realmsec, int gamemsec ) { }
void CL_Disconnect( const char * message ) { }

void Con_Print( const char * text ) { }

static void Bind() { }
void Key_Init() {
	Cmd_AddCommand( "bind", Bind );
}
void Key_Shutdown() {
	Cmd_RemoveCommand( "bind" );
}

void SCR_BeginLoadingPlaque() { }
void SCR_EndLoadingPlaque() { }
