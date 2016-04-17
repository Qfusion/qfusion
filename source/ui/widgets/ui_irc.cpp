#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "widgets/ui_widgets.h"
#include "widgets/ui_idiv.h"
#include "formatters/ui_colorcode_formatter.h"

namespace WSWUI {

using namespace Rocket::Core;

class IrcLogWidget : public Element
{
public:
	IrcLogWidget( const String &tag ) : Element(tag), history_size( 0 )
	{
		Rocket::Core::XMLAttributes attr;

		formatter = new ColorCodeFormatter();

		SetProperty( "display", "block" );
		SetProperty( "overflow-y", "auto" );

		body = Factory::InstanceElement( this, "*", "irclogbody", attr);
		AppendChild( body );
		body->RemoveReference();
	}

	virtual ~IrcLogWidget()
	{}

	void OnRender()
	{
		Element::OnRender();

		bool scrollDown = true;
		if( GetScrollTop() > 0 && fabs( GetScrollTop() - (GetScrollHeight() - GetClientHeight()) ) > 1.0f ) {
			// do not scroll, if not at the bottom or at the top
			scrollDown = false;
		}

		size_t new_history_size = trap::Irc_HistoryTotalSize();
		if( history_size != new_history_size ) {
			// dirty
			const String br = "<br/>", e = "";
			String line, text = "";
			StringList list;

			// add IRC history lines one by one, converting 
			// warsow color codes and HTML special chars to RML code
			const struct irc_chat_history_node_s *n = trap::Irc_GetHistoryHeadNode();
			while( n ) {
				list.push_back( trap::Irc_GetHistoryNodeLine( n ) );
				formatter->FormatData( line, list );

				// prepend
				text = line + ( text.Empty() ? e : br ) + text;
				n = trap::Irc_GetNextHistoryNode( n );

				list.clear();
			}

			body->SetInnerRML( text );

			UpdateLayout();

			// keep the scrollbar at the bottom
			if( scrollDown ) {
				SetScrollTop( GetScrollHeight() - GetClientHeight() );
			}

			history_size = new_history_size;
		}
	}

private:
	size_t history_size;
	Element *body;
	ColorCodeFormatter *formatter;
};

//==============================================================

ElementInstancer *GetIrcLogWidgetInstancer( void )
{
	return __new__( GenericElementInstancer<IrcLogWidget> )();
}

}
