/*
Copyright (C) 2016 Victor Luchits

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

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "as/asui.h"
#include "as/asui_local.h"

namespace ASUI
{

#define MAX_PRINTMSG 4096

// dummy cConsole class, single referenced
class Console
{
public:
	Console() {
	}
};

static Console dummyConsole;

// =====================================================================================

void PrebindConsole( ASInterface *as ) {
	ASBind::Class<Console, ASBind::class_singleref>( as->getEngine() );
}

static void Console_Log( Console *console, const asstring_t &s ) {
	trap::Print( s.buffer );
}

static void Console_Debug( Console *console, const asstring_t &s ) {
	if( UI_Main::Get()->debugOn() ) {
		trap::Print( s.buffer );
	}
}

static void Console_Error( Console *console, const asstring_t &s ) {
	char msg[MAX_PRINTMSG];
	Q_snprintfz( msg, sizeof( msg ), S_COLOR_RED "ERROR: %s\n", s.buffer );
	trap::Print( msg );
}

static void Console_Warn( Console *console, const asstring_t &s ) {
	char msg[MAX_PRINTMSG];
	Q_snprintfz( msg, sizeof( msg ), S_COLOR_YELLOW "WARNING: %s\n", s.buffer );
	trap::Print( msg );
}

static void Console_Trace( Console *console ) {
	char msg[MAX_PRINTMSG];
	auto *ctx = UI_Main::Get()->getAS()->getActiveContext();

	// Show the call stack
	Q_snprintfz( msg, sizeof( msg ), S_COLOR_CYAN "Stacktrace for %s:\n", ctx->GetFunction( 0 )->GetModuleName() );
	trap::Print( msg );

	for( asUINT n = 0; n < ctx->GetCallstackSize(); n++ ) {
		asIScriptFunction *func;
		const char *scriptSection;
		int line, column;

		func = ctx->GetFunction( n );
		line = ctx->GetLineNumber( n, &column, &scriptSection );

		Q_snprintfz( msg, sizeof( msg ), S_COLOR_CYAN "  %s:%s:%d,%d\n", scriptSection, func->GetDeclaration(), line, column );
		trap::Print( msg );
	}
}

static void Console_Assert( Console *console, bool condition ) {
	if( condition == false ) {
		trap::Print( S_COLOR_RED "Assertion failed\n" );
		Console_Trace( console );
	}
}

void BindConsole( ASInterface *as ) {
	ASBind::GetClass<Console>( as->getEngine() )
	.constmethod( Console_Log, "log", true )
	.constmethod( Console_Debug, "debug", true )
	.constmethod( Console_Warn, "warn", true )
	.constmethod( Console_Error, "error", true )
	.constmethod( Console_Trace, "trace", true )
	.constmethod( Console_Assert, "assert", true )
	;
}

void BindConsoleGlobal( ASInterface *as ) {
	ASBind::Global( as->getEngine() )

	// global variable
	.var( &dummyConsole, "console" )
	;
}

}

ASBIND_TYPE( ASUI::Console, Console )
