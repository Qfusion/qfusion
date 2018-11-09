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
#include "datasources/ui_huds_datasource.h"

#define HUDS_SOURCE "huds"
#define TABLE_NAME "list"
#define FIELDS "name"

namespace WSWUI
{

HudsDataSource::HudsDataSource( void ) :
	Rocket::Controls::DataSource( HUDS_SOURCE ) {
	UpdateHudsList();
}

HudsDataSource::~HudsDataSource( void ) {
}

void HudsDataSource::UpdateHudsList( void ) {
	hudsList.clear();
	getFileList( hudsList, "huds", ".hud" );

	for( size_t i = 0; i < hudsList.size(); i++ )
		NotifyRowAdd( TABLE_NAME, i, 1 );
}

void HudsDataSource::GetRow( StringList &row, const String &table, int row_index, const StringList &columns ) {
	if( row_index < 0 || (size_t)row_index >= hudsList.size() ) {
		return;
	}

	if( table == TABLE_NAME ) {
		// there should be only 1 column, but we watch ahead in the future
		for( size_t i = 0; i < columns.size(); i++ ) {
			if( columns[i] == FIELDS ) {
				row.push_back( hudsList[row_index].c_str() );
			}
		}
	}
}

int HudsDataSource::GetNumRows( const String &table ) {
	return hudsList.size();
}
}
