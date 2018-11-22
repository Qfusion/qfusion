#ifndef __UI_MAPS_DATASOURCE_H__
#define __UI_MAPS_DATASOURCE_H__

#include <Rocket/Controls/DataSource.h>

namespace WSWUI
{
/// Provides a list of available maps
class MapsDataSource : public Rocket::Controls::DataSource
{
public:
	MapsDataSource();

	virtual void GetRow( Rocket::Core::StringList &row, const Rocket::Core::String&, int row_index, const Rocket::Core::StringList& cols );
	virtual int GetNumRows( const Rocket::Core::String &table );

private:
	typedef std::vector<std::string> MapList;

	MapList mapList;

	template<typename C>
	void getMapsList( C& maps_list );
};
}

#endif // __UI_MAPS_DATASOURCE_H__
