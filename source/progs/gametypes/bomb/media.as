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

int[] iconWeapons( WEAP_TOTAL );

// MODELS
int modelBombModel;
int modelBombModelActive;
int modelBombBackpack;
int modelIndicator;

// SPRITES ETC
int imgBombDecal;

// SOUNDS
int sndBeep;
int sndBombTaken;
int sndBongo;

int[] sndAnnouncementsOff( Announcement_Count );
int[] sndAnnouncementsDef( Announcement_Count );

enum Announcement {
	Announcement_Started,
	Announcement_InPlace,
	Announcement_Armed,
	Announcement_Defused,
	Announcement_Hurry,

	Announcement_Count = Announcement_Hurry,
}

const uint MSG_ALIVE_ALPHA = CS_GENERAL;
const uint MSG_ALIVE_BETA = CS_GENERAL + 1;
const uint MSG_TOTAL_ALPHA = CS_GENERAL + 2;
const uint MSG_TOTAL_BETA = CS_GENERAL + 3;

int getWeaponIcon( int weapon ) {
	return iconWeapons[weapon - 1];
}

void announce( Announcement announcement ) {
	announceOff( announcement );
	announceDef( announcement );
}

void announceOff( Announcement announcement ) {
	if( sndAnnouncementsOff[announcement] != 0 ) {
		G_AnnouncerSound( null, sndAnnouncementsOff[announcement], attackingTeam, true, null );
	}
}

void announceDef( Announcement announcement ) {
	if( sndAnnouncementsDef[announcement] != 0 ) {
		G_AnnouncerSound( null, sndAnnouncementsDef[announcement], defendingTeam, true, null );
	}
}

void mediaInit() {
	iconCarrying = G_ImageIndex( "gfx/bomb/carriericon" );
	iconCarrier  = G_ImageIndex( "gfx/hud/icons/vsay/onoffense" ); // TODO: less crappy icon
	iconReady    = G_ImageIndex( "gfx/hud/icons/vsay/yes" );

	modelBombModel    = G_ModelIndex( "models/objects/misc/bomb_centered.md3", true );
	modelBombModelActive    = G_ModelIndex( "models/objects/misc/bomb_centered_active.md3", true );
	modelBombBackpack = G_ModelIndex( "models/objects/misc/bomb.md3", true );

	imgBombDecal   = G_ImageIndex( "gfx/indicators/radar_decal" );

	sndBeep      = G_SoundIndex( "sounds/bomb/bombtimer", false );
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
		G_SoundIndex( "sounds/announcer/bomb/offense/start", false ),
		G_SoundIndex( "sounds/announcer/bomb/offense/inplace", false ),
		G_SoundIndex( "sounds/announcer/bomb/offense/planted", false ),
		G_SoundIndex( "sounds/announcer/bomb/offense/defused", false ),
		G_SoundIndex( "sounds/misc/timer_bip_bip", false )
	};

	sndAnnouncementsOff = ggASIsBad2;

	int[] ggASIsBad3 =
	{
		G_SoundIndex( "sounds/announcer/bomb/defense/start", false ),
		0,
		G_SoundIndex( "sounds/announcer/bomb/defense/planted", false ),
		G_SoundIndex( "sounds/announcer/bomb/defense/defused", false ),
		G_SoundIndex( "sounds/misc/timer_bip_bip", false )
	};

	sndAnnouncementsDef = ggASIsBad3;
}
