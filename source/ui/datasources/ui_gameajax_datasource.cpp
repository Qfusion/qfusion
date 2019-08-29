/*
Copyright (C) 2013 Victor Luchits

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
#include "datasources/ui_gameajax_datasource.h"

#define GAMEAJAX_SOURCE "gameajax"

using namespace Rml::Core;
using namespace Rml::Controls;

namespace WSWUI
{

typedef std::map<std::string, std::string> Row;

class Table
{
public:
	Table( const std::string &name ) : name( name ) {
	}

	~Table() {
		rows.clear();
	}

	std::string GetName( void ) const {
		return name;
	}

	size_t GetNumRows( void ) const {
		return rows.size();
	}

	void AddRow( const Row &row ) {
		rows.push_back( row );
	}

	void GetRocketRow( Rml::Core::StringList &rocketRow, int row_index, const Rml::Core::StringList& cols ) const {
		RowsList::const_iterator r_it = rows.begin();
		std::advance( r_it, row_index );
		if( r_it == rows.end() ) {
			;
			return;
		}

		const Row &row = *r_it;
		for( Rml::Core::StringList::const_iterator it = cols.begin(); it != cols.end(); ++it ) {
			Row::const_iterator v = row.find( ( *it ).c_str() );
			rocketRow.push_back( v == row.end() ? "" : v->second.c_str() );
		}
	}

private:
	std::string name;
	typedef std::vector<Row> RowsList;
	RowsList rows;
};

class DynTable : public Table
{
public:
	DynTable( const std::string &name, unsigned int updateTime, const std::string &baseURL )
		: Table( name ), updateTime( updateTime ), baseURL( baseURL ) {
	}

	unsigned int GetUpdateTime() const {
		return updateTime;
	}

	const std::string GetBaseURL() const {
		return baseURL;
	}

private:
	unsigned int updateTime;
	std::string baseURL;
};

// ============================================================================

GameAjaxDataSource::GameAjaxDataSource() : DataSource( GAMEAJAX_SOURCE ) {
}

GameAjaxDataSource::~GameAjaxDataSource( void ) {
	for( DynTableList::iterator it = tableList.begin(); it != tableList.end(); ++it ) {
		__delete__( it->second->table );
		__delete__( it->second );
	}
}

void GameAjaxDataSource::GetRow( Rml::Core::StringList &row, const String &table, int row_index, const Rml::Core::StringList& cols ) {
	DynTableList::const_iterator it = tableList.find( table.c_str() );
	if( it == tableList.end() ) {
		return;
	}
	it->second->table->GetRocketRow( row, row_index, cols );
}

int GameAjaxDataSource::GetNumRows( const String &tableName ) {
	int64_t now = trap::Milliseconds();

	char baseURL[1024];
	trap::GetBaseServerURL( baseURL, sizeof( baseURL ) );

	DynTable *table, *oldTable = NULL;
	DynTableList::iterator t_it = tableList.find( tableName.c_str() );

	if( t_it != tableList.end() ) {
		oldTable = t_it->second->table;

		// return cached counter
		if( oldTable->GetBaseURL() == baseURL ) {
			if( oldTable->GetUpdateTime() + UPDATE_INTERVAL > now ) {
				return oldTable->GetNumRows();
			}
		}

		//tableList.erase( t_it );
	}

	// trigger AJAX-style query to server

	std::string stdTableName = tableName.c_str();
	table = __new__( DynTable )( stdTableName, now, baseURL );

	// fetch list now and notify listeners when we get the reply in async manner
	std::string url = std::string( baseURL ) + "/game/" + stdTableName;

	trap::AsyncStream_PerformRequest(
		url.c_str(), "GET", "", 10,
		&GameAjaxDataSource::StreamRead, &GameAjaxDataSource::StreamDone,
		static_cast<void *>( __new__( SourceFetcherPair )( this, __new__( DynTableFetcher )( table ) ) )
		);

	return oldTable != NULL ? oldTable->GetNumRows() : 0;
}

void GameAjaxDataSource::FlushCache( void ) {
	// do nothing
}

size_t GameAjaxDataSource::StreamRead( const void *buf, size_t numb, float percentage,
									   int status, const char *contentType, void *privatep ) {
	if( status < 0 || status >= 300 ) {
		return 0;
	}

	// appened new data to the global buffer
	SourceFetcherPair *fp = static_cast< SourceFetcherPair *>( privatep );
	DynTableFetcher *fetcher = fp->second;
	fetcher->buf += static_cast< const char * >( buf );
	return numb;
}

void GameAjaxDataSource::StreamDone( int status, const char *contentType, void *privatep ) {
	SourceFetcherPair *fp = static_cast< SourceFetcherPair *>( privatep );
	DynTableFetcher *fetcher = fp->second;
	GameAjaxDataSource *ds = fp->first;
	DynTable *table = fetcher->table;
	std::string tableName = table->GetName();
	String rocketTableName = tableName.c_str();
	DynTableList::iterator t_it = ds->tableList.find( tableName );
	bool hasOldTable = t_it != ds->tableList.end();
	DynTableFetcher *oldFetcher = hasOldTable ? t_it->second : NULL;
	DynTable *oldTable = hasOldTable ? oldFetcher->table : NULL;
	const char *data = fetcher->buf.c_str();

	// simply exit on error or if nothing has changed in table data
	if( status < 0 || status >= 300 || ( hasOldTable && ( oldFetcher->buf == data ) ) ) {
		__delete__( table );
		__delete__( fetcher );
		__delete__( fp );
		return;
	}

	// parse server response:
	// {
	// "key1" = "value1"
	// "key2" = "value2"
	// }
	char *token;
	std::string key, value;
	for(; ( token = COM_Parse( &data ) ) && token[0] == '{'; ) {
		Row row;

		while( 1 ) {
			token = COM_ParseExt( &data, true );
			if( !token[0] ) {
				break; // error
			}
			if( token[0] == '}' ) {
				break; // end of callvote

			}
			key = Q_trim( token );
			value = COM_ParseExt( &data, true );
			row[key] = value;
		}

		table->AddRow( row );
	}

	if( oldTable != NULL ) {
		ds->tableList[tableName] = fetcher;

		ds->NotifyRowChange( rocketTableName );

		__delete__( oldTable );
		__delete__( oldFetcher );
	} else {
		ds->tableList[tableName] = fetcher;
		ds->NotifyRowAdd( rocketTableName, 0, table->GetNumRows() );
	}

	__delete__( fp );
}

}
