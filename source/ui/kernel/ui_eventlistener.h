/*
 * UI_EventListener.h
 *
 *  Created on: 27.6.2011
 *      Author: hc
 */

#ifndef UI_EVENTLISTENER_H_
#define UI_EVENTLISTENER_H_

#include <Rocket/Core/Event.h>
#include <Rocket/Core/EventListener.h>

namespace WSWUI
{

// just testing stuff
class BaseEventListener : public Rocket::Core::EventListener
{
	typedef Rocket::Core::Event Event;

public:
	BaseEventListener();
	virtual ~BaseEventListener();

	virtual void ProcessEvent( Event &event );

private:
	virtual void StartTargetPropertySound( Rocket::Core::Element *target, const Rocket::Core::String &property );
};

// Basic event listener with default handling for all elements
Rocket::Core::EventListener *GetBaseEventListener( void );
// get instance of global eventlistener
Rocket::Core::EventListener *UI_GetMainListener( void );

}

#endif /* UI_EVENTLISTENER_H_ */
