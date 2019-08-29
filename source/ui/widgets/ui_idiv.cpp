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

namespace WSWUI
{

using namespace Rml::Core;

InlineDiv::InlineDiv( const String &tag ) : Element( tag ), timeout( WSW_UI_STREAMCACHE_TIMEOUT ), onAddLoad( false ), loading( false ) {
}

void InlineDiv::ReadFromFile( const char *fileName ) {
	FileHandle handle = GetFileInterface()->Open( fileName );

	if( !handle ) {
		// missing file
		SetInnerRML( String( "Failed to load " ) + fileName );
	} else {
		size_t length = GetFileInterface()->Length( handle );

		// allocate temporary buffer
		char *buffer = __newa__( char, length + 1 );

		// read the whole file contents
		GetFileInterface()->Read( buffer, length, handle );
		GetFileInterface()->Close( handle );
		buffer[length] = '\0';

		// set elements RML code
		SetInnerRML( buffer );

		// free
		__delete__( buffer );
	}

	DispatchEvent( "load", Dictionary(), false );
}

void InlineDiv::CacheRead( const char *fileName, void *privatep ) {
	ElementOwner *eo = ( ElementOwner *)privatep;
	InlineDiv *element = static_cast< InlineDiv * >(eo->elem.get());

	element->ReadFromFile( fileName );

	__delete__( eo );
}

void InlineDiv::OnChildAdd( Element* element ) {
	Element::OnChildAdd( element );

	if( element == this ) {
		Element *document = GetOwnerDocument();
		if( document == NULL ) {
			return;
		}
		if( onAddLoad ) {
			LoadSource();
		}
	}
}

void InlineDiv::LoadSource() {
	if( loading ) {
		// prevent recursive entries
		return;
	}

	// Get the source URL for the image.
	String source = GetAttribute< String >( "src", "" );
	bool nocache = GetAttribute< int >( "nocache", 0 ) != 0;
	int expires = GetAttribute< int >( "expires", WSW_UI_STREAMCACHE_CACHE_TTL );

	onAddLoad = false;
	loading = true;

	if( source.empty() ) {
		SetInnerRML( "" );
		SetPseudoClass( "loading", false );
		DispatchEvent( "load", Dictionary(), false );
		loading = false;
		return;
	}

	SetPseudoClass( "loading", true );

	if( trap::FS_IsUrl( source.c_str() ) ) {
		// the stream cache object references this element
		// (passed as the void * pointer below)
		auto *eo = __new__( ElementOwner )( this );

		UI_Main::Get()->getStreamCache()->PerformRequest(
			source.c_str(), "GET", NULL,
			NULL, NULL, &CacheRead, ( void * )eo, timeout, nocache ? 0 : expires
			);
	} else {
		// get full path to the source.
		// without the leading "/", path is considered to be relative to the document
		Rml::Core::ElementDocument* document = GetOwnerDocument();

		if( !document && source.front() != '/' ) {
			onAddLoad = true;
			loading = false;
			return;
		}

		URL source_url( document == NULL ? "" : document->GetSourceURL() );
		String source_directory = source_url.GetPath();

		String path;
		if( source.front() == '?' ) {
			path = source;
		} else {
			GetSystemInterface()->JoinPath( path, Rml::Core::StringUtilities::Replace( source_directory, "|", ":" ), source );
		}

		ReadFromFile( path.c_str() );

		SetPseudoClass( "loading", false );
	}

	loading = false;
}

// Called when attributes on the element are changed.
void InlineDiv::OnAttributeChange( const Rml::Core::ElementAttributes& changed_attributes ) {
	Element::OnAttributeChange( changed_attributes );

	// Check for a changed 'src' attribute. If this changes, we need to reload
	// contents of the element.
	auto it = changed_attributes.find( "src" );
	if( it != changed_attributes.end() ) {
		LoadSource();
	}

	// timeout for remote URL's
	it = changed_attributes.find( "timeout" );
	if( it != changed_attributes.end() ) {
		timeout = atoi( GetAttribute< String >( "src", "" ).c_str() );
	}
}

//==============================================================

ElementInstancer *GetInlineDivInstancer( void ) {
	return __new__( GenericElementInstancer<InlineDiv> )();
}

}
