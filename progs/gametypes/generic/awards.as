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

// global constants
const uint ONEVS_AWARD_COUNT = 1;	// how many enemies you have to win in 1vsX situation

const uint SPEEDFRAG_SPEED = 750.0;	// for gametypes with 'normal' speed
const uint SPEEDFRAG_SPEED2 = 1000.0;	// for gametypes that have higher speed cause of no selfdamage

void award_playerKilled( Entity @victim, Entity @attacker, Entity @inflictor )
{
	if( @victim == null || @attacker == null || @attacker.client == null )
		return;
		
	/********** speedfrag ************/
	
	if( @attacker != @victim )
	{
		Cvar g_allow_selfdamage( "g_allow_selfdamage", "", 0 );
		Vec3 avel = attacker.velocity;
		Vec3 vvel = victim.velocity;
		float speed, compSpeed;
	
		// CA and other modes without selfdamage require higher speed
		compSpeed = g_allow_selfdamage.integer == 0 ? SPEEDFRAG_SPEED2 : SPEEDFRAG_SPEED;
		
		// clear vertical velocity
		avel.z = 0.0;
		speed = avel.length();
		if( speed >= compSpeed )
		{
			attacker.client.addAward( S_COLOR_CYAN + "Meep Meep!" );
			// victim.client.addAward( S_COLOR_CYAN + "You got Meep Meeped!" );
			// from headhunt.as
			if ( attacker.client.weapon == WEAP_GUNBLADE )
				attacker.client.addAward( S_COLOR_CYAN + "Gunblade Rush!" );
		}
	
		vvel.z = 0.0;
		speed = vvel.length();
		if( speed >= compSpeed )
		{
			attacker.client.addAward( S_COLOR_CYAN + "Coyote wins!" );
			// victim.client.addAward( S_COLOR_CYAN + "Meep Meep fail!" );
		}
	}

}
