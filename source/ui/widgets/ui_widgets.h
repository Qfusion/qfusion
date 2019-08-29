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
#include <RmlUi/Core/Element.h>

namespace WSWUI
{

// "my generic element instancer"
template<typename T>
struct GenericElementInstancer : Rml::Core::ElementInstancer {
	Rml::Core::ElementPtr InstanceElement( Rml::Core::Element *parent, const std::string &tag, const Rml::Core::XMLAttributes &attributes ) override {
		auto elem = static_cast<Rml::Core::Element*>(__new__(T)( tag ));
		UI_Main::Get()->getRocket()->registerElementDefaults( elem );
		return Rml::Core::ElementPtr(elem);
	}

	void ReleaseElement( Rml::Core::Element *element ) override {
		__delete__( element );
	}
};

// "my generic element instancer" that sends attributes to the child
template<typename T>
struct GenericElementInstancerAttr : Rml::Core::ElementInstancer {
	Rml::Core::ElementPtr InstanceElement( Rml::Core::Element *parent, const std::string &tag, const Rml::Core::XMLAttributes &attributes ) override {
		auto *elem = static_cast<Rml::Core::Element*>(__new__( T )( tag, attributes ));
		UI_Main::Get()->getRocket()->registerElementDefaults( elem );
		return Rml::Core::ElementPtr(elem);
	}

	void ReleaseElement( Rml::Core::Element *element ) override {
		__delete__( element );
	}
};

// "my generic element instancer" that attaches click/blur events that toggle the soft keyboard
template<typename T>
struct GenericElementInstancerSoftKeyboard : GenericElementInstancer<T>{
	virtual Rml::Core::ElementPtr InstanceElement( Rml::Core::Element *parent, const std::string &tag, const Rml::Core::XMLAttributes &attributes ) override {
		Rml::Core::ElementPtr elem = GenericElementInstancer<T>::InstanceElement( parent, tag, attributes );
		elem->AddEventListener( "click", UI_GetSoftKeyboardListener() );
		elem->AddEventListener( "blur", UI_GetSoftKeyboardListener() );
		return elem;
	}
};

//=======================================

Rml::Core::ElementInstancer *GetKeySelectInstancer( void );
Rml::Core::ElementInstancer *GetAnchorWidgetInstancer( void );
Rml::Core::ElementInstancer *GetOptionsFormInstancer( void );
Rml::Core::ElementInstancer *GetLevelShotInstancer( void );
Rml::Core::ElementInstancer *GetSelectableDataGridInstancer( void );
Rml::Core::ElementInstancer *GetDataSpinnerInstancer( void );
Rml::Core::ElementInstancer *GetModelviewInstancer( void );
Rml::Core::ElementInstancer *GetWorldviewInstancer( void );
Rml::Core::ElementInstancer *GetColorBlockInstancer( void );
Rml::Core::ElementInstancer *GetColorSelectorInstancer( void );
Rml::Core::ElementInstancer *GetInlineDivInstancer( void );
Rml::Core::ElementInstancer *GetImageWidgetInstancer( void );
Rml::Core::ElementInstancer *GetElementFieldInstancer( void );
Rml::Core::ElementInstancer *GetVideoInstancer( void );
Rml::Core::ElementInstancer *GetIFrameWidgetInstancer( void );
Rml::Core::ElementInstancer *GetElementL10nInstancer( void );
Rml::Core::ElementInstancer *GetElementBlurInstancer( void );
}

#endif /* __WIDGETS_H__ */
