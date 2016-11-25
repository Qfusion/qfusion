/*
Copyright (C) 2016 SiPlus, Warsow development team

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
// sys_vfs_null.c -- null virtual filesystem for systems that load paks directly from the fs

#include "../qcommon/qcommon.h"

void Sys_VFS_Init( void ) {
}

void Sys_VFS_TouchGamePath( const char *gamedir, bool initial ) {
}

char **Sys_VFS_ListFiles( const char *pattern, const char *prependBasePath, int *numFiles, bool listFiles, bool listDirs ) {
	if( numFiles ) {
		*numFiles = 0;
	}

	return NULL;
}

void *Sys_VFS_FindFile( const char *filename ) {
	return NULL;
}

const char *Sys_VFS_VFSName( void *handle ) {
	return NULL;
}

unsigned Sys_VFS_FileOffset( void *handle ) {
	return 0;
}

unsigned Sys_VFS_FileSize( void *handle ) {
	return 0;
}

void Sys_VFS_Shutdown( void ) {
}
