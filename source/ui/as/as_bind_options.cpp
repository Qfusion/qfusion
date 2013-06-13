#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"

#include "as/asui.h"
#include "as/asui_local.h"

#include "widgets/ui_optionsform.h"

namespace ASUI {

typedef WSWUI::OptionsForm OptionsForm;

WSWUI::OptionsForm* Element_CastToOptionsForm( Element *self ) {
	WSWUI::OptionsForm *form = dynamic_cast<WSWUI::OptionsForm*>( self );
	if( form ) {
		form->AddReference();
	}
	return form;
}

static Element *OptionsForm_CastToElement( OptionsForm *self ) {
	Element *e = dynamic_cast<Element *>( self );
	if( e ) {
		e->AddReference();
	}
	return e;
}

void PrebindOptionsForm( ASInterface *as )
{
	ASBind::Class<WSWUI::OptionsForm, ASBind::class_ref>( as->getEngine() );
}

void BindOptionsForm( ASInterface *as )
{
	asIScriptEngine *engine = as->getEngine();

	ASBind::GetClass<OptionsForm>( engine )
		.refs( &OptionsForm::AddReference, &OptionsForm::RemoveReference )

		.method( &OptionsForm::restoreOptions, "restoreOptions" )
		.method( &OptionsForm::storeOptions, "storeOptions" )
		.method( &OptionsForm::applyOptions, "applyOptions" )

		.refcast( &OptionsForm_CastToElement, true, true )
		;

	// Cast behavior for the Element class
	ASBind::GetClass<Rocket::Core::Element>( engine )
		.refcast( &Element_CastToOptionsForm, true, true )
		;
}

}

ASBIND_TYPE( WSWUI::OptionsForm, ElementOptionsForm );
