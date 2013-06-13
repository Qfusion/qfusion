/*
Copyright (C) 2012 Victor Luchits

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

#include "../cin/cin_public.h"

void CIN_LoadLibrary( qboolean verbose );
void CIN_UnloadLibrary( qboolean verbose );

struct cinematics_s *CIN_Open( const char *name, unsigned int start_time, int flags );
qboolean CIN_NeedNextFrame( struct cinematics_s *cin, unsigned int curtime );
qbyte *CIN_ReadNextFrame( struct cinematics_s *cin, int *width, int *height, int *aspect_numerator, int *aspect_denominator, qboolean *redraw );
void CIN_Close( struct cinematics_s *cin );
