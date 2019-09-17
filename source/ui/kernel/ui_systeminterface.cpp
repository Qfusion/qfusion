/*
 * UI_SystemInterface.cpp
 *
 *  Created on: 25.6.2011
 *      Author: hc
 */

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_systeminterface.h"

using namespace Rml::Core;

namespace WSWUI
{

UI_SystemInterface::UI_SystemInterface() {
}

UI_SystemInterface::~UI_SystemInterface() {
}


double UI_SystemInterface::GetElapsedTime() {
	return double( trap::Milliseconds() ) * 0.001;
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
			trap::Print( console_msg.c_str() );
			break;
		case Log::LT_WARNING:
			console_msg = String( S_COLOR_YELLOW ) + "LibRocket: WARNING: " + message + "\n";
			trap::Print( console_msg.c_str() );
			break;
		case Log::LT_INFO:

			//if( trap::Cvar_Value( "developer" ) ) {
			console_msg = "LibRocket: " + message + "\n";
			trap::Print( console_msg.c_str() );

			//}
			break;
		case Log::LT_DEBUG:
		case Log::LT_MAX:
			if( trap::Cvar_Value( "developer" ) ) {
				console_msg = String( S_COLOR_CYAN ) + "LibRocket: DEBUG: " + message + "\n";
				trap::Print( console_msg.c_str() );
			}
			break;
	}
	return true;
}

int UI_SystemInterface::TranslateString( Rml::Core::String& translated, const Rml::Core::String& input ) {
	const char *l10ned;

	l10ned = trap::L10n_TranslateString( input.c_str() );
	if( l10ned ) {
		if( !strcmp( input.c_str(), l10ned ) ) {
			// handle cases when translation matches the input
			// to prevent libRocket from going into endless loop,
			// trying to translate the same string over and over
			translated = input;
			return 0;
		}
		translated = l10ned;
		return 1;
	}

	translated = input;
	return 0;
}

void UI_SystemInterface::GetClipboardText( Rml::Core::String &text ) {
	Rml::Core::String clipboard_content;

	char *data = trap::CL_GetClipboardData();
	if( data == NULL ) {
		text.clear();
		return;
	}

	text = data;
	trap::CL_FreeClipboardData( data );
}

void UI_SystemInterface::SetClipboardText( const Rml::Core::String &text ) {
	trap::CL_SetClipboardData( text.c_str() );
}

}
