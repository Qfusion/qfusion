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

void item_flag_team1( Entity @ent )
{
    team_CTF_teamflag( ent, TEAM_ALPHA );
}

void info_player_team1( Entity @ent )
{
    team_CTF_genericSpawnpoint( ent, TEAM_ALPHA );
	ent.classname = "team_CTF_alphaspawn";
}

// ============ 

void item_flag_team2( Entity @ent )
{
    team_CTF_teamflag( ent, TEAM_BETA );
}

void info_player_team2( Entity @ent )
{
    team_CTF_genericSpawnpoint( ent, TEAM_BETA );
	ent.classname = "team_CTF_betaspawn";
}

// ============ 

void func_ctf_wall( Entity @ent )
{
    ent.setupModel( ent.model ); // set up the brush model
    ent.solid = SOLID_YES;
    ent.linkEntity();
}
