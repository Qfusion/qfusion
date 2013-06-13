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

UI_SystemInterface::UI_SystemInterface()
{
}

UI_SystemInterface::~UI_SystemInterface()
{
}


float UI_SystemInterface::GetElapsedTime()
{
	return float( trap::Milliseconds() ) * 0.001;
}

bool UI_SystemInterface::LogMessage(Log::Type type, const String& message)
{
	String console_msg;

	switch( type ) {
		case Log::LT_ALWAYS:
			// ignore?
			break;
		case Log::LT_ERROR:
		case Log::LT_ASSERT:
			console_msg = String( S_COLOR_RED ) + "ERROR: " + message + "\n";
			trap::Print( console_msg.CString() );
			break;
		case Log::LT_WARNING:
			console_msg = String( S_COLOR_YELLOW ) + "WARNING: " + message + "\n";
			trap::Print( console_msg.CString() );
			break;
		case Log::LT_INFO:
			console_msg = message + "\n";
			//trap::Print( console_msg.CString() );
			break;
		case Log::LT_DEBUG:
		case Log::LT_MAX:
			if( trap::Cvar_Value( "developer" ) ) {
				console_msg = String( S_COLOR_CYAN ) + "DEBUG: " + message + "\n";
				trap::Print( console_msg.CString() );
			}
			break;
	}
	return true;
}

}
