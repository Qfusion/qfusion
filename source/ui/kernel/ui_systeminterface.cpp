/*
 * UI_SystemInterface.cpp
 *
 *  Created on: 25.6.2011
 *      Author: hc
 */

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_systeminterface.h"

using namespace Rocket::Core;

namespace WSWUI
{

UI_SystemInterface::UI_SystemInterface() {
}

UI_SystemInterface::~UI_SystemInterface() {
}


float UI_SystemInterface::GetElapsedTime() {
	return float( trap::Milliseconds() ) * 0.001;
}

bool UI_SystemInterface::LogMessage( Log::Type type, const String& message ) {
	String console_msg;

	switch( type ) {
		case Log::LT_ALWAYS:

			// ignore?
			break;
		case Log::LT_ERROR:
		case Log::LT_ASSERT:
			console_msg = String( S_COLOR_RED ) + "LibRocket: ERROR: " + message + "\n";
			trap::Print( console_msg.CString() );
			break;
		case Log::LT_WARNING:
			console_msg = String( S_COLOR_YELLOW ) + "LibRocket: WARNING: " + message + "\n";
			trap::Print( console_msg.CString() );
			break;
		case Log::LT_INFO:

			//if( trap::Cvar_Value( "developer" ) ) {
			console_msg = "LibRocket: " + message + "\n";
			trap::Print( console_msg.CString() );

			//}
			break;
		case Log::LT_DEBUG:
		case Log::LT_MAX:
			if( trap::Cvar_Value( "developer" ) ) {
				console_msg = String( S_COLOR_CYAN ) + "LibRocket: DEBUG: " + message + "\n";
				trap::Print( console_msg.CString() );
			}
			break;
	}
	return true;
}

void UI_SystemInterface::GetClipboardText( Rocket::Core::WString &text ) {
	Rocket::Core::WString clipboard_content;

	char *data = trap::CL_GetClipboardData();
	if( data == NULL ) {
		text = "";
		return;
	}

	text = data;
	trap::CL_FreeClipboardData( data );
}

void UI_SystemInterface::SetClipboardText( const Rocket::Core::WString &text ) {
	Rocket::Core::String utf8_text;
	text.ToUTF8( utf8_text );
	trap::CL_SetClipboardData( utf8_text.CString() );
}

}
