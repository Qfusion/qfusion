#include <stdio.h>
#if defined(WIN32)
#include <conio.h>
#endif
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif
#include "angelscript.h"

namespace TestBasic        { void Test(); }
namespace TestBasic2       { void Test(); }
namespace TestCall         { void Test(); }
namespace TestCall2        { void Test(); }
namespace TestFib          { void Test(); }
namespace TestInt          { void Test(); }
namespace TestIntf         { void Test(); }
namespace TestMthd         { void Test(); }
namespace TestString       { void Test(); }
namespace TestStringPooled { void Test(); }
namespace TestString2      { void Test(); }
namespace TestThisProp     { void Test(); }
namespace TestVector3      { void Test(); }
namespace TestAssign       { void Test(); }

void DetectMemoryLeaks()
{
#if defined(_MSC_VER)
	_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF|_CRTDBG_ALLOC_MEM_DF);
	_CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
}

int main(int argc, char **argv)
{
	DetectMemoryLeaks();

	printf("Performance test");
#ifdef _DEBUG 
	printf(" (DEBUG)");
#endif
	printf("\n");
	printf("AngelScript %s\n", asGetLibraryVersion()); 

	TestBasic::Test();
	TestBasic2::Test();
	TestCall::Test();
	TestCall2::Test();
	TestFib::Test();
	TestInt::Test();
	TestIntf::Test();
	TestMthd::Test();
	TestString::Test();
	TestString2::Test();
	TestStringPooled::Test();
	TestThisProp::Test();
	TestVector3::Test();
	TestAssign::Test();
	
	printf("--------------------------------------------\n");
	printf("Press any key to quit.\n");
#if defined(WIN32)
	while(!getch());
#endif
	return 0;
}
