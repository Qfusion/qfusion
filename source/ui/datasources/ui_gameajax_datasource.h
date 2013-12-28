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
#pragma once
#ifndef __UI_CALLVOTES_DATASOURCE_H__
#define __UI_CALLVOTES_DATASOURCE_H__

#include <Rocket/Controls/DataSource.h>

namespace WSWUI
{

class DynTable;

class GameAjaxDataSource : public Rocket::Controls::DataSource
{
public:
	GameAjaxDataSource( void );
	~GameAjaxDataSource( void );

	void GetRow( StringList& row, const String& table, int row_index, const StringList& columns );
	int GetNumRows( const String& table );

	// forces HTTP request on the next update
	void FlushCache( void );

private:
	static const int UPDATE_INTERVAL = 10000; // in milliseconds

	class DynTableFetcher
	{
	public:
		DynTableFetcher( DynTable *table ) : table( table ), buf( "" ) {}
		DynTable *table;
		std::string buf;
	};

	typedef std::pair<GameAjaxDataSource *, DynTableFetcher *> SourceFetcherPair;
	typedef std::map<std::string, DynTableFetcher *> DynTableList;
	DynTableList tableList;

	static size_t StreamRead( const void *buf, size_t numb, float percentage, int status,
			const char *contentType, void *privatep );
	static void StreamDone( int status, const char *contentType, void *privatep );
};

Rocket::Controls::DataSource *GetCallvotesDataSourceInstance();
void DestroyCallvotesDataSourceInstance(Rocket::Controls::DataSource *instance);

}
#endif
