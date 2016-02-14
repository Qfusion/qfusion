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

#include <dirent.h>
#include "android_sys.h"
#include "../qcommon/sys_vfs_zip.h"

static void Sys_VFS_Android_GetOBBVersions( const char *dir, int *mainVersion, int *patchVersion )
{
	DIR *fdir;
	char mainFormat[PATH_MAX], patchFormat[PATH_MAX], checkPath[PATH_MAX];
	struct dirent *entry;
	int version, maxMain = -1, maxPatch = -1;
	// Don't load OBBs for wrong versions of the game. A major update requires the game to be redownloaded entirely.
	const int minVersion = APP_VERSION_MAJOR * 100000 + APP_VERSION_MINOR * 10000, maxVersion = minVersion + 9999;

	fdir = opendir( dir );
	if( !fdir )
	{
		*mainVersion = -1;
		*patchVersion = -1;
		return;
	}

	Q_snprintfz( mainFormat, sizeof( mainFormat ), "main.%%d.%s.obb", sys_android_packageName );
	Q_snprintfz( patchFormat, sizeof( patchFormat ), "patch.%%d.%s.obb", sys_android_packageName );

	while( ( entry = readdir( fdir ) ) != NULL )
	{
		if( entry->d_type == DT_DIR )
			continue;

		if ( sscanf( entry->d_name, mainFormat, &version ) == 1 )
		{
			if( ( version < minVersion ) || ( version > maxVersion ) || ( version <= maxMain ) )
				continue;
			// Make sure it's .obb, not .obb.tmp.
			Q_snprintfz( checkPath, sizeof( checkPath ), mainFormat, version );
			if( strcmp( entry->d_name, checkPath ) )
				continue;
			maxMain = version;
		}
		else if ( sscanf( entry->d_name, patchFormat, &version ) == 1 )
		{
			if( ( version < minVersion ) || ( version > maxVersion ) || ( version <= maxPatch ) )
				continue;
			Q_snprintfz( checkPath, sizeof( checkPath ), patchFormat, version );
			if( strcmp( entry->d_name, checkPath ) )
				continue;
			maxPatch = version;
		}
	}

	closedir( fdir );

	*mainVersion = maxMain;
	if( maxPatch >= maxMain )
		*patchVersion = maxPatch;
	else
		*patchVersion = -1;
}

void Sys_VFS_Init( void )
{
	JNIEnv *env = Sys_Android_GetJNIEnv();
	char obbDir[PATH_MAX];
	int mainVersion, patchVersion;
	char mainPath[PATH_MAX], patchPath[PATH_MAX];
	const char *vfsNames[2];
	int numVFS = 0;

	{
		jclass environment, file;
		jmethodID getExternalStorageDirectory, getAbsolutePath;
		jobject dirFile;
		jstring dirJS;
		const char *dirUTF;

		environment = (*env)->FindClass( env, "android/os/Environment" );
		getExternalStorageDirectory = (*env)->GetStaticMethodID( env, environment,
			"getExternalStorageDirectory", "()Ljava/io/File;" );
		dirFile = (*env)->CallStaticObjectMethod( env, environment, getExternalStorageDirectory );
		(*env)->DeleteLocalRef( env, environment );
		if( !dirFile )
		{
			Com_Printf( "Failed to get external storage directory to load APK expansion files\n" );
			return;
		}

		file = (*env)->FindClass( env, "java/io/File" );
		getAbsolutePath = (*env)->GetMethodID( env, file, "getAbsolutePath", "()Ljava/lang/String;" );
		(*env)->DeleteLocalRef( env, file );
		dirJS = ( jstring )( (*env)->CallObjectMethod( env, dirFile, getAbsolutePath ) );
		(*env)->DeleteLocalRef( env, dirFile );

		dirUTF = (*env)->GetStringUTFChars( env, dirJS, NULL );
		Q_snprintfz( obbDir, sizeof( obbDir ), "%s/Android/obb/%s", dirUTF, sys_android_packageName );
		(*env)->ReleaseStringUTFChars( env, dirJS, dirUTF );
		(*env)->DeleteLocalRef( env, dirJS );
	}

	Sys_VFS_Android_GetOBBVersions( obbDir, &mainVersion, &patchVersion );
	if( mainVersion >= 0 )
	{
		Q_snprintfz( mainPath, sizeof( mainPath ), "%s/main.%d.%s.obb", obbDir, mainVersion, sys_android_packageName );
		vfsNames[numVFS++] = mainPath;
	}
	if( patchVersion >= 0 )
	{
		Q_snprintfz( patchPath, sizeof( patchPath ), "%s/patch.%d.%s.obb", obbDir, patchVersion, sys_android_packageName );
		vfsNames[numVFS++] = patchPath;
	}

	Sys_VFS_Zip_Init( numVFS, vfsNames );
}

void Sys_VFS_TouchGamePath( const char *gamedir, bool initial )
{
}

char **Sys_VFS_ListFiles( const char *pattern, const char *prependBasePath, int *numFiles, bool listFiles, bool listDirs )
{
	return Sys_VFS_Zip_ListFiles( pattern, prependBasePath, numFiles, listFiles, listDirs );
}

void *Sys_VFS_FindFile( const char *filename )
{
	return Sys_VFS_Zip_FindFile( filename );
}

const char *Sys_VFS_VFSName( void *handle )
{
	return Sys_VFS_Zip_VFSName( handle );
}

unsigned Sys_VFS_FileOffset( void *handle )
{
	return Sys_VFS_Zip_FileOffset( handle );
}

unsigned Sys_VFS_FileSize( void *handle )
{
	return Sys_VFS_Zip_FileSize( handle );
}

void Sys_VFS_Shutdown( void )
{
	Sys_VFS_Zip_Shutdown();
}
