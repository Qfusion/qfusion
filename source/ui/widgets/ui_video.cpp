#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "widgets/ui_video.h"
#include "widgets/ui_widgets.h"

namespace WSWUI
{
Video::Video( const Rml::Core::String& tag ) : ElementImage( tag ) {

}

void Video::OnAttributeChange( const Rml::Core::ElementAttributes& anl ) {
	auto it = anl.find( "src" );
	if( it != anl.end() ) {
		trap::R_RegisterVideo( it->second.Get<std::string>().c_str() );   // register a default video-shader, so R_RegisterPic will return this shader
	}
	ElementImage::OnAttributeChange( anl );
}

Rml::Core::ElementInstancer *GetVideoInstancer( void ) {
	return __new__( GenericElementInstancer<Video> )();
}

}
