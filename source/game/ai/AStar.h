/*
Copyright (C) 2006 Pekka Lampila ("Medar"), Damien Deville ("Pb")
and German Garcia Fernandez ("Jal") for Chasseur de bots association.


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

typedef struct astarpath_s
{
	int numNodes;
	short int nodes[2048]; //MAX_NODES jabot092(2)
	int originNode;
	int goalNode;
	int totalDistance;

} astarpath_t;

//	A* PROPS
//===========================================
int AStar_nodeIsInClosed( int node );
int AStar_nodeIsInOpen( int node );
int AStar_nodeIsInPath( int node );
int AStar_ResolvePath( int origin, int goal, int movetypes );
//===========================================
int AStar_GetPath( int origin, int goal, int movetypes, struct astarpath_s *path );
