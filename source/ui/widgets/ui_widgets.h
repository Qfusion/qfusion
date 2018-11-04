/*
 * widgets.h
 *
 *  Created on: 29.6.2011
 *      Author: hc
 */

#ifndef __WIDGETS_H__
#define __WIDGETS_H__

#include "kernel/ui_main.h"
#include "kernel/ui_eventlistener.h"
#include <Rocket/Core/Element.h>

namespace WSWUI
{

// "my generic element instancer"
template<typename T>
struct GenericElementInstancer : Rocket::Core::ElementInstancer {
	Rocket::Core::Element *InstanceElement( Rocket::Core::Element *parent, const String &tag, const Rocket::Core::XMLAttributes &attributes ) {
		Rocket::Core::Element *elem = __new__( T )( tag );
		UI_Main::Get()->getRocket()->registerElementDefaults( elem );
		return elem;
	}

	void ReleaseElement( Rocket::Core::Element *element ) {
		__delete__( element );
	}

	void Release() {
		__delete__( this );
	}
};

// "my generic element instancer" that sends attributes to the child
template<typename T>
struct GenericElementInstancerAttr : Rocket::Core::ElementInstancer {
	Rocket::Core::Element *InstanceElement( Rocket::Core::Element *parent, const String &tag, const Rocket::Core::XMLAttributes &attributes ) {
		Rocket::Core::Element *elem = __new__( T )( tag, attributes );
		UI_Main::Get()->getRocket()->registerElementDefaults( elem );
		return elem;
	}

	void ReleaseElement( Rocket::Core::Element *element ) {
		__delete__( element );
	}

	void Release() {
		__delete__( this );
	}
};

// "my generic element instancer" that attaches click/blur events that toggle the soft keyboard
template<typename T>
struct GenericElementInstancerSoftKeyboard : GenericElementInstancer<T>{
	Rocket::Core::Element *InstanceElement( Rocket::Core::Element *parent, const String &tag, const Rocket::Core::XMLAttributes &attributes ) {
		Rocket::Core::Element *elem = GenericElementInstancer<T>::InstanceElement( parent, tag, attributes );
		elem->AddEventListener( "click", UI_GetSoftKeyboardListener() );
		elem->AddEventListener( "blur", UI_GetSoftKeyboardListener() );
		return elem;
	}
};

//=======================================

Rocket::Core::ElementInstancer *GetKeySelectInstancer( void );
Rocket::Core::ElementInstancer *GetAnchorWidgetInstancer( void );
Rocket::Core::ElementInstancer *GetOptionsFormInstancer( void );
Rocket::Core::ElementInstancer *GetLevelShotInstancer( void );
Rocket::Core::ElementInstancer *GetSelectableDataGridInstancer( void );
Rocket::Core::ElementInstancer *GetDataSpinnerInstancer( void );
Rocket::Core::ElementInstancer *GetModelviewInstancer( void );
Rocket::Core::ElementInstancer *GetWorldviewInstancer( void );
Rocket::Core::ElementInstancer *GetColorBlockInstancer( void );
Rocket::Core::ElementInstancer *GetColorSelectorInstancer( void );
Rocket::Core::ElementInstancer *GetInlineDivInstancer( void );
Rocket::Core::ElementInstancer *GetImageWidgetInstancer( void );
Rocket::Core::ElementInstancer *GetElementFieldInstancer( void );
Rocket::Core::ElementInstancer *GetIFrameWidgetInstancer( void );
Rocket::Core::ElementInstancer *GetElementL10nInstancer( void );
Rocket::Core::ElementInstancer *GetElementBlurInstancer( void );
}

#endif /* __WIDGETS_H__ */
