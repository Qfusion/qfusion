/*
Copyright (C) 2011 Cervesato Andrea ("koochi")

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
#include "kernel/ui_utils.h"
#include "datasources/ui_video_datasource.h"

#define VIDEO_SOURCE "video"
#define TABLE_NAME "list"
#define RESOLUTION "resolution"
#define MODE "mode"

namespace WSWUI
{

VideoDataSource::VideoDataSource( void ) :
	Rocket::Controls::DataSource( VIDEO_SOURCE ), 
		wideScreen( false )
{
	updateVideoModeList();
}

VideoDataSource::~VideoDataSource( void )
{
}

void VideoDataSource::updateVideoModeList( void )
{
	qboolean qwideScreen;
	char resolution[64];
	int i, width, height;

	// lists must be clear before
	modesList.clear();

	// native desktop resolution
	modesList.push_back( Mode( toString( -2 ), "desktop" ) );

    for( i = 0; trap::VID_GetModeInfo( &width, &height, &qwideScreen, i ); i++ ) ;
	for( i = 0; trap::VID_GetModeInfo( &width, &height, &qwideScreen, i ); i++ )
    {
		Q_snprintfz( resolution, sizeof( resolution ), "%s%i x %i", ( qwideScreen ? "W " : "" ), width, height );

		// save resolution and index
		Mode m( toString( i ), resolution );
		modesList.push_back( m );
    }

	// custom resolution
	modesList.push_back( Mode( toString( -1 ), "custom" ) );

	// notify updates
	int size = modesList.size();
	for( int i = 0; i < size; i++ )
		NotifyRowAdd( TABLE_NAME, i, 1 );
}

void VideoDataSource::GetRow( StringList &row, const String &table, int row_index, const StringList &columns )
{
	if( row_index < 0 || (size_t)row_index >= modesList.size() )
		return;

	// populate table
	if( table == TABLE_NAME )
	{
		for( StringList::const_iterator it = columns.begin(); it != columns.end(); ++it )
		{
			if( *it == RESOLUTION )
				row.push_back( modesList[row_index].second.c_str() );
			else if( *it == MODE )
				row.push_back( modesList[row_index].first.c_str() );
		}
	}
}

int VideoDataSource::GetNumRows( const String &table )
{
	return modesList.size();
}

}
