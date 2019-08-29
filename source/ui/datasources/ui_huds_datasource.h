#pragma once
#ifndef __UI_HUDS_DATASOURCE_H__
#define __UI_HUDS_DATASOURCE_H__

#include <RmlUi/Controls/DataSource.h>

namespace WSWUI
{

class HudsDataSource :
	public Rml::Controls::DataSource
{
public:
	HudsDataSource( void );
	~HudsDataSource( void );

	// methods which must be overridden
	void GetRow( Rml::Core::StringList& row, const std::string& table, int row_index, const Rml::Core::StringList& columns );
	int GetNumRows( const std::string& table );

private:
	typedef std::vector<std::string> HudList;
	HudList hudsList;

	// populates the table
	void UpdateHudsList( void );
};

}
#endif
