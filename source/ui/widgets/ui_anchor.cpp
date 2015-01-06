#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "widgets/ui_widgets.h"
#include "widgets/ui_idiv.h"

namespace WSWUI {

using namespace Rocket::Core;

class AnchorWidget : public Element /* , public EventListener */
{
public:
	AnchorWidget( const String &tag ) : Element(tag)
	{
	}

	virtual ~AnchorWidget()
	{}

	static void CacheRead( const char *fileName, void *privatep )
	{
		AnchorWidget *element = static_cast<AnchorWidget *> (privatep);
		String target = element->GetAttribute<String>("target", "");
		InlineDiv *idiv = NULL;

		// allow targeting specific idiv's via the "target" attribute
		// this is similar to targetting specific frames in real browsers
		if( !target.Empty() && target[0] != '_' ) {
			Element *_target = element->GetOwnerDocument()->GetElementById( target );
			if( _target && _target->GetTagName() == "idiv" ) {
				idiv = ( InlineDiv * )_target;
			}
			if( !idiv ) {
				Com_Printf("AnchorWidget::CacheRead: target idiv '%s' was not found\n", target.CString());
				return;
			}
		} else {
			idiv = ( InlineDiv * )element->GetParentIDiv();
		}

		if( idiv ) {
			idiv->ReadFromFile( fileName );
		}
		else {
			ElementDocument *document = element->GetOwnerDocument();
			WSWUI::Document *ui_document = static_cast<WSWUI::Document *>(document->GetScriptObject());
			if( ui_document ) {
				WSWUI::NavigationStack *stack = ui_document->getStack();
				if( stack )
					stack->pushDocument( fileName );
			}
		}

		element->RemoveReference();
	}

	virtual void ProcessEvent( Event &event )
	{
		if( event == "click" )
		{
			// TODO: wrap this to UI_Main that will catch errors and the
			// new rootdocument (along with populating href with correct
			// path)
			String href = GetAttribute<String>("href", "");
			if( href.Empty() )
			{
				Com_Printf("AnchorWidget::ProcessEvent: empty href\n");
				return;
			}

			// FIXME: a rather stupid special case
			if( href == "#" ) {
				return;
			}

			// check for warsow:// and warsow{protocol}:// href's
			String 
				gameProtocol (trap::Cvar_String( "gamename" )),
				gameProtocolSchema( 32,  "%s%i", trap::Cvar_String( "gamename" ), UI_Main::Get()->getGameProtocol() );

			URL url( href );
			String urlProtocol = url.GetProtocol().ToLower();

			if( urlProtocol == gameProtocol.ToLower() || urlProtocol == gameProtocolSchema.ToLower() ) {
				// connect to game server
				trap::Cmd_ExecuteText( EXEC_APPEND, va( "connect \"%s\"\n", href.CString() ) );
				return;
			}
			else if( trap::FS_IsUrl( href.CString() ) ) {
				String target = GetAttribute<String>("target", "");

				if( target == "_browser" ) {
					// open the link in OS browser
					trap::CL_OpenURLInBrowser( href.CString() );
				}
				else {
					AddReference();

					UI_Main::Get()->getStreamCache()->PerformRequest(
						href.CString(), "GET", NULL,
						NULL, NULL, &CacheRead, ( void * )this
					);
				}
				return;
			}

			WSWUI::Document *ui_document = static_cast<WSWUI::Document *>(GetOwnerDocument()->GetScriptObject());
			if( ui_document ) {
				ui_document->getStack()->pushDocument( href.CString() );
			}
		}
		else
		{
			Element::ProcessEvent( event );
		}
	}

private:
	// returns the parent <idiv> element for anchor, if any
	Element *GetParentIDiv( void ) const
	{
		Element *parent;

		parent = ( Element * )this;
		while( 1 ) {
			parent = parent->GetParentNode();
			if( !parent ) {
				break;
			}
			if( parent->GetTagName() == "idiv" ) {
				return parent;
			}
		}

		return NULL;
	}
};

//==============================================================

ElementInstancer *GetAnchorWidgetInstancer( void )
{
	return __new__( GenericElementInstancer<AnchorWidget> )();
}

}
