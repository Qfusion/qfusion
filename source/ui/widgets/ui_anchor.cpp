#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "widgets/ui_widgets.h"
#include "widgets/ui_idiv.h"

namespace WSWUI
{

using namespace Rml::Core;

class AnchorWidget : public Element /* , public EventListener */
{
public:
	AnchorWidget( const String &tag ) : Element( tag ) {
	}

	virtual ~AnchorWidget()
	{}

	static void CacheRead( const char *fileName, void *privatep ) {
		AnchorWidget *element = static_cast<AnchorWidget *> ( privatep );
		String target = element->GetAttribute<String>( "target", "" );
		InlineDiv *idiv = NULL;

		// allow targeting specific idiv's via the "target" attribute
		// this is similar to targetting specific frames in real browsers
		if( !target.empty() && target.front() != '_' ) {
			Element *_target = element->GetOwnerDocument()->GetElementById( target );
			if( _target && _target->GetTagName() == "idiv" ) {
				idiv = ( InlineDiv * )_target;
			}
			if( !idiv ) {
				Com_Printf( "AnchorWidget::CacheRead: target idiv '%s' was not found\n", target.c_str() );
				return;
			}
		} else {
			idiv = ( InlineDiv * )element->GetParentIDiv();
		}

		if( idiv ) {
			idiv->ReadFromFile( fileName );
		} else {
			ElementDocument *document = element->GetOwnerDocument();
			WSWUI::Document *ui_document = static_cast<WSWUI::Document *>( document->GetScriptObject() );
			if( ui_document ) {
				WSWUI::NavigationStack *stack = ui_document->getStack();
				if( stack ) {
					stack->pushDocument( fileName );
				}
			}
		}
	}

	virtual void ProcessDefaultAction( Event &event ) override {
		if( event == "click" ) {
			// TODO: wrap this to UI_Main that will catch errors and the
			// new rootdocument (along with populating href with correct
			// path)
			String href = GetAttribute<String>( "href", "" );
			if( href.empty() ) {
				Com_Printf( "AnchorWidget::ProcessDefaultAction: empty href\n" );
				return;
			}

			// FIXME: a rather stupid special case
			if( href == "#" ) {
				return;
			}

			// check for warsow:// and warsow{protocol}:// href's
			std::string gameProtocol( trap::Cvar_String( "gamename" ) );
			std::string gameProtocolSchema = Rml::Core::CreateString( 32,  "%s%i", trap::Cvar_String( "gamename" ), UI_Main::Get()->getGameProtocol() );

			URL url( href );
			String urlProtocol = Rml::Core::StringUtilities::ToLower( url.GetProtocol() );

			if( urlProtocol == Rml::Core::StringUtilities::ToLower( gameProtocol ) || urlProtocol == Rml::Core::StringUtilities::ToLower( gameProtocolSchema ) ) {
				// connect to game server
				trap::Cmd_ExecuteText( EXEC_APPEND, va( "connect \"%s\"\n", href.c_str() ) );
				return;
			} else if( trap::FS_IsUrl( href.c_str() ) ) {
				String target = GetAttribute<String>( "target", "" );

				if( target == "_browser" ) {
					// open the link in OS browser
					trap::CL_OpenURLInBrowser( href.c_str() );
				} else {
					UI_Main::Get()->getStreamCache()->PerformRequest(
						href.c_str(), "GET", NULL,
						NULL, NULL, &CacheRead, ( void * )this
						);
				}
				return;
			}

			WSWUI::Document *ui_document = static_cast<WSWUI::Document *>( GetOwnerDocument()->GetScriptObject() );
			if( ui_document ) {
				ui_document->getStack()->pushDocument( href.c_str() );
			}
		} else {
			Element::ProcessDefaultAction( event );
		}
	}

private:
	// returns the parent <idiv> element for anchor, if any
	Element *GetParentIDiv( void ) const {
		Element *p;

		p = ( Element * )this;
		while( 1 ) {
			p = p->GetParentNode();
			if( !p ) {
				break;
			}
			if( p->GetTagName() == "idiv" ) {
				return p;
			}
		}

		return NULL;
	}
};

//==============================================================

ElementInstancer *GetAnchorWidgetInstancer( void ) {
	return __new__( GenericElementInstancer<AnchorWidget> )();
}

}
