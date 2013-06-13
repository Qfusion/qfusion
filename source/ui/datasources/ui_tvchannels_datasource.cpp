#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_utils.h"
#include "datasources/ui_tvchannels_datasource.h"

#define TVCHANNELS_SOURCE "tvchannels"
#define MAINTABLE_NAME "list"

using namespace Rocket::Core;
using namespace Rocket::Controls;

namespace WSWUI
{

TVChannelsDataSource::TVChannelsDataSource() : DataSource( TVCHANNELS_SOURCE )
{
}

TVChannelsDataSource::~TVChannelsDataSource(void)
{
	channelList.clear();
}

void TVChannelsDataSource::GetRow(StringList &row, const String &table, int row_index, const StringList& cols)
{
	if( table != MAINTABLE_NAME ) {
		return;
	}

	ChannelList::const_iterator chan_it = channelList.begin();
	std::advance( chan_it, row_index );
	if( chan_it == channelList.end() ) {;
		return;
	}

	int id = chan_it->first;
	const TVChannel &chan = chan_it->second;
	std::string chan_name( chan.realname.empty() ? chan.name.c_str() : chan.realname.c_str() );

	for( StringList::const_iterator it = cols.begin(); it != cols.end(); ++it )
	{
		if( *it == "id" ) row.push_back( String( va( "%i", id ) ) );
		else if( *it == "name" ) row.push_back( chan_name.c_str() );
		else if( *it == "players" ) row.push_back( String( va( "%i", chan.numPlayers ) ) );
		else if( *it == "spectators" ) row.push_back( String( va( "%i", chan.numSpecs ) ) );
		else if( *it == "map" ) row.push_back( chan.mapname.c_str() );
		else if( *it == "gametype" ) row.push_back( chan.gametype.c_str() );
		else if( *it == "matchname" ) row.push_back( chan.matchname.c_str() );
		else if( *it == "address" ) row.push_back( chan.address.c_str() );
		else if( *it == "complexname" ) row.push_back( chan.matchname.empty() ? chan_name.c_str() : chan.matchname.c_str() );
		else row.push_back("");
	}
}

int TVChannelsDataSource::GetNumRows(const String &table)
{
	if( table != MAINTABLE_NAME ) {
		return 0;
	}
	return channelList.size();
}

void TVChannelsDataSource::AddChannel( int id, const TVChannel &chan )
{
	ChannelList::iterator it = channelList.find( id );
	channelList[id] = chan;

	if( it == channelList.end() ) {
		it = channelList.find( id );
		NotifyRowAdd( MAINTABLE_NAME, std::distance( channelList.begin(), it ), 1 );
	}
	else {
		NotifyRowChange( MAINTABLE_NAME, std::distance( channelList.begin(), it ), 1 );
	}
}

void TVChannelsDataSource::RemoveChannel( int id )
{
	ChannelList::iterator it = channelList.find( id );
	if( it == channelList.end() ) { 
		return;
	}

	NotifyRowRemove( MAINTABLE_NAME, std::distance( channelList.begin(), it ), 1 );
	channelList.erase( id );
}

}
