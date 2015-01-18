/*
Copyright (C) 2013 Victor Luchits

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

/*
==============================================================

L10N SUBSYSTEM

==============================================================
*/
void L10n_Init( void );
const char *L10n_GetUserLanguage( void );
void L10n_LoadLangPOFile( const char *domainname, const char *filepath );
const char *L10n_TranslateString( const char *domainname, const char *string );
void L10n_ClearDomain( const char *domainname );
void L10n_ClearDomains( void );
void L10n_Shutdown( void );
