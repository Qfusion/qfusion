/*
Copyright (C) 2011 Cervesato Andrea ("koochi")

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

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/EventListener.h>

#include "kernel/ui_main.h"

namespace WSWUI
{
using namespace Rml::Core;

// forward-declare the instancer for keyselects
class UI_KeySelectInstancer;

/*
    Keyselect widget used to manage the binds in the Rocket ui.
 */
class UI_KeySelect : public Element, public EventListener
{
public:
	// initialize keyselect with it's own tag and command bind
	UI_KeySelect( const String &tag, const String &bind, UI_KeySelectInstancer *instancer );
	virtual ~UI_KeySelect();

	/// Called for every event sent to this element or one of its descendants.
	/// @param[in] event The event to process.
	virtual void ProcessEvent( Event &event );

	// index can be 0 or 1
	int GetKey( int index );
	// release key with index 0 or 1
	void ReleaseKey( int index );
	// release both keys
	void ReleaseKeys( void );

	// returns the command used by keyselect
	const String &GetBindCmd( void );

private:
	// used to know if the widget has the focus
	bool focusMode;

	// we can store 2 different keys for the same command
	int boundKey[2];
	String cmd;

	// position of the cursor at the first click event
	int mouse_x;
	int mouse_y;

	// instancer contains the list of the keyselect widgets.
	// This reference is needed to reset binds conflicts when
	// a new key has been bound.
	UI_KeySelectInstancer *instancer;

	void InitializeBinds( void );

	// return rocket module. It's used to fix mouse position
	// during focus event
	static RocketModule* GetRocketModule( void ) { return UI_Main::Get()->getRocket(); }

	// Some usefull functions to get keybinds
	static int GetKeyboardKey( Event &evt ) {
		int rkey = evt.GetParameter<int>( "key_identifier", 0 );
		return KeyConverter::fromRocketKey( rkey );
	}
	static int GetWheelKey( Event &evt ) {
		int rkey = evt.GetParameter<int>( "wheel_delta", 0 );
		return KeyConverter::fromRocketWheel( rkey );
	}

	// generic utilization
	inline bool KeysAreFree( void ) { return !boundKey[0] && !boundKey[1]; }
	inline bool KeysAreBound( void ) { return boundKey[0] && boundKey[1]; }
	inline bool FirstKeyIsBound( void ) { return boundKey[0] && !boundKey[1]; }

	// resolve binds conflicts with the others keyselect using instancer.
	void ResolveConflictsForKey( int key );

	// Get the name of a keycode that is visible to the user.
	std::string KeynumToString( int keynum ) const;

	// Initialize the text inside the widget
	// i.e. if "r" and "b" keys are bound, the text inside the widget
	//      will look like: "R OR B".
	// when no keys are bound, text will look like "???".
	void WriteText( void );

	// assign a keybind to this widget
	void SetKeybind( int key );
};


/*
    Used to initialize instances of UI_KeySelect and
    provides to manage the list of all the keybinds, removing
    binds conflicts.
 */
class UI_KeySelectInstancer : public ElementInstancer
{
public:
	UI_KeySelectInstancer() : ElementInstancer() {}

	/// Instances an element given the tag name and attributes.
	/// @param[in] parent The element the new element is destined to be parented to.
	/// @param[in] tag The tag of the element to instance.
	/// @param[in] attributes Dictionary of attributes.
	virtual ElementPtr InstanceElement( Element *parent, const String &tag, const XMLAttributes &attr );
	/// Releases an element instanced by this instancer.
	/// @param[in] element The element to release.
	virtual void ReleaseElement( Element *element );

	// Returns a keyselect which has the same bound key of the excluded one
	UI_KeySelect *getKeySelectByKey( int key, const UI_KeySelect *exclude );

	// Returns a keyselect which has the same bound command of the excluded one
	UI_KeySelect *getKeySelectByCmd( const String &cmd, const UI_KeySelect *exclude );

private:
	typedef std::list<UI_KeySelect*> KeySelectList;
	KeySelectList keyselect_widgets;
};

}
