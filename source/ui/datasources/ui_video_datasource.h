#include <RmlUi/Controls/DataSource.h>

namespace WSWUI
{

class VideoDataSource :
	public Rml::Controls::DataSource
{
public:
	VideoDataSource( void );
	~VideoDataSource( void );

	// methods which must be overridden
	void GetRow( Rml::Core::StringList& row, const std::string& table, int row_index, const Rml::Core::StringList& columns );
	int GetNumRows( const std::string& table );

private:
	std::vector<std::string> modesList;

	// populate the table
	void updateVideoModeList( void );
};

}
