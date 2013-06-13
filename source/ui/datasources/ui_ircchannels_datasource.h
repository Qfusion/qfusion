#pragma once
#ifndef __UI_IRCCHANNELS_DATASOURCE_H__
#define __UI_IRCCHANNELS_DATASOURCE_H__

#include <Rocket/Controls/DataSource.h>

namespace WSWUI
{

class IrcChannelsDataSource : public Rocket::Controls::DataSource
{
public:
	IrcChannelsDataSource( void );
	~IrcChannelsDataSource( void );

	void GetRow( StringList& row, const String& table, int row_index, const StringList& columns );
	int GetNumRows( const String& table );

	void UpdateFrame( void );

private:
	dynvar_t *irc_channels;
	std::string channelString;

	typedef std::vector<std::string> ChannelList;
	ChannelList channelList;
};

}
#endif
