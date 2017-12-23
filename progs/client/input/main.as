/*
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

void Init()
{
	CGame::Input::Keyboard::Init();
}

void Shutdown()
{
}

void Frame( int64 curTime_, int frameTime_ )
{
	curTime = curTime_;
	frameTime = frameTime_;
}

void ClearState()
{
}

void MouseMove( int mx, int my )
{
	CGame::Input::Mouse::Move( mx, my );
}

uint GetButtonBits()
{
	uint bits = BUTTON_NONE;
	
	bits |= CGame::Input::Keyboard::GetButtonBits();
	
	return bits;
}

Vec3 AddViewAngles( const Vec3 angles )
{
	Vec3 diff;
	
	diff += CGame::Input::Keyboard::AddViewAngles();
	diff += CGame::Input::Mouse::AddViewAngles();

	return angles + diff;
}

Vec3 AddMovement( const Vec3 move )
{
	Vec3 diff;

	diff += CGame::Input::Keyboard::AddMovement();

	return move + diff;
}

}

}
