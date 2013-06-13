//
// Test author: Andreas Jonsson
//

#include "utils.h"
#include <string>
using std::string;

namespace TestBasic
{

#define TESTNAME "TestBasic"

static const char *scriptBegin =
"void main()                                                 \n"
"{                                                           \n"
"   int[] arr(2);                                            \n"
"   int[][] PWToGuild(26);                                   \n"
"   string test;                                             \n";

static const char *scriptMiddle = 
"   arr[0] = 121; arr[1] = 196; PWToGuild[0] = arr; test = '%d';  \n";

static const char *scriptEnd =
"}                                                           \n";


void Test()
{
	printf("---------------------------------------------\n");
	printf("%s\n\n", TESTNAME);
	printf("Machine 1\n");
	printf("AngelScript 1.10.1 WIP 1: ??.?? secs\n");
	printf("\n");
	printf("Machine 2\n");
	printf("AngelScript 1.10.1 WIP 1: 9.544 secs\n");
	printf("AngelScript 1.10.1 WIP 2: .6949 secs\n");

	printf("\nBuilding...\n");

 	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

	COutStream out;
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);

	RegisterScriptArray(engine, true);
	RegisterStdString(engine);

	string script = scriptBegin;
	for( int n = 0; n < 40000; n++ )
	{
		char buf[500];
		sprintf(buf, scriptMiddle, n);
		script += buf;
	}
	script += scriptEnd;

	double time = GetSystemTimer();

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(TESTNAME, script.c_str(), script.size(), 0);
	int r = mod->Build();

	time = GetSystemTimer() - time;

	if( r != 0 )
		printf("Build failed\n", TESTNAME);
	else
		printf("Time = %f secs\n", time);

	engine->Release();
}

} // namespace



