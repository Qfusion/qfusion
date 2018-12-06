/*
Copyright (C) 2011 Cervesato Andrea ("koochi"), Victor Luchits

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
#include "datasources/ui_demos_datasource.h"

#define DEMOS_SOURCE        "demos"
#define FIELD_NAME          "name"
#define FIELD_PATH          "path"
#define FIELD_ISDIR         "is_dir"

// fixed paths
#define PATH_ROOT       "demos"
#define PATH_PARENT     ".."

namespace WSWUI
{

typedef std::vector<std::string> DirList;

DemoCollection::DemoCollection( void ) :
	path( "" ), demoExtension( "" ), defaultItemName( "" ), numDirectories( 0 ) {
}

DemoCollection::DemoCollection( const std::string &path, const std::string &demoExtension ) :
	path( path ), demoExtension( demoExtension ), defaultItemName( "" ), numDirectories( 0 ) {
	PopulateList();
}

DemoCollection::~DemoCollection( void ) {
	demoList.clear();
}

void DemoCollection::PopulateList( void ) {
	std::string fullPath = std::string( PATH_ROOT ) + ( IsRoot() ? "" : "/" + path );

	// populate the list of directories and demo files
	demoList.clear();

	if( !IsRoot() ) {
		// back dir ".."
		demoList.push_back( PATH_PARENT );
	}

	DirList dirList;
	dirList.clear();

	getFileList( dirList, fullPath, "/", true );
	for( DirList::iterator it = dirList.begin(); it != dirList.end(); ++it ) {
		std::string fullName = *it + "/";
		demoList.push_back( fullName );
	}

	// populate directories
	numDirectories = demoList.size();

	getFileList( demoList, fullPath, demoExtension.c_str(), true );
}

bool DemoCollection::IsRoot( void ) const {
	return path.empty();
}

int DemoCollection::GetNumItems( void ) const {
	return int( demoList.size() );
}

int DemoCollection::GetNumDirectories( void ) const {
	return int( numDirectories );
}

const std::string & DemoCollection::GetItemName( int index ) const {
	assert( index >= 0 && index < int(demoList.size() ) );
	return demoList[index];
}

std::string DemoCollection::GetItemPath( const int index ) const {
	if( !index && !IsRoot() ) {
		// return path to part dir
		return GetPathToParentDir();
	}

	assert( index >= 0 && index < int(demoList.size() ) );
	return ( IsRoot() ? "" : path + "/" ) + demoList[index];
}

std::string DemoCollection::GetPathToParentDir( void ) const {
	if( IsRoot() ) {
		return "";
	}

	// chop off the last "/"
	std::string::size_type lastSlash = path.find_last_of( "/" );
	if( lastSlash == std::string::npos ) {
		return "";
	}

	// now truncate at "/"+1 pos
	return path.substr( 0, lastSlash + 1 );
}

// ===================================================================================

DemosDataSourceHelper::DemosDataSourceHelper( void ) :
	DemoCollection(), updateIndex( 0 ) {
}

DemosDataSourceHelper::DemosDataSourceHelper( const std::string &path, const std::string &demoExtension ) :
	DemoCollection( path, demoExtension ), updateIndex( 0 ) {
}

bool DemosDataSourceHelper::UpdateFrame( int *firstRowAdded, int *numRowsAdded ) {
	if( updateIndex >= demoList.size() ) {
		return false;
	}

	// add 1 row at a time
	*firstRowAdded = int( updateIndex );
	*numRowsAdded = 1;
	updateIndex++;

	return true;
}

int DemosDataSourceHelper::GetUpdateIndex( void ) const {
	return int( updateIndex );
}

// ===================================================================================

DemosDataSource::DemosDataSource( const std::string &demoExtension ) :
	DataSource( "demos" ), lastQueryTable( "" ), demoExtension( demoExtension ) {
}

DemosDataSource::~DemosDataSource( void ) {
	Reset();
}

void DemosDataSource::Reset( void ) {
	for( DemoPathList::const_iterator it = demoPaths.begin(); it != demoPaths.end(); ++it ) {
		NotifyRowRemove( it->first, 0, it->second.GetUpdateIndex() );
	}
	demoPaths.clear();
}

void DemosDataSource::UpdateFrame( void ) {
	for( DemoPathList::iterator it = demoPaths.begin(); it != demoPaths.end(); ++it ) {
		int firstRowAdded, numRowsAdded;

		if( it->second.UpdateFrame( &firstRowAdded, &numRowsAdded ) ) {
			// notify add
			NotifyRowAdd( it->first, firstRowAdded, numRowsAdded );
		}
	}
}

void DemosDataSource::GetRow( StringList& row, const String& table, int row_index, const StringList& columns ) {
	if( demoPaths.find( table ) == demoPaths.end() ) {
		return;
	}

	const DemosDataSourceHelper &demoPath = demoPaths[table];
	const int numDirectories = demoPath.GetNumDirectories();

	if( row_index < 0 || row_index >= demoPath.GetNumItems() ) {
		return;
	}

	for( StringList::const_iterator it = columns.begin(); it != columns.end(); ++it ) {
		const Rocket::Core::String &col = *it;

		if( col == FIELD_NAME ) {
			row.push_back( demoPath.GetItemName( row_index ).c_str() );
		} else if( col == FIELD_PATH ) {
			row.push_back( demoPath.GetItemPath( row_index ).c_str() );
		} else if( col == FIELD_ISDIR ) {
			row.push_back( row_index < numDirectories ? "1" : "0" );
		}
	}
}

int DemosDataSource::GetNumRows( const String& table ) {
	// if we haven't yet traversed the queried path, do it now
	if( demoPaths.find( table ) == demoPaths.end() ) {
		std::string pathStr( table.CString() );

		// chop off the trailing "/"
		if( pathStr.find_last_of( "/" ) + 1 == pathStr.length() ) {
			pathStr = pathStr.substr( 0, pathStr.length() - 1 );
		}

		// the helper will start sending updates the next frame
		demoPaths[table] = DemosDataSourceHelper( pathStr, demoExtension );
	}

	if( !lastQueryTable.Empty() ) {
		// reset cache on directory change
		if( lastQueryTable != table ) {
			demoPaths.erase( demoPaths.find( lastQueryTable ) );
			NotifyRowChange( lastQueryTable );
		}
	}

	lastQueryTable = table;
	const DemosDataSourceHelper &demoPath = demoPaths[table];
	return demoPath.GetUpdateIndex();
}
}
