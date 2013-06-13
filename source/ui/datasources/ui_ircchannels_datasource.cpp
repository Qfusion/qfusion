#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_utils.h"
#include "datasources/ui_ircchannels_datasource.h"

#define TVCHANNELS_SOURCE	"ircchannels"
#define MAINTABLE_NAME		"list"

using namespace Rocket::Core;
using namespace Rocket::Controls;

namespace WSWUI
{

IrcChannelsDataSource::IrcChannelsDataSource() : DataSource( TVCHANNELS_SOURCE ), channelString( "" )
{
	channelList.clear();
}

IrcChannelsDataSource::~IrcChannelsDataSource( void )
{
	channelList.clear();
}

void IrcChannelsDataSource::GetRow( StringList &row, const String &table, int row_index, const StringList& cols )
{
	if( table != MAINTABLE_NAME ) {
		return;
	}

	ChannelList::const_iterator chan_it = channelList.begin();
	std::advance( chan_it, row_index );
	if( chan_it == channelList.end() ) {;
		return;
	}

	const std::string &chan_name = *chan_it;
	for( StringList::const_iterator it = cols.begin(); it != cols.end(); ++it )
	{
		// we only support one column atm..
		if( *it == "name" ) row.push_back( chan_name.c_str() );
		else row.push_back("");
	}
}

int IrcChannelsDataSource::GetNumRows( const String &table )
{
	if( table != MAINTABLE_NAME ) {
		return 0;
	}
	return channelList.size();
}

void IrcChannelsDataSource::UpdateFrame( void )
{
	const char *c = "";

	irc_channels = trap::Dynvar_Lookup( "irc_channels" );
	if( irc_channels ) {
		trap::Dynvar_GetValue( irc_channels, (void **) &c );
	}

	if( channelString == c ) {
		return;
	}

	channelString = c;
	tokenize( channelString, ' ', channelList );

	// notify libRocket of table update
	NotifyRowChange( MAINTABLE_NAME );
}

}
