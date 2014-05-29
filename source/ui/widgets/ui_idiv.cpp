/*
Copyright (C) 2011 Victor Luchits

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

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "widgets/ui_widgets.h"
#include "widgets/ui_idiv.h"

namespace WSWUI {

using namespace Rocket::Core;

InlineDiv::InlineDiv( const String &tag ) : Element(tag), timeout(WSW_UI_STREAMCACHE_TIMEOUT)
{
}

void InlineDiv::ReadFromFile( const char *fileName )
{
	int file, length;
	
	length = trap::FS_FOpenFile( fileName, &file, FS_READ );
	if( length < 1 ) {
		// missing file
		SetInnerRML( String( "Failed to load " ) + fileName );
	}
	else {
		// allocate temporary buffer
		char *buffer = __newa__( char, length + 1 );

		// read the whole file contents
		trap::FS_Read( buffer, length, file );
		trap::FS_FCloseFile( file );
		buffer[length] = '\0';

		// set elements RML code
		SetInnerRML( buffer );

		// free
		__delete__( buffer );
	}

	DispatchEvent( "load", Dictionary(), false );
}

void InlineDiv::CacheRead( const char *fileName, void *privatep )
{
	InlineDiv *element = ( InlineDiv * )privatep;

	element->ReadFromFile( fileName );

	// remove reference by stream cache object
	element->RemoveReference();
}

void InlineDiv::LoadSource()
{
	// Get the source URL for the image.
	String source = GetAttribute< String >("src", "");
	bool nocache = GetAttribute< int >("nocache", 0) != 0;

	if( source.Empty() ) {
		SetInnerRML( "" );
		SetPseudoClass( "loading", false );
		DispatchEvent( "load", Dictionary(), false );
		return;
	}

	SetPseudoClass( "loading", true );

	if( trap::FS_IsUrl( source.CString() ) ) {
		// the stream cache object references this element
		// (passed as the void * pointer below)
		AddReference();

		UI_Main::Get()->getStreamCache()->PerformRequest( 
			source.CString(), "GET", NULL, 
			NULL, NULL, &CacheRead, ( void * )this, timeout, nocache ? 0 : WSW_UI_STREAMCACHE_CACHE_TTL
		);
	}
	else {
		// get full path to the source.
		// without the leading "/", path is considered to be relative to the document
		Rocket::Core::ElementDocument* document = GetOwnerDocument();
		URL source_url( document == NULL ? "" : document->GetSourceURL() );
		String source_directory = source_url.GetPath();

		String path;
		if( source.Substring( 0, 1 ) == "?" ) {
			path = source;
		} else {
			GetSystemInterface()->JoinPath( path, source_directory.Replace( "|", ":" ), source );
		}

		ReadFromFile( path.CString() );

		SetPseudoClass( "loading", false );
	}
}

// Called when attributes on the element are changed.
void InlineDiv::OnAttributeChange( const Rocket::Core::AttributeNameList& changed_attributes )
{
	Element::OnAttributeChange(changed_attributes);

	AttributeNameList::const_iterator it;

	// Check for a changed 'src' attribute. If this changes, we need to reload
	// contents of the element.
	it = changed_attributes.find( "src" );
	if( it != changed_attributes.end() ) {
		LoadSource();
	}

	// timeout for remote URL's
	it = changed_attributes.find( "timeout" );
	if( it != changed_attributes.end() ) {
		timeout = atoi(GetAttribute< String >("src", "").CString());
	}
}

//==============================================================

ElementInstancer *GetInlineDivInstancer( void )
{
	return __new__( GenericElementInstancer<InlineDiv> )();
}

}
