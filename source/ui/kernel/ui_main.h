/*
 * UI_Main.h
 *
 *  Created on: 25.6.2011
 *      Author: hc
 */

#ifndef UI_MAIN_H_
#define UI_MAIN_H_

#include "kernel/ui_rocketmodule.h"
#include "kernel/ui_documentloader.h"
#include "kernel/ui_streamcache.h"
#include "kernel/ui_demoinfo.h"
#include "kernel/ui_downloadinfo.h"
#include "as/asmodule.h"

namespace WSWUI
{

class RefreshState
{
public:
	unsigned int time;
	int clientState;
	int serverState;
	bool drawBackground;
	int backgroundNum;
	int width, height;
};

class ServerBrowserDataSource;
class GameTypesDataSource;
class MapsDataSource;
class ProfilesDataSource;
class HudsDataSource;
class VideoDataSource;
class DemosDataSource;
class ModsDataSource;
class ModelsDataSource;
class CrosshairDataSource;
class TVChannelsDataSource;
class IrcChannelsDataSource;
class GameAjaxDataSource;

class LevelShotFormatter;
class DatetimeFormatter;
class DurationFormatter;
class CrosshairFormatter;
class FiletypeFormatter;
class ColorCodeFormatter;
class EmptyFormatter;

class UI_Main
{
public:
	virtual ~UI_Main();

	void refreshScreen( unsigned int time, int clientState, int serverState, 
		bool demoPlaying, const char *demoName, bool demoPaused, unsigned int demoTime, 
		bool backGround, bool showCursor );
	void drawConnectScreen( const char *serverName, const char *rejectmessage, 
		int downloadType, const char *downloadfilename, float downloadPercent, int downloadSpeed, 
		int connectCount, bool backGround );

	void forceMenuOff( void );
	void addToServerList( const char *adr, const char *info );

	void mouseMove( int dx, int dy );
	void textInput( qwchar c );
	void keyEvent( int key, bool pressed );

	// Commands (these could be private)
	static void ReloadUI_Cmd_f( void );
	static void DumpAPI_f( void );
	static void M_Menu_Force_f( void );
	static void M_Menu_Open_f( void );
	static void M_Menu_Tv_f( void );
	static void M_Menu_DemoPlay_f( void );
	static void M_Menu_Close_f( void );	
	static void M_Menu_AddTVChannel_f( void );
	static void M_Menu_RemoveTVChannel_f( void );

	// DEBUG
	static void PrintDocuments_Cmd( void );
	
	// Other static functions
	static UI_Main *Instance( int vidWidth, int vidHeight, int protocol, const char *demoExtension );
	static UI_Main *Get( void );
	static void Destroy( void );

	// Public methods
	void showUI( bool show );
	void forceUI( bool force );

	ASUI::ASInterface *getAS( void ) { return asmodule; };
	RocketModule *getRocket( void ) { return rocketModule; }
	NavigationStack *getNavigator( void ) { return navigator; }
	ServerBrowserDataSource *getServerBrowser( void ) { return serverBrowser; }
	DemoInfo *getDemoInfo( void ) { return &demoInfo; }

	// TODO: eliminate this, either way DONT USE THIS!
	Rocket::Core::Context *getRocketContext( void );

	StreamCache *getStreamCache( void ) { return streamCache; }

	// backwards development compatibility
	DocumentLoader *getDocumentLoader() { return currentLoader; }
	void setDocumentLoader( DocumentLoader *loader ) { currentLoader = loader; }

	const RefreshState &getRefreshState( void ) { return refreshState; }

	std::string getServerName( void ) const { return serverName; }
	std::string getRejectMessage( void ) const { return rejectMessage; }
	const DownloadInfo *getDownloadInfo ( void ) const { return &downloadInfo; }
	int getGameProtocol( void ) const;

	bool debugOn( void );

	void clearShaderCache( void );
	void touchAllCachedShaders( void );
	void flushAjaxCache( void );

	unsigned int getConnectCount( void ) const { return connectCount; }

private:
	UI_Main( int vidWidth, int vidHeight, int protocol, const char *demoExtension );

	//// METHODS
	bool initAS( void );
	void shutdownAS( void );

	void preloadUI( void );
	void reloadUI( void );

	bool initRocket( void );
	void registerRocketCustoms( void );
	void unregisterRocketCustoms( void );
	void shutdownRocket( void );

	void createDataSources( void );
	void destroyDataSources( void );

	void createFormatters( void );
	void destroyFormatters( void );

	void loadCursor( void );

	void customRender( void );

	static UI_Main *self;	// for static functions

	// modules
	ASUI::ASInterface *asmodule;

	RocketModule *rocketModule;

	LevelShotFormatter *levelshot_fmt;
	DatetimeFormatter *datetime_fmt;
	DurationFormatter *duration_fmt;
	FiletypeFormatter *filetype_fmt;
	ColorCodeFormatter *colorcode_fmt;
	CrosshairFormatter *crosshair_fmt;
	EmptyFormatter *empty_fmt;

	ServerBrowserDataSource *serverBrowser;
	GameTypesDataSource *gameTypes;
	MapsDataSource *maps;
	ProfilesDataSource *vidProfiles;
	HudsDataSource *huds;
	VideoDataSource *videoModes;
	DemosDataSource *demos;
	ModsDataSource *mods;
	ModelsDataSource *playerModels;
	CrosshairDataSource *crosshairs;
	TVChannelsDataSource *tvchannels;
	IrcChannelsDataSource *ircchannels;
	GameAjaxDataSource *gameajax;

	NavigationStack *navigator;

	DocumentLoader *currentLoader;

	StreamCache *streamCache;

	RefreshState refreshState;

	int mousex, mousey;
	int gameProtocol;
	bool menuVisible;
	bool forceMenu;
	bool showNavigationStack;

	DemoInfo demoInfo;
	DownloadInfo downloadInfo;

	std::string serverName;
	std::string rejectMessage;
	std::string demoExtension;

	unsigned int connectCount;
	bool invalidateAjaxCache;

	vec4_t colorWhite;

	static const std::string ui_index;
	static const std::string ui_connectscreen;

	cvar_t *ui_basepath;
	cvar_t *ui_cursor;
	cvar_t *ui_developer;
};

}

#endif /* UI_MAIN_H_ */
