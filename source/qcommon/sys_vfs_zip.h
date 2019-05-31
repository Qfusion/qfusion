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
// sys_vfs_zip.h - uncompressed zip file virtual filesystem

#ifndef __SYS_VFS_ZIP_H
#define __SYS_VFS_ZIP_H

#include "sys_fs.h"

void        Sys_VFS_Zip_Init( int numvfs, const char * const *vfsnames );
char        **Sys_VFS_Zip_ListFiles( const char *pattern, const char *prependBasePath, int *numFiles, bool listFiles, bool listDirs );
void        *Sys_VFS_Zip_FindFile( const char *filename );
const char  *Sys_VFS_Zip_VFSName( void *handle );
unsigned    Sys_VFS_Zip_FileOffset( void *handle );
unsigned    Sys_VFS_Zip_FileSize( void *handle );
void        Sys_VFS_Zip_Shutdown( void );

#endif // __SYS_VFS_ZIP_H
