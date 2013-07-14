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
#include "datasources/ui_callvotes_datasource.h"

#define CALLVOTES_SOURCE	"callvotes"
#define MAINTABLE_NAME		"list"

using namespace Rocket::Core;
using namespace Rocket::Controls;

namespace WSWUI
{

CallvotesDataSource::CallvotesDataSource() : DataSource( CALLVOTES_SOURCE )
{
	fetchList = true;
}

CallvotesDataSource::~CallvotesDataSource( void )
{
	callvotes.clear();
}

void CallvotesDataSource::GetRow( StringList &row, const String &table, int row_index, const StringList& cols )
{
	if( table != MAINTABLE_NAME ) {
		return;
	}

	CallvotesList::const_iterator cv_it = callvotes.begin();
	std::advance( cv_it, row_index );
	if( cv_it == callvotes.end() ) {;
		return;
	}

	const Callvote &callvote = *cv_it;
	for( StringList::const_iterator it = cols.begin(); it != cols.end(); ++it )
	{
		if( *it == "name" ) row.push_back( callvote.name.c_str() );
		else if( *it == "name" ) row.push_back( callvote.help.c_str() );
		else if( *it == "argument_format" ) row.push_back( callvote.argformat.c_str() );
		else if( *it == "expectedargs" ) row.push_back( va( "%i", callvote.expectedargs ) );
		else row.push_back("");
	}
}

size_t CallvotesDataSource::StreamRead( const void *buf, size_t numb, float percentage, int status,
	const char *contentType, void *privatep )
{
	if( status < 0 || status >= 300 ) {
		return 0;
	}
	// appened new data to the global buffer
	CallvotesDataSource *ds = static_cast< CallvotesDataSource *>( privatep );
	ds->fetchBuf += static_cast< const char * >( buf );
	return numb;
}

void CallvotesDataSource::StreamDone( int status, const char *contentType, void *privatep )
{
	if( status < 0 || status >= 300 ) {
		return;
	}

	CallvotesDataSource *ds = static_cast< CallvotesDataSource *>( privatep );
	const char *data = ds->fetchBuf.c_str();

	// parse server response:
	// {
	// "key1" = "value1"
	// "key2" = "value2"
	// }
	char *token;
	std::string key, value;
	for(; ( token = COM_Parse( &data ) ) && token[0] == '{'; )
	{
		Callvote cv;

		while( 1 )
		{
			token = COM_ParseExt( &data, qtrue );
			if( !token[0] )
				break; // error
			if( token[0] == '}' )
				break; // end of callvote

			key = Q_trim( token );

			token = COM_ParseExt( &data, qtrue );
			value = token;

			if( key == "name" ) { cv.name = value; }
			else if( key == "help" ) { cv.help = value; }
			else if( key == "expectedargs" ) { cv.expectedargs = atoi( value.c_str() ); }
			else if( key == "argument_format" ) { cv.argformat = value; }
		}

		if( !cv.name.empty() ) {
			ds->callvotes.push_back( cv );
		}
	}

	// sort the list alphabetically
	sort( ds->callvotes.begin(), ds->callvotes.end(), &CallvotesNameCompare );

	ds->NotifyRowAdd( MAINTABLE_NAME, 0, ds->callvotes.size() );
}

int CallvotesDataSource::GetNumRows( const String &table )
{
	if( table != MAINTABLE_NAME ) {
		return 0;
	}

	if( fetchList ) {
		fetchList = false;

		// fetch list now and notify listeners when we get the reply in async manner
		char buf[1024];
		trap::GetBaseServerURL( buf, sizeof( buf ) );
		std::string url = std::string( buf ) + "/game/callvotes/";

		trap::AsyncStream_PerformRequest(
			url.c_str(), "GET", "", 10,
			&CallvotesDataSource::StreamRead, &CallvotesDataSource::StreamDone, static_cast<void *>(this)
		);

		return 0;
	}

	return callvotes.size();
}


}
