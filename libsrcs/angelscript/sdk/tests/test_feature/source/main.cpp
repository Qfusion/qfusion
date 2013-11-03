#include <stdio.h>
#if defined(WIN32) || defined(_WIN64)
#include <conio.h>
#endif
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

#ifdef __dreamcast__
#include <kos.h>

KOS_INIT_FLAGS(INIT_DEFAULT | INIT_MALLOCSTATS);

#endif

namespace MyTest { bool Test(); }

#include "utils.h"

void DetectMemoryLeaks()
{
#if defined(_MSC_VER)
	_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF|_CRTDBG_ALLOC_MEM_DF);
	_CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);

	// Use _CrtSetBreakAlloc(n) to find a specific memory leak
	//_CrtSetBreakAlloc(924);

#endif
}

extern "C" void BreakPoint()
{
	printf("Breakpoint\n");
}

//----------------------------------
// Test with these flags as well
//
// + AS_MAX_PORTABILITY
//----------------------------------

int main(int argc, char **argv)
{
	DetectMemoryLeaks();

	printf("AngelScript version: %s\n", asGetLibraryVersion());
	printf("AngelScript options: %s\n", asGetLibraryOptions());

#ifdef __dreamcast__
	fs_chdir(asTestDir);
#endif

	InstallMemoryManager();

	if( MyTest::Test() ) goto failed; else printf("MyTest::Test passed\n");

	RemoveMemoryManager();

//succeed:
	printf("--------------------------------------------\n");
	printf("All of the tests passed with success.\n\n");
#if !defined(DONT_WAIT) && (defined(WIN32) || defined(_WIN64))
	printf("Press any key to quit.\n");
	while(!_getch());
#endif
	return 0;

failed:
	printf("--------------------------------------------\n");
	printf("One of the tests failed, see details above.\n\n");
#if !defined(DONT_WAIT) && (defined(WIN32) || defined(_WIN64))
	printf("Press any key to quit.\n");
	while(!_getch());
#endif
	return -1;
}
