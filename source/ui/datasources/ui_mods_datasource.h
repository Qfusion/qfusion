#pragma once
#ifndef __UI_MODS_DATASOURCE_H__
#define __UI_MODS_DATASOURCE_H__

#include <Rocket/Controls/DataSource.h>

namespace WSWUI
{
class ModsDataSource :
	public Rocket::Controls::DataSource
{
public:
	ModsDataSource(void);
	~ModsDataSource(void);

	// methods which must be overridden
	void GetRow( StringList& row, const String& table, int row_index, const StringList& columns );
	int GetNumRows( const String& table );

	void UpdatePath( void );
private:
	typedef std::vector<std::string> ModsList;
	ModsList modsList;
};
}
#endif