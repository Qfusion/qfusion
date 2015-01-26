#include "../qcommon/qcommon.h"
#include <windows.h>

/*
* Sys_GetClipboardData
*/
char *Sys_GetClipboardData( bool primary )
{
	char *utf8text = NULL;
	int utf8size;
	WCHAR *cliptext;

	if( OpenClipboard( NULL ) != 0 )
	{
		HANDLE hClipboardData;

		if( ( hClipboardData = GetClipboardData( CF_UNICODETEXT ) ) != 0 )
		{
			if( ( cliptext = GlobalLock( hClipboardData ) ) != 0 )
			{
				utf8size = WideCharToMultiByte( CP_UTF8, 0, cliptext, -1, NULL, 0, NULL, NULL );
				utf8text = Q_malloc( utf8size );
				WideCharToMultiByte( CP_UTF8, 0, cliptext, -1, utf8text, utf8size, NULL, NULL );
				GlobalUnlock( hClipboardData );
			}
		}
		CloseClipboard();
	}
	return utf8text;
}

/*
* Sys_SetClipboardData
*/
bool Sys_SetClipboardData( const char *data )
{
	size_t size;
	HGLOBAL hglbCopy;
	UINT uFormat;
	LPCSTR cliptext = data;
	LPWSTR lptstrCopy;

	// open the clipboard, and empty it
	if( !OpenClipboard( NULL ) ) 
		return false;

	EmptyClipboard();

	size = MultiByteToWideChar( CP_UTF8, 0, cliptext, -1, NULL, 0 );

	// allocate a global memory object for the text
	hglbCopy = GlobalAlloc( GMEM_MOVEABLE, (size + 1) * sizeof( *lptstrCopy ) ); 
	if( hglbCopy == NULL )
	{
		CloseClipboard(); 
		return false; 
	} 

	// lock the handle and copy the text to the buffer
	lptstrCopy = GlobalLock( hglbCopy ); 

	uFormat = CF_UNICODETEXT;
	MultiByteToWideChar( CP_UTF8, 0, cliptext, -1, lptstrCopy, size );
	lptstrCopy[size] = 0;

	GlobalUnlock( hglbCopy ); 

	// place the handle on the clipboard
	SetClipboardData( uFormat, hglbCopy );

	// close the clipboard
	CloseClipboard();

	return true;
}

/*
* Sys_FreeClipboardData
*/
void Sys_FreeClipboardData( char *data )
{
	Q_free( data );
}