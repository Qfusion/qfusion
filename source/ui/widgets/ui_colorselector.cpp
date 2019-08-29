#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "kernel/ui_utils.h"

#include "widgets/ui_widgets.h"

#include <string>
#include <sstream>

#include <RmlUi/Controls/ElementFormControl.h>

/*
    <colorselector name="color" cvar="cg_teamALPHAcolor">
        <color rgb="#ff00ff" />		<!-- pink in "#hex" -->
        <color rgb="255 255 255" /> <!-- white in "r g b" -->
    </colorselector>

    <!--
    you can use other elements inside colorselector to control
    the layout too
    -->

    <colorselector name="color" cvar="color">
        <div>
            <color rgb="#0000ff" /> <color rgb="#00ff00" />
        </div>
        <div>
            <color rgb="#ff0000" /> <color rgb="#ff00ff" />
        </div>
    </colorselector>

    <!--
    <color> tags will select themselves when clicked by setting the pseudoclass
    :selected and resetting it on other color blocks.
    it will automatically set the css background property to the given color.
    if no color is given, it tries to get the cvar attribute from the
    parent <colorselector> and use the value of that cvar. Otherwise it will
    revert to hardcoded default color.

    <colorselector> is derived from FormControl and thus can be used within
    <form> or <optionsform>. GetValue() will return string "r g b".
    (TODO: update notification when the cvar is changed!)
    -->
*/
/*
    ch : FIXME:
    cvar changes internally.. we need to monitor that somehow?
    without relying to optionsform.. cvar->changed ? in onUpdate ..?
*/
namespace WSWUI
{

using namespace Rml::Core;

// forward decl
class ColorBlock;
class ColorSelector;

// ch : Use rockets String inside this elements, since they are derived from
// rocket and it goes with the context. Otherwise, we like std::string

//===================================================

// One of the color elements in the colorselector
class ColorBlock : public Element
{
	// FIXME: whats a nice default color?
	// maybe grab the cvar from colorselector parent and
	// utilize that?
	static const char *DEFAULT_COLOR;

public:
	ColorBlock( const String &tag, const XMLAttributes &attr ) : Element( tag ), 
		selector( nullptr ), cvar( nullptr ), color(), hasRGB( false ) {
		setColor( "" );
		hasRGB = false;
			
		// grab the rgb attribute
		auto attrRgbIt = attr.find( "rgb" );
		if( attrRgbIt == attr.end() ) {
			return;
		}

		auto attrRgb = attrRgbIt->second.Get<std::string>();
		if( !attrRgb.empty() ) {
			setColor( attrRgb );
			hasRGB = true;
		}
	}

	virtual ~ColorBlock();

	// Element methods
	virtual void ProcessDefaultAction( Event &event ) override;

	// Custom methods
	void setSelector( ColorSelector *_selector );

	const String &getColor( void ) { return color; }

	// value can be "r g b" or #hex
	void setColor( const std::string &c ) {
		// this is the ultimate end case when we dont even have cvar associated
		if( c.empty() ) {
			color = DEFAULT_COLOR;
			SetProperty( "background", rgb2hex( DEFAULT_COLOR ).c_str() );
			return;
		}

		// we want to pass #hex as background property and
		// r g b as the value passed to warsow
		String hex = ( c[0] == '#' ? c : rgb2hex( c.c_str() ).c_str() );
		color = ( c[0] == '#' ? hex2rgb( c.c_str() ).c_str() : c );

		SetProperty( "background", hex );
	}

	bool isCustom() const { return hasRGB == false; }

private:
	ColorSelector *selector;
	cvar_t *cvar;           // set, if this is "custom color"
	String color;           // just use rocket string in rocket element
	bool hasRGB;            // if rgb attribute was given on construction
};

const char *ColorBlock::DEFAULT_COLOR = "85 86 102";

//===================================================

// Main colorselector widget
class ColorSelector : public Rml::Controls::ElementFormControl
{
public:
	ColorSelector( const String &tag, const XMLAttributes &attr ) : ElementFormControl( tag ), cvar( 0 ) {
		auto cvarNameIt = attr.find( "cvar" );
		if( cvarNameIt == attr.end() ) {
			return;
		}
		
		auto cvarName = cvarNameIt->second.Get<std::string>();
		if( !cvarName.empty() ) {
			cvar = trap::Cvar_Get( cvarName.c_str(), "", 0 );
		}
	}

	virtual ~ColorSelector() {
		// for 'color' children, unset parent
		ElementList colors;
		GetElementsByTagName( colors, "color" );
		for( ElementList::iterator it = colors.begin(); it != colors.end(); ++it ) {
			ColorBlock *cb = dynamic_cast<ColorBlock*>( *it );
			if( cb ) {
				cb->setSelector( 0 );
			}
		}

		// FIXME: do i need to release the elements?
	}

	// FormControl methods
	virtual String GetValue( void ) const {
		// search for <color> children, find the "selected" one
		// and return its value
		ElementList colors;
		const_cast<ColorSelector*>( this )->GetElementsByTagName( colors, "color" );
		for( ElementList::const_iterator it = colors.begin(); it != colors.end(); ++it ) {
			ColorBlock *cb = dynamic_cast<ColorBlock*>( *it );
			if( cb && cb->IsPseudoClassSet( "selected" ) ) {
				return cb->getColor();
			}
		}

		// FIXME: do i need to release the elements?

		// FIXME: default value from THE CVAR??
		return "";
	}

	virtual void SetValue( const String &value ) {
		// search for <color> children, find the one with matching value
		// and "select" it
		ElementList colors;
		ElementList::iterator it;
		GetElementsByTagName( colors, "color" );
		for( it = colors.begin(); it != colors.end(); ++it ) {
			ColorBlock *cb = dynamic_cast<ColorBlock*>( *it );
			if( cb && cb->getColor() == value ) {
				selectColorBlock( cb );
				break;
			}
		}

		// FIXME: do i need to release the elements?

		// TODO: if no block with matching color was found, find the
		// custom colorblock and set its color to value and select it.
		if( it == colors.end() ) {
			// DEBUG
			//Com_Printf( "ColorSelector searching for custom\n" );
			for( it = colors.begin(); it != colors.end(); ++it ) {
				ColorBlock *cb = dynamic_cast<ColorBlock*>( *it );
				if( cb && cb->isCustom() ) {
					cb->setColor( value );
					selectColorBlock( cb );
					break;
				}
			}
		}

		// this calls out onchange event i guess
		SetAttribute( "value", value );
	}

	virtual bool IsSubmitted( void ) {
		return true;
	}

	// Element method
	virtual void OnChildAdd( Element* child ) {
		Rml::Controls::ElementFormControl::OnChildAdd( child );

		ColorBlock *cb = dynamic_cast<ColorBlock*>( child );
		if( cb ) {
			cb->setSelector( this );
		}
	}

	virtual void OnUpdate( void ) {
		// we should figure out if the cvar is changed and update
		// the possible custom color child with the cvars value
	}

	// Own methods
	void selectColorBlock( ColorBlock *element ) {
		ElementList colors;
		GetElementsByTagName( colors, "color" );
		for( ElementList::iterator it = colors.begin(); it != colors.end(); ++it )
			( *it )->SetPseudoClass( "selected", ( /* *it == element ? true : */ false ) );

		// FIXME: do i need to release the elements?

		element->SetPseudoClass( "selected", true );
	}

	// called from ColorBlock, bypasses matching element searching
	void setValueDirect( const String &value ) {
		// this calls out onchange event i guess
		SetAttribute( "value", value );
	}

	cvar_t *getCvar( void ) { return cvar; }

private:
	// do we even bother to store this?
	// String value;		// just use rockets strings here dude
	cvar_t *cvar;
};

//===================================================

// ColorBlock implementation after ColorSelector
ColorBlock::~ColorBlock() {
	selector = nullptr;
}

// Element methods
void ColorBlock::ProcessDefaultAction( Event &event ) {
	// TODO: allow selection via tab/arrows/enter/space

	// FIXME: if there happens to be more colorblocks with the
	// same color, doing this on the latter ones will just select
	// the first one with matching color..
	if( event.GetType() == "click" && selector != nullptr ) {
		selector->selectColorBlock( this );
		selector->setValueDirect( color );
	}

	Element::ProcessDefaultAction( event );
}

void ColorBlock::setSelector( ColorSelector *_selector ) {
	selector = _selector;

	// if attributes didnt specify color, see if we can fetch the
	// associated cvar from the parent
	if( !hasRGB ) {
		const char *value;
		cvar = selector ? _selector->getCvar() : 0;
		value = ( cvar && cvar->string ) ? cvar->string : "";
		setColor( value );
	}
}

//===================================================

// ch : note that these are our own implementation of GenericElementInstancer,
// and not those of libRocket's
ElementInstancer *GetColorBlockInstancer( void ) {
	return __new__( GenericElementInstancerAttr<ColorBlock> )();
}

ElementInstancer *GetColorSelectorInstancer( void ) {
	return __new__( GenericElementInstancerAttr<ColorSelector> )();
}
}
