#import <Cocoa/Cocoa.h>
#include <SDL2/SDL.h>
#include "../client/client.h"
/*****************************************************************************/

/*
   =================
   Sys_GetClipboardData

   Orginally from EzQuake
   There should be a smarter place to put this
   =================
 */
char *Sys_GetClipboardData( qboolean primary )
{
    return SDL_GetClipboardText();
}

/*
* Sys_SetClipboardData
*/
qboolean Sys_SetClipboardData( char *data )
{
	return qtrue;
}

/*
   =================
   Sys_FreeClipboardData
   =================
 */
void Sys_FreeClipboardData( char *data )
{
    SDL_free(data);
}

void	Sys_OpenURLInBrowser( const char *url )
{
  NSString *string_url = [NSString stringWithUTF8String:url];
  NSURL *ns_url = [NSURL URLWithString:string_url];
  [[NSWorkspace sharedWorkspace] openURL:ns_url];
}
