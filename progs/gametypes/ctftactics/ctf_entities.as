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

// flag bases

cFlagBase @fbHead = null;

class cFlagBase
{
	int checkBlockages;
    bool touched;
    Entity @owner;
    Entity @carrier;
    Entity @minimap;
    int dropper;
    bool handDropped;
    int64 droppedTime;
    cFlagBase @next;

    void Initialize( Entity @spawner )
    {
        this.touched = false;
        this.dropper = 0;
        this.handDropped = false;
        this.droppedTime = 0;
		this.checkBlockages = 0;
        @this.next = @fbHead;
        @fbHead = @this;

        @this.owner = @spawner;
        @this.carrier = @spawner;

        if ( @this.owner == null )
            return;

        Vec3 mins( -16.0, -16.0, -16.0 ), maxs( 16.0, 16.0, 40.0 );

        this.owner.type = ET_FLAG_BASE;
        this.owner.effects = EF_CARRIER|EF_FLAG_TRAIL;
        this.owner.setupModel( "models/objects/flag/flag_base.md3" );
        this.owner.setSize( mins, maxs );
        this.owner.solid = SOLID_TRIGGER;
        this.owner.svflags &= ~uint(SVF_NOCLIENT);
        this.owner.nextThink = levelTime + 1500;

        if ( ( this.owner.spawnFlags & 1 ) != 0 ) // float spawnFlag
            this.owner.moveType = MOVETYPE_NONE;
        else
            this.owner.moveType = MOVETYPE_TOSS;

        this.owner.linkEntity();
		AI::AddGoal( this.owner ); // bases are special because of the timers, use custom reachability checks

        // drop to floor
        Trace tr;
        tr.doTrace( this.owner.origin, vec3Origin, vec3Origin, this.owner.origin - Vec3( 0.0f, 0.0f, 128.0f ), 0, MASK_DEADSOLID );

        @this.minimap = @G_SpawnEntity( "flag_minimap_icon" );
        this.minimap.type = ET_MINIMAP_ICON;
        this.minimap.solid = SOLID_NOT;
        this.minimap.origin = this.owner.origin;
        this.minimap.origin2 = this.owner.origin;
        this.minimap.modelindex = prcFlagIcon;
        this.minimap.team = this.owner.team;
        this.minimap.frame = 24; // size in case of a ET_MINIMAP_ICON
        this.minimap.svflags = (this.owner.svflags & ~uint(SVF_NOCLIENT)) | uint(SVF_BROADCAST);
        this.minimap.linkEntity();
        this.minimap.nextThink = levelTime + 1000;
    }

    cFlagBase()
    {
        Initialize( null );
    }

    cFlagBase( Entity @owner )
    {
        Initialize( owner );
    }

    ~cFlagBase()
    {
    }

    void setCarrier( Entity @ent )
    {
        if ( @this.carrier != @ent )
        {
            this.carrier.effects &= ~uint( EF_CARRIER|EF_FLAG_TRAIL );
        }

        @this.carrier = @ent;
        this.carrier.effects |= EF_CARRIER|EF_FLAG_TRAIL;
        this.dropper = 0;
        this.handDropped = false;
        this.droppedTime = 0;

        if ( @this.carrier == @this.owner )
        {
            this.owner.solid = SOLID_TRIGGER;
        }
        else
        {
            this.owner.solid = SOLID_NOT;
        }

        this.owner.linkEntity();
    }

    void resetFlag()
    {
        this.setCarrier( this.owner );
    }

	void returnFlag()
	{
		G_PrintMsg( null, "The " + this.flagTeamNameColored() + " flag has returned!\n" );
		this.resetFlag();
	}

    void touch( Entity @activator )
    {
        if ( @this.owner == null )
            return;

        if ( match.getState() >= MATCH_STATE_POSTMATCH )
            return;

        if ( @activator == null || @activator.client == null )
            return;

        // if the flag is not at base, no worth continue
        if ( @this.carrier != @this.owner )
            return;

        this.touched = true;

        // activator is touching the flagbase bbox for picking up
        // unlocking is made at flag base thinking

        if ( this.owner.team == activator.team )
        {
            if ( ( activator.effects & EF_CARRIER ) == 0 )
                return;

            this.flagCaptured( activator );
            this.owner.linkEntity();

			AI::ReachedGoal( this.owner ); // let bots know their mission was completed

            return;
        }

        if ( ( activator.effects & EF_CARRIER ) == 0 )
        {
            this.flagStolen( activator );
            this.owner.linkEntity();

			AI::ReachedGoal( this.owner ); // let bots know their mission was completed

            return;
        }
    }

    void think()
    {
        this.owner.nextThink = levelTime + 1;

        if ( match.getState() >= MATCH_STATE_POSTMATCH )
            return;

        // if the flag is not at base, no worth continue
        if ( ( this.owner.effects & EF_CARRIER ) == 0 )
            return;

        // find players around
        Trace tr;
        Vec3 center, mins, maxs;
        Entity @target = null;
        Entity @stop = null;
        Vec3 origin = this.owner.origin;

		// Check for blockers, turrets, ammo dispensers
		int timeNow = levelTime;
		cDispenser @dispenser = null;
		cTurret @turret = null;
		if ( this.checkBlockages < timeNow )
		{
			// Destroy dispensers
			for ( int i = 0; i < MAX_DISPENSERS; i++ )
			{
				if ( gtDispensers[i].inuse == true )
				{
					@dispenser = @gtDispensers[i];

					if( dispenser.bodyEnt.origin.distance( origin ) < CTFT_BUILD_DESTROY_RADIUS )
						dispenser.die(target,target);
				}
			}

			// Destroy turrets
			for ( int i = 0; i < MAX_TURRETS; i++ )
			{
				if ( gtTurrets[i].inuse == true )
				{
					@turret = @gtTurrets[i];

					if( turret.bodyEnt.origin.distance( origin ) < CTFT_BUILD_DESTROY_RADIUS )
						turret.die(target,target);
				}
			}
			this.checkBlockages = timeNow + 3000;
		}
    }

    void thinkRules()
    {
        // check that this flag carrier is sane
        if ( @this.owner != @this.carrier )
        {
			bool returnFlag = false;

            if ( @this.carrier == null || this.carrier.inuse == false ||
                    ( ( this.carrier.effects & EF_CARRIER ) == 0 ) )
            {
                returnFlag = true;
            }
            else if ( @this.carrier.client != null &&
                      ( this.carrier.isGhosting() || this.carrier.team == this.owner.team ) )
            {
                returnFlag = true;
            }
			else if ( @this.carrier.client == null && ( ( G_PointContents( this.carrier.origin ) & CONTENTS_NODROP ) != 0 ) )
			{
				returnFlag = true;
			}

			if ( returnFlag )
			{
			    this.returnFlag();
			}			
        }
    }

    String flagTeamName()
    {
        if ( @this.owner == null )
            return "";

        return G_GetTeam( this.owner.team ).name;
    }

    String flagTeamNameColored()
    {
        return S_COLOR_MAGENTA + this.flagTeamName() + S_COLOR_WHITE;
    }

    Entity @flagDropped( Entity @player, bool cmdDropped )
    {
        if ( @player != @this.carrier )
            return null;

        if ( !cmdDropped )
        {
            G_PrintMsg( null, player.client.name + " has lost the " + this.flagTeamNameColored() + " flag!\n" );
        }

        if ( ( G_PointContents( player.origin ) & CONTENTS_NODROP ) != 0 )
        {
            this.resetFlag();
            return null;
        }

        Entity @flag;
        Vec3 mins( -16.0, -16.0, -16.0 ), maxs( 16.0, 16.0, 40.0 );

        @flag = @G_SpawnEntity( "ctf_flag" );
		@flag.die = ctf_flag_die;
		@flag.touch = ctf_flag_touch;
		@flag.stop = ctf_flag_stop;
		@flag.think = ctf_flag_think;
        flag.team = this.owner.team;
        flag.type = ET_GENERIC;
        flag.effects = 0;
        flag.setSize( mins, maxs );
        flag.solid = SOLID_TRIGGER;
        flag.moveType = MOVETYPE_TOSS;
        flag.svflags &= ~uint(SVF_NOCLIENT);
        flag.nextThink = levelTime + uint( CTF_AUTORETURN_TIME * 1000 );

        // try to launch it if possible
        Trace tr;
        Vec3 end, dir, r, u;

        player.angles.angleVectors( dir, r, u );
        end = ( player.origin + ( 0.5 * ( maxs + mins ) ) ) + ( dir * 24 );

        tr.doTrace( player.origin, mins, maxs, end, player.entNum, MASK_SOLID );

        flag.origin = tr.endPos;
        flag.origin2 = tr.endPos;

        dir *= 100;
        dir.z = 250;
        flag.velocity = dir;

        flag.linkEntity();

        // set the new entity as this base carrier
        this.setCarrier( flag );

        // keep track of last dropper
        this.dropper = player.entNum;
        this.handDropped = cmdDropped;
        this.droppedTime = levelTime + 1000;

        return flag;
    }

    void flagCaptured( Entity @player )
    {
        // reset captured flag's base
        cFlagBase @capturedFlagBase = @CTF_getBaseForCarrier( player );

        if ( @capturedFlagBase != null )
            G_PrintMsg( null, player.client.name + " has captured the " + capturedFlagBase.flagTeamNameColored() + " flag!\n" );

        player.client.addAward( S_COLOR_GREEN + "Flag Capture!" );
        player.client.stats.addScore( CTF_BONUS_CAPTURE );
        G_GetTeam( player.team ).stats.addScore( 1 );

        // announcer sound
        G_AnnouncerSound( player.client, random() > 0.5f ? prcAnnouncerFlagScore01 : prcAnnouncerFlagScore02, player.team, true, null );

        // to teammates
        G_AnnouncerSound( null, random() > 0.5f ? prcAnnouncerFlagScoreTeam01 : prcAnnouncerFlagScoreTeam02, player.team, true, player.client );

        // to enemies
        G_AnnouncerSound( null, random() > 0.5f ? prcAnnouncerFlagScoreEnemy01 : prcAnnouncerFlagScoreEnemy02, player.team == TEAM_ALPHA ? TEAM_BETA : TEAM_ALPHA, true, null );

        if ( @capturedFlagBase != null )
            capturedFlagBase.resetFlag();

    }

    void flagStolen( Entity @player )
    {
        // if it was dropped by a team mate don't add any score
        if ( player.entNum != this.dropper && this.handDropped == false )
        {
            // flag was at base?
            if ( @this.carrier == @this.owner )
            {
                G_PrintMsg( null, player.client.name + " has stolen the " + this.flagTeamNameColored() + " flag!\n" );
                player.client.addAward( S_COLOR_GREEN + "Flag Steal!" );
            }
            else // is a dropped flag
            {
                G_PrintMsg( null, player.client.name + " has picked up the " + this.flagTeamNameColored() + " flag\n" );
                player.client.addAward( S_COLOR_GREEN + "Dropped Flag Steal!" );
            }

            player.client.stats.addScore( CTF_BONUS_STEAL );

            // announcer sound
            G_AnnouncerSound( player.client, prcAnnouncerFlagTaken, player.team, true, null );

            // to teammates
            G_AnnouncerSound( null, random() > 0.5f ? prcAnnouncerFlagTakenTeam01 : prcAnnouncerFlagTakenTeam02, player.team, true, player.client );

            // to enemies
            G_AnnouncerSound( null, random() > 0.5f ? prcAnnouncerFlagTakenEnemy01 : prcAnnouncerFlagTakenEnemy02, player.team == TEAM_ALPHA ? TEAM_BETA : TEAM_ALPHA, true, null );
        }

        this.setCarrier( player );
    }

    void flagRecovered( Entity @player )
    {
        int bonus = CTF_BONUS_RECOVERY;

        // if he was close to capture the flag give extra bonus
        cFlagBase @enemyBase = @CTF_getBaseForTeam( this.owner.team == TEAM_ALPHA ? TEAM_BETA : TEAM_ALPHA );
        if ( @enemyBase == null )
            return;

        if ( enemyBase.owner.origin.distance( player.origin ) < CTF_FLAG_RECOVERY_BONUS_DISTANCE )
            bonus *= 3;

        G_PrintMsg( null, player.client.name + " has recovered the " + this.flagTeamNameColored() + " flag\n" );

        player.client.stats.addScore( bonus );
        if ( bonus == CTF_BONUS_RECOVERY )
            player.client.addAward( S_COLOR_GREEN + "Flag Recovery!" );
        else
            player.client.addAward( S_COLOR_GREEN + "Flag Super Recovery!" );

        // announcer sound
        G_AnnouncerSound( player.client, random() > 0.5f ? prcAnnouncerRecovery01 : prcAnnouncerRecovery02, player.team, true, null );

        // to teammates
        G_AnnouncerSound( null, prcAnnouncerRecoveryTeam, player.team, true, player.client );

        // to enemies
        G_AnnouncerSound( null, prcAnnouncerRecoveryEnemy, player.team == TEAM_ALPHA ? TEAM_BETA : TEAM_ALPHA, true, null );

        this.resetFlag();
    }

    void carrierKilled( Entity @player, Entity @enemy )
    {
        if ( player.team == enemy.team ) // ouch, that was a team kill
            return;

        // if he was just stealing the flag give no bonus
        if ( this.owner.origin.distance( enemy.origin ) < CTF_CARRIER_KILL_BONUS_DISTANCE )
            return;

        // if he was close to capture the flag give extra bonus
        cFlagBase @enemyBase = @CTF_getBaseForTeam( enemy.team );
        if ( @enemyBase == null )
            return;

        int bonus = CTF_BONUS_CARRIER_KILL;

        if ( enemyBase.owner.origin.distance( enemy.origin ) < CTF_CARRIER_KILL_BONUS_DISTANCE )
        {
            bonus *= 3;
        }

        if ( @player.client != null )
        {
            player.client.stats.addScore( bonus );

            if ( bonus == CTF_BONUS_CARRIER_KILL )
                player.client.addAward( S_COLOR_GREEN + "Carrier Kill!" );
            else
                player.client.addAward( S_COLOR_GREEN + "Carrier Super Kill!" );
        }
    }

    void offenderKilled( Entity @player, Entity @enemy )
    {
        if ( player.team == enemy.team ) // ouch, that was a team kill
            return;

        if ( @this.carrier == @enemy || @this.carrier == null )
            return;

        if ( @player.client == null )
            return;

        // flag defense
        if ( @this.carrier == @this.owner )
        {
            if( this.carrier.origin.distance( enemy.origin ) < CTF_OBJECT_DEFENSE_BONUS_DISTANCE )
            {
                if( G_InPVS( this.carrier.origin, enemy.origin ) )
                {
                    player.client.stats.addScore( CTF_BONUS_FLAG_DEFENSE );
                    player.client.addAward( S_COLOR_GREEN + "Flag Defense!" );
                }
            }
        }

        // carrier defense
        cFlagBase @enemyBase = @CTF_getBaseForTeam( enemy.team );
        if( @enemyBase != null && @enemyBase.carrier != @enemyBase.owner && @enemyBase.carrier != @player && enemyBase.carrier.team == player.team )
        {
            if( enemyBase.carrier.origin.distance( enemy.origin ) < CTF_OBJECT_DEFENSE_BONUS_DISTANCE )
            {
                if( G_InPVS( enemyBase.carrier.origin, enemy.origin ) )
                {
                    player.client.stats.addScore( CTF_BONUS_CARRIER_PROTECT );
                    player.client.addAward( S_COLOR_GREEN + "Carrier Defense!" );
                }
            }
        }
    }
}

cFlagBase @CTF_getBaseForOwner( Entity @ent )
{
    for ( cFlagBase @flagBase = @fbHead; @flagBase != null; @flagBase = @flagBase.next )
    {
        if ( @flagBase.owner == @ent )
            return flagBase;
    }

    return null;
}

cFlagBase @CTF_getBaseForCarrier( Entity @ent )
{
    for ( cFlagBase @flagBase = @fbHead; @flagBase != null; @flagBase = @flagBase.next )
    {
        if ( @flagBase.carrier == @ent )
            return flagBase;
    }

    return null;
}

cFlagBase @CTF_getBaseForTeam( int team )
{
    for ( cFlagBase @flagBase = @fbHead; @flagBase != null; @flagBase = @flagBase.next )
    {
        if ( flagBase.owner.team == team )
            return flagBase;
    }

    return null;
}

///*****************************************************************
/// SPAWNED ENTITIES
///*****************************************************************

void flag_minimap_icon_think( Entity @ent )
{
    cFlagBase @flagBase = @CTF_getBaseForTeam( ent.team );

    if ( @flagBase != null && @flagBase.carrier != null )
    {
        ent.origin = flagBase.carrier.origin;

        // is dropped
        if ( @flagBase.carrier != @flagBase.owner && @flagBase.carrier.client == null )
            flagBase.minimap.modelindex = prcFlagIconLost;
        // is stolen
        else if ( @flagBase.carrier.client != null )
            flagBase.minimap.modelindex = prcFlagIconStolen;
        // is at base
        else
            flagBase.minimap.modelindex = prcFlagIcon;
    }

    ent.nextThink = levelTime + 1;
}

void ctf_flag_die( Entity @ent, Entity @inflictor, Entity @attacker )
{
    ctf_flag_think( ent );
}

void ctf_flag_touch( Entity @ent, Entity @other, const Vec3 planeNormal, int surfFlags )
{
    if ( @other.client == null )
        return;

    cFlagBase @flagBase = @CTF_getBaseForCarrier( ent );

    // dropper can not touch the flag during the first second
    if ( flagBase.dropper == other.entNum && flagBase.droppedTime > levelTime )
        return;

    if ( @flagBase != null )
    {
        if ( other.team == ent.team )
        {
            flagBase.flagRecovered( other );
        }
        else
        {
            flagBase.flagStolen( other );
        }
    }

    ent.freeEntity();
}

// the flag is dropped in motion, add it to AI goals when it stops
void ctf_flag_stop( Entity @ent )
{
	AI::AddGoal( ent );
}

void ctf_flag_think( Entity @ent )
{
    cFlagBase @flagBase = @CTF_getBaseForCarrier( ent );

    if ( @flagBase != null )
    {
		flagBase.returnFlag();
    }

    ent.freeEntity();
}

void CTF_PlayerDropFlag( Entity @player, bool cmdDropped )
{
    // find a base having this player as carrier
    cFlagBase @flagBase = CTF_getBaseForCarrier( player );

    if ( @flagBase != null )
        flagBase.flagDropped( player, cmdDropped );
}

void CTF_ResetFlags()
{
    for ( cFlagBase @flagBase = @fbHead; @flagBase != null; @flagBase = @flagBase.next )
        flagBase.resetFlag();
}

///*****************************************************************
/// NEW MAP ENTITY DEFINITIONS
///*****************************************************************

void team_CTF_flag_touch( Entity @ent, Entity @other, const Vec3 planeNormal, int surfFlags )
{
    cFlagBase @flagBase = @CTF_getBaseForOwner( ent );

    if ( @flagBase != null )
        flagBase.touch( other );
}

void team_CTF_flag_think( Entity @ent )
{
    cFlagBase @flagBase = @CTF_getBaseForOwner( ent );

    if ( @flagBase != null )
        flagBase.think();
}

void team_CTF_teamflag( Entity @ent, int team )
{
    ent.team = team;

    Vec3 mins( -16.0, -16.0, -16.0 ), maxs( 16.0, 16.0, 40.0 );

    // check for spawning inside solid, and try to avoid at least the case of shared leaf
    Trace trace;
    trace.doTrace( ent.origin, mins, maxs, ent.origin, -1, MASK_DEADSOLID );
    if ( trace.startSolid || trace.allSolid )
    {
        // try to resolve the shared leaf case by moving it up by a little
        Vec3 start = ent.origin;
        start.z += 16;
        trace.doTrace( start, mins, maxs, start, -1, MASK_DEADSOLID );
        if ( trace.startSolid || trace.allSolid )
        {
            G_Print( ent.classname + " starts inside solid. Inhibited\n" );
            ent.freeEntity();
            return;
        }
    }

	@ent.think = team_CTF_flag_think;
	@ent.touch = team_CTF_flag_touch;

    ent.origin = trace.endPos;
    ent.origin2 = trace.endPos;
	
    cFlagBase thisFlagBase( ent ); // spawn a local holder
}

void team_CTF_betaflag( Entity @ent )
{
    team_CTF_teamflag( ent, TEAM_BETA );
}

void team_CTF_alphaflag( Entity @ent )
{
    team_CTF_teamflag( ent, TEAM_ALPHA );
}

void team_CTF_genericSpawnpoint( Entity @ent, int team )
{
    ent.team = team;

    // drop to floor

    Trace tr;
    Vec3 start, end, mins( -16.0f, -16.0f, -24.0f ), maxs( 16.0f, 16.0f, 40.0f );

    end = start = ent.origin;
    end.z -= 1024;
    start.z += 16;

    // check for starting inside solid
    tr.doTrace( start, mins, maxs, start, ent.entNum, MASK_DEADSOLID );
    if ( tr.startSolid || tr.allSolid )
    {
        G_Print( ent.classname + " starts inside solid. Inhibited\n" );
        ent.freeEntity();
        return;
    }

    if ( ( ent.spawnFlags & 1 ) == 0 ) // do not drop if having the float flag
    {
        if ( tr.doTrace( start, mins, maxs, end, ent.entNum, MASK_DEADSOLID ) )
        {
            start = tr.endPos + tr.planeNormal;
            ent.origin = start;
            ent.origin2 = start;
        }
    }
}

void team_CTF_alphaspawn( Entity @ent )
{
    team_CTF_genericSpawnpoint( ent, TEAM_ALPHA );
}

void team_CTF_betaspawn( Entity @ent )
{
    team_CTF_genericSpawnpoint( ent, TEAM_BETA );
}

void team_CTF_alphaplayer( Entity @ent )
{
    team_CTF_genericSpawnpoint( ent, TEAM_ALPHA );
}

void team_CTF_betaplayer( Entity @ent )
{
    team_CTF_genericSpawnpoint( ent, TEAM_BETA );
}
