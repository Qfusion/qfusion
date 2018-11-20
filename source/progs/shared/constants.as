/*
Copyright (C) 2009-2011 Chasseur de bots

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
const String S_COLOR_BLACK = "^0";
const String S_COLOR_RED = "^1";
const String S_COLOR_GREEN = "^2";
const String S_COLOR_YELLOW = "^3";
const String S_COLOR_BLUE = "^4";
const String S_COLOR_CYAN = "^5";
const String S_COLOR_MAGENTA = "^6";
const String S_COLOR_WHITE = "^7";
const String S_COLOR_ORANGE = "^8";
const String S_COLOR_GREY = "^9";

const float ATTN_NONE      = 0.0;   // full volume the entire level
const float ATTN_DISTANT   = 0.5;   // distant sound (most likely explosions)
const float ATTN_NORM      = 0.875; // players, weapons, etc
const float ATTN_IDLE      = 2.5;   // stuff around you
const float ATTN_STATIC    = 5.0;   // diminish very rapidly with distance
const float ATTN_FOOTSTEPS = 10.0;  // must be very close to hear it

const Vec3 vec3Origin( 0.0f );
