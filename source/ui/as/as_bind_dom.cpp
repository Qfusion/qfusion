#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"

#include "as/asui.h"
#include "as/asui_local.h"
#include "as/asui_url.h"

#include <RmlUi/Controls.h>
#include <RmlUi/Controls/ElementTabSet.h>
#include <RmlUi/Controls/ElementFormControlDataSelect.h>
#include "widgets/ui_image.h"

namespace ASUI
{

// dummy class since ASBIND only can only bind unique classes
// and AngelScript arrays are more like composite classes
class ASElementsArray : public CScriptArrayInterface
{
};
static asITypeInfo *elementsArrayType;

class ASStringsArray : public CScriptArrayInterface
{
};
static asITypeInfo *stringsArrayType;

typedef Rml::Controls::ElementForm ElementForm;
typedef Rml::Controls::ElementFormControl ElementFormControl;
typedef Rml::Controls::ElementFormControlDataSelect ElementFormControlDataSelect;

typedef Rml::Controls::ElementDataGrid ElementDataGrid;
typedef Rml::Controls::ElementDataGridRow ElementDataGridRow;

typedef Rml::Controls::ElementTabSet ElementTabSet;

typedef WSWUI::ElementImage ElementImage;

}

//==========================================================

ASBIND_TYPE( Rml::Controls::ElementForm, ElementForm );
ASBIND_TYPE( Rml::Controls::ElementFormControl, ElementFormControl );
ASBIND_TYPE( Rml::Controls::ElementFormControlDataSelect, ElementFormControlDataSelect );

ASBIND_TYPE( Rml::Controls::ElementDataGrid, ElementDataGrid );
ASBIND_TYPE( Rml::Controls::ElementDataGridRow, ElementDataGridRow );

ASBIND_TYPE( Rml::Controls::ElementTabSet, ElementTabSet );

ASBIND_TYPE( WSWUI::ElementImage, ElementImage );

// array of Element handlers
ASBIND_ARRAY_TYPE( ASUI::ASElementsArray, Element @ );
ASBIND_ARRAY_TYPE( ASUI::ASStringsArray, String @ );

//==============================================================

namespace ASUI
{

//
// EVENT

void PrebindEvent( ASInterface *as ) {
	ASBind::Class<Rml::Core::Event, ASBind::class_nocount>( as->getEngine() );
}

static Element *Event_GetTargetElement( Event *self ) {
	Element *e = self->GetTargetElement();
	return e;
}

// String -> asstring_t*
static asstring_t *Event_GetType( Event *self ) {
	return ASSTR( self->GetType() );
}

static asstring_t *Event_GetParameterS( Event *self, const asstring_t &a, const asstring_t &b ) {
	Rml::Core::String name = ASSTR( a );
	Rml::Core::String default_value = ASSTR( b );
	return ASSTR( self->GetParameter( name, default_value ) );
}

static int Event_GetParameterI( Event *self, const asstring_t &a, const int default_value ) {
	Rml::Core::String name = ASSTR( a );
	return self->GetParameter( name, default_value );
}

static unsigned Event_GetParameterU( Event *self, const asstring_t &a, const unsigned default_value ) {
	Rml::Core::String name = ASSTR( a );
	return self->GetParameter( name, default_value );
}

static float Event_GetParameterF( Event *self, const asstring_t &a, const float default_value ) {
	Rml::Core::String name = ASSTR( a );
	return self->GetParameter( name, default_value );
}

static bool Event_GetParameterB( Event *self, const asstring_t &a, const bool default_value ) {
	Rml::Core::String name = ASSTR( a );
	return self->GetParameter( name, default_value );
}

static CScriptDictionaryInterface *Event_GetParameters( Event *self ) {
	CScriptDictionaryInterface *dict = UI_Main::Get()->getAS()->createDictionary();
	int stringObjectTypeId = UI_Main::Get()->getAS()->getStringObjectType()->GetTypeId();

	const Rml::Core::Dictionary &parameters = self->GetParameters();

	Rml::Core::String name;
	Rml::Core::String value;
	for ( Rml::Core::Dictionary::const_iterator it = parameters.begin(); it != parameters.end(); ++it ) {
		const std::string &val = it->second.Get<std::string>();
		dict->Set( *( ASSTR( it->first ) ), ASSTR( val ), stringObjectTypeId );
	}

	return dict;
}

static void Event_StopPropagation( Event *self ) {
	self->StopPropagation();
}

static int Event_GetPhase( Event *self ) {
	return int(self->GetPhase());
}

void BindEvent( ASInterface *as ) {
	ASBind::Enum( as->getEngine(), "eEventPhase" )
		( "EVENT_PHASE_CAPTURE", int(EventPhase::Capture) )
		( "EVENT_PHASE_TARGET",  int(EventPhase::Target) )
		( "EVENT_PHASE_BUBBLE", int(EventPhase::Bubble) )
	;

	ASBind::Enum( as->getEngine(), "eInputKey" )
		( "KI_ESCAPE", Input::KI_ESCAPE )
		( "KI_0", Input::KI_0 )
		( "KI_1", Input::KI_1 )
		( "KI_2", Input::KI_2 )
		( "KI_3", Input::KI_3 )
		( "KI_4", Input::KI_4 )
		( "KI_5", Input::KI_5 )
		( "KI_6", Input::KI_6 )
		( "KI_7", Input::KI_7 )
		( "KI_8", Input::KI_8 )
		( "KI_9", Input::KI_9 )
	;

	// reference (without factory)
	ASBind::GetClass<Rml::Core::Event>( as->getEngine() )

	.method( &Event_GetType, "getType", true )
	.method( &Event_GetTargetElement, "getTarget", true )
	.method( &Event_GetParameterS, "getParameter", true )
	.method( &Event_GetParameterI, "getParameter", true )
	.method( &Event_GetParameterU, "getParameter", true )
	.method( &Event_GetParameterF, "getParameter", true )
	.method( &Event_GetParameterB, "getParameter", true )
	.method( &Event_GetParameters, "getParameters", true )
	.method( &Event_GetPhase, "getPhase", true )
	.method( &Event_StopPropagation, "stopPropagation", true )
	;
}

//==============================================================

//
// EVENT LISTENER

// EVENT LISTENER IS DANGEROUS, USES DUMMY REFERENCING!
void PrebindEventListener( ASInterface *as ) {
	ASBind::Class<Rml::Core::EventListener, ASBind::class_nocount>( as->getEngine() )
	;
}

//==============================================================

//
// ELEMENT

// TODO: investigate if "self" here needs some reference counting tricks?

// ch : note that the ordering in these wrapping functions went like this:
//  1) we need few wrapper functions to look-a-like jquery
//	2) we need to provide separate api for Form, Controls etc..
//	3) we need to convert all Rml::Core::String to asstring_t*
// and thats why you have loads of misc functions in the end that use strings

// dummy funcdef
static void Element_EventListenerCallback( Element *elem, Event *event ) {
}


static Element *Element_Factory( void ) {
	ElementPtr eptr = Factory::InstanceElement( NULL, "*", "#element", XMLAttributes() );
	if( eptr == nullptr ) {
		return nullptr;
	}
	return eptr.get();
}

static Element *Element_Factory2( Element *parent ) {
	ElementPtr eptr = Factory::InstanceElement( parent, "*", "#element", XMLAttributes() );
	if( eptr == nullptr ) {
		return nullptr;
	}
	return eptr.get();
}

static Element *Element_FactoryRML( Element *parent, const asstring_t &rml ) {
	ElementPtr eptr = Factory::InstanceElement( parent, "*", "#element", XMLAttributes() );
	if( eptr == nullptr ) {
		return nullptr;
	}

	Element *e = eptr.get();
	e->SetInnerRML( ASSTR( rml ) );
	return e;
}

static EventListener *Element_AddEventListener( Element *elem, const asstring_t &event, asIScriptFunction *func ) {
	EventListener *listener = CreateScriptEventCaller( UI_Main::Get()->getAS(), func );
	elem->AddEventListener( ASSTR( event ), listener );
	if( func ) {
		func->Release();
	}
	return listener;
}

static void Element_RemoveEventListener( Element *elem, const asstring_t &event, EventListener *listener ) {
	elem->RemoveEventListener( ASSTR( event ), listener );
}

// CSS
static Element *Element_AddClass( Element *self, const asstring_t &c ) {
	self->SetClass( ASSTR( c ), true );
	return self;
}

static Element *Element_RemoveClass( Element *self, const asstring_t &c ) {
	self->SetClass( ASSTR( c ), false );
	return self;
}

static Element *Element_ToggleClass( Element *self, const asstring_t &c ) {
	String sc( ASSTR( c ) );
	bool set = self->IsClassSet( sc );
	self->SetClass( sc, !set );
	return self;
}

static Element *Element_SetCSS( Element *self, const asstring_t &prop, const asstring_t &value ) {
	if( !value.len ) {
		self->RemoveProperty( ASSTR( prop ) );
	} else {
		self->SetProperty( ASSTR( prop ), ASSTR( value ) );
	}
	return self;
}

static asstring_t *Element_GetCSS( Element *self, const asstring_t &name ) {
	const Property* prop = self->GetProperty( ASSTR( name ) );
	return ASSTR( prop ? prop->ToString() : "" );
}

// NODES
static Element *Element_GetParentNode( Element *self ) {
	Element *e = self->GetParentNode();
	return e;
}

static Element *Element_GetNextSibling( Element *self ) {
	Element *e = self->GetNextSibling();
	return e;
}

static Element *Element_GetPreviousSibling( Element *self ) {
	Element *e = self->GetPreviousSibling();
	return e;
}

static Element *Element_GetFirstChild( Element *self ) {
	Element *e = self->GetFirstChild();
	return e;
}

static Element *Element_GetLastChild( Element *self ) {
	Element *e = self->GetLastChild();
	return e;
}

static Element *Element_GetChild( Element *self, unsigned int index ) {
	Element *e = self->GetChild( index );
	return e;
}

// CONTENTS

static asstring_t *Element_GetInnerRML( Element *elem ) {
	String srml;
	elem->GetInnerRML( srml );
	return ASSTR( srml );
}

static void Element_SetInnerRML( Element *elem, const asstring_t &rml ) {
	elem->SetInnerRML( ASSTR( rml ) );
}

// TODO: wrap all other functions like this
static Element *Element_GetElementById( Element *elem, const asstring_t &id ) {
	Element *r = elem->GetElementById( ASSTR( id ) );
	return r;
}

static ASElementsArray *Element_GetElementsByTagName( Element *elem, const asstring_t &tag ) {
	ElementList elements;

	elem->GetElementsByTagName( elements, ASSTR( tag ) );

	CScriptArrayInterface *arr = UI_Main::Get()->getAS()->createArray( elements.size(), elementsArrayType );
	if( !arr ) {
		return NULL;
	}

	unsigned int n = 0;
	for( ElementList::iterator it = elements.begin(); it != elements.end(); ++it ) {
		Element *child = *it;
		*( (Element **)arr->At( n++ ) ) = child;
	}

	return static_cast<ASElementsArray *>( arr );
}

static ASElementsArray *Element_GetElementsByClassName( Element *elem, const asstring_t &tag ) {
	ElementList elements;

	elem->GetElementsByClassName( elements, ASSTR( tag ) );

	CScriptArrayInterface *arr = UI_Main::Get()->getAS()->createArray( elements.size(), elementsArrayType );
	if( !arr ) {
		return NULL;
	}

	unsigned int n = 0;
	for( ElementList::iterator it = elements.begin(); it != elements.end(); ++it ) {
		Element *child = *it;
		*( (Element **)arr->At( n++ ) ) = child;
	}

	return static_cast<ASElementsArray *>( arr );
}

static ElementDocument *Element_GetOwnerDocument( Element *elem ) {
	ElementDocument *d = elem->GetOwnerDocument();
	return d;
}

//
//
// NOW THE TEDIOUS PART OF WRAPPING REST OF THE FUNCTIONS USING Rml::Core::String to use asstring_t* ...

static bool Element_SetProperty( Element *elem, const asstring_t &a, const asstring_t &b ) {
	return elem->SetProperty( ASSTR( a ), ASSTR( b ) );
}

static asstring_t *Element_GetProperty( Element *elem, const asstring_t &a ) {
	return ASSTR( elem->GetProperty<String>( ASSTR( a ) ) );
}

static void Element_RemoveProperty( Element *elem, const asstring_t &a ) {
	elem->RemoveProperty( ASSTR( a ) );
}

static void Element_SetClass( Element *elem, const asstring_t &a, bool b ) {
	elem->SetClass( ASSTR( a ), b );
}

static bool Element_IsClassSet( Element *elem, const asstring_t &a ) {
	return elem->IsClassSet( ASSTR( a ) );
}

static void Element_SetClassNames( Element *elem, const asstring_t &a ) {
	elem->SetClassNames( ASSTR( a ) );
}

static asstring_t *Element_GetClassNames( Element *elem ) {
	return ASSTR( elem->GetClassNames() );
}

static void Element_SetPseudoClass( Element *elem, const asstring_t &a, bool b ) {
	elem->SetPseudoClass( ASSTR( a ), b );
}

static bool Element_IsPseudoClassSet( Element *elem, const asstring_t &a ) {
	return elem->IsPseudoClassSet( ASSTR( a ) );
}

static Element *Element_SetAttributeS( Element *elem, const asstring_t &a, const asstring_t &b ) {
	elem->SetAttribute( ASSTR( a ), ASSTR( b ) );
	return elem;
}

static Element *Element_SetAttributeI( Element *elem, const asstring_t &a, const int b ) {
	elem->SetAttribute( ASSTR( a ), b );
	return elem;
}

static Element *Element_SetAttributeF( Element *elem, const asstring_t &a, const float b ) {
	elem->SetAttribute( ASSTR( a ), b );
	return elem;
}

static asstring_t *Element_GetAttributeS( Element *elem, const asstring_t &a, const asstring_t &b ) {
	return ASSTR( elem->GetAttribute<String>( ASSTR( a ), ASSTR( b ) ) );
}

static int Element_GetAttributeI( Element *elem, const asstring_t &a, const int b ) {
	return elem->GetAttribute<int>( ASSTR( a ), b );
}

static unsigned Element_GetAttributeU( Element *elem, const asstring_t &a, const unsigned b ) {
	return elem->GetAttribute<unsigned>( ASSTR( a ), b );
}

static float Element_GetAttributeF( Element *elem, const asstring_t &a, const float b ) {
	return elem->GetAttribute<float>( ASSTR( a ), b );
}

static bool Element_HasAttribute( Element *elem, const asstring_t &a ) {
	return elem->HasAttribute( ASSTR( a ) );
}

static void Element_RemoveAttribute( Element *elem, const asstring_t &a ) {
	elem->RemoveAttribute( ASSTR( a ) );
}

static asstring_t *Element_GetTagName( Element *elem ) {
	return ASSTR( elem->GetTagName() );
}

static asstring_t *Element_GetId( Element *elem ) {
	return ASSTR( elem->GetId() );
}

static void Element_SetId( Element *elem, const asstring_t &a ) {
	elem->SetId( ASSTR( a ) );
}

static float Element_GetContainingBlockWidth( Element *self ) {
	return self->GetContainingBlock().x;
}

static float Element_GetContainingBlockHeight( Element *self ) {
	return self->GetContainingBlock().y;
}

static float Element_ResolveNumericProperty( Element *self, const asstring_t &p ) {
	return self->ResolveNumericProperty( ASSTR( p ) );
}

//==============================================================

//
// FORM

static ElementForm *Element_CastToElementForm( Element *self ) {
	ElementForm *f = dynamic_cast<ElementForm *>( self );
	return f;
}

static Element *ElementForm_CastToElement( ElementForm *self ) {
	Element *e = dynamic_cast<Element *>( self );
	return e;
}

void ElementForm_Submit( ElementForm *self ) {
	self->Submit();
}

static void PreBindElementForm( ASInterface *as ) {
	ASBind::Class<ElementForm, ASBind::class_nocount>( as->getEngine() );
}

static void BindElementForm( ASInterface *as ) {
	asIScriptEngine *engine = as->getEngine();

	ASBind::GetClass<ElementForm>( engine )

	.method( &ElementForm_Submit, "submit", true )
	.refcast( &ElementForm_CastToElement, true, true )
	;

	// Cast behavior for the Element class
	ASBind::GetClass<Element>( engine )
	.refcast( &Element_CastToElementForm, true, true )
	;
}

//==============================================================

//
// TABSET

static ElementTabSet *Element_CastToElementTabSet( Element *self ) {
	ElementTabSet *f = dynamic_cast<ElementTabSet *>( self );
	return f;
}

static Element *ElementTabSet_CastToElement( ElementTabSet *self ) {
	Element *e = dynamic_cast<Element *>( self );
	return e;
}

/// Remove one of the tab set's panels and its corresponding tab.
static void ElementTabSet_RemoveTab( ElementTabSet *self, int tabIndex ) {
	self->RemoveTab( tabIndex );
}

/// Retrieve the number of tabs in the tabset.
static int ElementTabSet_GetNumTabs( ElementTabSet *self ) {
	return self->GetNumTabs();
}

/// Sets the currently active (visible) tab index.
static void ElementTabSet_SetActiveTab( ElementTabSet *self, int tabIndex ) {
	self->SetActiveTab( tabIndex );
}

/// Get the current active tab index.
static int ElementTabSet_GetActiveTab( ElementTabSet *self ) {
	return self->GetActiveTab();
}

static void PreBindElementTabSet( ASInterface *as ) {
	ASBind::Class<ElementTabSet, ASBind::class_nocount>( as->getEngine() );
}

static void BindElementTabSet( ASInterface *as ) {
	asIScriptEngine *engine = as->getEngine();

	ASBind::GetClass<ElementTabSet>( engine )


	.method( &ElementTabSet_RemoveTab, "removeTab", true )
	.constmethod( &ElementTabSet_GetNumTabs, "getNumTabs", true )
	.method( &ElementTabSet_SetActiveTab, "setActiveTab", true )
	.constmethod( &ElementTabSet_GetActiveTab, "getActiveTab", true )

	.refcast( &ElementTabSet_CastToElement, true, true )
	;

	// Cast behavior for the Element class
	ASBind::GetClass<Element>( engine )
	.refcast( &Element_CastToElementTabSet, true, true )
	;
}

//==============================================================

//
// DOCUMENT

static ElementDocument *Element_CastToElementDocument( Element *self ) {
	ElementDocument *d = dynamic_cast<ElementDocument *>( self );
	return d;
}

static Element *ElementDocument_CastToElement( ElementDocument *self ) {
	Element *e = dynamic_cast<Element *>( self );
	return e;
}

/// Returns URL of the current document.
static ASURL ElementDocument_GetURL( ElementDocument *self ) {
	return ASURL( self->GetSourceURL().c_str() );
}

/// Returns title of the current document.
static asstring_t *ElementDocument_GetTitle( ElementDocument *self ) {
	return ASSTR( self->GetTitle() );
}

/// Returns the BODY node of the current document.
static Element *ElementDocument_GetBody( ElementDocument *self ) {
	Element *e = dynamic_cast<Element *>( self );
	return e;
}

static void PreBindElementDocument( ASInterface *as ) {
	ASBind::Class<ElementDocument, ASBind::class_nocount>( as->getEngine() );
}

static void BindElementDocument( ASInterface *as ) {
	asIScriptEngine *engine = as->getEngine();

	ASBind::GetClass<ElementDocument>( engine )

	.constmethod( ElementDocument_GetURL, "get_URL", true )
	.constmethod( ElementDocument_GetTitle, "get_title", true )
	.constmethod( ElementDocument_GetBody, "get_body", true )

	.refcast( &ElementDocument_CastToElement, true, true )
	;

	// Cast behavior for the Element class
	ASBind::GetClass<Element>( engine )
	.refcast( &Element_CastToElementDocument, true, true )
	;
}

//==============================================================

//
// FORM CONTROLS

static ElementFormControl *Element_CastToElementFormControl( Element *self ) {
	ElementFormControl *f = dynamic_cast<ElementFormControl *>( self );
	return f;
}

static Element *ElementFormControl_CastToElement( ElementFormControl *self ) {
	Element *e = dynamic_cast<Element *>( self );
	return e;
}

static asstring_t *ElementFormControl_GetName( ElementFormControl *self ) {
	return ASSTR( self->GetName() );
}

static void ElementFormControl_SetName( ElementFormControl *self, const asstring_t &name ) {
	self->SetName( ASSTR( name ) );
}

static asstring_t *ElementFormControl_GetValue( ElementFormControl *self ) {
	return ASSTR( self->GetValue() );
}

static void ElementFormControl_SetValue( ElementFormControl *self, const asstring_t &value ) {
	self->SetValue( ASSTR( value ) );
}

static bool ElementFormControl_IsSubmitted( ElementFormControl *self ) {
	return self->IsSubmitted();
}

static bool ElementFormControl_IsDisabled( ElementFormControl *self ) {
	return self->IsDisabled();
}

static void ElementFormControl_SetDisabled( ElementFormControl *self, bool disable ) {
	self->SetDisabled( disable );
}

static void PreBindElementFormControl( ASInterface *as ) {
	ASBind::Class<ElementFormControl, ASBind::class_nocount>( as->getEngine() );
}

static void BindElementFormControl( ASInterface *as ) {
	asIScriptEngine *engine = as->getEngine();

	ASBind::GetClass<ElementFormControl>( engine )

	.constmethod( ElementFormControl_GetName, "get_name", true )
	.method( ElementFormControl_SetName, "set_name", true )
	.constmethod( ElementFormControl_GetValue, "get_value", true )
	.method( ElementFormControl_SetValue, "set_value", true )
	.constmethod( ElementFormControl_IsSubmitted, "get_submitted", true )
	.constmethod( ElementFormControl_IsDisabled, "get_disabled", true )
	.method( ElementFormControl_SetDisabled, "set_disabled", true )

	.refcast( &ElementFormControl_CastToElement, true, true )
	;

	// Cast behavior for the Element class
	ASBind::GetClass<Element>( engine )
	.refcast( &Element_CastToElementFormControl, true, true )
	;
}

//
// DATA SELECT

static ElementFormControlDataSelect *Element_CastToFormControlDataSelect( Element *self ) {
	ElementFormControlDataSelect *r = dynamic_cast<ElementFormControlDataSelect *>( self );
	return r;
}

static Element *FormControlDataSelect_CastToElement( ElementFormControlDataSelect *self ) {
	Element *e = dynamic_cast<Element *>( self );
	return e;
}

static ElementFormControlDataSelect *FormControl_CastToFormControlDataSelect( ElementFormControl *self ) {
	ElementFormControlDataSelect *r = dynamic_cast<ElementFormControlDataSelect *>( self );
	return r;
}

static ElementFormControl *FormControlDataSelect_CastToFormControl( ElementFormControlDataSelect *self ) {
	ElementFormControl *e = dynamic_cast<ElementFormControl *>( self );
	return e;
}

static void ElementFormControlDataSelect_SetDataSource( ElementFormControlDataSelect *self, const asstring_t &source ) {
	self->SetDataSource( ASSTR( source ) );
}

static void ElementFormControlDataSelect_SetSelection( ElementFormControlDataSelect *self, int selection ) {
	self->SetSelection( selection );
}

static int ElementFormControlDataSelect_GetSelection( ElementFormControlDataSelect *self ) {
	return self->GetSelection();
}

static int ElementFormControlDataSelect_GetNumOptions( ElementFormControlDataSelect *self ) {
	return self->GetNumOptions();
}

static int ElementFormControlDataSelect_AddOption( ElementFormControlDataSelect *self, const asstring_t &rml, const asstring_t &value, int before, bool selectable ) {
	return self->Add( ASSTR( rml ), ASSTR( value ), before, selectable );
}

static void ElementFormControlDataSelect_RemoveOption( ElementFormControlDataSelect *self, int index ) {
	self->Remove( index );
}

static void ElementFormControlDataSelect_RemoveAllOptions( ElementFormControlDataSelect *self ) {
	self->RemoveAll();
}

static void ElementFormControlDataSelect_Spin( ElementFormControlDataSelect *self, int dir ) {
	int sel = self->GetSelection() + dir;
	if( sel < 0 ) {
		sel = self->GetNumOptions() - 1;
	} else if( sel >= self->GetNumOptions() ) {
		sel = 0;
	}
	self->SetSelection( sel );
}

static void PreBindElementFormControlDataSelect( ASInterface *as ) {
	ASBind::Class<ElementFormControlDataSelect, ASBind::class_nocount>( as->getEngine() );
}

static void BindElementFormControlDataSelect( ASInterface *as ) {
	asIScriptEngine *engine = as->getEngine();

	ASBind::GetClass<ElementFormControlDataSelect>( engine )

	.method( &ElementFormControlDataSelect_SetDataSource, "setDataSource", true )
	.method( &ElementFormControlDataSelect_GetSelection, "getSelection", true )
	.method( &ElementFormControlDataSelect_SetSelection, "setSelection", true )
	.method( &ElementFormControlDataSelect_GetNumOptions, "getNumOptions", true )
	.method2( &ElementFormControlDataSelect_AddOption, "void addOption(const String &rml, const String &value, int before = -1, bool selectable = true)", true )
	.method( &ElementFormControlDataSelect_RemoveOption, "removeOption", true )
	.method( &ElementFormControlDataSelect_RemoveAllOptions, "removeAllOptions", true )
	.method( &ElementFormControlDataSelect_Spin, "spin", true )

	.refcast( &FormControlDataSelect_CastToElement, true, true )
	.refcast( &FormControlDataSelect_CastToFormControl, true, true )
	;

	// Cast behavior for the Element class
	ASBind::GetClass<Element>( engine )
	.refcast( &Element_CastToFormControlDataSelect, true, true )
	;

	// Cast behavior for the FormControl class
	ASBind::GetClass<ElementFormControl>( engine )
	.refcast( &FormControl_CastToFormControlDataSelect, true, true )
	;
}

//==============================================================

//
// DATA GRID ROW

static ElementDataGridRow *Element_CastToDataGridRow( Element *self ) {
	ElementDataGridRow *r = dynamic_cast<ElementDataGridRow *>( self );
	return r;
}

static Element *DataGridRow_CastToElement( ElementDataGridRow *self ) {
	Element *e = dynamic_cast<Element *>( self );
	return e;
}

static ElementDataGrid *DataGridRow_GetParentGrid( ElementDataGridRow *self ) {
	ElementDataGrid *g = self->GetParentGrid();
	return g;
}

static unsigned int DataGridRow_GetIndex( ElementDataGridRow *self ) {
	return self->GetParentRelativeIndex();
}

static void PreBindElementDataGridRow( ASInterface *as ) {
	ASBind::Class<ElementDataGridRow, ASBind::class_nocount>( as->getEngine() );
}

static void BindElementDataGridRow( ASInterface *as ) {
	asIScriptEngine *engine = as->getEngine();

	ASBind::GetClass<ElementDataGridRow>( engine )

	.method( &DataGridRow_GetParentGrid, "getParentGrid", true )
	.method( &DataGridRow_GetIndex, "getIndex", true )
	.refcast( &DataGridRow_CastToElement, true, true )
	;

	// Cast behavior for the Element class
	ASBind::GetClass<Element>( engine )
	.refcast( &Element_CastToDataGridRow, true, true )
	;
}

//
// DATA GRID

static ElementDataGrid *Element_CastToDataGrid( Element *self ) {
	ElementDataGrid *g = dynamic_cast<ElementDataGrid *>( self );
	return g;
}

static Element *DataGrid_CastToElement( ElementDataGrid *self ) {
	Element *e = dynamic_cast<Element *>( self );
	return e;
}

static ElementDataGridRow *DataGrid_GetRow( ElementDataGrid *self, unsigned int index ) {
	ElementDataGridRow *r = self->GetRow( index );
	return r;
}

static unsigned int DataGrid_GetNumRows( ElementDataGrid *self ) {
	return self->GetNumRows();
}

static ASStringsArray *DataGrid_GetFields( ElementDataGrid *self, int idx ) {
	const ElementDataGrid::Column *column = self->GetColumn( idx );

	if( !column ) {
		return NULL;
	}

	CScriptArrayInterface *arr = UI_Main::Get()->getAS()->createArray( column->fields.size(), stringsArrayType );
	if( !arr ) {
		return NULL;
	}

	unsigned int n = 0;
	for( Rml::Core::StringList::const_iterator it = column->fields.begin(); it != column->fields.end(); ++it ) {
		*( (asstring_t **)arr->At( n++ ) ) = ASSTR( *it );
	}

	return static_cast<ASStringsArray *>( arr );
}

static Element *DataGrid_GetColumnHeader( ElementDataGrid *self, int idx ) {
	const ElementDataGrid::Column *column = self->GetColumn( idx );
	if( !column ) {
		return NULL;
	}
	Element *e = column->header->GetChild( idx );
	return e;
}

static unsigned int DataGrid_GetNumColumns( ElementDataGrid *self ) {
	return self->GetNumColumns();
}

static void DataGrid_SetDataSource( ElementDataGrid *self, const asstring_t &source ) {
	self->SetDataSource( ASSTR( source ) );
}

static void PreBindElementDataGrid( ASInterface *as ) {
	ASBind::Class<ElementDataGrid, ASBind::class_nocount>( as->getEngine() );
}

static void BindElementDataGrid( ASInterface *as ) {
	asIScriptEngine *engine = as->getEngine();

	ASBind::GetClass<ElementDataGrid>( engine )

	.method( &DataGrid_GetRow, "getRow", true )
	.constmethod( &DataGrid_GetNumRows, "getNumRows", true )
	.constmethod( &DataGrid_GetFields, "getFields", true )
	.method( &DataGrid_GetColumnHeader, "getColumnHeader", true )
	.constmethod( &DataGrid_GetNumColumns, "getNumColumns", true )
	.method( &DataGrid_SetDataSource, "setDataSource", true )
	.refcast( &DataGrid_CastToElement, true, true )
	;

	// Cast behavior for the Element class
	ASBind::GetClass<Element>( engine )
	.refcast( &Element_CastToDataGrid, true, true )
	;
}

//==============================================================

//
// IMAGE

static ElementImage *Element_CastToElementImage( Element *self ) {
	ElementImage *f = dynamic_cast<ElementImage *>( self );
	return f;
}

static Element *ElementImage_CastToElement( ElementImage *self ) {
	Element *e = dynamic_cast<Element *>( self );
	return e;
}

static float ElementImage_GetWidth( ElementImage *self ) {
	Rml::Core::Vector2f dimensions;
	self->GetIntrinsicDimensions( dimensions );
	return dimensions.x;
}

static float ElementImage_GetHeight( ElementImage *self ) {
	Rml::Core::Vector2f dimensions;
	self->GetIntrinsicDimensions( dimensions );
	return dimensions.y;
}

static void PreBindElementImage( ASInterface *as ) {
	ASBind::Class<ElementImage, ASBind::class_nocount>( as->getEngine() );
}

static void BindElementImage( ASInterface *as ) {
	asIScriptEngine *engine = as->getEngine();

	ASBind::GetClass<ElementImage>( engine )

	.method( ElementImage_GetWidth, "get_width", true )
	.method( ElementImage_GetHeight, "get_height", true )

	.refcast( &ElementImage_CastToElement, true, true )
	;

	// Cast behavior for the Element class
	ASBind::GetClass<Element>( engine )
	.refcast( &Element_CastToElementImage, true, true )
	;
}

//==============================================================

//
//
// Bind

void PrebindElement( ASInterface *as ) {
	ASBind::Class<Rml::Core::Element, ASBind::class_nocount>( as->getEngine() );

	PreBindElementDocument( as );

	PreBindElementDataGrid( as );

	PreBindElementDataGridRow( as );

	PreBindElementForm( as );

	PreBindElementFormControl( as );

	PreBindElementFormControlDataSelect( as );

	PreBindElementTabSet( as );

	PreBindElementImage( as );
}

void BindElement( ASInterface *as ) {
	asIScriptEngine *engine = as->getEngine();

	ASBind::Global( as->getEngine() )

	// setTimeout and setInterval callback funcdefs
	.funcdef( &Element_EventListenerCallback, "DOMEventListenerCallback" )
	;

	// Elements are bound as reference types
	ASBind::GetClass<Element>( engine )
	.factory( &Element_Factory )
	.factory( &Element_Factory2 )
	.factory( &Element_FactoryRML )
	
	// css/style
	.method( &Element_SetProperty, "setProp", true )
	.method( &Element_GetProperty, "getProp", true )
	.method( &Element_RemoveProperty, "removeProp", true )

	// jquery-like
	.method( &Element_SetCSS, "css", true )         // css('prop', '') removes the property
	.method( &Element_GetCSS, "css", true )

	// classes TODO: make addClass, removeClass etc.. like in jQuery
	.method( &Element_SetClass, "setClass", true )
	.method( &Element_IsClassSet, "hasClass", true )
	.method( &Element_SetClassNames, "setClasses", true )
	.method( &Element_GetClassNames, "getClasses", true )
	.method( &Element_AddClass, "addClass", true )
	.method( &Element_RemoveClass, "removeClass", true )
	.method( &Element_ToggleClass, "toggleClass", true )
	.method( &Element_SetClass, "toggleClass", true )           // note alias to setClass
	// pseudo-classes
	.method( &Element_SetPseudoClass, "togglePseudo", true )
	.method( &Element_IsPseudoClassSet, "hasPseudo", true )

	// html attributes
	.method( &Element_SetAttributeS, "setAttr", true )
	.method( &Element_SetAttributeI, "setAttr", true )
	.method( &Element_SetAttributeF, "setAttr", true )
	.method( &Element_GetAttributeS, "getAttr", true )
	.method( &Element_GetAttributeI, "getAttr", true )
	.method( &Element_GetAttributeU, "getAttr", true )
	.method( &Element_GetAttributeF, "getAttr", true )
	.method( &Element_HasAttribute, "hasAttr", true )
	.method( &Element_RemoveAttribute, "removeAttr", true )
	.method( &Element::GetNumAttributes, "numAttr" )

	// dom
	.constmethod( &Element_GetTagName, "get_tagName", true )
	.constmethod( &Element_GetId, "get_id", true )
	.method( &Element_SetId, "set_id", true )

	.method( &Element_GetParentNode, "getParent", true )
	.method( &Element_GetNextSibling, "getNextSibling", true )
	.method( &Element_GetPreviousSibling, "getPrevSibling", true )
	.method( &Element_GetFirstChild, "firstChild", true )
	.method( &Element_GetLastChild, "lastChild", true )
	.method2( &Element::GetNumChildren, "uint getNumChildren( bool includeNonDomElements = false )" )
	.method( &Element_GetChild, "getChild", true )
	.constmethod( &Element_GetInnerRML, "getInnerRML", true )
	.method( &Element_SetInnerRML, "setInnerRML", true )

	.method( &Element::Focus, "focus" )
	.method( &Element::Blur, "unfocus" )
	.method( &Element::Click, "click" )
	.method( &Element::HasChildNodes, "hasChildren" )

	.method( Element_GetElementById, "getElementById", true )
	.method( Element_GetElementsByTagName, "getElementsByTagName", true )
	.method( Element_GetElementsByClassName, "getElementsByClassName", true )
	.method( Element_GetOwnerDocument, "get_ownerDocument", true )

	.method2( Element_AddEventListener, "void addEventListener( const String &event, DOMEventListenerCallback @callback )", true )
	.method( Element_RemoveEventListener, "removeEventListener", true )

	.method( &Element::GetClientLeft, "clientLeft" )
	.method( &Element::GetClientTop, "clientTop" )
	.method( &Element::GetClientHeight, "clientHeight" )
	.method( &Element::GetClientWidth, "clientWidth" )

	.method( &Element::GetOffsetParent, "offsetParent" )
	.method( &Element::GetOffsetLeft, "offsetLeft" )
	.method( &Element::GetOffsetTop, "offsetTop" )
	.method( &Element::GetOffsetHeight, "offsetHeight" )
	.method( &Element::GetOffsetWidth, "offsetWidth" )

	.method( &Element::GetScrollLeft, "scrollLeft" )
	.method( &Element::SetScrollLeft, "scrollLeft" )
	.method( &Element::GetScrollTop, "scrollTop" )
	.method( &Element::SetScrollTop, "scrollTop" )
	.method( &Element::GetScrollHeight, "scrollHeight" )
	.method( &Element::GetScrollWidth, "scrollWidth" )

	.method( &Element::GetAbsoluteLeft, "absLeft" )
	.method( &Element::GetAbsoluteTop, "absTop" )

	.method( &Element_GetContainingBlockWidth, "containingBlockWith", true )
	.method( &Element_GetContainingBlockHeight, "containingBlockHeight", true )

	.method( &Element_ResolveNumericProperty, "resolveNumericProperty", true )
	;

	// cache type id for array<Element @>
	elementsArrayType = engine->GetTypeInfoById( engine->GetTypeIdByDecl( ASBind::typestr<ASElementsArray>() ) );

	// cache type id for array<String @>
	stringsArrayType = engine->GetTypeInfoById( engine->GetTypeIdByDecl( ASBind::typestr<ASStringsArray>() ) );

	// ElementDocument
	BindElementDocument( as );

	// ElementDataGrid
	BindElementDataGrid( as );

	// ElementDataGridRow
	BindElementDataGridRow( as );

	// ElementForm
	BindElementForm( as );

	// ElementFormControl
	BindElementFormControl( as );

	// ElementFormControlDataSelect
	BindElementFormControlDataSelect( as );

	// ElementTabSet
	BindElementTabSet( as );

	// ElementImage
	BindElementImage( as );
}

}
