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

int64 curTime;
int frameTime;
float pixelRatio;

void Init()
{
	pixelRatio = CGame::Camera::GetViewport().screenPixelRatio;

	CGame::Input::Keys::Init();
}

void Shutdown()
{
}

void Frame( int64 curTime_, int frameTime_ )
{
	curTime = curTime_;
	frameTime = frameTime_;

	CGame::Input::Gamepad::Frame();
	CGame::Input::Touch::Frame();
}

void ClearState()
{
	frameTime = 0;

	CGame::Input::Gamepad::ClearState();
}

void MouseMove( int mx, int my )
{
	CGame::Input::Mouse::Move( mx, my );
}

uint GetButtonBits()
{
	uint bits = BUTTON_NONE;
	
	bits |= CGame::Input::Keys::GetButtonBits();
	
	return bits;
}

Vec3 GetAngularMovement()
{
	Vec3 move;
	
	move += CGame::Input::Keys::GetAngularMovement();
	move += CGame::Input::Mouse::GetAngularMovement();
	move += CGame::Input::Gamepad::GetAngularMovement();
	move += CGame::Input::Touch::GetAngularMovement();

	return move;
}

Vec3 GetMovement()
{
	Vec3 move;

	move += CGame::Input::Keys::GetMovement();
	move += CGame::Input::Gamepad::GetMovement();
	move += CGame::Input::Touch::GetMovement();

	return move;
}

}

}
