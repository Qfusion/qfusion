#pragma once
#ifndef __UI_HUDS_DATASOURCE_H__
#define __UI_HUDS_DATASOURCE_H__

#include <Rocket/Controls/DataSource.h>

namespace WSWUI
{

class HudsDataSource :
	public Rocket::Controls::DataSource
{
public:
	HudsDataSource( void );
	~HudsDataSource( void );

	// methods which must be overridden
	void GetRow( StringList& row, const String& table, int row_index, const StringList& columns );
	int GetNumRows( const String& table );

private:
	typedef std::vector<std::string> HudList;
	HudList hudsList;

	// populates the table
	void UpdateHudsList( void );
};

}
#endif