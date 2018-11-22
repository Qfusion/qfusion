#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "datasources/ui_maps_datasource.h"

#define MAPS_SOURCE "maps"
#define TABLE_NAME  "list"
#define MAP_FILE    "name"

namespace WSWUI
{
MapsDataSource::MapsDataSource() : Rocket::Controls::DataSource( MAPS_SOURCE ) {
	getMapsList( mapList );

	// notify the changes
	NotifyRowAdd( TABLE_NAME, 0, mapList.size() );
}

// it returns a list of the maps file
template<typename C>
void MapsDataSource::getMapsList( C& maps_list ) {
	char map_info[MAX_CONFIGSTRING_CHARS];
	char * map_shortname;
	for( int i = 0; trap::ML_GetMapByNum( i, map_info, sizeof( map_info ) ); ++i ) {
		map_shortname = map_info;
		maps_list.push_back( std::string( map_shortname ) );
	}
}

void MapsDataSource::GetRow( Rocket::Core::StringList &row, const Rocket::Core::String&, int row_index, const Rocket::Core::StringList& cols ) {
	if( row_index < 0 || (size_t)row_index > mapList.size() ) {
		return;
	}

	for( Rocket::Core::StringList::const_iterator it = cols.begin(); it != cols.end(); ++it ) {
		if( *it == MAP_FILE ) {
			row.push_back( mapList[row_index].c_str() );
		} else {
			row.push_back( "" );
		}
	}
}

int MapsDataSource::GetNumRows( const Rocket::Core::String& ) {
	return mapList.size();
}
}
