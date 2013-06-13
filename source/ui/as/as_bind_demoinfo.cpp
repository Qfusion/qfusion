/*
Copyright (C) 2011 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "kernel/ui_demoinfo.h"
#include "kernel/ui_utils.h"
#include "as/asui.h"
#include "as/asui_local.h"

namespace ASUI {

typedef WSWUI::DemoInfo DemoInfo;
typedef WSWUI::DemoMetaData DemoMetaData;

void PrebindDemoInfo( ASInterface *as )
{
	ASBind::Class<DemoInfo, ASBind::class_class>( as->getEngine() );
}

static void DemoInfo_StringConstructor( DemoInfo *self, const asstring_t & name )
{
	new( self ) DemoInfo( ASSTR( name ) );
}

static const asstring_t *DemoInfo_GetName( DemoInfo *demoInfo )
{
	return ASSTR( demoInfo->getName() );
}

static void DemoInfo_SetName( DemoInfo *demoInfo, const asstring_t &name )
{
	demoInfo->setName( ASSTR( name ) );
}

static asstring_t *DemoInfo_GetMeta( DemoInfo *demoInfo, const asstring_t &key )
{
	const DemoMetaData &metaData = demoInfo->getMetaData();
	DemoMetaData::const_iterator it = metaData.find( key.buffer );

	if( it == metaData.end() ) { 
		return ASSTR( "" );
	}
	return ASSTR( it->second.c_str() );
}

void BindDemoInfo( ASInterface *as )
{
	// this allows AS to access properties and control demos
	// some of the methods are only relevant for the playing instance
	ASBind::GetClass<DemoInfo>( as->getEngine() )
		.constructor<void()>()
		.constructor( &DemoInfo_StringConstructor, true )
		
		.constructor<void(const DemoInfo &other)>()
		.destructor()

		.method( &DemoInfo::operator =, "opAssign" )

		.method( &DemoInfo::getPlaying, "get_isPlaying" )
		.method( &DemoInfo::getPaused, "get_isPaused" )
		.method( &DemoInfo::getTime, "get_time" )
		.method( &DemoInfo::Play, "play" )
		.method( &DemoInfo::Stop, "stop" )
		.method( &DemoInfo::Pause, "pause" )
		.method( &DemoInfo::Jump, "jump" )

		.constmethod( &DemoInfo_GetName, "get_name", true )
		.constmethod( &DemoInfo_SetName, "set_name", true )

		.constmethod( &DemoInfo_GetMeta, "getMeta", true )
	;
}

}
