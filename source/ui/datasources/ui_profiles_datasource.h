#pragma once
#ifndef __UI_PROFILES_DATASOURCE_H__
#define __UI_PROFILES_DATASOURCE_H__

#include <Rocket/Controls/DataSource.h>

namespace WSWUI
{

class ProfilesDataSource :
	public Rocket::Controls::DataSource
{
public:
	ProfilesDataSource( void );
	~ProfilesDataSource( void );

	// methods which must be overridden
	void GetRow( StringList& row, const String& table, int row_index, const StringList& columns );
	int GetNumRows( const String& table );

private:
	std::vector<std::string> profilesList;
	void UpdateProfiles( void );
};

}

#endif