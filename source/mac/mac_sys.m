#import <Cocoa/Cocoa.h>
#include "../client/client.h"

void Sys_OpenURLInBrowser( const char *url )
{
	NSString *string_url = [NSString stringWithUTF8String:url];
	NSURL *ns_url = [NSURL URLWithString:string_url];
	[[NSWorkspace sharedWorkspace] openURL:ns_url];
}
