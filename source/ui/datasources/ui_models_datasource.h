#pragma once
#ifndef __UI_MODELS_DATASOURCE_H__
#define __UI_MODELS_DATASOURCE_H__

#include <RmlUi/Controls/DataSource.h>

namespace WSWUI
{
class ModelsDataSource :
	public Rml::Controls::DataSource
{
public:
	ModelsDataSource( void );
	~ModelsDataSource( void );

	// methods which must be overridden
	void GetRow( Rml::Core::StringList& row, const std::string& table, int row_index, const Rml::Core::StringList& columns );
	int GetNumRows( const std::string& table );

private:
	typedef std::vector<std::string> ModelsList;
	ModelsList modelsList;

	void UpdateModelsList( void );
};
}
#endif
