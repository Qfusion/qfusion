/*
Copyright (C) 2011 Cervesato Andrea

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
#include "kernel/ui_keyconverter.h"
#include "widgets/ui_widgets.h"
#include "widgets/ui_keyselect.h"
#include "kernel/ui_utils.h"
#include "../gameshared/q_keycodes.h"
#include <RmlUi/Core/Input.h>

namespace WSWUI
{

using namespace Rml::Core;

UI_KeySelect::UI_KeySelect( const String &tag, const String &bind, UI_KeySelectInstancer *instancer )
	: Element( tag ), cmd( bind ) {
	this->focusMode = false;
	this->boundKey[0] = 0;
	this->boundKey[1] = 0;
	this->mouse_x = 0;
	this->mouse_y = 0;
	this->instancer = instancer;

	InitializeBinds();
	WriteText();
}

UI_KeySelect::~UI_KeySelect() {}

void UI_KeySelect::InitializeBinds( void ) {
	int j, count = 0;
	const char *b;

	for( j = 0; j < 256; j++ ) {
		b = trap::Key_GetBindingBuf( j );
		if( !b ) {
			continue;
		}
		if( !Q_stricmp( b, this->cmd.c_str() ) ) {
			boundKey[count] = j;
			count++;
			if( count == 2 ) {
				break;
			}
		}
	}
}

// index can be 0 or 1
int UI_KeySelect::GetKey( int index ) {
	if( !index ) {
		return boundKey[0];
	} else {
		return boundKey[1];
	}
}

// release key with index 0 or 1
void UI_KeySelect::ReleaseKey( int index ) {
	int saveKey;

	if( !index ) {
		saveKey = boundKey[0];
		boundKey[0] = boundKey[1];
	} else {
		saveKey = boundKey[1];
	}

	boundKey[1] = 0;

	// unbind key
	if( saveKey ) {
		trap::Key_SetBinding( saveKey, NULL );
	}

	WriteText();
}

// release both keys
void UI_KeySelect::ReleaseKeys( void ) {
	// stack like
	ReleaseKey( 1 );
	ReleaseKey( 0 );

	WriteText();
}

// returns the command used by keyselect
const String &UI_KeySelect::GetBindCmd( void ) {
	return cmd;
}

// resolve binds conflicts with the others keyselect using instancer.
void UI_KeySelect::ResolveConflictsForKey( int key ) {
	UI_KeySelect *other = instancer->getKeySelectByKey( key, this );
	if( other ) {
		if( other->GetKey( 0 ) == key ) {
			other->ReleaseKey( 0 );
		} else {
			other->ReleaseKey( 1 );
		}
	}
}

// Get the name of a keycode that is visible to the user.
std::string UI_KeySelect::KeynumToString( int keynum ) const {
	if( ( keynum >= 'a' ) && ( keynum <= 'z' ) ) {
		char upper[2];
		upper[0] = keynum - 'a' + 'A';
		upper[1] = '\0';
		return upper;
	}

	return trap::Key_KeynumToString( keynum );
}

// Initialize the text inside the widget
// i.e. if "r" and "b" keys are bound, the text inside the widget
//      will look like: "R OR B".
// when no keys are bound, text will be "???".
void UI_KeySelect::WriteText( void ) {
	std::string text;

	if( KeysAreFree() ) {
		text = "???";
	} else {
		const char *or_ = "%s or %s";
		const char *or_l10n = trap::L10n_TranslateString( or_ );
		if( !or_l10n ) {
			or_l10n = or_;
		}

		if( FirstKeyIsBound() ) {
			std::string b0 = KeynumToString( boundKey[0] );
			if( focusMode ) {
				text = va( or_l10n, b0.c_str(), "???" );
			} else {
				text = b0;
			}
		} else if( KeysAreBound() ) {
			std::string b0 = KeynumToString( boundKey[0] );
			std::string b1 = KeynumToString( boundKey[1] );
			text += va( or_l10n, b0.c_str(), b1.c_str() );
		}
	}
	this->SetInnerRML( text.c_str() );
}

// assign a keybind to this widget
void UI_KeySelect::SetKeybind( int key ) {
	int index;

	if( !key || ( key == K_ESCAPE ) ) {
		return;
	}

	// koochi: moved into the focus event
	//if( KeysAreBound() )
	//	ReleaseKeys();

	// we don't need to rebind the same key
	if( ( key == boundKey[0] ) || ( key == boundKey[1] ) ) {
		this->Blur();
		return;
	}

	// save the key
	index = FirstKeyIsBound() ? 1 : 0;
	boundKey[index] = key;

	// resolve conflicts before bind apply
	ResolveConflictsForKey( key );

	// apply the bind
	char bindCmd[1024];
	Q_snprintfz( bindCmd, sizeof( bindCmd ), "bind \"%s\" \"%s\"\n", trap::Key_KeynumToString( key ), cmd.c_str() );
	trap::Cmd_ExecuteText( EXEC_INSERT, bindCmd );

	Blur();
}

/// Called for every event sent to this element or one of its descendants.
/// @param[in] event The event to process.
void UI_KeySelect::ProcessEvent( Event& event ) {
	RocketModule *rocketModule = GetRocketModule();
	int contextId = rocketModule->idForContext( GetContext() );

	if( event == "blur" ) {
		focusMode = false;
		rocketModule->hideCursor( contextId, 0, RocketModule::HIDECURSOR_ELEMENT );
		WriteText();
	} else if( event == "focus" ) {
		focusMode = true;
		rocketModule->hideCursor( contextId, RocketModule::HIDECURSOR_ELEMENT, 0 );

		// old C ui functionality
		if( KeysAreBound() ) {
			ReleaseKeys();
		}

		WriteText();
	}

	// get the key
	if( focusMode ) {
		int key = 0;

		if( event == "keyselect" ) {
			key = event.GetParameter< int >( "key", 0 );
			this->SetKeybind( key );
			event.StopPropagation();
			return;
		} else if( event == "textinput" ) {
			// not supported yet
		} else if( event == "mousedown" ) {
			// fix mouse position inside the widget
			mouse_x = event.GetParameter<int>( "mouse_x", 0 );
			mouse_y = event.GetParameter<int>( "mouse_y", 0 );
			return;
		} else if( event == "mousemove" || event == "mouseout" ) {
			rocketModule->mouseMove( contextId, mouse_x, mouse_y );
			event.StopPropagation();
			return;
		}
	}

	Element::ProcessDefaultAction( event );
}

/// Instances an element given the tag name and attributes.
/// @param[in] parent The element the new element is destined to be parented to.
/// @param[in] tag The tag of the element to instance.
/// @param[in] attributes Dictionary of attributes.
ElementPtr UI_KeySelectInstancer::InstanceElement( Element *parent, const String &tag, const XMLAttributes &attr ) {
	std::string bind;
	auto it = attr.find("bind");
	if (it != attr.end()) {
		bind = it->second.Get<std::string>();
	}

	UI_KeySelect *keyselect = __new__( UI_KeySelect )( tag, bind, this );
	keyselect_widgets.push_back( keyselect );
	UI_Main::Get()->getRocket()->registerElementDefaults( keyselect );
	return ElementPtr(keyselect);
}

/// Releases an element instanced by this instancer.
/// @param[in] element The element to release.
void UI_KeySelectInstancer::ReleaseElement( Element *element ) {
	// first remove from the list
	keyselect_widgets.erase( std::remove( keyselect_widgets.begin(), keyselect_widgets.end(), element ),
							 keyselect_widgets.end() );

	// then delete
	__delete__( element );
}

// Returns a keyselect which has the same bound key of the excluded one
UI_KeySelect* UI_KeySelectInstancer::getKeySelectByKey( int key, const UI_KeySelect *exclude ) {
	for( KeySelectList::iterator it = keyselect_widgets.begin() ; it != keyselect_widgets.end(); ++it ) {
		if( key ) {
			if( ( ( *it )->GetKey( 0 ) == key || ( *it )->GetKey( 1 ) == key ) && *it != exclude ) {
				return *it;
			}
		}
	}

	// not found
	return 0;
}

// Returns a keyselect which has the same bound command of the excluded one
UI_KeySelect* UI_KeySelectInstancer::getKeySelectByCmd( const String &cmd, const UI_KeySelect *exclude ) {
	for( KeySelectList::iterator it = keyselect_widgets.begin(); it != keyselect_widgets.end(); ++it ) {
		// case insensitive?
		if( ( *it )->GetBindCmd() == cmd && *it != exclude ) {
			return *it;
		}
	}

	// not found
	return 0;
}

//============================================

ElementInstancer *GetKeySelectInstancer( void ) {
	ElementInstancer *instancer = __new__( UI_KeySelectInstancer )();

	// instancer->RemoveReference();
	return instancer;
}
}
