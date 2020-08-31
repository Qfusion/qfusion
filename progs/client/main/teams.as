namespace CGame {

int ForceTeam( int team ) {
	if( cg_forceMyTeamAlpha.boolean ) {
		int myteam = CGame::PredictedPlayerState.stats[STAT_TEAM];
		if( myteam == TEAM_BETA ) {
			if( team == TEAM_ALPHA ) {
				return TEAM_BETA;
			}
			if( team == TEAM_BETA ) {
				return TEAM_ALPHA;
			}
		}
	}
	return team;
}

void RegisterForceModel( Cvar &in teamForceModel, Cvar &in teamForceModelToggle, Cvar &in teamForceSkin, PlayerModel @&out pmodelinfo, SkinHandle @&out skin ) {
	if( teamForceModel.modified ) {
		teamForceModel.modified = false;
	}

	if( teamForceModelToggle.modified ) {
		teamForceModelToggle.modified = false;
	}

	if( teamForceSkin.modified ) {
		teamForceSkin.modified = false;
	}

	@pmodelinfo = null; // disabled force models
	@skin = null;

	// register new ones if possible
    const String @forceModelName = @teamForceModel.string;
    const String @forceModelSkin = @teamForceSkin.string;
	if( teamForceModelToggle.boolean && !forceModelName.empty() ) {
		@pmodelinfo = CGame::RegisterPlayerModel( StringUtils::Format( "models/players/%s", forceModelName ) );

		// if it failed, it will be NULL, so also disabled
		if( @pmodelinfo !is null ) {
			// when we register a new model, we must re-register the skin, even if the cvar is not modified
			if( !cgs.pure || CGame::IsPureFile( StringUtils::Format( "models/players/%s/%s.skin", forceModelName, forceModelSkin ) ) ) {
				@skin = CGame::RegisterSkin( StringUtils::Format( "models/players/%s/%s", forceModelName, forceModelSkin ) );
			}

			// if the skin failed, we can still try with default value (so only setting model cvar has a visible effect)
			if( @skin is null ) {
				@skin = CGame::RegisterSkin( StringUtils::Format( "models/players/%s/%s", forceModelName, teamForceSkin.defaultString ) );
			}
		}

		if( @pmodelinfo is null  or @skin is null ) {
			@pmodelinfo = null;
			@skin = null;
		}
	}
}

void CheckUpdateTeamModelRegistration( int team ) {
	switch( team ) {
		case TEAM_ALPHA:
			if( cg_teamALPHAmodel.modified || cg_teamALPHAmodelForce.modified || cg_teamALPHAskin.modified ) {
				RegisterForceModel( cg_teamALPHAmodel, cg_teamALPHAmodelForce, cg_teamALPHAskin, cgs.teamModelInfo[TEAM_ALPHA], cgs.teamCustomSkin[TEAM_ALPHA] );
			}
			break;
		case TEAM_BETA:
			if( cg_teamBETAmodel.modified || cg_teamBETAmodelForce.modified || cg_teamBETAskin.modified ) {
				RegisterForceModel( cg_teamBETAmodel, cg_teamBETAmodelForce, cg_teamBETAskin, cgs.teamModelInfo[TEAM_BETA], cgs.teamCustomSkin[TEAM_BETA] );
			}
			break;
		case TEAM_PLAYERS:
			if( cg_teamPLAYERSmodel.modified || cg_teamPLAYERSmodelForce.modified || cg_teamPLAYERSskin.modified ) {
				RegisterForceModel( cg_teamPLAYERSmodel, cg_teamPLAYERSmodelForce, cg_teamPLAYERSskin, cgs.teamModelInfo[TEAM_PLAYERS], cgs.teamCustomSkin[TEAM_PLAYERS] );
			}
			break;
		case TEAM_SPECTATOR:
		default:
			break;
	}
}

bool PModelForCentity( CEntity @cent, PlayerModel @&out modelinfo, SkinHandle @&out skin )
{
	int team;
	CEntity @owner;
	int ownerNum;

	@owner = @cent;
	if( cent.current.type == ET_CORPSE && cent.current.bodyOwner != 0 ) { // it's a body
		@owner = @cgEnts[cent.current.bodyOwner];
	}
	ownerNum = owner.current.number;

	team = ForceTeam( owner.current.team );

	CheckUpdateTeamModelRegistration( team ); // check for cvar changes

	// use the player defined one if not forcing
	@modelinfo = @cgs.pModels[cent.current.modelindex];
	@skin = @cgs.skinPrecache[cent.current.skinNum];

	if( GS::CanForceModels() && ( ownerNum < GS::maxClients + 1 ) ) {
		if( ( team == TEAM_ALPHA ) || ( team == TEAM_BETA ) ||
		    // Don't force the model for the local player in non-team modes to distinguish the sounds from enemies'
			( ( team == TEAM_PLAYERS ) && ( ownerNum != int( cgs.playerNum)  + 1 ) ) ) {
			if( @cgs.teamModelInfo[team] !is null ) {
				// There is a force model for this team
				@modelinfo = @cgs.teamModelInfo[team];

				if( @cgs.teamCustomSkin[team] !is null ) {
					// There is a force skin for this team
					@skin = @cgs.teamCustomSkin[team];
				}

				return true;
			}
		}
	}

	return false;
}

void RegisterTeamColor( int team ) {
	Cvar teamForceColor(cg_teamPLAYERScolor);
	int rgbcolor;

	switch( team ) {
		case TEAM_ALPHA:
			teamForceColor = cg_teamALPHAcolor;
    		break;
		case TEAM_BETA:
			teamForceColor = cg_teamBETAcolor;
	    	break;
		case TEAM_PLAYERS:
		default:
            team = TEAM_PLAYERS;
    		break;
	}

	if( teamForceColor.modified || ( team == TEAM_PLAYERS && cg_teamPLAYERScolorForce.modified ) ) {
		// load default one if in team based gametype
		if( team >= TEAM_ALPHA ) {
			rgbcolor = ReadColorRGBString( teamForceColor.defaultString );
			if( rgbcolor != -1 ) { // won't be -1 unless some coder defines a weird cvar
				cgs.teamColor[team] = rgbcolor;
			}
		}

		// if there is a force color, update with it
		if( !teamForceColor.string.empty() && ( team != TEAM_PLAYERS || cg_teamPLAYERScolorForce.boolean ) ) {
			rgbcolor = ReadColorRGBString( teamForceColor.string );
			if( rgbcolor != -1 ) {
				cgs.teamColor[team] = rgbcolor;
			} else {
				// didn't work, disable force color
				teamForceColor.set( "" );
				if( team == TEAM_PLAYERS ) {
					cg_teamPLAYERScolorForce.set( "" );
				}
			}
		}

		teamForceColor.modified = false;
		if( team == TEAM_PLAYERS ) {
			cg_teamPLAYERScolorForce.modified = false;
		}
	}
}

}
