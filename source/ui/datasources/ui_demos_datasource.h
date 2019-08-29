/*
Copyright (C) 2011 Victor Luchits

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

#ifndef __UI_DEMOS_DATASOURCE_H__
#define __UI_DEMOS_DATASOURCE_H__

#include <RmlUi/Controls/DataSource.h>
#include "kernel/ui_demoinfo.h"

namespace WSWUI
{

/// Collection of directory items inside a demos directory or a subdirectory
class DemoCollection
{
public:
	DemoCollection( void );
	DemoCollection( const std::string &path, const std::string &demoExtension );
	~DemoCollection( void );

	/// Returns total number of items (subdirectories and files) in the path
	int GetNumItems( void ) const;

	/// Returns the number of directories in the path
	int GetNumDirectories( void ) const;

	/// Shortname for a path item
	const std::string & GetItemName( int index ) const;

	/// Relative filesystem path to an item
	std::string GetItemPath( int index ) const;

protected:
	typedef std::vector<std::string> DemoList;

	/// current path, relative to the demo directory
	std::string path;
	std::string demoExtension;
	std::string defaultItemName;

	/// list of path items
	DemoList demoList;
	DemoList::size_type numDirectories;

	/// Fills the demoList
	void PopulateList( void );

	/// Upper ("..) directory path
	std::string GetPathToParentDir( void ) const;

	/// True if object represents path which matches the root demo folder
	bool IsRoot( void ) const;
};

/// This class merely exists to help us counting on table row updates
/// for libRocket DataSource listeners
class DemosDataSourceHelper : public DemoCollection
{
public:
	DemosDataSourceHelper( void );
	DemosDataSourceHelper( const std::string &path, const std::string &demoExtension );

	/// Returns the number of rows the parent DataSource object
	/// should return to its listeners
	bool UpdateFrame( int *firstRowAdded, int *numRowsAdded );

	/// The number of items/rows already updated
	int GetUpdateIndex( void ) const;

private:
	DemoList::size_type updateIndex;
};

class DemosDataSource : public Rml::Controls::DataSource
{
public:
	DemosDataSource( const std::string &demoExtension );
	~DemosDataSource( void );

	// methods which must be overridden
	void GetRow( Rml::Core::StringList& row, const std::string& table, int row_index, const Rml::Core::StringList& columns );
	int GetNumRows( const std::string& table );

	// fetches meta data and notifies of the updates
	void UpdateFrame( void );

	// clears all tables
	void Reset( void );

private:
	typedef std::map<std::string, DemosDataSourceHelper> DemoPathList;
	DemoPathList demoPaths;
	std::string lastQueryTable;

	const std::string demoExtension;
};

}

#endif
