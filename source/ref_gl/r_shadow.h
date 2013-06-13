/*
Copyright (C) 2002-2007 Victor Luchits

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
#ifndef __R_SHADOW_H__
#define __R_SHADOW_H__

#define MAX_SHADOWGROUPS    32

typedef struct shadowGroup_s
{
	unsigned int		id;
	unsigned int		bit;
	image_t				*shadowmap;

	int					viewportSize[2];
	int					textureSize[2];

	vec3_t				origin;
	float				radius;
	qbyte				*vis;

	vec3_t				lightDir;
	vec4_t				lightAmbient;

	float				alpha;

	float				projDist;
	vec3_t				mins, maxs;
	vec3_t				visMins, visMaxs;

	qboolean			useOrtho;
	mat4_t			cameraProjectionMatrix;
	struct shadowGroup_s *hashNext;
} shadowGroup_t;

extern drawList_t r_shadowlist;

void		R_ClearShadowGroups( void );
qboolean	R_AddLightOccluder( const entity_t *ent );
void		R_BuildShadowGroups( void );
void		R_DrawShadowmaps( void );

#endif /*__R_SHADOW_H__*/
