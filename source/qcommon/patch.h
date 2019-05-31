/*
Copyright (C) 2013 Victor Luchits

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

#define PATCH_EVALUATE_DECL( type )                                       \
	void Patch_Evaluate_ ## type                                          \
		( const type * p, int *numcp, const int *tess, type * dest, int comp, int stride )

PATCH_EVALUATE_DECL( vec_t );
PATCH_EVALUATE_DECL( uint8_t );

#define Patch_Evaluate( type,comp,p,numcp,tess,dest,stride )              \
	Patch_Evaluate_ ## type( p,numcp,tess,dest,comp,stride )

void Patch_GetFlatness( float maxflat, const float *points, int comp, const int *patch_cp, int *flat );

void Patch_RemoveLinearColumnsRows( vec_t *verts, int comp, int *pwidth, int *pheight,
									int numattribs, uint8_t * const *attribs, const int *attribsizes );
