#import <Cocoa/Cocoa.h>

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
	char* clipboard = NULL;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSPasteboard	*myPasteboard = NULL;
    NSArray 		*myPasteboardTypes = NULL;
	
    myPasteboard = [NSPasteboard generalPasteboard];
    myPasteboardTypes = [myPasteboard types];
    if ([myPasteboardTypes containsObject: NSStringPboardType])
    {
        NSString	*myClipboardString;
		
        myClipboardString = [myPasteboard stringForType: NSStringPboardType];
        if (myClipboardString != NULL && [myClipboardString length] > 0)
        {
			int bytes = [myClipboardString length];
			clipboard = Q_malloc( bytes + 1 );
			Q_strncpyz( clipboard, (char *)[myClipboardString UTF8String], bytes + 1 );
        }
    }
	[pool release];
    return (clipboard);
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
	Q_free( data );
}

void	Sys_OpenURLInBrowser( const char *url )
{
  NSString *string_url = [NSString stringWithUTF8String:url];
  NSURL *ns_url = [NSURL URLWithString:string_url];
  [[NSWorkspace sharedWorkspace] openURL:ns_url];
}

