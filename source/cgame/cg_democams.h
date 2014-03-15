/*
Copyright (C) 2009 German Garcia Fernandez ("Jal")

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

bool CG_DemoCam_IsFree( void );
bool CG_DemoCam( void ); // Called each frame
bool CG_DemoCam_Update( void );
void CG_DrawDemocam2D( void );
void CG_DemocamInit( void );
void CG_DemocamShutdown( void );
void CG_DemocamReset( void );
int CG_DemoCam_GetViewType( void );
bool CG_DemoCam_GetThirdPerson( void );
float CG_DemoCam_GetOrientation( vec3_t origin, vec3_t angles, vec3_t velocity );
void CG_DemoCam_GetViewDef( cg_viewdef_t *view );
