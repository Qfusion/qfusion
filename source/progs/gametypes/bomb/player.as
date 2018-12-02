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

enum PrimaryWeapon {
	PrimaryWeapon_EBRL,
	PrimaryWeapon_RLLG,
	PrimaryWeapon_EBLG,
	PrimaryWeapon_Pending, // used for pending test
}

enum SecondaryWeapon {
	SecondaryWeapon_PG = WEAP_PLASMAGUN,
	SecondaryWeapon_RG = WEAP_RIOTGUN,
	SecondaryWeapon_GL = WEAP_GRENADELAUNCHER,
	SecondaryWeapon_Pending,
}

const int AMMO_EB = 15;
const int AMMO_RL = 15;
const int AMMO_LG = 180;
const int AMMO_PG = 140;
const int AMMO_RG = 15;
const int AMMO_GL = 10;

cPlayer@[] players( maxClients ); // array of handles
bool playersInitialized = false;

class cPlayer {
	Client @client;

	PrimaryWeapon weapPrimary;
	SecondaryWeapon weapSecondary;

	PrimaryWeapon pendingPrimary;
	SecondaryWeapon pendingSecondary;

	int64 lastLoadoutChangeTime; // so people can't spam change weapons during warmup

	int killsThisRound;

	uint arms;
	uint defuses;

	bool dueToSpawn; // used for respawning during countdown

	bool isCarrier;

	cPlayer( Client @player ) {
		@this.client = @player;

		String loadout = this.client.getUserInfoKey( "cg_loadout" );
		String primary = loadout.getToken( 0 );
		String secondary = loadout.getToken( 1 );
		bool primaryOk = true;
		bool secondaryOk = true;

		if( primary == "ebrl" )
			this.weapPrimary = PrimaryWeapon_EBRL;
		else if( primary == "rllg" )
			this.weapPrimary = PrimaryWeapon_RLLG;
		else if( primary == "eblg" )
			this.weapPrimary = PrimaryWeapon_EBLG;
		else
			primaryOk = false;

		if( secondary == "pg" )
			this.weapSecondary = SecondaryWeapon_PG;
		else if( secondary == "rg" )
			this.weapSecondary = SecondaryWeapon_RG;
		else if( secondary == "gl" )
			this.weapSecondary = SecondaryWeapon_GL;
		else
			secondaryOk = false;

		if( !primaryOk || !secondaryOk ) {
			this.weapPrimary = PrimaryWeapon_EBRL;
			this.weapSecondary = SecondaryWeapon_PG;
		}

		this.pendingPrimary = PrimaryWeapon_Pending;
		this.pendingSecondary = SecondaryWeapon_Pending;

		this.lastLoadoutChangeTime = -1;

		this.arms = 0;
		this.defuses = 0;

		this.dueToSpawn = false;

		this.isCarrier = false;

		@players[player.playerNum] = @this;
	}

	void giveInventory() {
		this.client.inventoryClear();

		if( this.pendingPrimary != PrimaryWeapon_Pending ) {
			this.weapPrimary = this.pendingPrimary;
			this.pendingPrimary = PrimaryWeapon_Pending;
		}

		if( this.pendingSecondary != SecondaryWeapon_Pending ) {
			this.weapSecondary = this.pendingSecondary;
			this.pendingSecondary = SecondaryWeapon_Pending;
		}

		this.client.inventoryGiveItem( WEAP_GUNBLADE );
		this.client.getEnt().health = 200;

		switch( this.weapPrimary ) {
			case PrimaryWeapon_EBRL:
				this.client.inventoryGiveItem( WEAP_ROCKETLAUNCHER );
				this.client.inventorySetCount( AMMO_ROCKETS, AMMO_RL );
				this.client.inventoryGiveItem( WEAP_ELECTROBOLT );
				this.client.inventorySetCount( AMMO_BOLTS, AMMO_EB );
				break;

			case PrimaryWeapon_RLLG:
				this.client.inventoryGiveItem( WEAP_ROCKETLAUNCHER );
				this.client.inventorySetCount( AMMO_ROCKETS, AMMO_RL );
				this.client.inventoryGiveItem( WEAP_LASERGUN );
				this.client.inventorySetCount( AMMO_LASERS, AMMO_LG );
				break;

			case PrimaryWeapon_EBLG:
				this.client.inventoryGiveItem( WEAP_ELECTROBOLT );
				this.client.inventorySetCount( AMMO_BOLTS, AMMO_EB );
				this.client.inventoryGiveItem( WEAP_LASERGUN );
				this.client.inventorySetCount( AMMO_LASERS, AMMO_LG );
				break;
		}

		switch( this.weapSecondary ) {
			case SecondaryWeapon_PG:
				this.client.inventoryGiveItem( WEAP_PLASMAGUN );
				this.client.inventorySetCount( AMMO_PLASMA, AMMO_PG );
				break;

			case SecondaryWeapon_RG:
				this.client.inventoryGiveItem( WEAP_RIOTGUN );
				this.client.inventorySetCount( AMMO_SHELLS, AMMO_RG );
				break;

			case SecondaryWeapon_GL:
				this.client.inventoryGiveItem( WEAP_GRENADELAUNCHER );
				this.client.inventorySetCount( AMMO_GRENADES, AMMO_GL );
				break;
		}

		this.client.selectWeapon( -1 );
	}

	void showPrimarySelection() {
		if( this.client.team == TEAM_SPECTATOR ) {
			return;
		}

		String command = "mecu \"Primary weapons\""
			+ " \"EB + RL\" \"weapselect eb; gametypemenu2\""
			+ " \"RL + LG\" \"weapselect rl; gametypemenu2\""
			+ " \"EB + LG\" \"weapselect lg; gametypemenu2\"";

		if( cvarEnableCarriers.boolean ) {
			if( this.isCarrier ) {
				command += " \"Carrier opt-out\" \"carrier\"";
			}
			else {
				command += " \"Carrier opt-in\" \"carrier\"";
			}
		}

		this.client.execGameCommand( command );
	}

	void showSecondarySelection() {
		if( this.client.team == TEAM_SPECTATOR ) {
			return;
		}

		this.client.execGameCommand( "mecu \"Secondary weapons\""
			+ " \"Plasmagun\" \"weapselect pg\""
			+ " \"Riotgun\" \"weapselect rg\""
			+ " \"Grenade Launcher\" \"weapselect gl\""
		);
	}

	void selectPrimaryWeapon( PrimaryWeapon weapon ) {
		this.pendingPrimary = weapon;
	}

	void selectSecondaryWeapon( SecondaryWeapon weapon ) {
		this.pendingSecondary = weapon;

		if( match.getState() == MATCH_STATE_WARMUP ) {
			if( lastLoadoutChangeTime == -1 || levelTime - lastLoadoutChangeTime >= 1000 ) {
				giveInventory();
				lastLoadoutChangeTime = levelTime;
			}
			else {
				G_PrintMsg( @this.client.getEnt(), "Wait a second\n" );
			}
		}
	}

	void selectWeapon( String &weapon ) {
		String token;
		int len;

		String invalid_tokens;
		bool has_invalid = false;

		// :DD
		for( int i = 0; ( len = ( token = weapon.getToken( i ) ).len() ) > 0; i++ ) {
			if( token == "EB" || token == "EBRL" ) {
				this.selectPrimaryWeapon( PrimaryWeapon_EBRL );
			}
			else if( token == "RL" || token == "RLLG" ) {
				this.selectPrimaryWeapon( PrimaryWeapon_RLLG );
			}
			else if( token == "LG" || token == "EBLG" ) {
				this.selectPrimaryWeapon( PrimaryWeapon_EBLG );
			}
			else if( token == "PG" ) {
				this.selectSecondaryWeapon( SecondaryWeapon_PG );
			}
			else if( token == "RG" ) {
				this.selectSecondaryWeapon( SecondaryWeapon_RG );
			}
			else if( token == "GL" ) {
				this.selectSecondaryWeapon( SecondaryWeapon_GL );
			}
			else {
				invalid_tokens += " " + token;
				has_invalid = true;
			}
		}

		if( has_invalid ) {
			G_PrintMsg( @this.client.getEnt(), "Unrecognised tokens:" + invalid_tokens + "\n" );
		}

		// set cg_loadout
		String loadout = "";
		if( this.pendingPrimary == PrimaryWeapon_EBRL )
			loadout = "ebrl";
		else if( this.pendingPrimary == PrimaryWeapon_RLLG )
			loadout = "rllg";
		else if( this.pendingPrimary == PrimaryWeapon_EBLG )
			loadout = "eblg";
		else
			return;

		if( this.pendingSecondary == SecondaryWeapon_PG )
			loadout += " pg";
		else if( this.pendingSecondary == SecondaryWeapon_RG )
			loadout += " rg";
		else if( this.pendingSecondary == SecondaryWeapon_GL )
			loadout += " gl";
		else
			return;

		this.client.execGameCommand( "saveloadout " + loadout );
	}
}

// since i am using an array of handles this must
// be done to avoid null references if there are players
// already on the server
void playersInit() {
	// do initial setup (that doesn't spawn any entities, but needs clients to be created) only once, not every round
	if( !playersInitialized ) {
		for( int i = 0; i < maxClients; i++ ) {
			Client @client = @G_GetClient( i );

			if( client.state() >= CS_CONNECTING ) {
				cPlayer( @client );
			}
		}

		playersInitialized = true;
	}
}

// using a global counter would be faster
uint getCarrierCount( int teamNum ) {
	uint count = 0;

	Team @team = @G_GetTeam( teamNum );

	for( int i = 0; @team.ent( i ) != null; i++ ) {
		Client @client = @team.ent( i ).client; // stupid AS...
		cPlayer @player = @playerFromClient( @client );

		if( player.isCarrier ) {
			count++;
		}
	}

	return count;
}

void resetKillCounters() {
	for( int i = 0; i < maxClients; i++ ) {
		if( @players[i] != null ) {
			players[i].killsThisRound = 0;
		}
	}
}

cPlayer @playerFromClient( Client @client ) {
	cPlayer @player = @players[client.playerNum];

	// XXX: as of 0.18 this check shouldn't be needed as playersInit works
	if( @player == null ) {
		assert( false, "player.as playerFromClient: no player exists for client - state: " + client.state() );

		return cPlayer( @client );
	}

	return @player;
}

void team_CTF_genericSpawnpoint( Entity @ent, int team ) {
	ent.team = team;

	Trace trace;

	Vec3 start, end;
	Vec3 mins( -16, -16, -24 ), maxs( 16, 16, 40 );

	start = end = ent.origin;

	start.z += 16;
	end.z -= 1024;

	trace.doTrace( start, mins, maxs, end, ent.entNum, MASK_SOLID );

	if( trace.startSolid ) {
		G_Print( ent.classname + " starts inside solid, removing...\n" );

		ent.freeEntity();

		return;
	}

	if( ent.spawnFlags & 1 == 0 ) {
		// move it 1 unit away from the plane

		ent.origin = trace.endPos + trace.planeNormal;
	}
}

void team_CTF_alphaspawn( Entity @ent ) {
	team_CTF_genericSpawnpoint( ent, defendingTeam );
}

void team_CTF_betaspawn( Entity @ent ) {
	team_CTF_genericSpawnpoint( ent, attackingTeam );
}

void team_CTF_alphaplayer( Entity @ent ) {
	team_CTF_genericSpawnpoint( ent, defendingTeam );
}

void team_CTF_betaplayer( Entity @ent ) {
	team_CTF_genericSpawnpoint( ent, attackingTeam );
}
