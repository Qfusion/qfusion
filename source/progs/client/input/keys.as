/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2017 Victor Luchits

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

namespace CGame {

namespace Input {

namespace Keys {

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition

===============================================================================
*/

class Kbutton
{
	int[] down(2);    // key nums holding it down
	int64 downtime;   // msec timestamp
	uint msec;        // msec down this frame
	int state;
};

Kbutton in_forward, in_back, in_left, in_right;
Kbutton in_attack;
Kbutton in_jump;
Kbutton in_special;
Kbutton in_crouch;
Kbutton in_walk;
Kbutton in_zoom;

/*
* Init
*/
void Init() {
	CGame::Cmd::AddCommand( "+forward", ForwardDown );
	CGame::Cmd::AddCommand( "-forward", ForwardUp );
	CGame::Cmd::AddCommand( "+back", BackDown );
	CGame::Cmd::AddCommand( "-back", BackUp );
	CGame::Cmd::AddCommand( "+left", LeftDown );
	CGame::Cmd::AddCommand( "-left", LeftUp );
	CGame::Cmd::AddCommand( "+right", RightDown );
	CGame::Cmd::AddCommand( "-right", RightUp );

	CGame::Cmd::AddCommand( "+jump", JumpDown );
	CGame::Cmd::AddCommand( "-jump", JumpUp );
	CGame::Cmd::AddCommand( "+special", SpecialDown );
	CGame::Cmd::AddCommand( "-special", SpecialUp );
	CGame::Cmd::AddCommand( "+crouch", CrouchDown );
	CGame::Cmd::AddCommand( "-crouch", CrouchUp );
	CGame::Cmd::AddCommand( "+walk", WalkDown );
	CGame::Cmd::AddCommand( "-walk", WalkUp );

	CGame::Cmd::AddCommand( "+attack", AttackDown );
	CGame::Cmd::AddCommand( "-attack", AttackUp );
	CGame::Cmd::AddCommand( "+zoom", ZoomDown );
	CGame::Cmd::AddCommand( "-zoom", ZoomUp );

	// legacy command names
	CGame::Cmd::AddCommand( "+moveleft", LeftDown );
	CGame::Cmd::AddCommand( "-moveleft", LeftUp );
	CGame::Cmd::AddCommand( "+moveright", RightDown );
	CGame::Cmd::AddCommand( "-moveright", RightUp );
	CGame::Cmd::AddCommand( "+moveup", JumpDown );
	CGame::Cmd::AddCommand( "-moveup", JumpUp );
	CGame::Cmd::AddCommand( "+movedown", CrouchDown );
	CGame::Cmd::AddCommand( "-movedown", CrouchUp );
	CGame::Cmd::AddCommand( "+speed", WalkDown );
	CGame::Cmd::AddCommand( "-speed", WalkUp );
}

/*
* KeyDown
*/
void KeyDown( Kbutton @b ) {
	int k;

	const auto @c = CGame::Cmd::Argv( 1 );
	if( c.empty() ) {
		k = -1; // typed manually at the console for continuous down
	} else {
		k = c.toInt();
	}

	if( k == b.down[0] || k == b.down[1] ) {
		return; // repeating key
	}

	if( b.down[0] == 0 ) {
		b.down[0] = k;
	} else if( b.down[1] == 0 ) {
		b.down[1] = k;
	} else {
		CGame::Print( "Three keys down for a button!\n" );
		return;
	}

	if( ( b.state & 1 ) != 0 ) {
		return; // still down
	}

	// save timestamp
	@c = CGame::Cmd::Argv( 2 );
	b.downtime = c.toInt();
	if( b.downtime == 0 ) {
		b.downtime = curTime - 100;
	}

	b.state |= 1 + 2; // down + impulse down
}

/*
* KeyUp
*/
void KeyUp( Kbutton @b ) {
	int uptime;

	const auto @c = CGame::Cmd::Argv( 1 );
	if( c.empty() ) {
		// typed manually at the console, assume for unsticking, so clear all
		b.down[0] = b.down[1] = 0;
		b.state = 4; // impulse up
		return;
	}
	
	int k = c.toInt();
	if( b.down[0] == k ) {
		b.down[0] = 0;
	} else if( b.down[1] == k ) {
		b.down[1] = 0;
	} else {
		return; // key up without corresponding down (menu pass through)
	}

	if( b.down[0] != 0 || b.down[1] != 0 ) {
		return; // some other key is still holding it down
	}

	if( ( b.state & 1 ) == 0 ) {
		return; // still up (this should not happen)
	}

	// save timestamp
	@c = CGame::Cmd::Argv( 2 );
	uptime = c.toInt();
	if( uptime != 0 ) {
		b.msec += uptime - b.downtime;
	} else {
		b.msec += 10;
	}

	b.state &= ~1; // now up
	b.state |= 4;  // impulse up
}


void JumpDown( void ) { KeyDown( in_jump ); }
void JumpUp( void ) { KeyUp( in_jump ); }
void CrouchDown( void ) { KeyDown( in_crouch ); }
void CrouchUp( void ) { KeyUp( in_crouch ); }
void ForwardDown( void ) { KeyDown( in_forward ); }
void ForwardUp( void ) { KeyUp( in_forward ); }
void BackDown( void ) { KeyDown( in_back ); }
void BackUp( void ) { KeyUp( in_back ); }
void LeftDown( void ) { KeyDown( in_left ); }
void LeftUp( void ) { KeyUp( in_left ); }
void RightDown( void ) { KeyDown( in_right ); }
void RightUp( void ) { KeyUp( in_right ); }
void WalkDown( void ) { KeyDown( in_walk ); }
void WalkUp( void ) { KeyUp( in_walk ); }
void AttackDown( void ) { KeyDown( in_attack ); }
void AttackUp( void ) { KeyUp( in_attack ); }
void SpecialDown( void ) { KeyDown( in_special ); }
void SpecialUp( void ) { KeyUp( in_special ); }
void ZoomDown( void ) { KeyDown( in_zoom ); }
void ZoomUp( void ) { KeyUp( in_zoom ); }

/*
* KeyState
*/
float KeyState( Kbutton @key ) {
	key.state &= 1; // clear impulses

	int msec = key.msec;
	key.msec = 0;

	if( key.state != 0 ) {
		// still down
		msec += curTime - key.downtime;
		key.downtime = curTime;
	}

	if( frameTime == 0 ) {
		return 0;
	}

	float val = float(msec) / float(frameTime);
	if( val <= 0.0 ) {
		return 0.0;
	}
	if( val >= 1.0 ) {
		return 1.0;
	}
	return val;
}

/*
* GetMovement
*/
Vec3 GetMovement() {
	float down;
	Vec3 movement;

	movement[0] += KeyState( in_right );
	movement[0] -= KeyState( in_left );

	movement[1] += KeyState( in_forward );
	movement[1] -= KeyState( in_back );

	movement[2] += KeyState( in_jump );
	movement[2] -= KeyState( in_crouch );
	
	return movement;
}

/*
* GetButtonBits
*/
uint GetButtonBits() {
	uint buttons = BUTTON_NONE;

	// figure button bits

	if( ( in_attack.state & 3 ) != 0 ) {
		buttons |= BUTTON_ATTACK;
	}
	in_attack.state &= ~2;

	if( ( in_special.state & 3 ) != 0 ) {
		buttons |= BUTTON_SPECIAL;
	}
	in_special.state &= ~2;

	if( ( in_walk.state & 1 ) != 0 ) {
		buttons |= BUTTON_WALK;
	}

	if( ( in_zoom.state & 3 ) != 0 ) {
		buttons |= BUTTON_ZOOM;
	}
	in_zoom.state &= ~2;

	return buttons;
}

}

}

}
