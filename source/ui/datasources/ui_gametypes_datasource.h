#ifndef __UI_GAMETYPES_DATASOURCE_H__
#define __UI_GAMETYPES_DATASOURCE_H__

#include <RmlUi/Controls/DataSource.h>

namespace WSWUI
{
/// Provides information about game types (full name and id).
/// Used in the "start local game" screen to populate the game type
/// dropdown select box with data.
class GameTypesDataSource : public Rml::Controls::DataSource
{
public:
	/// Reads the available game types from /progs/gametypes
	GameTypesDataSource();

	/// Retrieve a "row".
	/// If row_index is negative or too big, the function will silently
	/// return.
	/// For each element in @param cols, the following strings will be
	/// added to @param row:
	///  - the id of the gametype corresponding to row_index if the
	///    value of the element is "val"
	///  - the name of the gametype corresponding to row_index if the
	///    value of the element is "name"
	///  - an empty string in all the other cases
	virtual void GetRow( Rml::Core::StringList &row, const Rml::Core::String&, int row_index, const Rml::Core::StringList& cols );

	/// Returns the number of the available game types
	virtual int GetNumRows( const Rml::Core::String &table );

private:
	struct gametype {
		std::string name;
		std::string title;
		std::string description;

		gametype() : name( "" ), title( "" ), description( "" ) {};
		gametype( const std::string &name ) : name( name ), title( name ), description( "" ) {};
	};

	typedef std::vector<gametype> GameTypeList;
	GameTypeList gameTypes;     /// Contains all accessible gametypes
};
}

#endif //__UI_GAMETYPES_DATASOURCE_H__
