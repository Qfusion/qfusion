/*
Copyright (C) 2018 Victor Luchits

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

#include "cg_local.h"

cg_overlay_t cg_overlay;

void CG_Overlay_Init( cg_overlay_t *overlay ) {
	memset( overlay, 0, sizeof( *overlay ) );

	overlay->cursor_x = cgs.vidWidth / 2;
	overlay->cursor_y = cgs.vidHeight / 2;

	trap_SCR_ShowOverlay( false, false );
}

void CG_Overlay_MouseMove( cg_overlay_t *overlay, int mx, int my ) {
	if( trap_SCR_HaveOverlay() && overlay->showCursor ) {
		overlay->cursor_x += mx;
		overlay->cursor_y += my;

		trap_SCR_OverlayMouseMove( mx, my, false );
	}
}

bool CG_Overlay_Hover( cg_overlay_t *overlay ) {
	if( trap_SCR_HaveOverlay() && overlay->showCursor ) {
		if( trap_SCR_OverlayHover() ) {
			return true;
		}
	}
	return false;
}

void CG_Overlay_KeyEvent( cg_overlay_t *overlay, int key, bool down ) {
	trap_SCR_OverlayKeyEvent( key, down );
}

void CG_Overlay_Show( cg_overlay_t *overlay, bool show, bool showCursor ) {
	if( show ) {
		trap_SCR_OverlayMouseMove( overlay->cursor_x, overlay->cursor_y, true );
	}
	overlay->showCursor = showCursor;
	trap_SCR_ShowOverlay( show, showCursor );
}
