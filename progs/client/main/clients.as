namespace CGame {

class ClientInfo {
    String name;
    String cleanName;
    int color;
    int hand;

    void Parse( const String &in info ) {
        String @s;
        int rgbcolor;

        if( !GS::Info::Validate( info ) ) {
            CGame::Error( "Invalid client info" );
        }

        @s = GS::Info::ValueForKey( info, "name" );
        name = @s is null or s.empty() ? "badname" : s;

        // name with color tokes stripped
        cleanName = name.removeColorTokens();

        @s = GS::Info::ValueForKey( info, "hand" );
        hand = @s is null or s.empty() ? 2 :  s.toInt();

        @s = GS::Info::ValueForKey( info, "color" );
        color = @s is null or s.empty() ? -1 : ReadColorRGBString( s );
        if( color == -1 ) {
            color = COLOR_RGBA( 255, 255, 255, 255 );
        }
    }
}

void SexedSound( int entnum, int entchannel, const String &in name, float fvol, float attn ) {
	bool fixed;

    if( name.empty() ) {
        return;
    }
    if( entnum < 0 || entnum >= MAX_EDICTS ) {
        return;
    }

	fixed = ( entchannel & CHAN_FIXED ) != 0 ? true : false;
	entchannel &= ~CHAN_FIXED;

    auto @pmodel = @cgEnts[entnum].pmodel.pmodelinfo;
    if( @pmodel is null ) {
        return;
    }

    auto @sfx = pmodel.RegisterSexedSound( name );
    if( @sfx is null ) {
        return;
    }

	if( fixed ) {
		CGame::Sound::StartFixedSound( @sfx, cgEnts[entnum].current.origin, entchannel, fvol, attn );
	} else if( IsViewerEntity( entnum ) ) {
		CGame::Sound::StartGlobalSound( @sfx, entchannel, fvol );
	} else {
		CGame::Sound::StartRelativeSound( @sfx, entnum, entchannel, fvol, attn );
	}
}

/*
* Updates cached client info from the current CS_PLAYERINFOS configstring value
*/
void LoadClientInfo( int client ) {
    cgs.clientInfo[client].Parse( cgs.configStrings[CS_PLAYERINFOS + client] );
}

void ResetClientInfos( void ) {
	for( int i = 0; i < MAX_CLIENTS; i++ ) {
        int cs = CS_PLAYERINFOS + i;

        cgs.clientInfo[i] = ClientInfo();

		if( !cgs.configStrings[cs].empty() ) {
			LoadClientInfo( i );
		}
	}
}

}
