#include <windows.h>

// video drivers pick these up and make sure the game runs on the good GPU
__declspec( dllexport ) DWORD NvOptimusEnablement = 1;
__declspec( dllexport ) int AmdPowerXpressRequestHighPerformance = 1;
