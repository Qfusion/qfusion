#include <Rocket/Core.h>

#include "gameshared/q_shared.h"
#include "gameshared/q_math.h"
#include "gameshared/q_cvar.h"
#include "gameshared/gs_ref.h"

// few fixes regarding Quake and std compatibility
#ifdef min
	#undef min
#endif
#ifdef max
	#undef max
#endif

// STD
#include <string>
#include <new>
#include <functional>
#include <algorithm>
#include <stdexcept>
#include <utility>
#include <list>
#include "kernel/ui_syscalls.h"

#if defined ( __APPLE__ )
// LLVM GCC 4.2 complains min and max undefined in q_math.c
using std::max;
using std::min;
#endif
