#pragma once
#ifndef __UI_DECORATORS_H__
#define __UI_DECORATORS_H__

#include <RmlUi/Core/DecoratorInstancer.h>

namespace WSWUI
{

Rml::Core::DecoratorInstancer *GetGradientDecoratorInstancer( void );
Rml::Core::DecoratorInstancer *GetNinePatchDecoratorInstancer( void );
Rml::Core::DecoratorInstancer *GetSVGDecoratorInstancer( void );

}
#endif
