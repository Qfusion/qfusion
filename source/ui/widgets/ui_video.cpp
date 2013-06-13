#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "widgets/ui_video.h"
#include "widgets/ui_widgets.h"

namespace WSWUI
{	
	Video::Video(const Rocket::Core::String& tag) : ElementImage(tag)
	{

	} 

	void Video::OnAttributeChange(const Rocket::Core::AttributeNameList& anl)
	{
		if(anl.find("src") != anl.end())
			trap::R_RegisterVideo( GetAttribute<Rocket::Core::String>("src", "").CString() ); // register a default video-shader, so R_RegisterPic will return this shader
		ElementImage::OnAttributeChange(anl);
	}

	Rocket::Core::ElementInstancer *GetVideoInstancer(void)
	{
		return __new__( GenericElementInstancer<Video> )(); 
	}

}
