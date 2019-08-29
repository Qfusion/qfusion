#pragma once
#ifndef __UI_PROFILES_DATASOURCE_H__
#define __UI_PROFILES_DATASOURCE_H__

#include <RmlUi/Controls/DataSource.h>

namespace WSWUI
{

class ProfilesDataSource :
	public Rml::Controls::DataSource
{
public:
	ProfilesDataSource( void );
	~ProfilesDataSource( void );

	// methods which must be overridden
	void GetRow( Rml::Core::StringList& row, const std::string& table, int row_index, const Rml::Core::StringList& columns );
	int GetNumRows( const std::string& table );

private:
	std::vector<std::string> profilesList;
	void UpdateProfiles( void );
};

}

#endif
