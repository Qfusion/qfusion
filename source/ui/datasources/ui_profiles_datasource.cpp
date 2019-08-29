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
#include "datasources/ui_profiles_datasource.h"

#define PROFILES_SOURCE "profiles"
#define TABLE_NAME "list"
#define PROFILES_NAME "name"

namespace WSWUI
{

ProfilesDataSource::ProfilesDataSource( void ) :
	Rml::Controls::DataSource( PROFILES_SOURCE ) {
	UpdateProfiles();
}

ProfilesDataSource::~ProfilesDataSource( void ) {
}

// populates profiles list
void ProfilesDataSource::UpdateProfiles( void ) {
	profilesList.clear();
	getFileList( profilesList, "profiles", ".cfg", false );
}

void ProfilesDataSource::GetRow( Rml::Core::StringList &row, const std::string &table, int row_index, const Rml::Core::StringList &columns ) {
	if( row_index < 0 || (size_t)row_index >= profilesList.size() ) {
		return;
	}

	// populate table
	if( table == TABLE_NAME ) {
		for( size_t i = 0; i < columns.size(); i++ ) {
			if( columns[i] == PROFILES_NAME ) {
				row.push_back( profilesList[row_index].c_str() );
			}
		}
	}
}

int ProfilesDataSource::GetNumRows( const std::string &table ) {
	return profilesList.size();
}

}
