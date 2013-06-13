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
	// if it's true, we take only wide screen resolutions
	bool wideScreen;

	typedef std::pair<std::string, std::string> Mode;
	std::vector<Mode> modesList;

	// populate the table
	void updateVideoModeList( void );
};

}
