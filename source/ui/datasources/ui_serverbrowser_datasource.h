#ifndef __UI_SERVERBROWSER_H__
#define __UI_SERVERBROWSER_H__

#include <Rocket/Controls/DataSource.h>

#include <string>
#include <vector>
#include <list>
#include <queue>

#include "kernel/ui_utils.h"
/*
    ch : ok, sketch up some initial stuff on serverbrowser.

    in .rml we would define <datagrid source="serverbrowser_source" id="serverbrowser" />
    and for that to work, we need to define DataSource

    <datagrid source="..." id="..">
        <col fields="foo" [formatter="my_formatter" width="100">Foo</col>
        <col fields="bar" [formatter="my_formatter2" width="150">Bar</col>
    </datagrid>

    TODO: ServerBrowserOptions !
    Backend should classificate by (full | empty | insta | passwd | ranked | reg-only)
    and either (exclude | show | only)
    ServerBrowserOptions could be used to filter servers.
*/

namespace WSWUI
{

// Forward declare
class ServerBrowserDataSource;

// structure for parsed serverinfo's
class ServerInfo
{
	bool has_changed;
	bool ping_updated;
	bool has_ping;

public:
	// Attributes
	std::string address;
	uint64_t iaddress;

	std::string hostname;
	std::string cleanname;
	std::string locleanname;
	std::string map;
	int curuser;
	int maxuser;
	int bots;
	std::string gametype;
	std::string modname;
	bool instagib;
	bool race;
	int skilllevel;
	bool password;
	bool mm;
	unsigned int ping;
	unsigned int ping_retries;
	bool favorite;

	// TODO: batches.. just ignore the batch mechanism for now

	ServerInfo( const char *adr, const char *info = 0 );
	ServerInfo( const ServerInfo &other );

	ServerInfo &operator=( const ServerInfo &other );

	void fromInfo( const char *info );
	void fromOther( const ServerInfo &other );

	// TODO: move this up as utility func
	void fixString( std::string &s );
	void fixStrings();

	bool isChanged() const { return has_changed; }
	void setChanged( bool changed ) { has_changed = changed; }
	bool hasPing() const { return has_ping; }

	// comparison function type
	typedef bool (*CompareFunction)( const ServerInfo &, const ServerInfo & );
	typedef bool (*ComparePtrFunction)( const ServerInfo *, const ServerInfo * );

	// general templated comparison function (less)
	template<typename T, T ServerInfo::*comp_member>
	static bool LessBinary( const ServerInfo &lhs, const ServerInfo &rhs ) {
		return lhs.*comp_member < rhs.*comp_member;
	}

	// general templated comparison function (less) for pointers
	template<typename T, T ServerInfo::*comp_member>
	static bool LessPtrBinary( const ServerInfo *lhs, const ServerInfo *rhs ) {
		return lhs->*comp_member < rhs->*comp_member;
	}

	// general templated comparison function (greater)
	template<typename T, T ServerInfo::*comp_member>
	static bool GreaterBinary( const ServerInfo &lhs, const ServerInfo &rhs ) {
		return lhs.*comp_member > rhs.*comp_member;
	}

	// general templated comparison function (less) for pointers
	template<typename T, T ServerInfo::*comp_member>
	static bool GreaterPtrBinary( const ServerInfo *lhs, const ServerInfo *rhs ) {
		return lhs->*comp_member > rhs->*comp_member;
	}

	// General invertors for above functions
	struct InvertCompareFunction {
		CompareFunction function;
		InvertCompareFunction( CompareFunction _function ) : function( _function ) {}
		bool operator()( const ServerInfo &lhs, const ServerInfo &rhs ) {
			return !function( lhs, rhs );
		}
	};

	struct InvertComparePtrFunction {
		ComparePtrFunction function;
		InvertComparePtrFunction( ComparePtrFunction _function ) : function( _function ) {}
		bool operator()( const ServerInfo *lhs, const ServerInfo *rhs ) {
			return !function( lhs, rhs );
		}
	};

	// struct that can be used for both values and pointers
	template<typename T, T ServerInfo::*comp_member>
	struct _LessBinary {
		bool operator()( const ServerInfo &lhs, const ServerInfo &rhs ) const {
			return lhs.*comp_member < rhs.*comp_member;
		}
		bool operator()( const ServerInfo *lhs, const ServerInfo *rhs ) const {
			return lhs->*comp_member < rhs->*comp_member;
		}
	};

	// templated boolean 'matches'
	template<typename T, T ServerInfo::*comp_member>
	struct EqualUnary {
		T compare_to;
		EqualUnary( const T &_compare_to ) : compare_to( _compare_to ) {}
		bool operator()( const ServerInfo &lhs ) {
			return lhs.*comp_member == compare_to;
		}
		bool operator()( const ServerInfo *lhs ) {
			return lhs->*comp_member == compare_to;
		}
	};

	template<typename T, T ServerInfo::*comp_member>
	static bool EqualBinary( const ServerInfo &lhs, const ServerInfo &rhs ) {
		return lhs.*comp_member == rhs.*comp_member;
	}

	static bool DefaultCompareBinary( const ServerInfo *lhs, const ServerInfo *rhs );

private:
	void tokenizeInfo( const char *info, __stl_vector( __stl_string ) &tokens );
};

// filtering mechanism for serverbrowser
class ServerBrowserFilter
{
	// is this like, totally stupid?

public:
	enum VisibilityState {
		HIDE = 0,       // exclude from visibleServers
		SHOW=1,         // include in visibleServers
		ONLY=2          // only show these servers
	};

	ServerBrowserFilter() : 
		full( HIDE ), empty( HIDE ), instagib( HIDE ), 
		password( HIDE ), ranked( HIDE ), registered( HIDE ),
		gametype( "" ) {}

	// called by ServerBrowserDataSource to filter servers as per settings
	// TODO: proper implementation
	bool filterServer( const ServerInfo &info ) { return true; }

	// private:
	// options
	VisibilityState full;
	VisibilityState empty;
	VisibilityState instagib;
	VisibilityState password;
	VisibilityState ranked;
	VisibilityState registered;
	String gametype;
};

// Module that will queue server pings
class ServerInfoFetcher
{
	// amount if simultaneous queries
	static const unsigned int TIMEOUT_SEC = 5;      // secs until we replace with another job
	static const unsigned int QUERY_TIMEOUT_MSEC = 50;      // time between subsequent queries to individual servers

	// waiting line
	typedef std::queue<std::string> StringQueue;
	StringQueue serverQueue;
	// active queries
	typedef std::pair<int64_t, std::string> ActiveQuery;
	typedef std::list<ActiveQuery> ActiveList;

	ActiveList activeQueries;

public:
	ServerInfoFetcher()
		: lastQueryTime( 0 ), numIssuedQueries( 0 )
	{}
	~ServerInfoFetcher() {}

	// add a query to the waiting line
	void addQuery( const char *adr );
	// called to tell fetcher to remove this from jobs
	void queryDone( const char *adr );
	// stop the whole process
	void clearQueries();
	// advance queries
	void updateFrame();

	unsigned int numActive() const { return activeQueries.size(); }
	unsigned int numWaiting() const { return serverQueue.size(); }
	unsigned int numIssued() const { return numIssuedQueries; }

private:
	int64_t lastQueryTime;
	unsigned int numIssuedQueries;

	// compare address of active query
	struct CompareAddress {
		std::string address;
		CompareAddress( const char *_address ) : address( _address ) {}
		bool operator()( const ActiveQuery &other ) {
			return address == other.second;
		}
	};

	// initiates a query
	void startQuery( const std::string &adr );
};

//================================================

class ServerBrowserDataSource : public Rocket::Controls::DataSource
{
	// typedefs
	// use set for serverinfo list to keep unique elements
	typedef std::set<ServerInfo, ServerInfo::_LessBinary<uint64_t, &ServerInfo::iaddress> > ServerInfoList;
	typedef std::list<ServerInfo*> ReferenceList;
	typedef std::map<String, ReferenceList> ReferenceListMap;
	typedef std::set<uint64_t> FavoritesList;

	// shortcut for the set insert
	typedef std::pair<ServerInfoList::iterator, bool> ServerInfoListPair;

	static const unsigned int MAX_RETRIES = 3;
	static const unsigned int REFRESH_TIMEOUT_MSEC = 1000;      // time between subsequent table updates

	// constants
	/*
	static const char *TABLE_NAME = "servers";
	static const char *COLUMN_NAMES[] =
	{
	    "skill", "ping", "players", "hostname", "gametype", "map"
	};
	static const int NUM_COLUMNS = sizeof(COLUMN_NAMES) / sizeof(COLUMN_NAMES[0]);
	*/

	// members

	ServerInfoList serverList;
	ReferenceListMap referenceListMap;
	ReferenceList referenceQueue;

	ServerBrowserFilter filter;
	ServerInfoFetcher fetcher;

	FavoritesList favorites;

	// we use pointers on referenceList! how can we use struct here?
	ServerInfo::ComparePtrFunction sortCompare;
	ServerInfo::ComparePtrFunction lastSortCompare;
	int sortDirection;      // 1 ascending, -1 descending

	// need to separate full update and refresh?
	bool active;
	unsigned updateId;
	int64_t lastActiveTime;

	// DEBUG
	int numNotifies;

public:
	ServerBrowserDataSource();     // : Rocket::Core::DataSource("serverbrowser_source")
	virtual ~ServerBrowserDataSource();

	//
	// functions overriding DataSource ones to provide our functionality

	// this should returns the asked 'columns' on row with 'row_index' from 'table' into 'row'
	virtual void GetRow( StringList &row, const String &table, int row_index, const StringList &columns );

	// this should return the number of rows in 'table'
	virtual int GetNumRows( const String &table );

	//
	// functions available to call when our data changes (e.g. AddToServerList is called)
	// these inform listeners attached (like datagrid)

	// we have new row
	// void NotifyRowAdd(const String &table, int first_row_added, int num_rows_added);

	#if 0
	// benchmarking edition ltd
	void NotifyRowAdd( const Rocket::Core::String &table, int first_row_added, int num_rows_added ) {
		BenchmarkTimer bt;
		Rocket::Controls::DataSource::NotifyRowAdd( table, first_row_added, num_rows_added );
		//Com_Printf("NotifyRowAdd %u\n", bt() );

		numNotifies++;
	}

	// we removed a row
	// void NotifyRowRemove(const String &table, int first_row_removed, int num_rows_removed);
	void NotifyRowRemove( const String &table, int first_row_removed, int num_rows_removed ) {
		BenchmarkTimer bt;
		Rocket::Controls::DataSource::NotifyRowRemove( table, first_row_removed, num_rows_removed );
		//Com_Printf("NotifyRowRemove %u\n", bt() );
		numNotifies++;
	}

	// change of data in said rows
	// void NotifyRowChange(const String &table, int first_row_changed, int num_rows_changed);
	void NotifyRowChange( const String &table, int first_row_changed, int num_rows_changed ) {
		BenchmarkTimer bt;
		Rocket::Controls::DataSource::NotifyRowChange( table, first_row_changed, num_rows_changed );
		//Com_Printf("NotifyRowChange %u\n", bt() );
		numNotifies++;
	}

	// notify full refresh of table
	// void NotifyRowChange(const String &table);
	void NotifyRowChange( const String &table ) {
		BenchmarkTimer bt;
		Rocket::Controls::DataSource::NotifyRowChange( table );
		//Com_Printf("NotifyRowChange(full) %u\n", bt() );
		numNotifies++;
	}
	#endif

	//
	// wsw functions

	// called each frame to progress queries
	void updateFrame();

	// initiates master server query -> export to AS
	void startFullUpdate( void );

	// callback from client after initiating serverquery
	void addToServerList( const char *adr, const char *info );

	// refreshes (pings) current list -> export to AS
	void startRefresh( void );

	// and stop current update/refresh -> export to AS
	void stopUpdate( void );

	// called to re-sort the data (on visibleServers) -> export to AS
	void sortByField( const char *field );

	// called to reform visibleServers and hiddenServers -> export to AS?
	void filtersUpdated( void );

	// adds a serveraddress to our favorites -> export to AS
	bool addFavorite( const char *fav );

	// we don't like that server anymore -> export to AS
	bool removeFavorite( const char *fav );

	//
	void compileSuggestionsList( void );

	bool isUpdating( void ) { return active; }
	unsigned getUpdateId( void ) { return updateId; }
	int64_t getLastActiveTime( void ) { return lastActiveTime; }

	// DEBUG
	int getActivity( void ) { return numNotifies; }

private:
	int64_t lastUpdateTime;

	void tableNameForServerInfo( const ServerInfo &, String &table ) const;
	void addServerToTable( ServerInfo &info, const String &tableName );
	void removeServerFromTable( ServerInfo &info, const String &tableName );
	void notifyOfFavoriteChange( uint64_t iaddr, bool add );
};

}
#endif // __UI_SERVERBROWSER_H__
