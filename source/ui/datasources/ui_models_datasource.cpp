#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_utils.h"
#include "datasources/ui_models_datasource.h"

#define MODELS_SOURCE   "models"
#define TABLE_NAME      "list"
#define FIELDS          "name"

namespace WSWUI
{

ModelsDataSource::ModelsDataSource( void ) :
	Rml::Controls::DataSource( MODELS_SOURCE ) {
	UpdateModelsList();
}

ModelsDataSource::~ModelsDataSource( void ) {
}

void ModelsDataSource::UpdateModelsList( void ) {
	// clear the list
	modelsList.clear();

	// scan for directories, storing results in temporary list
	ModelsList tempList;
	getFileList( tempList, "models/players", "/" );

	// now scan for proper models:
	// we need the main model file, animation config and default skin
	for( ModelsList::const_iterator it = tempList.begin(); it != tempList.end(); ++it ) {
		size_t i;
		static const std::string mustHaveFiles[] = { "tris.iqm", "animation.cfg", "default.skin" };
		static const size_t numMustHaveFiles = sizeof( mustHaveFiles ) / sizeof( mustHaveFiles[0] );

		std::string basePath = std::string( "models/players/" ) + *it + "/";
		for( i = 0; i < numMustHaveFiles; i++ ) {
			std::string filePath = basePath + mustHaveFiles[i];
			if( trap::FS_FOpenFile( filePath.c_str(), NULL, FS_READ ) < 0 ) {
				break;
			}
		}

		// we didn't find all files we need, ignore
		if( i != numMustHaveFiles ) {
			continue;
		}

		modelsList.push_back( *it );
	}

	NotifyRowAdd( TABLE_NAME, 0, modelsList.size() );
}

void ModelsDataSource::GetRow( Rml::Core::StringList &row, const std::string &table, int row_index, const Rml::Core::StringList &columns ) {
	if( row_index < 0 || (size_t)row_index >= modelsList.size() ) {
		return;
	}

	if( table != TABLE_NAME ) {
		return;
	}

	// there should be only 1 column, but we watch ahead in the future
	for( Rml::Core::StringList::const_iterator it = columns.begin(); it != columns.end(); ++it ) {
		if( *it == FIELDS ) {
			row.push_back( modelsList[row_index].c_str() );
		}
	}
}

int ModelsDataSource::GetNumRows( const std::string &table ) {
	return modelsList.size();
}
}
