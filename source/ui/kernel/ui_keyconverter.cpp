/*
 * KeyConverter.cpp
 *
 *  Created on: 26.6.2011
 *      Author: hc
 */

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "../gameshared/q_keycodes.h"
#include "kernel/ui_keyconverter.h"
#include <RmlUi/Core/Input.h>

namespace WSWUI
{

using namespace Rml::Core::Input;

/* Special punctuation characters */
const char * KeyConverter::oem_keys = ";=,-./`[\\]'";

int KeyConverter::getModifiers( void ) {
	int mod = 0;
	if( trap::Key_IsDown( K_LALT ) || trap::Key_IsDown( K_RALT ) ) {
		mod |= KM_ALT;
	}
	if( trap::Key_IsDown( K_LCTRL ) || trap::Key_IsDown( K_RCTRL ) ) {
		mod |= KM_CTRL;
	}
	if( trap::Key_IsDown( K_LSHIFT ) || trap::Key_IsDown( K_RSHIFT ) ) {
		mod |= KM_SHIFT;
	} else {
		mod |= KM_NUMLOCK;
	}

	return mod;
}

int KeyConverter::toRocketKey( int key ) {
	if( key >= '0' && key <= '9' ) {
		return KI_0 + ( key - '0' );
	}
	if( key >= 'a' && key <= 'z' ) {
		return KI_A + ( key - 'a' );
	}

	for( unsigned i = 0; i < sizeof( oem_keys ) - 1; ++i ) {
		if( key == oem_keys[i] ) {
			return KI_OEM_1 + i;
		}
	}

	switch( key ) {
		case K_TAB:    return KI_TAB;
		case K_ENTER:  return KI_RETURN;
		case K_ESCAPE: return KI_ESCAPE;
		case K_SPACE:  return KI_SPACE; // K_SPACE == ' '
		case K_BACKSPACE:   return KI_BACK;
		case K_CAPSLOCK:    return KI_CAPITAL;
		case K_SCROLLLOCK:  return KI_SCROLL;
		case K_PAUSE:       return KI_PAUSE;
		case K_UPARROW:     return KI_UP;
		case K_DOWNARROW:   return KI_DOWN;
		case K_LEFTARROW:   return KI_LEFT;
		case K_RIGHTARROW:  return KI_RIGHT;
		case K_LALT:        return KI_LMENU;// ??
		case K_LCTRL:   return KI_LCONTROL;
		case K_LSHIFT:  return KI_LSHIFT;
		case K_RALT:        return KI_RMENU;// ??
		case K_RCTRL:   return KI_RCONTROL;
		case K_RSHIFT:  return KI_RSHIFT;
		case K_F1:      return KI_F1;
		case K_F2:      return KI_F2;
		case K_F3:      return KI_F3;
		case K_F4:      return KI_F4;
		case K_F5:      return KI_F5;
		case K_F6:      return KI_F6;
		case K_F7:      return KI_F7;
		case K_F8:      return KI_F8;
		case K_F9:      return KI_F9;
		case K_F10:     return KI_F10;
		case K_F11:     return KI_F11;
		case K_F12:     return KI_F12;
		case K_F13:     return KI_F13;
		case K_F14:     return KI_F14;
		case K_F15:     return KI_F15;
		case K_INS:     return KI_INSERT;
		case K_DEL:     return KI_DELETE;
		case K_PGDN:    return KI_PRIOR;
		case K_PGUP:    return KI_NEXT;
		case K_HOME:    return KI_HOME;
		case K_END:     return KI_END;
		case K_WIN:     return KI_LWIN;
		case K_MENU:    return KI_LMENU;
		case K_COMMAND: return KI_LMETA;    // Mac - different keycode from K_WIN (ch: turn back?)
		case K_OPTION:  return KI_LMENU;    // Mac - different keycode from K_ALT (ch: turn back?)
		case K_NUMLOCK: return KI_NUMLOCK;
		case KP_SLASH:  return KI_DIVIDE;
		case KP_STAR:   return KI_MULTIPLY;
		case KP_HOME:       return KI_NUMPAD7;
		case KP_UPARROW:    return KI_NUMPAD8;
		case KP_PGUP:       return KI_NUMPAD9;
		case KP_MINUS:      return KI_SUBTRACT;
		case KP_LEFTARROW:  return KI_NUMPAD4;
		case KP_5:          return KI_NUMPAD5;
		case KP_RIGHTARROW: return KI_NUMPAD6;
		case KP_PLUS:       return KI_ADD;
		case KP_END:        return KI_NUMPAD1;
		case KP_DOWNARROW:  return KI_NUMPAD2;
		case KP_PGDN:       return KI_NUMPAD3;
		case KP_INS:        return KI_NUMPAD0;
		case KP_DEL:        return KI_DELETE;
		case KP_ENTER:      return KI_NUMPADENTER;
		case KP_MULT:       return KI_MULTIPLY;
		case KP_EQUAL:      return KI_RETURN;
		default:       return 0;
	}

	return 0;
}

int KeyConverter::fromRocketKey( int key ) {
	if( key >= KI_0 && key <= KI_9 ) {
		return '0' + ( key - KI_0 );
	}
	if( key >= KI_A && key <= KI_Z ) {
		return 'a' + ( key - KI_A );
	}
	if( key >= KI_OEM_1 && key <= KI_OEM_7 ) {
		return oem_keys[key - KI_OEM_1];
	}

	switch( key ) {
		case KI_TAB:       return K_TAB;
		case KI_RETURN:    return K_ENTER;
		case KI_ESCAPE:    return K_ESCAPE;
		case KI_SPACE:     return K_SPACE;// K_SPACE == ' '
		case KI_BACK:       return K_BACKSPACE;
		case KI_CAPITAL:    return K_CAPSLOCK;
		case KI_SCROLL:     return K_SCROLLLOCK;
		case KI_PAUSE:      return K_PAUSE;
		case KI_UP:         return K_UPARROW;
		case KI_DOWN:       return K_DOWNARROW;
		case KI_LEFT:       return K_LEFTARROW;
		case KI_RIGHT:      return K_RIGHTARROW;
		case KI_LMENU:      return K_LALT;// ch : on mac K_MENU or K_OPTION ?
		case KI_LCONTROL:   return K_LCTRL;
		case KI_LSHIFT:     return K_LSHIFT;
		case KI_RMENU:      return K_RALT;// ch : on mac K_MENU or K_OPTION ?
		case KI_RCONTROL:   return K_RCTRL;
		case KI_RSHIFT:     return K_RSHIFT;
		case KI_F1:         return K_F1;
		case KI_F2:         return K_F2;
		case KI_F3:         return K_F3;
		case KI_F4:         return K_F4;
		case KI_F5:         return K_F5;
		case KI_F6:         return K_F6;
		case KI_F7:         return K_F7;
		case KI_F8:         return K_F8;
		case KI_F9:         return K_F9;
		case KI_F10:        return K_F10;
		case KI_F11:        return K_F11;
		case KI_F12:        return K_F12;
		case KI_F13:        return K_F13;
		case KI_F14:        return K_F14;
		case KI_F15:        return K_F15;
		case KI_INSERT:     return K_INS;
		case KI_DELETE:     return K_DEL;
		case KI_PRIOR:      return K_PGDN;
		case KI_NEXT:       return K_PGUP;
		case KI_HOME:       return K_HOME;
		case KI_END:        return K_END;
		case KI_LWIN:       return K_WIN;
		case KI_LMETA:      return K_COMMAND;   // Mac - different keycode from K_WIN (ch: turn back?)
		case KI_NUMLOCK:    return K_NUMLOCK;
		case KI_DIVIDE:     return KP_SLASH;
		case KI_MULTIPLY:   return KP_STAR;
		case KI_NUMPAD7:    return KP_HOME;
		case KI_NUMPAD8:    return KP_UPARROW;
		case KI_NUMPAD9:    return KP_PGUP;
		case KI_SUBTRACT:   return KP_MINUS;
		case KI_NUMPAD4:    return KP_LEFTARROW;
		case KI_NUMPAD5:    return KP_5;
		case KI_NUMPAD6:    return KP_RIGHTARROW;
		case KI_ADD:        return KP_PLUS;
		case KI_NUMPAD1:    return KP_END;
		case KI_NUMPAD2:    return KP_DOWNARROW;
		case KI_NUMPAD3:    return KP_PGDN;
		case KI_NUMPAD0:    return KP_INS;
		case KI_NUMPADENTER:        return KP_ENTER;
		default:       return 0;
	}

	return 0;
}

int KeyConverter::specialChar( int c ) {
	// base this on ascii characters
	// return 0 when not special char or the char when is
	// !"#$%&'().. etc..
	// FIXME: not necessarily all of these are bindable
	if( c >= '!' && c <= '/' ) {
		return c;
	}
	if( c >= ':' && c <= '@' ) {
		return c;
	}
	if( c >= '[' && c <= '`' ) {
		return c;
	}
	if( c >= '{' && c <= '~' ) {
		return c;
	}

	return 0;
}

int KeyConverter::toRocketWheel( int wheel ) {
	return ( wheel == K_MWHEELUP ? -1 : ( wheel == K_MWHEELDOWN ? 1 : 0 ) );
}

int KeyConverter::fromRocketWheel( int wheel ) {
	return ( wheel > 0 ? K_MWHEELDOWN : ( wheel < 0 ? K_MWHEELUP : 0 ) );
}

}
