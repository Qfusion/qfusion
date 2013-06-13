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
#include "datasources/ui_crosshair_datasource.h"

#define NUMBER_OF_CROSSHAIRS 14

#define CROSSHAIR_SOURCE "crosshair"
#define TABLE_NAME "list"
#define FIELD_INDEX "index"
#define FIELD_IMAGE "image"

namespace WSWUI
{

CrosshairDataSource::CrosshairDataSource( void ) :
	Rocket::Controls::DataSource( CROSSHAIR_SOURCE )
{
	UpdateCrosshairList();
}

CrosshairDataSource::~CrosshairDataSource( void )
{
}

void CrosshairDataSource::UpdateCrosshairList( void )
{
	crosshairList.clear();

	for( int i = 0; i < NUMBER_OF_CROSSHAIRS; i++ )
	{
		CrossHair c( toString( i ), va( "/gfx/hud/crosshair%i.tga", i ) );
		crosshairList.push_back( c );

		NotifyRowAdd( TABLE_NAME, i, 1 );
	}
}

void CrosshairDataSource::GetRow( StringList &row, const String &table, int row_index, const StringList &columns )
{
	if( row_index < 0 || (size_t)row_index >= crosshairList.size() )
		return;

	if( table == TABLE_NAME )
	{
		// populate table
		for( size_t i = 0; i < columns.size(); i++)
		{
			if( columns[i] == FIELD_INDEX )
			{
				row.push_back( crosshairList[row_index].first.c_str() );
			}
			else if( columns[i] == FIELD_IMAGE )
			{
				row.push_back( crosshairList[row_index].second.c_str() );
			} 
		}
	}
}

int CrosshairDataSource::GetNumRows( const String &table )
{
	return crosshairList.size();
}

}