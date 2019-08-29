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
#include "datasources/ui_mods_datasource.h"

#define SOURCE_NAME "mods"
#define TABLE_NAME "list"
#define FIELDS "name"

namespace WSWUI
{

ModsDataSource::ModsDataSource( void ) :
	Rml::Controls::DataSource( SOURCE_NAME ) {
	UpdatePath();
}

ModsDataSource::~ModsDataSource() {}

// update the list of the mod files
void ModsDataSource::UpdatePath( void ) {
	const char *s;
	char buffer[8 * 1024], foldername[MAX_QPATH];
	int numfolders, length, i;

	if( ( numfolders = trap::FS_GetGameDirectoryList( buffer, sizeof( buffer ) ) ) == 0 ) {
		return;
	}

	s = buffer;
	length = 0;
	for( i = 0; i < numfolders; i++, s += length + 1 ) {
		length = strlen( s );
		Q_strncpyz( foldername, s, sizeof( foldername ) );

		modsList.push_back( foldername );
		NotifyRowAdd( TABLE_NAME, i, 1 );
	}
}

void ModsDataSource::GetRow( Rml::Core::StringList &row, const std::string &table, int row_index, const Rml::Core::StringList &columns ) {
	if( row_index < 0 || (size_t)row_index >= modsList.size() ) {
		return;
	}

	if( table == TABLE_NAME ) {
		// there should be only 1 column, but we watch ahead in the future
		for( size_t i = 0; i < columns.size(); i++ ) {
			if( columns[i] == FIELDS ) {
				row.push_back( modsList[row_index].c_str() );
			}
		}
	}
}

int ModsDataSource::GetNumRows( const std::string &table ) {
	return modsList.size();
}

}
