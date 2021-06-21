#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"

#include "as/asui.h"
#include "as/asui_local.h"

#include "datasources/ui_serverbrowser_datasource.h"

namespace ASUI
{

typedef WSWUI::ServerBrowserDataSource ServerBrowserDataSource;

void PrebindServerbrowser( ASInterface *as ) {
	ASBind::Class<ServerBrowserDataSource, ASBind::class_singleref>( as->getEngine() );
}

static bool Serverbrowser_addFavorite( ServerBrowserDataSource *browser, const asstring_t &fav ) {
	return browser->addFavorite( fav.buffer );
}

static bool Serverbrowser_removeFavorite( ServerBrowserDataSource *browser, const asstring_t &fav ) {
	return browser->removeFavorite( fav.buffer );
}

static void Serverbrowser_sortByField( ServerBrowserDataSource *browser, const asstring_t &field ) {
	browser->sortByField( field.buffer );
}

static ServerBrowserDataSource &Serverbrowser_getInstance( void ) {
	return *( UI_Main::Get()->getServerBrowser() );
}

void BindServerbrowser( ASInterface *as ) {
	ASBind::GetClass<ServerBrowserDataSource>( as->getEngine() )

	.method( &ServerBrowserDataSource::startFullUpdate, "fullUpdate" )
	.method( &ServerBrowserDataSource::startRefresh, "refresh" )
	.method( &ServerBrowserDataSource::isUpdating, "isUpdating" )
	.method( &ServerBrowserDataSource::stopUpdate, "stopUpdate" )
	.method( &Serverbrowser_addFavorite, "addFavorite", true )
	.method( &Serverbrowser_removeFavorite, "removeFavorite", true )
	.method( &Serverbrowser_sortByField, "sortByField", true )
	.method( &ServerBrowserDataSource::getLastActiveTime, "getLastActiveTime" )
	.method( &ServerBrowserDataSource::getUpdateId, "getUpdateId" )
	;
}

void BindServerbrowserGlobal( ASInterface *as ) {
	// globals
	ASBind::Global( as->getEngine() )

	// global variables
	.function( &Serverbrowser_getInstance, "get_serverBrowser" )
	;
}

}

ASBIND_TYPE( WSWUI::ServerBrowserDataSource, ServerBrowser )
