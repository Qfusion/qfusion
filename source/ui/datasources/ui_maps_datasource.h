#ifndef __UI_MAPS_DATASOURCE_H__
#define __UI_MAPS_DATASOURCE_H__

#include <RmlUi/Controls/DataSource.h>

namespace WSWUI
{
/// Provides a list of available maps, with their full names, short
/// names and pictures
class MapsDataSource : public Rml::Controls::DataSource
{
public:
	MapsDataSource();

	virtual void GetRow( Rml::Core::StringList &row, const Rml::Core::String&, int row_index, const Rml::Core::StringList& cols );
	virtual int GetNumRows( const Rml::Core::String &table );

private:
	typedef std::pair<std::string, std::string> MapInfo;
	typedef std::vector<MapInfo> MapList;

	MapList mapList;

	template<typename C>
	void getMapsList( C& maps_list );
};
}

#endif // __UI_MAPS_DATASOURCE_H__
