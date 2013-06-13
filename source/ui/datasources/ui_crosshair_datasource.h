#pragma once
#ifndef __UI_CROSSHAIR_DATASOURCE_H__
#define __UI_CROSSHAIR_DATASOURCE_H__

#include <Rocket/Controls/DataSource.h>

namespace WSWUI
{

class CrosshairDataSource :
	public Rocket::Controls::DataSource
{
public:
	CrosshairDataSource( void );
	~CrosshairDataSource( void );

	// methods which must be overridden
	void GetRow( StringList& row, const String& table, int row_index, const StringList& columns );
	int GetNumRows( const String& table );

private:
	typedef std::pair<std::string, std::string> CrossHair;
	std::vector<CrossHair> crosshairList;

	void UpdateCrosshairList( void );
};

}
#endif