/*
Copyright (C) 2009-2010 Chasseur de bots

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

void team_CTF_redflag( Entity @ent )
{
    team_CTF_teamflag( ent, TEAM_ALPHA );
}

void team_CTF_redplayer( Entity @ent )
{
    team_CTF_genericSpawnpoint( ent, TEAM_ALPHA );
	ent.classname = "team_CTF_alphaplayer";
}

void team_CTF_redspawn( Entity @ent )
{
    team_CTF_genericSpawnpoint( ent, TEAM_ALPHA );
	ent.classname = "team_CTF_alphaspawn";
}

// ============ 

void team_CTF_blueflag( Entity @ent )
{
    team_CTF_teamflag( ent, TEAM_BETA );
}

void team_CTF_blueplayer( Entity @ent )
{
    team_CTF_genericSpawnpoint( ent, TEAM_BETA );
	ent.classname = "team_CTF_betaplayer" ;
}

void team_CTF_bluespawn( Entity @ent )
{
    team_CTF_genericSpawnpoint( ent, TEAM_BETA );
	ent.classname = "team_CTF_betaspawn";
}
