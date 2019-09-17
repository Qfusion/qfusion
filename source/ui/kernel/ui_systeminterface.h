/*
 * UI_SystemInterface.h
 *
 *  Created on: 25.6.2011
 *      Author: hc
 *
 * Implements the SystemInterface and FileInterface for libRmlUi
 */

#ifndef UI_SYSTEMINTERFACE_H_
#define UI_SYSTEMINTERFACE_H_

#include <RmlUi/Core/SystemInterface.h>

namespace WSWUI
{

class UI_SystemInterface : public Rml::Core::SystemInterface
{
public:
	UI_SystemInterface();
	virtual ~UI_SystemInterface();

	//// Implement SystemInterface

	/// Get the number of seconds elapsed since the start of the application
	/// @returns Seconds elapsed
	virtual double GetElapsedTime();

	/// Log the specified message.
	/// @param[in] type Type of log message, ERROR, WARNING, etc.
	/// @param[in] message Message to log.
	/// @return True to continue execution, false to break into the debugger.
	virtual bool LogMessage( Rml::Core::Log::Type type, const Rml::Core::String & message );

	/// Translate the input string into the translated string.
	/// @param[out] translated Translated string ready for display.
	/// @param[in] input String as received from XML.
	/// @return Number of translations that occured.
	virtual int TranslateString( Rml::Core::String & translated, const Rml::Core::String & input );

	/// Set clipboard text.
	/// @param[in] text Text to apply to clipboard.
	virtual void SetClipboardText( const Rml::Core::String& text );
	
	/// Get clipboard text.
	/// @param[out] text Retrieved text from clipboard.
	virtual void GetClipboardText( Rml::Core::String & text );
};

}

#endif /* UI_SYSTEMINTERFACE_H_ */
