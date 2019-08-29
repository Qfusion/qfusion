#pragma once
#ifndef __UI_MODS_DATASOURCE_H__
#define __UI_MODS_DATASOURCE_H__

#include <RmlUi/Controls/DataSource.h>

namespace WSWUI
{
class ModsDataSource :
	public Rml::Controls::DataSource
{
public:
	ModsDataSource( void );
	~ModsDataSource( void );

	// methods which must be overridden
	void GetRow( Rml::Core::StringList& row, const std::string& table, int row_index, const Rml::Core::StringList& columns );
	int GetNumRows( const std::string& table );

	void UpdatePath( void );

private:
	typedef std::vector<std::string> ModsList;
	ModsList modsList;
};
}
#endif
