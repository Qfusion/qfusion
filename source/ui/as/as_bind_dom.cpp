#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"

#include "as/asui.h"
#include "as/asui_local.h"
#include "as/asui_url.h"

#include <Rocket/Controls.h>
#include <Rocket/Controls/ElementTabSet.h>
#include <Rocket/Controls/ElementFormControlDataSelect.h>

// macro to addref a return object (rocket element)
#define _RETREF(a)	if( (a) ) { (a)->AddReference(); } return (a);
// macro to addref
#define _ADDREF(a)	if( (a) ) { (a)->AddReference(); }
// macro to remref from a parameter (rocket element)
#define _DECREF(a)	if( (a) ) { (a)->RemoveReference(); }

namespace ASUI {

// dummy class since ASBIND only can only bind unique classes 
// and AngelScript arrays are more like composite classes
typedef CScriptArrayInterface ASElementsArray;
static asIObjectType *elementsArrayType;

typedef Rocket::Controls::ElementForm ElementForm;
typedef Rocket::Controls::ElementFormControl ElementFormControl;
typedef Rocket::Controls::ElementFormControlDataSelect ElementFormControlDataSelect;

typedef Rocket::Controls::ElementDataGrid ElementDataGrid;
typedef Rocket::Controls::ElementDataGridRow ElementDataGridRow;

typedef Rocket::Controls::ElementTabSet ElementTabSet;

}

//==========================================================

ASBIND_TYPE( Rocket::Controls::ElementForm, ElementForm );
ASBIND_TYPE( Rocket::Controls::ElementFormControl, ElementFormControl );
ASBIND_TYPE( Rocket::Controls::ElementFormControlDataSelect, ElementFormControlDataSelect );

ASBIND_TYPE( Rocket::Controls::ElementDataGrid, ElementDataGrid );
ASBIND_TYPE( Rocket::Controls::ElementDataGridRow, ElementDataGridRow );

ASBIND_TYPE( Rocket::Controls::ElementTabSet, ElementTabSet );

// array of Element handlers
ASBIND_ARRAY_TYPE( ASUI::ASElementsArray, Element @ );

//==============================================================

namespace ASUI {

//
// EVENT

void PrebindEvent( ASInterface *as )
{
	ASBind::Class<Rocket::Core::Event, ASBind::class_ref>( as->getEngine() );
}

static Element *Event_GetTargetElement( Event *self ) {
	Element *e = self->GetTargetElement();
	_RETREF(e);
}

// String -> asstring_t*
static asstring_t *Event_GetType( Event *self ) {
	return ASSTR( self->GetType() );
}

static asstring_t *Event_GetParameter( Event *self, const asstring_t &a, const asstring_t &b ) {
	Rocket::Core::String name = ASSTR(a);
	Rocket::Core::String default_value = ASSTR(b);
	return ASSTR( self->GetParameter( name, default_value ) );
}

static CScriptDictionaryInterface *Event_GetParameters( Event *self ) {
	CScriptDictionaryInterface *dict = UI_Main::Get()->getAS()->createDictionary();
	int stringObjectTypeId = UI_Main::Get()->getAS()->getStringObjectType()->GetTypeId();

	const Rocket::Core::Dictionary *parameters = self->GetParameters();

	int index = 0;
	Rocket::Core::String name;
	Rocket::Core::String value;
	while( parameters->Iterate( index, name, value ) ) {
		dict->Set( *(ASSTR( name )), ASSTR( value ), stringObjectTypeId );
	}

	return dict;
}

static void Event_StopPropagation( Event *self ) {
	self->StopPropagation();
}

static int Event_GetPhase( Event *self ) {
	return self->GetPhase();
}

void BindEvent( ASInterface *as )
{
	ASBind::Enum( as->getEngine(), "eEventPhase" )
		( "EVENT_PHASE_UNKNOWN", Event::PHASE_UNKNOWN )
		( "EVENT_PHASE_CAPTURE", Event::PHASE_CAPTURE )
		( "EVENT_PHASE_TARGET",  Event::PHASE_TARGET )
		( "EVENT_PHASE_BUBBLE", Event::PHASE_BUBBLE )
		;

	// reference (without factory)
	ASBind::GetClass<Rocket::Core::Event>( as->getEngine() )
		.refs( &Event::AddReference, &Event::RemoveReference )

		.method( &Event_GetType, "getType", true )
		.method( &Event_GetTargetElement, "getTarget", true )
		.method( &Event_GetParameter, "getParameter", true )
		.method( &Event_GetParameters, "getParameters", true )
		.method( &Event_GetPhase, "getPhase", true )
		.method( &Event_StopPropagation, "stopPropagation", true )
		;
}

//==============================================================

//
// EVENT LISTENER

// EVENT LISTENER IS DANGEROUS, USES DUMMY REFERENCING!
void PrebindEventListener( ASInterface *as )
{
	ASBind::Class<Rocket::Core::EventListener, ASBind::class_nocount>( as->getEngine() )
		;
}

//==============================================================

//
// ELEMENT

// TODO: investigate if "self" here needs some reference counting tricks?

// ch : note that the ordering in these wrapping functions went like this:
// 	1) we need to wrap a few functions to handle reference-counting
// 	2) we need few wrapper functions to look-a-like jquery
//	3) we need to provide separate api for Form, Controls etc..
//	4) we need to convert all Rocket::Core::String to asstring_t*
// and thats why you have loads of misc functions in the end that use strings

// dummy funcdef
static void Element_EventListenerCallback( Element *elem, Event *event )
{
}

static EventListener *Element_AddEventListener( Element *elem, const asstring_t &event, asIScriptFunction *func ) {
	EventListener *listener = CreateScriptEventCaller( UI_Main::Get()->getAS(), func );
	elem->AddEventListener( ASSTR(event), listener );
	if( func ) {
		func->Release();
	}
	return listener;	// RETREF?
}

static void Element_RemoveEventListener( Element *elem, const asstring_t &event, EventListener *listener ) {
	elem->RemoveEventListener( ASSTR(event), listener );
	// _DECREF(listener);
}

// CSS
static Element *Element_AddClass(Element *self, const asstring_t &c) {
	self->SetClass( ASSTR(c), true );
	_RETREF(self);
}

static Element *Element_RemoveClass(Element *self, const asstring_t &c) {
	self->SetClass( ASSTR(c), false );
	_RETREF(self);
}

static Element *Element_ToggleClass(Element *self, const asstring_t &c) {
	String sc( ASSTR(c) );
	bool set = self->IsClassSet( sc );
	self->SetClass( sc, !set );
	_RETREF(self);
}

static Element *Element_SetCSS(Element *self, const asstring_t &prop, const asstring_t &value) {
	if( !value.len )
		self->RemoveProperty( ASSTR(prop) );
	else
		self->SetProperty( ASSTR(prop), ASSTR(value) );
	_RETREF(self);
}

static asstring_t *Element_GetCSS(Element *self, const asstring_t &prop) {
	return ASSTR( self->GetProperty<String>( ASSTR(prop) ) );
}

// NODES
static Element *Element_GetParentNode(Element *self) {
	Element *e = self->GetParentNode();
	_RETREF(e);
}

static Element *Element_GetNextSibling(Element *self) {
	Element *e = self->GetNextSibling();
	_RETREF(e);
}

static Element *Element_GetPreviousSibling(Element *self) {
	Element *e = self->GetPreviousSibling();
	_RETREF(e);
}

static Element *Element_GetFirstChild(Element *self) {
	Element *e = self->GetFirstChild();
	_RETREF(e);
}

static Element *Element_GetLastChild(Element *self) {
	Element *e = self->GetLastChild();
	_RETREF(e);
}

static Element *Element_GetChild(Element *self, unsigned int index) {
	Element *e = self->GetChild( index );
	_RETREF(e);
}

static void Element_AppendChild(Element *self, Element *child) {
	self->AppendChild(child);
	_DECREF(child);
}

static void Element_InsertBefore(Element *self, Element *a, Element *b) {
	self->InsertBefore(a,b);
	_DECREF(a);
	_DECREF(b);
}

static void Element_RemoveChild(Element *self, Element *a) {
	self->RemoveChild(a);
	_DECREF(a);
}

static Element *Element_Clone(Element *self) {
	Element *e = self->Clone();
	_RETREF(e);
}

// CONTENTS

static asstring_t *Element_GetInnerRML( Element *elem ) {
	String srml;
	elem->GetInnerRML( srml );
	return ASSTR( srml );
}

static void Element_SetInnerRML( Element *elem, const asstring_t &rml ) {
	elem->SetInnerRML( ASSTR(rml) );
}

// TODO: wrap all other functions like this
static Element *Element_GetElementById( Element *elem, const asstring_t &id ) {
	Element *r = elem->GetElementById( ASSTR(id) );
	_RETREF(r);
}

static ASElementsArray *Element_GetElementsByTagName( Element *elem, const asstring_t &tag )
{
	ElementList elements;

	elem->GetElementsByTagName( elements, ASSTR( tag ) );

	ASElementsArray *arr = UI_Main::Get()->getAS()->createArray( elements.size(), elementsArrayType );

	unsigned int n = 0;
	for( ElementList::iterator it = elements.begin(); it != elements.end(); ++it ) {
		Element *child = *it;
		child->AddReference();
		*((Element **)arr->At(n++)) = child;
	}

	return arr;
}

static ASElementsArray *Element_GetElementsByClassName( Element *elem, const asstring_t &tag )
{
	ElementList elements;

	elem->GetElementsByClassName( elements, ASSTR( tag ) );

	ASElementsArray *arr = UI_Main::Get()->getAS()->createArray( elements.size(), elementsArrayType );

	unsigned int n = 0;
	for( ElementList::iterator it = elements.begin(); it != elements.end(); ++it ) {
		Element *child = *it;
		child->AddReference();
		*((Element **)arr->At(n++)) = child;
	}

	return arr;
}

static ElementDocument *Element_GetOwnerDocument( Element *elem ) {
	ElementDocument *d = elem->GetOwnerDocument();
	_RETREF(d);
}

//
//
// NOW THE TEDIOUS PART OF WRAPPING REST OF THE FUNCTIONS USING Rocket::Core::String to use asstring_t* ...

static bool Element_SetProperty( Element *elem, const asstring_t &a, const asstring_t &b ) {
	return elem->SetProperty( ASSTR(a), ASSTR(b) );
}

static asstring_t *Element_GetProperty( Element *elem, const asstring_t &a ) {
	return ASSTR( elem->GetProperty<String>( ASSTR(a) ) );
}

static void Element_RemoveProperty( Element *elem, const asstring_t &a ) {
	elem->RemoveProperty( ASSTR(a) );
}

static void Element_SetClass( Element *elem, const asstring_t &a, bool b ) {
	elem->SetClass( ASSTR(a), b );
}

static bool Element_IsClassSet( Element *elem, const asstring_t &a ) {
	return elem->IsClassSet( ASSTR(a) );
}

static void Element_SetClassNames( Element *elem, const asstring_t &a ) {
	elem->SetClassNames( ASSTR(a) );
}

static asstring_t *Element_GetClassNames( Element *elem ) {
	return ASSTR( elem->GetClassNames() );
}

static void Element_SetPseudoClass( Element *elem, const asstring_t &a, bool b ) {
	elem->SetPseudoClass( ASSTR(a), b );
}

static bool Element_IsPseudoClassSet( Element *elem, const asstring_t &a ) {
	return elem->IsPseudoClassSet( ASSTR(a) );
}

static Element *Element_SetAttribute( Element *elem, const asstring_t &a, const asstring_t &b ) {
	elem->SetAttribute( ASSTR(a), ASSTR(b) );
	_RETREF(elem);
}

static asstring_t *Element_GetAttribute( Element *elem, const asstring_t &a, const asstring_t &b ) {
	return ASSTR( elem->GetAttribute<String>( ASSTR(a), ASSTR(b) ) );
}

static bool Element_HasAttribute( Element *elem, const asstring_t &a ) {
	return elem->HasAttribute( ASSTR(a) );
}

static void Element_RemoveAttribute( Element *elem, const asstring_t &a ) {
	elem->RemoveAttribute( ASSTR(a) );
}

static asstring_t *Element_GetTagName( Element *elem ) {
	return ASSTR( elem->GetTagName() );
}

static asstring_t *Element_GetId( Element *elem ) {
	return ASSTR( elem->GetId() );
}

static void Element_SetId( Element *elem, const asstring_t &a ) {
	elem->SetId( ASSTR(a) );
}

//==============================================================

//
// FORM

static ElementForm *Element_CastToElementForm( Element *self ) {
	ElementForm *f = dynamic_cast<ElementForm *>( self );
	_RETREF(f);
}

static Element *ElementForm_CastToElement( ElementForm *self ) {
	Element *e = dynamic_cast<Element *>( self );
	_RETREF(e);
}

void ElementForm_Submit(ElementForm *self) {
	self->Submit();
}

static void PreBindElementForm( ASInterface *as )
{
	ASBind::Class<ElementForm, ASBind::class_ref>( as->getEngine() );
}

static void BindElementForm( ASInterface *as )
{
	asIScriptEngine *engine = as->getEngine();

	ASBind::GetClass<ElementForm>( engine )
		.refs( &ElementForm::AddReference, &ElementForm::RemoveReference )

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
	_RETREF(f);
}

static Element *ElementTabSet_CastToElement( ElementTabSet *self ) {
	Element *e = dynamic_cast<Element *>( self );
	_RETREF(e);
}

/// Sets the specifed tab index's tab title RML.
static void ElementTabSet_SetTab( ElementTabSet *self, int tabIndex, const asstring_t & rml ) {
	self->SetTab( tabIndex, ASSTR( rml ) );
}

/// Sets the specifed tab index's tab panel RML.
static void ElementTabSet_SetPanel( ElementTabSet *self, int tabIndex, const asstring_t & rml ) {
	self->SetPanel( tabIndex, ASSTR( rml ) );
}

/// Set the specifed tab index's title element.
static void ElementTabSet_SetTab( ElementTabSet *self, int tabIndex, Element *e ) {
	self->SetTab( tabIndex, e );
	_DECREF(e);
}

/// Set the specified tab index's body element.
static void ElementTabSet_SetPanel( ElementTabSet *self, int tabIndex, Element *e ) {
	self->SetPanel( tabIndex, e );
	_DECREF(e);
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

static void PreBindElementTabSet( ASInterface *as )
{
	ASBind::Class<ElementTabSet, ASBind::class_ref>( as->getEngine() );
}

static void BindElementTabSet( ASInterface *as )
{
	asIScriptEngine *engine = as->getEngine();

	ASBind::GetClass<ElementTabSet>( engine )
		.refs( &ElementTabSet::AddReference, &ElementTabSet::RemoveReference )

		.method<void ( ElementTabSet *, int , const asstring_t & )>( &ElementTabSet_SetTab, "setTab", true )
		.method<void ( ElementTabSet *, int , Element * )>( &ElementTabSet_SetTab, "setTab", true )
		.method<void ( ElementTabSet *, int , const asstring_t & )>( &ElementTabSet_SetPanel, "setPanel", true )
		.method<void ( ElementTabSet *, int , Element * )>( &ElementTabSet_SetPanel, "setPanel", true )
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
	_RETREF(d);
}

static Element *ElementDocument_CastToElement( ElementDocument *self ) {
	Element *e = dynamic_cast<Element *>( self );
	_RETREF(e);
}

/// Returns URL of the current document.
static ASURL ElementDocument_GetURL( ElementDocument *self )
{
	return ASURL( self->GetSourceURL().CString() );
}

/// Returns title of the current document.
static asstring_t *ElementDocument_GetTitle( ElementDocument *self ) 
{
	return ASSTR( self->GetTitle() );
}

/// Returns the BODY node of the current document.
static Element *ElementDocument_GetBody( ElementDocument *self ) 
{
	Element *e = dynamic_cast<Element *>( self );
	_RETREF(e);
}

static void PreBindElementDocument( ASInterface *as )
{
	ASBind::Class<ElementDocument, ASBind::class_ref>( as->getEngine() );
}

static void BindElementDocument( ASInterface *as )
{
	asIScriptEngine *engine = as->getEngine();

	ASBind::GetClass<ElementDocument>( engine )
		.refs( &ElementDocument::AddReference, &ElementDocument::RemoveReference )

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
	_RETREF(f);
}

static Element *ElementFormControl_CastToElement( ElementFormControl *self ) {
	Element *e = dynamic_cast<Element *>( self );
	_RETREF(e);
}

static asstring_t *ElementFormControl_GetName( ElementFormControl *self ) {
	return ASSTR( self->GetName() );
}

static void ElementFormControl_SetName( ElementFormControl *self, const asstring_t &name ) {
	self->SetName( ASSTR( name ) );
}

static asstring_t *ElementFormControl_GetValue( ElementFormControl *self) {
	return ASSTR( self->GetValue() );
}

static void ElementFormControl_SetValue( ElementFormControl *self, const asstring_t &value ) {
	self->SetValue( ASSTR(value) );
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

static void PreBindElementFormControl( ASInterface *as )
{
	ASBind::Class<ElementFormControl, ASBind::class_ref>( as->getEngine() );
}

static void BindElementFormControl( ASInterface *as )
{
	asIScriptEngine *engine = as->getEngine();

	ASBind::GetClass<ElementFormControl>( engine )
		.refs( &ElementFormControl::AddReference, &ElementFormControl::RemoveReference )

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
	_RETREF(r);
}

static Element *FormControlDataSelect_CastToElement( ElementFormControlDataSelect *self ) {
	Element *e = dynamic_cast<Element *>( self );
	_RETREF(e);
}

static ElementFormControlDataSelect *FormControl_CastToFormControlDataSelect( ElementFormControl *self ) {
	ElementFormControlDataSelect *r = dynamic_cast<ElementFormControlDataSelect *>( self );
	_RETREF(r);
}

static ElementFormControl *FormControlDataSelect_CastToFormControl( ElementFormControlDataSelect *self ) {
	ElementFormControl *e = dynamic_cast<ElementFormControl *>( self );
	_RETREF(e);
}

static void ElementFormControlDataSelect_SetDataSource( ElementFormControlDataSelect *self, const asstring_t &source ) {
	self->SetDataSource( ASSTR( source ) );
}

static void PreBindElementFormControlDataSelect( ASInterface *as )
{
	ASBind::Class<ElementFormControlDataSelect, ASBind::class_ref>( as->getEngine() );
}

static void BindElementFormControlDataSelect( ASInterface *as )
{
	asIScriptEngine *engine = as->getEngine();

	ASBind::GetClass<ElementFormControlDataSelect>( engine )
		.refs( &ElementFormControlDataSelect::AddReference, &ElementFormControlDataSelect::RemoveReference )

		.method( &ElementFormControlDataSelect_SetDataSource, "setDataSource", true )

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
	_RETREF(r);
}

static Element *DataGridRow_CastToElement( ElementDataGridRow *self ) {
	Element *e = dynamic_cast<Element *>( self );
	_RETREF(e);
}

static ElementDataGrid *DataGridRow_GetParentGrid( ElementDataGridRow *self ) {
	ElementDataGrid *g = self->GetParentGrid();
	_RETREF(g);
}

static unsigned int DataGridRow_GetIndex( ElementDataGridRow *self ) {
	return self->GetParentRelativeIndex();
}

static void PreBindElementDataGridRow( ASInterface *as )
{
	ASBind::Class<ElementDataGridRow, ASBind::class_ref>( as->getEngine() );
}

static void BindElementDataGridRow( ASInterface *as )
{
	asIScriptEngine *engine = as->getEngine();

	ASBind::GetClass<ElementDataGridRow>( engine )
		.refs( &ElementDataGridRow::AddReference, &ElementDataGridRow::RemoveReference )

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
	_RETREF(g);
}

static Element *DataGrid_CastToElement( ElementDataGrid *self ) {
	Element *e = dynamic_cast<Element *>( self );
	_RETREF(e);
}

static ElementDataGridRow *DataGrid_GetRow( ElementDataGrid *self, unsigned int index ) {
	ElementDataGridRow *r = self->GetRow( index );
	_RETREF(r);
}

static unsigned int DataGrid_GetNumRows( ElementDataGrid *self ) {
	return self->GetNumRows();
}

static asstring_t *DataGrid_GetColumn( ElementDataGrid *self, int idx ) {
	// Tricky SOB, build a string from column->fields
	const ElementDataGrid::Column *column = self->GetColumn( idx );
	if( !column )
		return ASSTR( "" );
	String ret;
	for( StringList::const_iterator it = column->fields.begin(); it != column->fields.end(); ++it) {
		ret += *it + " ";
	}
	return ASSTR( ret.Substring( 0, std::max( 0U, ret.Length() - 1 ) ) );
}

static void DataGrid_SetDataSource( ElementDataGrid *self, const asstring_t &source ) {
	self->SetDataSource( ASSTR( source ) );
}

static void PreBindElementDataGrid( ASInterface *as )
{
	ASBind::Class<ElementDataGrid, ASBind::class_ref>( as->getEngine() );
}

static void BindElementDataGrid( ASInterface *as )
{
	asIScriptEngine *engine = as->getEngine();

	ASBind::GetClass<ElementDataGrid>( engine )
		.refs( &ElementDataGrid::AddReference, &ElementDataGrid::RemoveReference )

		.method( &DataGrid_GetRow, "getRow", true )
		.method( &DataGrid_GetNumRows, "getNumRows", true )
		.method( &DataGrid_GetColumn, "getColumn", true )
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
//
// Bind

void PrebindElement( ASInterface *as )
{
	ASBind::Class<Rocket::Core::Element, ASBind::class_ref>( as->getEngine() );

	PreBindElementDocument( as );

	PreBindElementDataGrid( as );

	PreBindElementDataGridRow( as );

	PreBindElementForm( as );

	PreBindElementFormControl( as );

	PreBindElementFormControlDataSelect( as );

	PreBindElementTabSet( as );
}

void BindElement( ASInterface *as )
{
	asIScriptEngine *engine = as->getEngine();

	ASBind::Global( as->getEngine() )
		// setTimeout and setInterval callback funcdefs
		.funcdef( &Element_EventListenerCallback, "EventListenerCallback" )
	;

	// Elements are bound as reference types that cant be explicitly constructed
	ASBind::GetClass<Element>( engine )
		.refs( &Element::AddReference, &Element::RemoveReference )

		// css/style
		.method( &Element_SetProperty, "setProp", true )
		.method( &Element_GetProperty, "getProp", true )
		.method( &Element_RemoveProperty, "removeProp", true )
		// jquery-like
		.method( &Element_SetCSS, "css", true )		// css('prop', '') removes the property
		.method( &Element_GetCSS, "css", true )

		// classes TODO: make addClass, removeClass etc.. like in jQuery
		.method( &Element_SetClass, "setClass", true )
		.method( &Element_IsClassSet, "hasClass", true )
		.method( &Element_SetClassNames, "setClasses", true )
		.method( &Element_GetClassNames, "getClasses", true )
		.method( &Element_AddClass, "addClass", true )
		.method( &Element_RemoveClass, "removeClass", true )
		.method( &Element_ToggleClass, "toggleClass", true )
		.method( &Element_SetClass, "toggleClass", true )		// note alias to setClass
		// pseudo-classes
		.method( &Element_SetPseudoClass, "togglePseudo", true )
		.method( &Element_IsPseudoClassSet, "hasPseudo", true )

		// html attributes
		.method( &Element_SetAttribute, "setAttr", true )
		.method( &Element_GetAttribute, "getAttr", true )
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
		.method( &Element_AppendChild, "addChild", true )
		.method( &Element_InsertBefore, "insertChild", true )
		.method( &Element_RemoveChild, "removeChild", true )
		.method( &Element::HasChildNodes, "hasChildren" )
		.method( Element_Clone, "clone", true )

		.method( Element_GetElementById, "getElementById", true )
		.method( Element_GetElementsByTagName, "getElementsByTagName", true )
		.method( Element_GetElementsByClassName, "getElementsByClassName", true )
		.method( Element_GetOwnerDocument, "get_ownerDocument", true )

		.method2( Element_AddEventListener, "void addEventListener( const String &event, EventListenerCallback @callback )", true )
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
		;

	// cache type id for array<Element @>
	elementsArrayType = engine->GetObjectTypeById(engine->GetTypeIdByDecl(ASBind::typestr<ASElementsArray>()));

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
}

}
