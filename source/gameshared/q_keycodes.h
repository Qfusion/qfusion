/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#pragma once

//
// these are the key numbers that should be passed to Key_Event
//
typedef enum {
	K_TAB = 9,
	K_ENTER = 13,
	K_ESCAPE = 27,
	K_SPACE = 32,

	// normal keys should be passed as lowercased ascii

	K_BACKSPACE = 127,

	K_CAPSLOCK,
	K_SCROLLLOCK,
	K_PAUSE,

	K_UPARROW,
	K_DOWNARROW,
	K_LEFTARROW,
	K_RIGHTARROW,

	K_LALT,
	K_RALT,
	K_LCTRL,
	K_RCTRL,
	K_LSHIFT,
	K_RSHIFT,

	K_F1,
	K_F2,
	K_F3,
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	K_F11,
	K_F12,
	K_F13,
	K_F14,
	K_F15,
	K_INS,
	K_DEL,
	K_PGDN,
	K_PGUP,
	K_HOME,
	K_END,

	K_WIN,
	K_MENU,
	K_COMMAND,      // Mac - different keycode from K_WIN
	K_OPTION,       // Mac - different keycode from K_ALT

	//
	// Keypad stuff..
	//

	K_NUMLOCK,
	KP_SLASH,
	KP_STAR,

	KP_HOME,
	KP_UPARROW,
	KP_PGUP,
	KP_MINUS,

	KP_LEFTARROW,
	KP_5,
	KP_RIGHTARROW,
	KP_PLUS,

	KP_END,
	KP_DOWNARROW,
	KP_PGDN,

	KP_INS,
	KP_DEL,
	KP_ENTER,

	KP_MULT,        // Mac
	KP_EQUAL,       // Mac

	//
	// mouse buttons generate virtual keys
	//
	K_MOUSE1 = 200,
	K_MOUSE2,
	K_MOUSE3,
	K_MOUSE4,
	K_MOUSE5,
	K_MOUSE6,
	K_MOUSE7,
	K_MOUSE8,

	K_MWHEELUP,
	K_MWHEELDOWN,

	K_MOUSE1DBLCLK,
} keyNum_t;

//
// these are the special keys that should be passed to Key_CharEvent
//
#define KC_CTRLC 3
#define KC_CTRLV 22
