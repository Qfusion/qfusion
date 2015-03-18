#include "../client/client.h"
#include <windows.h>

/**
 * IN_GetInputLanguage
 */
void IN_GetInputLanguage( char *dest, size_t size )
{
	char lang[16];

	lang[0] = '\0';

	GetLocaleInfo(
		MAKELCID( LOWORD( GetKeyboardLayout( 0 ) ), SORT_DEFAULT ),
		LOCALE_SISO639LANGNAME,
		lang, sizeof( lang ) );

	Q_strupr( lang );
	Q_strncpyz( dest, lang, size );
}