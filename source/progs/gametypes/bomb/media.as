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
int iconCarrier;
int iconReady;

int[] weaponIcons( WEAP_TOTAL );

// MODELS
int modelBombModel;
int modelBombModelActive;
int modelBombBackpack;
int modelIndicator;

// SPRITES ETC
int imgBombDecal;

// SOUNDS
int sndBeep;
int sndPlantStart;
int sndGoodGame;
int sndBombTaken;
int sndBongo;

int[] sndAnnouncementsOff( Announcement_Count );
int[] sndAnnouncementsDef( Announcement_Count );

enum Announcement {
	Announcement_Started,
	Announcement_Armed,
	Announcement_Defused,
	Announcement_Hurry,

	Announcement_Count,
}

const uint MSG_ALIVE_ALPHA = CS_GENERAL;
const uint MSG_ALIVE_BETA = CS_GENERAL + 1;
const uint MSG_TOTAL_ALPHA = CS_GENERAL + 2;
const uint MSG_TOTAL_BETA = CS_GENERAL + 3;

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
	iconCarrier  = G_ImageIndex( "gfx/hud/icons/vsay/onoffense" ); // TODO: less crappy icon
	iconReady    = G_ImageIndex( "gfx/hud/icons/vsay/yes" );

	modelBombModel    = G_ModelIndex( "models/objects/misc/bomb_centered.md3", true );
	modelBombModelActive    = G_ModelIndex( "models/objects/misc/bomb_centered_active.md3", true );
	modelBombBackpack = G_ModelIndex( "models/objects/misc/bomb.md3", true );

	imgBombDecal   = G_ImageIndex( "gfx/indicators/radar_decal" );

	sndBeep = G_SoundIndex( "sounds/bomb/bombtimer", false );
	sndPlantStart = G_SoundIndex( "sounds/misc/timer_bip_bip", false );
	sndGoodGame = G_SoundIndex( "sounds/vsay/goodgame", false );
	sndBombTaken = G_SoundIndex( "sounds/announcer/bomb/offense/taken", false );
	sndBongo = G_SoundIndex( "sounds/announcer/bomb/bongo", false );

	weaponIcons[ WEAP_GUNBLADE ] = G_ImageIndex( "gfx/hud/icons/weapon/gunblade_blast" );
	weaponIcons[ WEAP_MACHINEGUN ] = G_ImageIndex( "gfx/hud/icons/weapon/machinegun" );
	weaponIcons[ WEAP_RIOTGUN ] = G_ImageIndex( "gfx/hud/icons/weapon/riot" );
	weaponIcons[ WEAP_GRENADELAUNCHER ] = G_ImageIndex( "gfx/hud/icons/weapon/grenade" );
	weaponIcons[ WEAP_ROCKETLAUNCHER ] = G_ImageIndex( "gfx/hud/icons/weapon/rocket" );
	weaponIcons[ WEAP_PLASMAGUN ] = G_ImageIndex( "gfx/hud/icons/weapon/plasma" );
	weaponIcons[ WEAP_LASERGUN ] = G_ImageIndex( "gfx/hud/icons/weapon/laser" );
	weaponIcons[ WEAP_ELECTROBOLT ] = G_ImageIndex( "gfx/hud/icons/weapon/electro" );

	sndAnnouncementsOff[ Announcement_Started ] = G_SoundIndex( "sounds/announcer/bomb/offense/start", false );
	sndAnnouncementsOff[ Announcement_Armed ] = G_SoundIndex( "sounds/announcer/bomb/offense/planted", false );
	sndAnnouncementsOff[ Announcement_Defused ] = G_SoundIndex( "sounds/announcer/bomb/offense/defused", false );
	sndAnnouncementsOff[ Announcement_Hurry ] = G_SoundIndex( "sounds/misc/timer_bip_bip", false );

	sndAnnouncementsDef[ Announcement_Started ] = G_SoundIndex( "sounds/announcer/bomb/defense/start", false );
	sndAnnouncementsDef[ Announcement_Armed ] = G_SoundIndex( "sounds/announcer/bomb/defense/planted", false );
	sndAnnouncementsDef[ Announcement_Defused ] = G_SoundIndex( "sounds/announcer/bomb/defense/defused", false );
	sndAnnouncementsDef[ Announcement_Hurry ] = G_SoundIndex( "sounds/misc/timer_bip_bip", false );
}
