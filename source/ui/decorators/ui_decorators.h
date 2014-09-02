#pragma once
#ifndef __UI_DECORATORS_H__
#define __UI_DECORATORS_H__

#include <Rocket/Core/DecoratorInstancer.h>

namespace WSWUI {

	Rocket::Core::DecoratorInstancer *GetGradientDecoratorInstancer( void );
	Rocket::Core::DecoratorInstancer *GetNinePatchDecoratorInstancer( void );

}
#endif
