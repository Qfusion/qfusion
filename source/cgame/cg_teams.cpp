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

#include "cg_local.h"

/*
* CG_ForceTeam
*/
static int CG_ForceTeam( int entNum, int team )
{
	if( !GS_TeamBasedGametype() )
	{
		if( ISVIEWERENTITY( entNum ) )
		{
			if( cg_forceMyTeamAlpha->integer )
				return TEAM_ALPHA;
		}
		else
		{
			if( cg_forceTeamPlayersTeamBeta->integer )
				return TEAM_BETA;
		}

		return team;
	}
	else
	{
		int myteam = cg.predictedPlayerState.stats[STAT_TEAM];

		if( cg_forceMyTeamAlpha->integer && myteam != TEAM_SPECTATOR )
		{
			if( team == myteam )
				return TEAM_ALPHA;
			if( team == TEAM_ALPHA )
				return myteam;
		}

		return team;
	}
}

/*
* CG_SetSceneTeamColors
* Updates the team colors in the renderer with the ones assigned to each team.
*/
void CG_SetSceneTeamColors( void )
{
	int team;
	vec4_t color;

	// send always white for the team spectators
	trap_R_SetCustomColor( TEAM_SPECTATOR, 255, 255, 255 );
	for( team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ )
	{
		CG_TeamColor( team, color );
		trap_R_SetCustomColor( team, (qbyte)( color[0] * 255 ), (qbyte)( color[1] * 255 ), (qbyte)( color[2] * 255 ) ); // update the renderer
	}
}

/*
* CG_RegisterForceModel
*/
static void CG_RegisterForceModel( cvar_t *teamForceModel, cvar_t *teamForceSkin, pmodelinfo_t **ppmodelinfo, struct skinfile_s **pskin )
{
	pmodelinfo_t *pmodelinfo;
	struct skinfile_s *skin = NULL;

	if( teamForceModel )
		teamForceModel->modified = qfalse;

	if( teamForceSkin )
		teamForceSkin->modified = qfalse;

	if( !ppmodelinfo || !pskin )
		return;

	*ppmodelinfo = NULL; // disabled force models
	*pskin = NULL;

	// register new ones if possible
	if( teamForceModel->string[0] )
	{
		pmodelinfo = CG_RegisterPlayerModel( va( "models/players/%s", teamForceModel->string ) );
		// if it failed, it will be NULL, so also disabled
		if( pmodelinfo )
		{
			// when we register a new model, we must re-register the skin, even if the cvar is not modified
			if( !cgs.pure || trap_FS_IsPureFile( va( "models/players/%s/%s.skin", teamForceModel->string, teamForceSkin->string ) ) )
				skin = trap_R_RegisterSkinFile( va( "models/players/%s/%s", teamForceModel->string, teamForceSkin->string ) );
			// if the skin failed, we can still try with default value (so only setting model cvar has a visible effect)
			if( !skin )
				skin = trap_R_RegisterSkinFile( va( "models/players/%s/%s", teamForceModel->string, teamForceSkin->dvalue ) );
		}

		if( pmodelinfo && skin )
		{
			*ppmodelinfo = pmodelinfo;
			*pskin = skin;
		}
	}
}

/*
* CG_CheckUpdateTeamModelRegistration
*/
static void CG_CheckUpdateTeamModelRegistration( int team )
{
	switch( team )
	{
	case TEAM_ALPHA:
		if( cg_teamALPHAmodel->modified || cg_teamALPHAskin->modified )
		{
			CG_RegisterForceModel( cg_teamALPHAmodel, cg_teamALPHAskin, &cgs.teamModelInfo[TEAM_ALPHA], &cgs.teamCustomSkin[TEAM_ALPHA] );
		}
		break;
	case TEAM_BETA:
		if( cg_teamBETAmodel->modified || cg_teamBETAskin->modified )
		{
			CG_RegisterForceModel( cg_teamBETAmodel, cg_teamBETAskin, &cgs.teamModelInfo[TEAM_BETA], &cgs.teamCustomSkin[TEAM_BETA] );
		}
		break;
	case TEAM_PLAYERS:
		if( cg_teamPLAYERSmodel->modified || cg_teamPLAYERSskin->modified )
		{
			CG_RegisterForceModel( cg_teamPLAYERSmodel, cg_teamPLAYERSskin, &cgs.teamModelInfo[TEAM_PLAYERS], &cgs.teamCustomSkin[TEAM_PLAYERS] );
		}
		break;
	case TEAM_SPECTATOR:
	default:
		break;
	}
}

/*
* CG_PModelForCentity
*/
pmodelinfo_t *CG_PModelForCentity( centity_t *cent )
{
	int team;
	centity_t *owner;

	owner = cent;
	if( cent->current.type == ET_CORPSE && cent->current.bodyOwner )  // it's a body
		owner = &cg_entities[cent->current.bodyOwner];

	team = CG_ForceTeam( owner->current.number, owner->current.team );

	CG_CheckUpdateTeamModelRegistration( team ); // check for cvar changes

	if( GS_CanForceModels() && ( owner->current.number < gs.maxclients + 1 ) )
	{
		if( team >= TEAM_PLAYERS && team < GS_MAX_TEAMS )
		{
			if( cgs.teamModelInfo[team] )
			{
				// There is a force model for this team
				return cgs.teamModelInfo[team];
			}
		}
	}

	// return player defined one
	return cgs.pModelsIndex[cent->current.modelindex];
}

/*
* CG_SkinForCentity
*/
struct skinfile_s *CG_SkinForCentity( centity_t *cent )
{
	int team;
	centity_t *owner;

	owner = cent;
	if( cent->current.type == ET_CORPSE && cent->current.bodyOwner )  // it's a body
		owner = &cg_entities[cent->current.bodyOwner];

	team = CG_ForceTeam( owner->current.number, owner->current.team );

	if( GS_CanForceModels() && ( owner->current.number < gs.maxclients + 1 ) )
	{
		if( team >= TEAM_PLAYERS && team < GS_MAX_TEAMS )
		{
			if( cgs.teamCustomSkin[team] ) // There is a force model for this team
				return cgs.teamCustomSkin[team];
		}
	}

	// return player defined one
	return cgs.skinPrecache[cent->current.skinnum];
}

/*
* CG_RegisterTeamColor
*/
void CG_RegisterTeamColor( int team )
{
	cvar_t *teamForceColor = NULL;
	int rgbcolor;
	int *forceColor;

	switch( team )
	{
	case TEAM_ALPHA:
		{
			teamForceColor = cg_teamALPHAcolor;
			forceColor = &cgs.teamColor[TEAM_ALPHA];
		}
		break;
	case TEAM_BETA:
		{
			teamForceColor = cg_teamBETAcolor;
			forceColor = &cgs.teamColor[TEAM_BETA];
		}
		break;

	case TEAM_PLAYERS:
	default:
		{
			teamForceColor = cg_teamPLAYERScolor;
			forceColor = &cgs.teamColor[TEAM_PLAYERS];
		}
		break;
	}

	if( teamForceColor->modified )
	{
		// load default one if in team based gametype
		if( team >= TEAM_ALPHA )
		{
			rgbcolor = COM_ReadColorRGBString( teamForceColor->dvalue );
			if( rgbcolor != -1 )
			{          // won't be -1 unless some coder defines a weird cvar
				*forceColor = rgbcolor;
			}
		}

		// if there is a force color, update with it
		if( teamForceColor->string[0] )
		{
			rgbcolor = COM_ReadColorRGBString( teamForceColor->string );
			if( rgbcolor != -1 )
			{
				*forceColor = rgbcolor;
			}
			else
			{
				trap_Cvar_ForceSet( teamForceColor->name, "" ); // didn't work, disable force color
			}
		}

		teamForceColor->modified = qfalse;
	}
}

/*
* CG_TeamColor
*/
vec_t *CG_TeamColor( int team, vec4_t color )
{
	cvar_t *teamForceColor = NULL;
	int forcedteam;

	forcedteam = CG_ForceTeam( cg.view.POVent, team ); // check all teams against the client
	if( forcedteam < TEAM_PLAYERS || forcedteam >= GS_MAX_TEAMS )  // limit out of range and spectators team
		forcedteam = TEAM_PLAYERS;

	switch( forcedteam )
	{
	case TEAM_ALPHA:
		teamForceColor = cg_teamALPHAcolor;
		break;
	case TEAM_BETA:
		teamForceColor = cg_teamBETAcolor;
		break;
	case TEAM_PLAYERS:
	default:
		teamForceColor = cg_teamPLAYERScolor;
		break;
	}

	if( teamForceColor->modified )
		CG_RegisterTeamColor( forcedteam );

	color[0] = COLOR_R( cgs.teamColor[forcedteam] ) * ( 1.0/255.0 );
	color[1] = COLOR_G( cgs.teamColor[forcedteam] ) * ( 1.0/255.0 );
	color[2] = COLOR_B( cgs.teamColor[forcedteam] ) * ( 1.0/255.0 );
	color[3] = 1.0f;

	return color;
}

/*
* 
*/
qbyte *_ColorForEntity( int entNum, byte_vec4_t color, bool player )
{
	centity_t *cent;
	int team;
	centity_t *owner;
	cvar_t *teamForceColor = NULL;
	int rgbcolor;
	int *forceColor;

	if( entNum < 1 || entNum >= MAX_EDICTS )
	{
		Vector4Set( color, 255, 255, 255, 255 );
		return color;
	}

	owner = cent = &cg_entities[entNum];
	if( cent->current.type == ET_CORPSE && cent->current.bodyOwner ) // it's a body
		owner = &cg_entities[cent->current.bodyOwner];

	team = CG_ForceTeam( owner->current.number, owner->current.team );

	switch( team )
	{
	case TEAM_ALPHA:
		{
			teamForceColor = cg_teamALPHAcolor;
			forceColor = &cgs.teamColor[TEAM_ALPHA];
		}
		break;
	case TEAM_BETA:
		{
			teamForceColor = cg_teamBETAcolor;
			forceColor = &cgs.teamColor[TEAM_BETA];
		}
		break;

	case TEAM_PLAYERS:
	default:
		{
			teamForceColor = cg_teamPLAYERScolor;
			forceColor = &cgs.teamColor[TEAM_PLAYERS];
		}
		break;
	}

	if( teamForceColor->modified )
	{
		CG_RegisterTeamColor( team );
	}

	//if forced models is enabled or it is color forced team we do,
	if( (teamForceColor->string[0] || team >= TEAM_ALPHA) && cent->current.type != ET_CORPSE )
	{
		// skin color to team color
		rgbcolor = *forceColor;
		Vector4Set( color, COLOR_R( rgbcolor ), COLOR_G( rgbcolor ), COLOR_B( rgbcolor ), 255 );
	}
	// user defined colors if it's a player
	else if( ( player && ( owner->current.number - 1 < gs.maxclients ) ) && cent->current.type != ET_CORPSE )
	{
		Vector4Copy( cgs.clientInfo[owner->current.number - 1].color, color );
	} 
	// Make corpses grey
	else if ( cent->current.type == ET_CORPSE && cent->current.bodyOwner ) 
	{
		Vector4Set( color, 60, 60, 60, 255 );
	}
	else // white for everything else
	{
		Vector4Set( color, 255, 255, 255, 255 );
	}

	return color;
}

/*
* 
*/
qbyte *CG_TeamColorForEntity( int entNum, byte_vec4_t color )
{
	return _ColorForEntity( entNum, color, false );
}

/*
* 
*/
qbyte *CG_PlayerColorForEntity( int entNum, byte_vec4_t color )
{
	return _ColorForEntity( entNum, color, true );
}

/*
* CG_RegisterForceModels
* initialize all
*/
void CG_RegisterForceModels( void )
{
	int team;
	CG_RegisterForceModel( cg_teamPLAYERSmodel, cg_teamPLAYERSskin, &cgs.teamModelInfo[TEAM_PLAYERS], &cgs.teamCustomSkin[TEAM_PLAYERS] );
	CG_RegisterForceModel( cg_teamALPHAmodel, cg_teamALPHAskin, &cgs.teamModelInfo[TEAM_ALPHA], &cgs.teamCustomSkin[TEAM_ALPHA] );
	CG_RegisterForceModel( cg_teamBETAmodel, cg_teamBETAskin, &cgs.teamModelInfo[TEAM_BETA], &cgs.teamCustomSkin[TEAM_BETA] );

	for( team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ )
	{
		CG_RegisterTeamColor( team );
	}
}

