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

// ICONS
int iconCarrying;
int iconCarrier;
int iconReady;
int iconDrop;

int[] iconWeapons( WEAP_TOTAL ); // a couple more than needed but who cares it's only like 8 bytes

// MODELS
int modelBombModel;
int modelBombModelActive;
int modelBombBackpack;
int modelIndicator;

// SPRITES ETC
int imgBombSprite;
int imgBombDecal;

// SOUNDS
int sndBeep;
int sndBombTaken;
int sndBongo;

int[] sndAnnouncementsOff( ANNOUNCEMENT_MAX );
int[] sndAnnouncementsDef( ANNOUNCEMENT_MAX );

// commented out announcements were never used in old bomb
/*enum eAnnouncements FIXME enum
{
	ANNOUNCEMENT_STARTED,
	//ANNOUNCEMENT_DROPPED,
	//ANNOUNCEMENT_CARRIED,
	ANNOUNCEMENT_INPLACE,
	ANNOUNCEMENT_ARMED,
	ANNOUNCEMENT_DEFUSED,
	ANNOUNCEMENT_HURRY,
	ANNOUNCEMENT_MAX = ANNOUNCEMENT_HURRY
}*/

const uint ANNOUNCEMENT_STARTED = 0;
const uint ANNOUNCEMENT_INPLACE = 1;
const uint ANNOUNCEMENT_ARMED   = 2;
const uint ANNOUNCEMENT_DEFUSED = 3;
const uint ANNOUNCEMENT_HURRY   = 4;
const uint ANNOUNCEMENT_MAX     = ANNOUNCEMENT_HURRY;

/*enum eMessages FIXME enum
{
	MSG_ALIVE_ALPHA = CS_GENERAL,
	MSG_ALIVE_BETA
}*/

const uint MSG_ALIVE_ALPHA = CS_GENERAL;
const uint MSG_ALIVE_BETA = CS_GENERAL + 1;
const uint MSG_TOTAL_ALPHA = CS_GENERAL + 2;
const uint MSG_TOTAL_BETA = CS_GENERAL + 3;

// weapon is WEAP_* from globals.h
// i cba to write a switch statement
int getWeaponIcon( int weapon ) {
	return iconWeapons[weapon - 1];
}

//void announce( eAnnouncements announcement ) FIXME enum
void announce( uint announcement ) {
	announceOff( announcement );
	announceDef( announcement );
}

//void announceOff( eAnnouncements announcement ) FIXME enum
void announceOff( uint announcement ) {
	if( sndAnnouncementsOff[announcement] != 0 ) {
		G_AnnouncerSound( null, sndAnnouncementsOff[announcement], attackingTeam, true, null );
	}
}

//void announceDef( eAnnouncements announcement ) FIXME enum
void announceDef( uint announcement ) {
	if( sndAnnouncementsDef[announcement] != 0 ) {
		G_AnnouncerSound( null, sndAnnouncementsDef[announcement], defendingTeam, true, null );
	}
}

void mediaInit() {
	iconCarrying = G_ImageIndex( "gfx/bomb/carriericon" );
	iconCarrier  = G_ImageIndex( "gfx/hud/icons/vsay/onoffense" ); // TODO: less crappy icon
	iconReady    = G_ImageIndex( "gfx/hud/icons/vsay/yes" );
	iconDrop     = G_ImageIndex( "gfx/hud/icons/drop/bomb" );

	modelBombModel    = G_ModelIndex( "models/objects/misc/bomb_centered.md3", true );
	modelBombModelActive    = G_ModelIndex( "models/objects/misc/bomb_centered_active.md3", true );
	modelBombBackpack = G_ModelIndex( "models/objects/misc/bomb.md3", true );

	imgBombSprite  = G_ImageIndex( "gfx/indicators/radar" );
	imgBombDecal   = G_ImageIndex( "gfx/indicators/radar_decal" );

	//sndBeep      = G_SoundIndex( "sounds/bomb/bombtimer", true ); FIXME pure
	sndBeep      = G_SoundIndex( "sounds/bomb/bombtimer", false );
	//sndBombTaken = G_SoundIndex( "sounds/announcer/bomb/offense/taken", true ); FIXME pure
	sndBombTaken = G_SoundIndex( "sounds/announcer/bomb/offense/taken", false );
	sndBongo     = G_SoundIndex( "sounds/announcer/bomb/bongo", false );

	int[] ggASIsBad =
	{
		G_ImageIndex( "gfx/hud/icons/weapon/gunblade_blast" ),
		G_ImageIndex( "gfx/hud/icons/weapon/machinegun" ),
		G_ImageIndex( "gfx/hud/icons/weapon/riot" ),
		G_ImageIndex( "gfx/hud/icons/weapon/grenade" ),
		G_ImageIndex( "gfx/hud/icons/weapon/rocket" ),
		G_ImageIndex( "gfx/hud/icons/weapon/plasma" ),
		G_ImageIndex( "gfx/hud/icons/weapon/laser" ),
		G_ImageIndex( "gfx/hud/icons/weapon/electro" )
	};

	iconWeapons = ggASIsBad;

	int[] ggASIsBad2 =
	{
		//G_SoundIndex( "sounds/announcer/bomb/offense/start", true ), FIXME pure
		G_SoundIndex( "sounds/announcer/bomb/offense/start", false ),
		//0,
		//0,
		//G_SoundIndex( "sounds/announcer/bomb/offense/inplace", true ), FIXME pure
		G_SoundIndex( "sounds/announcer/bomb/offense/inplace", false ),
		//G_SoundIndex( "sounds/announcer/bomb/offense/planted", true ), FIXME pure
		G_SoundIndex( "sounds/announcer/bomb/offense/planted", false ),
		//G_SoundIndex( "sounds/announcer/bomb/offense/defused", true ), FIXME pure
		G_SoundIndex( "sounds/announcer/bomb/offense/defused", false ),
		//G_SoundIndex( "sounds/misc/timer_bip_bip", true ) FIXME pure
		G_SoundIndex( "sounds/misc/timer_bip_bip", false )
	};

	sndAnnouncementsOff = ggASIsBad2;

	int[] ggASIsBad3 =
	{
		//G_SoundIndex( "sounds/announcer/bomb/defense/start", true ), FIXME pure
		G_SoundIndex( "sounds/announcer/bomb/defense/start", false ),
		//0,
		//0,
		0,
		//G_SoundIndex( "sounds/announcer/bomb/defense/planted", true ), FIXME pure
		G_SoundIndex( "sounds/announcer/bomb/defense/planted", false ),
		//G_SoundIndex( "sounds/announcer/bomb/defense/defused", true ), FIXME pure
		G_SoundIndex( "sounds/announcer/bomb/defense/defused", false ),
		//G_SoundIndex( "sounds/misc/timer_bip_bip", true ) FIXME pure
		G_SoundIndex( "sounds/misc/timer_bip_bip", false )
	};

	sndAnnouncementsDef = ggASIsBad3;
}
