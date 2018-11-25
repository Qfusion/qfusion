#include "ui_precompiled.h"
#include "kernel/ui_utils.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"

#include "as/asui.h"
#include "as/asui_local.h"

namespace ASUI
{

//==============================================================

void BindAPI( ASInterface *as ) {
	PrebindURL( as );
	PrebindEvent( as );
	PrebindEventListener( as );
	PrebindElement( as );
	PrebindWindow( as );
	PrebindL10n( as );

	PrebindOptionsForm( as );
	PrebindServerbrowser( as );
	PrebindDataSource( as );
	PrebindDemoInfo( as );
	PrebindDownloadInfo( as );
	PrebindGame( as );
	PrebindConsole( as );

	// now bind the class functions
	BindURL( as );
	BindEvent( as );
	BindElement( as );
	BindWindow( as );
	BindL10n( as );

	BindOptionsForm( as );
	BindServerbrowser( as );
	BindDataSource( as );
	BindDemoInfo( as );
	BindDownloadInfo( as );
	BindGame( as );
	BindConsole( as );
}

// This needs to be called after globals are instantiated
void BindGlobals( ASInterface *as ) {
	// globals
	BindWindowGlobal( as );
	BindServerbrowserGlobal( as );
	BindGameGlobal( as );
	BindConsoleGlobal( as );
}

// update function for bound api
void BindFrame( ASInterface *as ) {
	RunWindowFrame();
}

// release bound resources (funcdefs, etc)
void BindShutdown( ASInterface *as ) {
	UnbindWindow();
}

}
