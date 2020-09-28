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
