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

#ifndef R_TRACE_H
#define R_TRACE_H

typedef struct {
	float fraction;             // time completed, 1.0 = didn't hit anything
	vec3_t endpos;              // final position
	cplane_t plane;             // surface normal at impact
	int surfFlags;              // surface hit
	int ent;
	struct shader_s *shader;    // surface shader
} rtrace_t;

msurface_t *R_TraceLine( rtrace_t *tr, const vec3_t start, const vec3_t end, int surfumask );

#endif // R_TRACE_H
