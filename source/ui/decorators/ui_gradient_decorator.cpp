#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"

/*
	ch : Gradient decorator usage in CSS:

		grad-decorator: gradient;    <-- keyword

		grad-dir: horizontal|vertical;	<--- direction
		grad-start: #ff00ff;		<-- starting color (left or top)
		grad-end: #00ff00;			<-- ending color (right or bottom)

		grad-gradient: horizontal|vertical #rrggbb #rrggbb	<-- shortcut
*/

namespace WSWUI
{
	typedef Rocket::Core::Element Element;
	typedef Rocket::Core::Decorator Decorator;
	typedef Rocket::Core::DecoratorDataHandle DecoratorDataHandle;
	typedef Rocket::Core::DecoratorInstancer DecoratorInstancer;
	typedef Rocket::Core::PropertyDictionary PropertyDictionary;
	typedef Rocket::Core::Colourb Colourb;

	//=======================================================

	class GradientDecorator : public Decorator
	{
		enum Direction {
			HORIZONTAL = 0,
			VERTICAL = 1
		};

		Direction dir;	// hori/vert
		Colourb start;	// color gradients
		Colourb end;

	public:
		GradientDecorator( const PropertyDictionary& properties )
		{
			// fetch the properties from the dict
			String prop_dir = properties.GetProperty( "dir" )->Get<String>();
			start = properties.GetProperty( "start" )->Get<Colourb>();
			end = properties.GetProperty( "end" )->Get<Colourb>();

			// enumerate direction property
			dir = ( prop_dir == "horizontal" ? HORIZONTAL : VERTICAL );
		}

		// decorator implementation
		virtual DecoratorDataHandle GenerateElementData( Element* element )
		{
			// nada
			return 0;
		}

		virtual void ReleaseElementData( DecoratorDataHandle element_data)
		{
			// nada
		}

		virtual void RenderElement( Element* element, DecoratorDataHandle element_data )
		{
			// just testing here, so lets use renderinterface directly
			typedef Rocket::Core::Vertex Vertex;
			typedef Rocket::Core::Vector2f Vector2f;

			// fetch the corners (WIP add some borders to debug the drawing area)
			Vector2f topleft = Vector2f( element->GetAbsoluteLeft() + element->GetClientLeft(),
										element->GetAbsoluteTop() + element->GetClientTop() );
			Vector2f bottomright = Vector2f( topleft.x + element->GetClientWidth(), topleft.y + element->GetClientHeight() );

			// create the renderable vertexes
			Vertex vertex[4];
			for( int i = 0; i < 4; i++ )
				vertex[i].tex_coord = Vector2f( 0.0f, 0.0f );

			vertex[0].position = topleft;
			vertex[1].position = Vector2f( bottomright.x, topleft.y );
			vertex[2].position = bottomright;
			vertex[3].position = Vector2f( topleft.x, bottomright.y );

			// gradient
			if( dir == HORIZONTAL ) {
				vertex[0].colour = vertex[3].colour = start;
				vertex[1].colour = vertex[2].colour = end;
			}
			else {
				vertex[0].colour = vertex[1].colour = start;
				vertex[2].colour = vertex[3].colour = end;
			}

			// do i need to comment this?
			int indices[6] = { 0, 1, 2, 0, 2, 3 };

			// bang
			Rocket::Core::RenderInterface *renderer = element->GetRenderInterface();
			renderer->RenderGeometry( vertex, 4, indices, 6, 0, Vector2f( 0.0, 0.0 ) );
			// C'MON! you say GetRenderInterface wont give a reference to refcounted object??
			// renderer->RemoveReference();
		}
	};

	//=======================================================

	class GradientDecoratorInstancer : public DecoratorInstancer
	{

	public:
		GradientDecoratorInstancer( void ) : DecoratorInstancer()
		{
			// register properties for the decorator
			RegisterProperty( "dir", "" ).AddParser( "string" ).AddParser( "keyword", "horizontal, vertical" );
			RegisterProperty( "start", "#ffff" ).AddParser( "color" );
			RegisterProperty( "end", "#ffff" ).AddParser( "color" );
			RegisterShorthand( "gradient", "dir, start, end" );

			// Com_Printf("decorator instancer created\n");
		}

		// decorator instancer implementation
		virtual Decorator* InstanceDecorator( const String& name, const PropertyDictionary& properties )
		{
			// Com_Printf("decorator instancer decorator instanced %s\n", name.CString() );
			return __new__( GradientDecorator )( properties );
		}

		virtual void ReleaseDecorator( Decorator* decorator )
		{
			// Com_Printf("decorator instancer decorator released\n");
			__delete__( decorator );
		}

		virtual void Release()
		{
			// Com_Printf("decorator instancer destroyed\n");
			__delete__( this );
		}
	};

	DecoratorInstancer *GetGradientDecoratorInstancer( void ) {
		return __new__( GradientDecoratorInstancer );
	}
}
