#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_utils.h"
#include "datasources/ui_gametypes_datasource.h"

namespace WSWUI
{
GameTypesDataSource::GameTypesDataSource() : Rocket::Controls::DataSource( "gametypes_source" ) {
	std::vector<std::string> listedGameTypes;
	getFileList( listedGameTypes, "progs/gametypes", ".gt" );

	for( std::vector<std::string>::const_iterator it = listedGameTypes.begin();
		 it != listedGameTypes.end(); ++it ) {
		if( std::find_if( gameTypes.begin(), gameTypes.end(), [it](const gametype& elem) { return elem.name == *it; } ) == gameTypes.end() )
			{
			gametype gt( *it );

			if( gt.name == "tutorial" ) {
				// HACK
				continue;
			}

			std::string filepath = std::string( "progs/gametypes" ) + "/" + gt.name + ".gtd";
			int filenum, filelen;

			filelen = trap::FS_FOpenFile( filepath.c_str(), &filenum, FS_READ );
			if( filenum ) {
				if( filelen > 0 ) {
					char *buffer = new char[filelen + 1], *end = buffer + filelen;
					trap::FS_Read( buffer, filelen, filenum );
					buffer[filelen] = '\0';

					// parse title and description
					char *ptr = buffer;

					// parse single line of title
					while( ptr ) {
						const char *token = COM_ParseExt( &ptr, true );
						if( *token ) {
							gt.title.clear();
						}
						while( *token ) {
							gt.title += ( gt.title.empty() ? "" : " " );
							gt.title += token;
							token = COM_ParseExt( &ptr, false );
						}
						break;
					}

					// the rest is description
					while( ptr && ptr < end && ( *ptr == '\n' || *ptr == '\r' || *ptr == ' ' ) ) ptr++;
					if( ptr && ptr != end ) {
						gt.description = ptr;
					}

					delete[] buffer;
				}

				trap::FS_FCloseFile( filenum );
			}

			gameTypes.push_back( gt );
		}
	}
}

void GameTypesDataSource::GetRow( Rocket::Core::StringList &row, const Rocket::Core::String&, int row_index, const Rocket::Core::StringList& cols ) {
	if( row_index < 0 || (size_t)row_index >= gameTypes.size() ) {
		return;
	}

	for( Rocket::Core::StringList::const_iterator it = cols.begin();
		 it != cols.end();
		 ++it ) {
		if( *it == "name" ) {
			row.push_back( gameTypes[row_index].name.c_str() );
		} else if( *it == "title" ) {
			row.push_back( gameTypes[row_index].title.c_str() );
		} else if( *it == "description" ) {
			row.push_back( gameTypes[row_index].description.c_str() );
		} else {
			row.push_back( "" );
		}
	}
}

int GameTypesDataSource::GetNumRows( const Rocket::Core::String & ) {
	return gameTypes.size();
}
}
