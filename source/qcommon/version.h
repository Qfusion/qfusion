/*
Copyright (C) 2007 Victor Luchits

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

#undef STR_HELPER
#undef STR_TOSTR

#define STR_HELPER( s )                 # s
#define STR_TOSTR( x )                  STR_HELPER( x )

#define APPLICATION                     "Cocaine Diesel"
#define APPLICATION_NOSPACES            "CocaineDiesel"
#define DEFAULT_BASEGAME                "base"

#define APP_VERSION_MAJOR               0
#define APP_VERSION_MINOR               0
#define APP_VERSION_UPDATE              1
#define APP_VERSION                     APP_VERSION_MAJOR + APP_VERSION_MINOR * 0.1 + APP_VERSION_UPDATE * 0.01
#define APP_VERSION_STR                 STR_TOSTR( APP_VERSION_MAJOR ) "." STR_TOSTR( APP_VERSION_MINOR ) STR_TOSTR( APP_VERSION_UPDATE )
#define APP_VERSION_STR_MAJORMINOR      STR_TOSTR( APP_VERSION_MAJOR ) STR_TOSTR( APP_VERSION_MINOR )

#ifdef PUBLIC_BUILD
#define APP_PROTOCOL_VERSION            1
#else
#define APP_PROTOCOL_VERSION            1001
#endif

#define APP_URL                         "http://www.e4m5.net/"

#define APP_COPYRIGHT_OWNER             "Aha Cheers"

#define APP_SCREENSHOTS_PREFIX          "cocainediesel_"
#define APP_DEMO_EXTENSION_STR          ".cddemo"

#define APP_URI_SCHEME                  "diesel://"
#define APP_URI_PROTO_SCHEME            "diesel" STR_TOSTR( APP_PROTOCOL_VERSION ) "://"

#define APP_UI_BASEPATH                 "ui/baseui"
#define APP_STARTUP_COLOR               0x1c1416

//
// the following macros are only used by the windows resource file
//
#ifdef __GNUC__

#define APP_VERSION_RC_STR              STR_TOSTR( APP_VERSION_MAJOR ) "." STR_TOSTR( APP_VERSION_MINOR )
#define APP_FILEVERSION_RC_STR          STR_TOSTR( APP_VERSION_MAJOR ) "," STR_TOSTR( APP_VERSION_MINOR ) "," STR_TOSTR( APP_VERSION_UPDATE ) ",0"

#else // __GNUC__

#define APP_VERSION_RC                  APP_VERSION_MAJOR.APP_VERSION_MINOR
#define APP_VERSION_RC_STR              STR_TOSTR( APP_VERSION_RC )
#define APP_FILEVERSION_RC              APP_VERSION_MAJOR,APP_VERSION_MINOR,APP_VERSION_UPDATE,0
#define APP_FILEVERSION_RC_STR          STR_TOSTR( APP_FILEVERSION_RC )

#endif // __GNUC__

#define APP_ICO_ICON                    "../../icons/forksow.ico"
#define APP_DEMO_ICO_ICON               "../../icons/forksow_demo.ico"
#define APP_XPM_ICON                    "../../icons/forksow.xpm"

/**
 * The Steam App ID of the game.
 * 0 disables integration.
 * 480 can be used for testing with Spacewar.
 */
#define APP_STEAMID                     0
