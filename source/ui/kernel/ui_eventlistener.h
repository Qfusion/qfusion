/*
 * UI_EventListener.h
 *
 *  Created on: 27.6.2011
 *      Author: hc
 */

#ifndef UI_EVENTLISTENER_H_
#define UI_EVENTLISTENER_H_

#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

namespace WSWUI
{

// just testing stuff
class BaseEventListener : public Rml::Core::EventListener
{
	typedef Rml::Core::Event Event;

public:
	BaseEventListener();
	virtual ~BaseEventListener();

	virtual void ProcessEvent( Event &event );

private:
	virtual void StartTargetPropertySound( Rml::Core::Element *target, const Rml::Core::String &property );
};

// Basic event listener with default handling for all elements
Rml::Core::EventListener *GetBaseEventListener( void );
// get instance of global eventlistener
Rml::Core::EventListener *UI_GetMainListener( void );
// get instance of eventlistener that opens the soft keyboard for text inputs
Rml::Core::EventListener *UI_GetSoftKeyboardListener( void );

}

#endif /* UI_EVENTLISTENER_H_ */
