/*
 * UI_Main.cpp
 *
 *  Created on: 25.6.2011
 *      Author: hc
 */

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "widgets/ui_widgets.h"

#include "as/asui.h"
#include "as/asmodule.h"

#include "../gameshared/q_keycodes.h"
#include "../gameshared/q_comref.h"

#include "datasources/ui_demos_datasource.h"
#include "datasources/ui_huds_datasource.h"
#include "datasources/ui_video_datasource.h"
#include "datasources/ui_gametypes_datasource.h"
#include "datasources/ui_maps_datasource.h"
#include "datasources/ui_mods_datasource.h"
#include "datasources/ui_crosshair_datasource.h"
#include "datasources/ui_models_datasource.h"
#include "datasources/ui_profiles_datasource.h"
#include "datasources/ui_serverbrowser_datasource.h"
#include "datasources/ui_tvchannels_datasource.h"
#include "datasources/ui_ircchannels_datasource.h"
#include "datasources/ui_gameajax_datasource.h"

#include "formatters/ui_crosshair_formatter.h"
#include "formatters/ui_levelshot_formatter.h"
#include "formatters/ui_datetime_formatter.h"
#include "formatters/ui_duration_formatter.h"
#include "formatters/ui_filetype_formatter.h"
#include "formatters/ui_colorcode_formatter.h"
#include "formatters/ui_empty_formatter.h"

namespace WSWUI
{
UI_Main *UI_Main::self = 0;

const std::string UI_Main::ui_index( "index.rml" );
const std::string UI_Main::ui_connectscreen( "connectscreen.rml" );

UI_Main::UI_Main( int vidWidth, int vidHeight, int protocol, const char *demoExtension )
	// pointers to zero
	: asmodule(0), rocketModule(0),
	levelshot_fmt(0), datetime_fmt(0), duration_fmt(0), filetype_fmt(0), colorcode_fmt(0), 
	crosshair_fmt(0), empty_fmt(0),
	serverBrowser(0), gameTypes(0), maps(0), vidProfiles(0), huds(0), videoModes(0), 
	demos(0), mods(0), 
	playerModels(0), crosshairs(0), tvchannels(0), ircchannels(0), gameajax(0),
	navigator(0), /* backwards development compatibility: */ currentLoader(0),

	// other members
	mousex(0), mousey(0), gameProtocol(protocol),
	menuVisible(false), forceMenu(false), showNavigationStack(false),
	serverName(""), rejectMessage(""), demoExtension(demoExtension),
	connectCount(0), invalidateAjaxCache(false)
{
	// instance
	self = this;

	Vector4Set( colorWhite, 1, 1, 1, 1 );
	ui_basepath = trap::Cvar_Get( "ui_basepath", "/ui/baseui", CVAR_ARCHIVE );
	ui_cursor = trap::Cvar_Get( "ui_cursor", "cursors/default.rml", CVAR_DEVELOPER );
	ui_developer = trap::Cvar_Get( "developer", "0", 0 );

	// temp fix for missing background on start.. populate refreshState with some nice values
	refreshState.clientState = CA_UNINITIALIZED;
	refreshState.width = vidWidth;
	refreshState.height = vidHeight;
	refreshState.drawBackground = true;

	demoInfo.setPlaying( false );

	if( !initRocket() )
		throw std::runtime_error( "UI: Failed to initialize libRocket" );

	registerRocketCustoms();

	createDataSources();
	createFormatters();

	navigator = __new__( NavigationStack )();
	streamCache = __new__( StreamCache )();

	streamCache->Init();

	if( !initAS() )
		throw std::runtime_error( "UI: Failed to initialize AngelScript" );

	// this after instantiation
	ASUI::BindGlobals( self->getAS() );

	// load cursor document
	loadCursor();

	// this has to be called after AS API is fully loaded
	preloadUI();

	// commands
	trap::Cmd_AddCommand( "ui_reload", ReloadUI_Cmd_f );
	trap::Cmd_AddCommand( "ui_dumpapi", DumpAPI_f );
	trap::Cmd_AddCommand( "ui_printdocs", PrintDocuments_Cmd );

	trap::Cmd_AddCommand( "menu_force", M_Menu_Force_f );
	trap::Cmd_AddCommand( "menu_open", M_Menu_Open_f );
	trap::Cmd_AddCommand( "menu_close", M_Menu_Close_f );

	trap::Cmd_AddCommand( "menu_tvchannel_add", &M_Menu_AddTVChannel_f );
	trap::Cmd_AddCommand( "menu_tvchannel_remove", &M_Menu_RemoveTVChannel_f );
}

UI_Main::~UI_Main()
{
	// remove commands
	trap::Cmd_RemoveCommand( "ui_reload" );
	trap::Cmd_RemoveCommand( "ui_dumpapi" );
	trap::Cmd_RemoveCommand( "ui_printdocs" );

	trap::Cmd_RemoveCommand( "menu_tvchannel_add" );
	trap::Cmd_RemoveCommand( "menu_tvchannel_remove" );

	trap::Cmd_RemoveCommand( "menu_force" );
	trap::Cmd_RemoveCommand( "menu_open" );
	trap::Cmd_RemoveCommand( "menu_close" );

	unregisterRocketCustoms();

	// shutdown AS before rocket, thus script objects get a chance to release
	// their references to rocket elements
	shutdownAS();

	shutdownRocket();

	streamCache->Shutdown();

	self = 0;
}

//==========

bool UI_Main::initAS( void )
{
	asmodule = ASUI::GetASModule( this );
	if( !asmodule->Init() )
		return false;

	// and now our API
	ASUI::BindAPI( asmodule );

	return true;
}

void UI_Main::shutdownAS( void )
{
	ASUI::BindShutdown( asmodule );
	asmodule->Shutdown();
	asmodule = NULL;
}

Rocket::Core::Context *UI_Main::getRocketContext( void )
{
	return rocketModule->getContext();
}

void UI_Main::preloadUI( void )
{
	navigator->popAllDocuments();

	// initialize with default document
	navigator->setDefaultPath( ui_basepath->string );

	// load translation strings

	trap::L10n_ClearDomain();

	// load base UI strings: l10n/ui
	trap::L10n_LoadLangPOFile( "l10n/ui" );
	
	// load strings provided by the theme: e.g. ui/l10n/porkui
	String l10nLocalPath( navigator->getDefaultPath().c_str() );
	l10nLocalPath += "l10n";
	l10nLocalPath.Erase( 0, 1 );
	trap::L10n_LoadLangPOFile( l10nLocalPath.CString() );

	// postpone displaying the document until the first valid refresh state
	navigator->pushDocument( ui_index, false, false );
	showNavigationStack = navigator->hasDocuments();
}

void UI_Main::reloadUI( void )
{
	// clear the navigation stack
	navigator->popAllDocuments();

	// clear the navigation stack from previous installment
	navigator->getCache()->clearCaches();

	// forget about all previously registed shaders
	rocketModule->clearShaderCache();

	if( serverBrowser ) {
		serverBrowser->stopUpdate();
	}
	if( demos ) {
		demos->Reset();
	}

	destroyDataSources();

	createDataSources();

	preloadUI();

	showUI( true );
}

void UI_Main::loadCursor( void )
{
	assert( rocketModule != NULL );

	// setup cursor
	std::string basecursor( ui_basepath->string );

	basecursor += "/";
	basecursor += ui_cursor->string;

	rocketModule->loadCursor( basecursor.c_str() );
}

bool UI_Main::initRocket( void )
{
	// this may throw runtime_error.. ok pass it back up
	rocketModule = __new__( RocketModule )( refreshState.width, refreshState.height );
	return true;
}

void UI_Main::registerRocketCustoms( void )
{
	rocketModule->registerCustoms();
}

void UI_Main::unregisterRocketCustoms( void )
{
	rocketModule->unregisterCustoms();
}

void UI_Main::shutdownRocket( void )
{
	// clear the navigation stack
	navigator->popAllDocuments();
	// clear the navigation stack from previous installment
	navigator->getCache()->clearCaches();
	// forget about all previously registed shaders
	rocketModule->clearShaderCache();

	destroyDataSources();
	destroyFormatters();

	__SAFE_DELETE_NULLIFY( navigator );

	__SAFE_DELETE_NULLIFY( rocketModule );
}

void UI_Main::clearShaderCache( void )
{
	if( rocketModule != NULL ) {
		rocketModule->clearShaderCache();
	}
	this->connectCount++;
}

void UI_Main::touchAllCachedShaders( void )
{
	if( rocketModule != NULL ) {
		rocketModule->touchAllCachedShaders();
	}
	navigator->invalidateAssets();
}

void UI_Main::flushAjaxCache( void )
{
	this->invalidateAjaxCache = true;
}

void UI_Main::createDataSources( void )
{
	serverBrowser = __new__( ServerBrowserDataSource )();
	gameTypes = __new__(GameTypesDataSource)();
	maps = __new__(MapsDataSource)();
	huds = __new__( HudsDataSource )();
	videoModes = __new__( VideoDataSource )();
	demos = __new__( DemosDataSource )( demoExtension );
	mods = __new__( ModsDataSource )();
	crosshairs = __new__( CrosshairDataSource )();
	tvchannels = __new__( TVChannelsDataSource )();
	ircchannels = __new__( IrcChannelsDataSource )();
	gameajax = __new__( GameAjaxDataSource )();
	playerModels = __new__( ModelsDataSource )();
	vidProfiles = __new__( ProfilesDataSource )();
}

void UI_Main::destroyDataSources( void )
{
	__SAFE_DELETE_NULLIFY( serverBrowser );
	__SAFE_DELETE_NULLIFY( gameTypes );
	__SAFE_DELETE_NULLIFY( maps );
	__SAFE_DELETE_NULLIFY( huds );
	__SAFE_DELETE_NULLIFY( videoModes );
	__SAFE_DELETE_NULLIFY( demos );
	__SAFE_DELETE_NULLIFY( mods );
	__SAFE_DELETE_NULLIFY( crosshairs );
	__SAFE_DELETE_NULLIFY( tvchannels );
	__SAFE_DELETE_NULLIFY( ircchannels );
	__SAFE_DELETE_NULLIFY( gameajax );
	__SAFE_DELETE_NULLIFY( playerModels );
	__SAFE_DELETE_NULLIFY( vidProfiles );
}

void UI_Main::createFormatters( void )
{
	crosshair_fmt = __new__( CrosshairFormatter )();
	levelshot_fmt = __new__(LevelShotFormatter)();
	datetime_fmt = __new__( DatetimeFormatter )();
	duration_fmt = __new__( DurationFormatter )();
	filetype_fmt = __new__( FiletypeFormatter )();
	colorcode_fmt = __new__( ColorCodeFormatter )();
	empty_fmt = __new__( EmptyFormatter )();
}

void UI_Main::destroyFormatters( void )
{
	__SAFE_DELETE_NULLIFY( crosshair_fmt );
	__SAFE_DELETE_NULLIFY( levelshot_fmt );
	__SAFE_DELETE_NULLIFY( datetime_fmt );
	__SAFE_DELETE_NULLIFY( duration_fmt );
	__SAFE_DELETE_NULLIFY( filetype_fmt );
	__SAFE_DELETE_NULLIFY( colorcode_fmt );
	__SAFE_DELETE_NULLIFY( empty_fmt );
}

//==============================================

void UI_Main::forceUI( bool force )
{
	forceMenu = force;
}

void UI_Main::showUI( bool show )
{
	// only disable menu if not forced to display it
	if( !show && forceMenu )
		return;

	menuVisible = show;
	trap::CL_SetKeyDest( show ? key_menu : key_game );

	if( !show )
		navigator->popAllDocuments();
}

void UI_Main::drawConnectScreen( const char *serverName, const char *rejectMessage, 
	int downloadType, const char *downloadFilename, float downloadPercent, int downloadSpeed, 
	int connectCount, bool backGround )
{
	DownloadInfo dlinfo( downloadFilename, downloadType );

	dlinfo.setPercent( downloadPercent );
	dlinfo.setSpeed( downloadSpeed );

	this->serverName = serverName ? serverName : "";
	this->rejectMessage = rejectMessage ? rejectMessage : "";
	this->downloadInfo = dlinfo;

	navigator->pushDocument( ui_connectscreen, false, true );

	forceUI( true );
	showUI( true );
}

int UI_Main::getGameProtocol( void ) const 
{
	return gameProtocol;
}

void UI_Main::customRender( void )
{
	// NO-OP for now
}

//===========================================

// CALLBACKS FROM MAIN PROGRAM

void UI_Main::mouseMove( int x, int y, bool absolute )
{
	// change the delta to window coordinates.
	if( absolute ) {
		mousex = x;
		mousey = y;
	} else {
		mousex += x;
		mousey += y;
	}

	if( mousex < 0 )
		mousex = 0;
	else if( mousex > refreshState.width )
		mousex = refreshState.width;
	if( mousey < 0 )
		mousey = 0;
	else if( mousey > refreshState.height )
		mousey = refreshState.height;

	rocketModule->mouseMove( mousex, mousey );
}

void UI_Main::textInput( qwchar c )
{
	// context->ProcessTextInput( c );
	rocketModule->textInput( c );
}

void UI_Main::keyEvent( int key, bool pressed )
{
	// TODO: handle some special keys here?
	rocketModule->keyEvent( key, pressed );
}

void UI_Main::addToServerList(const char *adr, const char *info)
{
	if( !serverBrowser )
		return;

	serverBrowser->addToServerList( adr, info );
}

void UI_Main::forceMenuOff(void)
{
	forceUI( false );
	showUI( false );
}

bool UI_Main::debugOn( void )
{
	return ui_developer->integer != 0;
}

void UI_Main::refreshScreen( unsigned int time, int clientState, int serverState, 
	bool demoPlaying, const char *demoName, bool demoPaused, unsigned int demoTime, 
	bool backGround, bool showCursor )
{
	refreshState.time = time;
	refreshState.clientState = clientState;
	refreshState.serverState = serverState;
	refreshState.drawBackground = backGround;

	if( demoPlaying && !demoInfo.getPlaying() ) {
		demoInfo.setName( demoName );
	}
	demoInfo.setTime( demoTime );
	demoInfo.setPaused( demoPaused );
	demoInfo.setPlaying( demoPlaying );

	// postponed showing of the stacked document, we need to set the refresh state first
	if( showNavigationStack ) {
		navigator->showStack( true );
		showNavigationStack = false;
	}

	// update necessary modules
	if( serverBrowser )
		serverBrowser->updateFrame();
	if( demos )
		demos->UpdateFrame();
	if( ircchannels )
		ircchannels->UpdateFrame();

	if( clientState == CA_ACTIVE && invalidateAjaxCache ) {
		gameajax->FlushCache();
		invalidateAjaxCache = false;
	}

	// TODO: handle the intervalled functions in AS somehow,
	// taking care that they are not called when menu is hidden.
	// i may need to make the interface public..
	BindFrame( asmodule );

	// run incremental garbage collection
	asmodule->garbageCollectOneStep();

	if( showCursor ) { 
		rocketModule->showCursor();
	}
	else {
		rocketModule->hideCursor();
	}

	if( !menuVisible ) {
		return;
	}

	if( !navigator->hasDocuments() ) {
		// no documents on stack, release the key dest
		showUI( false );
		return;
	}

	// rocket update+render
	rocketModule->update();
	rocketModule->render();

	// mark the top stack document as viwed for history tracking
	navigator->markTopAsViewed();

	// stuff we need to render without using rocket
	customRender();
}

//==================================

UI_Main *UI_Main::Instance( int vidWidth, int vidHeight, int protocol, const char *demoExtension  )
{
	if( !self ) {
		self = __new__( UI_Main )( vidWidth, vidHeight, protocol, demoExtension );
	}
	return self;
}

UI_Main *UI_Main::Get( void )
{
	return self;
}

void UI_Main::Destroy( void )
{
	if( self ) {
		__delete__( self );
		self = NULL;
	}
}

//==================================

void UI_Main::ReloadUI_Cmd_f( void )
{
	if( !self )
		return;

	self->reloadUI();
}

void UI_Main::DumpAPI_f( void )
{
	if( !self || !self->asmodule )
		return;

	self->asmodule->dumpAPI( va( "AS_API/v%.g-ui/", trap::Cvar_Value( "version" ) ) );
}

void UI_Main::M_Menu_Force_f( void )
{
	if( !self )
		return;

	//Com_Printf("UI_Main::M_Menu_Force_F..\n");

	NavigationStack *nav = self->getNavigator();
	if( !nav )
		return;

	bool force = atoi( trap::Cmd_Argv( 1 ) ) != 0;
	self->forceUI( force );

	if( !force ) {
		return;
	}

	// if forced, ensure we have at least the default page on stack
	if( !nav->hasDocuments() ) {
		nav->pushDocument( self->ui_index );
	}
	self->showUI( true );
}

void UI_Main::M_Menu_Open_f( void )
{
	int i;

	if( !self )
		return;
	if( trap::Cmd_Argc() < 2 )
		return;

	Rocket::Core::URL url;

	url.SetFileName( trap::Cmd_Argv( 1 ) );
	url.SetExtension( "rml" );

	for( i = 2; i < trap::Cmd_Argc() - 1; i += 2 ) {
		url.SetParameter( trap::Cmd_Argv( i ), trap::Cmd_Argv( i+1 ) );
	}

	Rocket::Core::String urlString = url.GetURL();
	//Com_Printf( "UI_Main::M_Menu_Open_f %s\n", urlString.CString() );

	NavigationStack *nav = self->getNavigator();
	if( !nav )
		return;

	nav->pushDocument( urlString.CString() );
	self->showUI( true );
}

void UI_Main::M_Menu_Close_f( void )
{
	if( !self )
		return;
	self->showUI( false );
}


void UI_Main::M_Menu_AddTVChannel_f( void )
{
	int id;

	if( !self || !self->tvchannels ) {
		return;
	}
	if( trap::Cmd_Argc() < 5 ) {
		return;
	}

	id = atoi( trap::Cmd_Argv( 1 ) );
	if( id <= 0 ) {
		return;
	}

	TVChannel chan;
	chan.name = trap::Cmd_Argv( 2 );
	chan.realname = trap::Cmd_Argv( 3 );
	chan.address = trap::Cmd_Argv( 4 );
	chan.numPlayers = atoi( trap::Cmd_Argv( 5 ) );
	chan.numSpecs = atoi( trap::Cmd_Argv( 6 ) );
	chan.gametype = trap::Cmd_Argv( 7 );
	chan.mapname = trap::Cmd_Argv( 8 );
	chan.matchname = trap::Cmd_Argv( 9 );
	if( chan.name.empty() ) {
		return;
	}

	self->tvchannels->AddChannel( id, chan );
}

void UI_Main::M_Menu_RemoveTVChannel_f( void )
{
	int id;

	if( !self || !self->tvchannels ) {
		return;
	}
	if( trap::Cmd_Argc() != 2 ) {
		return;
	}

	id = atoi( trap::Cmd_Argv( 1 ) );
	if( id <= 0 ) {
		return;
	}

	self->tvchannels->RemoveChannel( id );
}

// DEBUG
void UI_Main::PrintDocuments_Cmd( void )
{
	if( !self )
		return;

	NavigationStack *nav = self->getNavigator();
	if( !nav )
		return;
	Com_Printf("Navigation stack:\n");
	nav->printStack();

	DocumentCache *cache = nav->getCache();
	if( !cache )
		return;
	Com_Printf("Document cache:\n");
	cache->printCache();
}
}
