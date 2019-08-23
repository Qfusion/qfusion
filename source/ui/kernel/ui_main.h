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
#include "kernel/ui_navigationstack.h"
#include "kernel/ui_streamcache.h"
#include "kernel/ui_demoinfo.h"
#include "kernel/ui_downloadinfo.h"
#include "as/asmodule.h"

namespace WSWUI
{

class RefreshState
{
public:
	int64_t time;
	int clientState;
	int serverState;
	bool drawBackground;
	int width, height;
	float pixelRatio;
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
class GameAjaxDataSource;

class LevelShotFormatter;
class DatetimeFormatter;
class DurationFormatter;
class FiletypeFormatter;
class ColorCodeFormatter;
class EmptyFormatter;
class ServerFlagsFormatter;

class UI_Main
{
public:
	typedef std::list<NavigationStack *> UI_Navigation;

	virtual ~UI_Main();

	void refreshScreen( unsigned int time, int clientState, int serverState,
						bool demoPlaying, const char *demoName, bool demoPaused, unsigned int demoTime,
						bool backGround, bool showCursor );
	void drawConnectScreen( const char *serverName, const char *rejectmessage,
							int downloadType, const char *downloadfilename, float downloadPercent, int downloadSpeed,
							int connectCount, bool backGround );

	void forceMenuOff( void );
	void addToServerList( const char *adr, const char *info );

	void mouseMove( int contextId, int frameTime, int x, int y, bool absolute, bool showCursor );
	bool mouseHover( int contextId );
	void textInput( int contextId, wchar_t c );
	void keyEvent( int contextId, int key, bool pressed );
	bool touchEvent( int contextId, int id, touchevent_t type, int x, int y );
	bool isTouchDown( int contextId, int id );
	void cancelTouches( int contextId );
	void getMouseMoveDelta( int *dx, int *dy );

	// Commands (these could be private)
	static void ReloadUI_Cmd_f( void );
	static void DumpAPI_f( void );
	static void M_Menu_Open_Cmd_f_( bool modal );
	static void M_Menu_Force_f( void );
	static void M_Menu_Open_f( void );
	static void M_Menu_Modal_f( void );
	static void M_Menu_Close_f( void );

	// pops all documents from stack and inserts a new one _if_ the overlayMenuURL is different
	static void M_Menu_Quick_f( void );

	// DEBUG
	static void PrintDocuments_Cmd( void );

	// Other static functions
	static UI_Main *Instance( int vidWidth, int vidHeight, float pixelRatio,
							  int protocol, const char *demoExtension, const char *basePath );
	static UI_Main *Get( void );
	static void Destroy( void );
	static bool preloadEnabled( void );

	// Public methods
	void forceUI( bool force );
	void showUI( bool show );
	void showOverlayMenu( bool show, bool showCursor );
	bool haveOverlayMenu( void );

	ASUI::ASInterface *getAS( void ) { return asmodule; };
	RocketModule *getRocket( void ) { return rocketModule; }
	//NavigationStack *getNavigator( void ) { return navigator; }
	ServerBrowserDataSource *getServerBrowser( void ) { return serverBrowser; }
	DemoInfo *getDemoInfo( void ) { return &demoInfo; }

	StreamCache *getStreamCache( void ) { return streamCache; }

	const RefreshState &getRefreshState( void ) { return refreshState; }

	const std::string &getServerName( void ) const { return connectInfo.serverName; }
	const std::string &getRejectMessage( void ) const { return connectInfo.rejectMessage; }
	const DownloadInfo *getDownloadInfo( void ) const { return &connectInfo.downloadInfo; }
	unsigned int getConnectCount( void ) const { return connectInfo.connectCount; }

	static int getGameProtocol( void );

	bool debugOn( void );

	void clearShaderCache( void );
	void touchAllCachedShaders( void );
	void flushAjaxCache( void );

	NavigationStack *createStack( int contextId );

private:
	UI_Main( int vidWidth, int vidHeight, float pixelRatio,
			 int protocol, const char *demoExtension, const char *basePath );

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


	/**
	 * Adds cursor movement from the gamepad sticks.
	 *
	 * @param milliseconds since last UI input update
	 */
	void gamepadStickCursorMove( int frameTimeMsec );

	/**
	 * Adds cursor movement from the directional pad.
	 *
	 * @param milliseconds since last UI input update
	 */
	void gamepadDpadCursorMove( int frameTimeMsec );

	/**
	 * Adds cursor movement from the gamepad.
	 */
	void gamepadCursorMove( void );


	void customRender( void );

	static UI_Main *self;   // for static functions

	// modules
	ASUI::ASInterface *asmodule;

	RocketModule *rocketModule;

	LevelShotFormatter *levelshot_fmt;
	DatetimeFormatter *datetime_fmt;
	DurationFormatter *duration_fmt;
	FiletypeFormatter *filetype_fmt;
	ColorCodeFormatter *colorcode_fmt;
	EmptyFormatter *empty_fmt;
	ServerFlagsFormatter *serverflags_fmt;

	ServerBrowserDataSource *serverBrowser;
	GameTypesDataSource *gameTypes;
	MapsDataSource *maps;
	ProfilesDataSource *vidProfiles;
	HudsDataSource *huds;
	VideoDataSource *videoModes;
	DemosDataSource *demos;
	ModsDataSource *mods;
	ModelsDataSource *playerModels;
	GameAjaxDataSource *gameajax;

	UI_Navigation navigations[UI_NUM_CONTEXTS];
	Rocket::Core::String overlayMenuURL;

	StreamCache *streamCache;

	RefreshState refreshState;

	int mousex, mousey;
	int mousedx, mousedy; // relative mouse movement for this frame

	int gameProtocol;
	bool menuVisible;
	bool overlayMenuVisible;
	bool forceMenu;
	bool showNavigationStack;

	DemoInfo demoInfo;

	struct {
		std::string serverName;
		std::string rejectMessage;
		unsigned int connectCount;
		DownloadInfo downloadInfo;
	} connectInfo;

	std::string demoExtension;

	bool invalidateAjaxCache;

	vec4_t colorWhite;

	static const std::string ui_index;
	static const std::string ui_connectscreen;

	cvar_t *ui_basepath;
	cvar_t *ui_cursor;
	cvar_t *ui_developer;
	cvar_t *ui_preload;
};

}

#endif /* UI_MAIN_H_ */
