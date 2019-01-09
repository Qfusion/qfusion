/*
Copyright (C) 2007 Victor Luchits

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

#include "qalgo/hash.h"

#if __has_include( "gitversion.h" )
#include "gitversion.h"
#else
#define APP_VERSION ""
#define APP_VERSION_A 0
#define APP_VERSION_B 0
#define APP_VERSION_C 0
#define APP_VERSION_D 0
#endif

constexpr int APP_PROTOCOL_VERSION = int( Hash32_CT( APP_VERSION, sizeof( APP_VERSION ) ) );
