/*
Copyright (C) 2011 Victor Luchits

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

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "kernel/ui_demoinfo.h"
#include "as/asui.h"
#include "as/asui_local.h"

namespace ASUI {

class Irc 
{
public:
	Irc() : 
	  irc_connected( NULL ), irc_perform_str( " " )
	{
	}

	bool isConnected( void )
	{
		bool *c;

		if( !irc_connected )
			irc_connected = trap::Dynvar_Lookup( "irc_connected" );
		assert( irc_connected );

		trap::Dynvar_GetValue( irc_connected, (void **) &c );
		if( *c ) {
			return true;
		}

		return false;
	}

	void connect( void )
	{
		trap::Cmd_ExecuteText( EXEC_APPEND, "irc_connect\n" );
	}

	void connect( const asstring_t &hostname, const int port )
	{
		trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_connect %s %i\n", hostname.buffer, port ) );
	}

	void disconnect( void )
	{
		trap::Cmd_ExecuteText( EXEC_APPEND, "irc_disconnect\n" );
	}

	void join( const asstring_t &channel )
	{
		trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_join %s\n", channel.buffer ) );
	}

	void join( const asstring_t &channel, const asstring_t &password )
	{
		trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_join %s %s\n", channel.buffer, password.buffer ) );
	}

	void part( const asstring_t &channel )
	{
		trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_part %s\n", channel.buffer ) );
	}

	void privateMessage( const asstring_t &target, const asstring_t &message )
	{
		trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_privmsg %s %s\n", target.buffer, message.buffer ) );
	}

	void mode( const asstring_t &target, const asstring_t &modes )
	{
		trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_mode %s %s\n", target.buffer, modes.buffer ) );
	}

	void mode( const asstring_t &target, const asstring_t &modes, const asstring_t &param )
	{
		if( param.len > 0 ) {
			trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_mode %s %s %s\n", target.buffer, modes.buffer, param.buffer ) );
		}
		else {
			trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_mode %s %s\n", target.buffer, modes.buffer, param.buffer ) );
		}
	}

	void who( const asstring_t &nick )
	{
		trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_who %s\n", nick.buffer ) );
	}

	void whois( const asstring_t &nick )
	{
		trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_whois %s\n", nick.buffer ) );
	}

	void whowas( const asstring_t &nick )
	{
		trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_whowas %s\n", nick.buffer ) );
	}

	void quote( const asstring_t &string )
	{
		trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_quote %s\n", string.buffer ) );
	}

	void action( const asstring_t &action )
	{
		trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_action %s\n", action.buffer ) );
	}

	void channelMessage( const asstring_t &message )
	{
		trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_chanmsg %s\n", message.buffer ) );
	}

	void topic( const asstring_t &channel )
	{
		trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_topic %s\n", channel.buffer ) );
	}

	void topic( const asstring_t &channel, const asstring_t &topic )
	{
		if( topic.len > 0 ) {
			trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_topic %s %s\n", channel.buffer, topic.buffer ) );
		}
		else {
			trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_topic %s\n", channel.buffer ) );
		}
	}

	void names( const asstring_t &channel )
	{
		trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_names %s\n", channel.buffer ) );
	}

	void kick( const asstring_t &channel, const asstring_t &nick )
	{
		trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_kick %s %s %s\n", channel.buffer, nick.buffer ) );
	}

	void kick2( const asstring_t &channel, const asstring_t &nick, const asstring_t &reason )
	{
		if( reason.len > 0 ) {
			trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_kick %s %s %s\n", channel.buffer, nick.buffer, reason.buffer ) );
		}
		else {
			trap::Cmd_ExecuteText( EXEC_APPEND, va( "irc_kick %s %s\n", channel.buffer, nick.buffer ) );
		}
	}

	void joinOnEndOfMotd( const asstring_t &string )
	{
		cvar_t *irc_perform = trap::Cvar_Get( "irc_perform", "exec irc_perform.cfg\n", 0 );

		if( string.len > 0 ) {
			irc_perform_str += ";" + (std::string( "irc_join " ) + string.buffer );
			trap::Cvar_Set( irc_perform->name, (irc_perform_str + "\n" ).c_str() );
		}
		else {
			irc_perform_str.clear();
			trap::Cvar_Set( irc_perform->name, irc_perform->dvalue );
		}
	}

private:
	dynvar_t *irc_connected;
	std::string irc_perform_str;
};

// ch : whats up with these statics?
static Irc dummyIrc;

// =====================================================================================

void PrebindIrc( ASInterface *as )
{
	ASBind::Class<Irc, ASBind::class_singleref>( as->getEngine() );
}

void BindIrc( ASInterface *as )
{
	ASBind::GetClass<Irc>( as->getEngine() )
		.method( &Irc::isConnected, "get_connected" )
		.method<void (Irc::*)(void)>( &Irc::connect, "connect" )
		.method2<void (Irc::*)(const asstring_t &, const int )>( &Irc::connect, "void connect( const String &hostname, const int port = 0 )" )
		.method( &Irc::disconnect, "disconnect" )
		.method<void (Irc::*)(const asstring_t &)>( &Irc::join, "join" )
		.method<void (Irc::*)(const asstring_t &, const asstring_t &)>( &Irc::join, "join" )
		.method( &Irc::part, "part" )
		.method( &Irc::privateMessage, "privateMessage" )
		.method<void (Irc::*)(const asstring_t &, const asstring_t &)>( &Irc::mode, "mode" )
		.method<void (Irc::*)(const asstring_t &, const asstring_t &, const asstring_t &)>( &Irc::mode, "mode" )
		.method( &Irc::who, "who" )
		.method( &Irc::whois, "whois" )
		.method( &Irc::whowas, "whowas" )
		.method( &Irc::quote, "quote" )
		.method( &Irc::action, "action" )
		.method( &Irc::names, "names" )
		.method( &Irc::channelMessage, "channelMessage" )
		.method<void (Irc::*)(const asstring_t &)>( &Irc::topic, "topic" )
		.method<void (Irc::*)(const asstring_t &, const asstring_t &)>( &Irc::topic, "topic" )
		.method<void (Irc::*)(const asstring_t &, const asstring_t &)>( &Irc::kick, "kick" )
		.method<void (Irc::*)(const asstring_t &, const asstring_t &, const asstring_t &)>( &Irc::kick2, "kick" )
		.method( &Irc::joinOnEndOfMotd, "joinOnEndOfMotd")
	;
}

void BindIrcGlobal( ASInterface *as )
{
	ASBind::Global( as->getEngine() )
		// global variable
		.var( &dummyIrc, "irc" )
	;
}

}

ASBIND_TYPE( ASUI::Irc, Irc );
