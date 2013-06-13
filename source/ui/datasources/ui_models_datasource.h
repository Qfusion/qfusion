#pragma once
#ifndef __UI_MODELS_DATASOURCE_H__
#define __UI_MODELS_DATASOURCE_H__

#include <Rocket/Controls/DataSource.h>

namespace WSWUI
{
class ModelsDataSource :
	public Rocket::Controls::DataSource
{
public:
	ModelsDataSource(void);
	~ModelsDataSource(void);

	// methods which must be overridden
	void GetRow( StringList& row, const String& table, int row_index, const StringList& columns );
	int GetNumRows( const String& table );

private:
	typedef std::vector<std::string> ModelsList;
	ModelsList modelsList;

	void UpdateModelsList( void );
};
}
#endif