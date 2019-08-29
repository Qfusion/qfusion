/*
Copyright (C) 2011 Cervesato Andrea (koochi)

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
#include "widgets/ui_widgets.h"
#include "kernel/ui_main.h"

#include <RmlUi/Controls.h>
#include <RmlUi/Controls/DataSource.h>

namespace WSWUI
{
using namespace Rml::Core;
using namespace Rml::Controls;

class UI_DataSpinner : public ElementFormControl
{
public:
	// assign element tag name
	UI_DataSpinner( const String &tag, const XMLAttributes &attr ) :
		ElementFormControl( tag ), currentItem( 0 ) {
		initializeData();
		readValueFromCvar();
	}

	~UI_DataSpinner() { }

	/// Called for every event sent to this element or one of its descendants.
	/// @param[in] event The event to process.
	void ProcessDefaultAction( Event& evt ) {
		Element::ProcessDefaultAction( evt );

		if( evt.GetType() == "mousedown" ) {
			int button = evt.GetParameter<int>( "button", 0 );

			if( button == 0 ) { // left button
				nextItem();
			} else {   // right button
				previousItem();
			}
		}
	}

	/// Sets the current value of the form control.
	/// @param[in] value The new value of the form control.
	void SetValue( const String &value ) {
		SetAttribute( "value", value.c_str() );

		// this calls out onchange event
		Rml::Core::Dictionary parameters;
		parameters["value"] = value;
		DispatchEvent( "change", parameters );
	}

	/// Returns a string representation of the current value of the form control.
	/// @return The value of the form control.
	String GetValue( void ) const {
		if( items.empty() || formatted_items.empty() ||
			items.size() != formatted_items.size() ||
			currentItem < 0 || (size_t)currentItem >= items.size() ) {
			return "";
		}

		return items[currentItem].c_str();
	}

	/// Returns if this value should be submitted with the form.
	/// @return True if the value should be be submitted with the form, false otherwise.
	bool IsSubmitted() {
		return items.size() != 0;
	}

	// Called when attributes on the element are changed.
	void OnAttributeChange( const Rml::Core::ElementAttributes& changed_attributes ) {
		Element::OnAttributeChange( changed_attributes );

		bool reloadData = false;

		if( !reloadData ) {
			reloadData = changed_attributes.find( "source" ) != changed_attributes.end();
		}
		if( !reloadData ) {
			reloadData = changed_attributes.find( "fields" ) != changed_attributes.end();
		}
		if( !reloadData ) {
			reloadData = changed_attributes.find( "valuefield" ) != changed_attributes.end();
		}
		if( !reloadData ) {
			reloadData = changed_attributes.find( "formatter" ) != changed_attributes.end();
		}

		if( reloadData ) {
			initializeData();
			readValueFromCvar();
		}
	}

private:
	// koochi: taken from libR's DataSourceListener
	bool parseDataSource( DataSource*& data_source, String& table_name, const String& data_source_name ) {
		if( data_source_name.empty() ) {
			data_source = NULL;
			table_name = "";
			return false;
		}

		StringList data_source_parts;
		Rml::Core::StringUtilities::ExpandString( data_source_parts, data_source_name, '.' );

		Rml::Controls::DataSource* new_data_source = Rml::Controls::DataSource::GetDataSource( data_source_parts[0].c_str() );

		if( data_source_parts.size() != 2 || !new_data_source ) {
			data_source = NULL;
			table_name = "";
			return false;
		}

		data_source = new_data_source;
		table_name = data_source_parts[1];
		return true;
	}

	void initializeData() {
		source_attribute = GetAttribute< String >( "source", "" );
		fields_attribute = GetAttribute< String >( "fields", "" );
		valuefield_attribute = GetAttribute< String >( "valuefield", "" );
		data_formatter_attribute = GetAttribute< String >( "formatter", "" );

		items.clear();
		formatted_items.clear();

		// koochi: [be aware] part of the code has been taken from libR

		String data_table = "";
		DataSource *data_source;

		if( !parseDataSource( data_source, data_table, source_attribute ) ) {
			return; // no data no parse !

		}
		DataFormatter* data_formatter = NULL;

		// Process the attributes.
		if( fields_attribute.empty() ) {
			return;
		}

		if( valuefield_attribute.empty() ) {
			valuefield_attribute = fields_attribute.substr( 0, fields_attribute.find( ',' ) );
		}

		if( !data_formatter_attribute.empty() ) {
			data_formatter = DataFormatter::GetDataFormatter( data_formatter_attribute );
		}

		// Build a list of attributes
		String fields( valuefield_attribute );
		fields += ",";
		fields += fields_attribute;

		DataQuery query( data_source, data_table, fields );
		while( query.NextRow() ) {
			StringList raw_fields;
			String value = query.Get<String>( 0, "" );

			// save the item
			items.push_back( value );

			for( size_t i = 1; i < query.GetNumFields(); ++i )
				raw_fields.push_back( query.Get< String>( i, "" ) );

			String formatted( "" );
			if( raw_fields.size() > 0 ) {
				formatted = raw_fields[0];
			}

			if( data_formatter ) {
				data_formatter->FormatData( formatted, raw_fields );
			}

			// save formatted item
			formatted_items.push_back( formatted );
		}
	}

	void readValueFromCvar() {
		String cvarName = GetAttribute< String >( "cvar", "" );
		if( !cvarName.empty() ) {
			cvar_t* cvar = trap::Cvar_Get( cvarName.c_str(), "", 0 );
			SetValue( cvar->string );

			if( items.empty() ) {
				return;
			}

			// select the item
			for( size_t i = 0; i < items.size(); i++ ) {
				if( items[i] == cvar->string ) {
					currentItem = i;
					this->SetInnerRML( formatted_items[i].c_str() );
					break;
				}
			}
		}
	}

	// select the previous formatted item
	void nextItem() {
		if( formatted_items.empty() || items.empty() ||
			formatted_items.size() != items.size() ) {
			return;
		}

		if( currentItem < 0 ) {
			currentItem = 0;
		}

		currentItem++;

		if( (size_t)currentItem >= formatted_items.size() ) {
			currentItem = 0;
		}

		this->SetInnerRML( formatted_items[currentItem].c_str() );

		SetValue( items[currentItem].c_str() );
	}

	// select the previous formatted item
	void previousItem() {
		if( formatted_items.empty() || items.empty() ||
			formatted_items.size() != items.size() ) {
			return;
		}

		if( (size_t)currentItem >= formatted_items.size() ) {
			currentItem = 0;
		}

		currentItem--;

		if( currentItem < 0 ) {
			currentItem = formatted_items.size() - 1;
		}

		this->SetInnerRML( formatted_items[currentItem].c_str() );

		SetValue( items[currentItem].c_str() );
	}

	// attributes
	String source_attribute;
	String fields_attribute;
	String valuefield_attribute;
	String data_formatter_attribute;

	StringList items;
	StringList formatted_items;
	int currentItem;
};

//================================

class UI_DataSpinnerInstancer : public ElementInstancer
{
public:
	/// Instances an element given the tag name and attributes.
	/// @param[in] parent The element the new element is destined to be parented to.
	/// @param[in] tag The tag of the element to instance.
	/// @param[in] attributes Dictionary of attributes.
	Rml::Core::ElementPtr InstanceElement( Rml::Core::Element *parent, const String &tag, const XMLAttributes &attr ) override {
		return ElementPtr(__new__( UI_DataSpinner )( tag, attr ));
	}

	/// Releases an element instanced by this instancer.
	/// @param[in] element The element to release.
	void ReleaseElement( Rml::Core::Element *element ) override {
		__delete__( element );
	}
};


//================================

ElementInstancer *GetDataSpinnerInstancer( void ) {
	return __new__( UI_DataSpinnerInstancer )();
}

}
