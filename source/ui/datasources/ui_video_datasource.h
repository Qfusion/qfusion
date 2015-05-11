#include <Rocket/Controls/DataSource.h>

namespace WSWUI
{

class VideoDataSource :
	public Rocket::Controls::DataSource
{
public:
	VideoDataSource(void);
	~VideoDataSource(void);

	// methods which must be overridden
	void GetRow( StringList& row, const String& table, int row_index, const StringList& columns );
	int GetNumRows( const String& table );
private:

	std::vector<std::string> modesList;

	// populate the table
	void updateVideoModeList( void );
};

}
