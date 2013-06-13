#pragma once
#ifndef __UI_TVCHANNELS_DATASOURCE_H__
#define __UI_TVCHANNELS_DATASOURCE_H__

#include <Rocket/Controls/DataSource.h>

namespace WSWUI
{
struct TVChannel
{
	int id;
	std::string name;
	std::string realname;
	int numPlayers, numSpecs;
	std::string gametype;
	std::string mapname;
	std::string matchname;
	std::string address;

	TVChannel() : id(0), 
		name(""), realname(""), 
		numPlayers(0), numSpecs(0),
		gametype(""), mapname(""), matchname(""),
		address("")
	{
	}
};

class TVChannelsDataSource : public Rocket::Controls::DataSource
{
public:
	TVChannelsDataSource( void );
	~TVChannelsDataSource( void );

	void GetRow( StringList& row, const String& table, int row_index, const StringList& columns );
	int GetNumRows( const String& table );

	void AddChannel( int id, const TVChannel &chan );
	void RemoveChannel( int id );

private:
	typedef std::map<int, TVChannel> ChannelList;
	ChannelList channelList;
};

}
#endif
