/*
Copyright (C) 2012 Victor Luchits

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
#include "kernel/ui_downloadinfo.h"
#include "kernel/ui_utils.h"
#include "as/asui.h"
#include "as/asui_local.h"

namespace ASUI {

typedef WSWUI::DownloadInfo DownloadInfo;

void PrebindDownloadInfo( ASInterface *as )
{
	ASBind::Class<DownloadInfo, ASBind::class_class>( as->getEngine() );
}

static const asstring_t *DownloadInfo_GetName( DownloadInfo *downloadInfo )
{
	return ASSTR( downloadInfo->getName() );
}

void BindDownloadInfo( ASInterface *as )
{
	// this allows AS to access properties and control demos
	// some of the methods are only relevant for the playing instance
	ASBind::GetClass<DownloadInfo>( as->getEngine() )
		.constructor<void()>()
		.constructor<void(const DownloadInfo &other)>()
		.destructor()

		.method( &DownloadInfo::operator =, "opAssign" )

		.method( &DownloadInfo::getPercent, "get_percent" )
		.method( &DownloadInfo::getSpeed, "get_speed" )
		.method( &DownloadInfo::getType, "get_type" )
		.constmethod( &DownloadInfo_GetName, "get_name", true )
	;
}

}
