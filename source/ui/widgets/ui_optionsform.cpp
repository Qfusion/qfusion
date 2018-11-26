#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "kernel/ui_utils.h"

#include "widgets/ui_widgets.h"
#include "widgets/ui_optionsform.h"

#include <Rocket/Controls.h>

namespace WSWUI
{

// we use this alot
typedef Rocket::Core::Element Element;
typedef Rocket::Controls::ElementFormControl ElementFormControl;

//===================================

// CvarStorage

void CvarStorage::restoreValues() {
	for( CvarMap::iterator it = storedValues.begin(); it != storedValues.end(); ++it )
		trap::Cvar_Set( it->first.c_str(), it->second.c_str() );
}

// grab the values from the cvars to storage
void CvarStorage::storeValues() {
	for( CvarMap::iterator it = storedValues.begin(); it != storedValues.end(); ++it )
		it->second = trap::Cvar_String( it->first.c_str() );
}

// drop the storage
void CvarStorage::applyValues() {
	storedValues.clear();
}

// add a cvar to track/storage
void CvarStorage::addCvar( const char *name ) {
	// make sure we have a reasonable string in our hands
	const char *value = trap::Cvar_String( name );
	if( !value ) {
		value = "";
	}
	storedValues[std::string( name )] = std::string( value );
}

const CvarStorage::CvarMap &CvarStorage::getMap() {
	return storedValues;
}

//================================================

// Event listener for controls, sets the corresponding cvar to controls new value
class CvarChangeListener : public Rocket::Core::EventListener
{
public:
	CvarChangeListener() : Rocket::Core::EventListener() {}

	// helper to set cvar value from element, TODO: refactor out of this class
	static void setCvar( Element *elem ) {
		ElementFormControl *target = dynamic_cast<ElementFormControl*>( elem );
		if( target != 0 ) {
			String cvar = target->GetAttribute<String>( "cvar", "" );
			String type = target->GetAttribute<String>( "type", "" );

			// TODO: add support for <select> widgets
			if( type == "checkbox" || type == "radio" ) {
				float fvalue = target->HasAttribute( "checked" ) ? 1.0 : 0.0;
				trap::Cvar_SetValue( cvar.CString(), fvalue );

				//Com_Printf("onChange: Cvar_Set \"%s\" \"%g\"\n", cvar.CString(), fvalue );
			} else if( type == "range" ) {
				float fvalue = atof( target->GetValue().CString() );
				trap::Cvar_SetValue( cvar.CString(), fvalue );

				//Com_Printf("onChange: Cvar_Set \"%s\" \"%g\"\n", cvar.CString(), fvalue );
			} else {
				String value = target->GetValue();
				trap::Cvar_Set( cvar.CString(), value.CString() );

				//Com_Printf("onChange: Cvar_Set \"%s\" \"%s\"\n", cvar.CString(), value.CString() );
			}
		}
	}

	void ProcessEvent( Rocket::Core::Event &ev ) {
		if( ev.GetType() == "change" ) {
			setCvar( ev.GetTargetElement() );
		}
	}
};

//================================================

/*
    OptionsForm is derived from regular Rocket::Controls::Form and it has the addition of
    scanning its children for elements that have attribute called "cvar", which ties
    the control's value to the cvars value.

    Upon creation the controls value is set to the cvars value. (TODO: <select> boxes)

    Each such control has EventListener attached to the onChange event and the cvar value
    is changed when the control is changed. (TODO: provide "realtime" attribute to opt this)

    3 functions are provided and exported to Angelscript API:

        void restoreOptions()
    should be called on cancel event, sets all controls's values back to the original values before modification,
    and in the case of "realtime" cvars, also resets the cvars's values back to the original values.

        void storeOptions()
    should be called upon initialization, sets the controls's values to the values in corresponding
    cvars at the time. is called automatically after next function:

        void applyOptions()
    should be called upon submit/ok/apply/whatever. sets cvars's values to the controls's values, or in the
    case of "realtime" controls just drops the original value from the storage.

    TODO: possibly integrate <keyselect> support, which would require some architectural changes
    on that widget and managing them cause there's already additional layer created by the
    KeySelectWidgetInstaner that makes sure no keyselect has "double-bound" any key.
*/

// Ctor
OptionsForm::OptionsForm( const String &tag )
	: Rocket::Controls::ElementForm( tag ), cvarListener( __new__( CvarChangeListener ) )
{}

// Dtor
OptionsForm::~OptionsForm() {
	if( cvarListener ) {
		__delete__( cvarListener );
	}
}

// Rocket Form
void OptionsForm::ProcessEvent( Rocket::Core::Event &ev ) {
	// we want to handle onsubmit
	// Com_Printf("OptionsForm::ProcessEvent %s\n", ev.GetType().CString() );

	Rocket::Controls::ElementForm::ProcessEvent( ev );
}

// predicate for foreachChildren
namespace
{

bool is_realtime_control( Rocket::Core::Element *elem ) {
	return ( elem->GetAttribute<int>( "realtime", 0 ) != 0 );
}

// sets controls Value to attached cvar
struct fetch_cvar_value {
	inline void operator()( Element *elem ) {
		ElementFormControl *control = dynamic_cast<ElementFormControl*>( elem );
		if( control != 0 && elem->HasAttribute( "cvar" ) ) {
			String cvar = elem->GetAttribute<String>( "cvar", "" );
			if( cvar.Length() ) {
				// if this is checkbox/radio, we need to set/reset the checked attribute
				String type = control->GetAttribute<String>( "type", "" );
				if( type == "checkbox" || type == "radio" ) {
					bool checked = trap::Cvar_Value( cvar.CString() ) == 1 ? true : false;
					if( checked ) {
						control->RemoveAttribute( "checked" );
						control->SetAttribute<String>( "checked", "1" );
					} else {
						control->RemoveAttribute( "checked" );
					}

					//Com_Printf( "Restored %s to %d\n", cvar.CString(), checked ? 1 : 0 );
				} else {
					control->SetValue( trap::Cvar_String( cvar.CString() ) );

					//Com_Printf("Restored %s to %s\n", cvar.CString(), control->GetValue().CString() );
				}
			}
		}
	}
};

// sets cvar value for non-realtime controls after applying
struct set_cvar_value {
	inline void operator()( Element *elem ) {
		// realtime controls already have applied values in onChange listener
		if( !is_realtime_control( elem ) && elem->HasAttribute( "cvar" ) ) {
			CvarChangeListener::setCvar( elem );
		}
	}
};

// attach cvar listener to element (if realtime) and add to storage
// used in storeOptions (initial loading of form)
struct attach_and_add {
	Rocket::Core::EventListener *listener;
	CvarStorage &cvars;

	attach_and_add( Rocket::Core::EventListener *_listener, CvarStorage &_cvars )
		: listener( _listener ), cvars( _cvars )
	{}

	inline void operator()( Rocket::Core::Element *elem ) {
		ElementFormControl *control = dynamic_cast<ElementFormControl*>( elem );
		if( control != 0 && control->HasAttribute( "cvar" ) ) {
			if( is_realtime_control( control ) ) {
				// rocket blindly accepts double eventlisteners, so pull/push
				control->RemoveEventListener( "change", listener );
				control->AddEventListener( "change", listener );
			}

			// add the elements to storage and set the elements value
			String cvar = control->GetAttribute<String>( "cvar", "" );
			cvars.addCvar( cvar.CString() );

			// checkbox / radio type setup
			String type = control->GetAttribute<String>( "type", "" );
			if( type == "checkbox" || type == "radio" ) {
				bool checked = trap::Cvar_Value( cvar.CString() ) == 1 ? true : false;
				if( checked ) {
					control->RemoveAttribute( "checked" );
					control->SetAttribute<String>( "checked", "1" );
				} else {
					control->RemoveAttribute( "checked" );
				}
			} else {
				control->SetValue( trap::Cvar_String( cvar.CString() ) );
			}

			// Com_Printf("attach_and_add %s\n", elem->GetTagName().CString() );
		}
	}
};
}

// Our implementation

// move stored cvar values back to cvars (i.e. Cancel)
void OptionsForm::restoreOptions() {
	// Set the cvar's back to their old values
	cvars.restoreValues();

	// Fetch the cvar values to our controls
	foreachElem( this, fetch_cvar_value(), false );
}

// move cvar values to storage (i.e. onLoad/Show)
void OptionsForm::storeOptions() {
	// grap the cvar-attributed elements and store the cvar
	foreachElem( this, attach_and_add( this->cvarListener, this->cvars ), false );
}

// drop the storage and use current cvar values (i.e. Submit)
void OptionsForm::applyOptions() {
	// for non-realtime controls, set the cvar values to the controls value
	foreachElem( this, set_cvar_value(), false );

	// this essentially just drops the values from storage
	cvars.applyValues();

	// regenerate storage
	storeOptions();
}

//====================================================

Rocket::Core::ElementInstancer *GetOptionsFormInstancer( void ) {
	return __new__( GenericElementInstancer<OptionsForm> )();
}

}
