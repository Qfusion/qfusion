//
// Test author: Andreas Jonsson
//

#include "utils.h"

namespace TestThisProp
{

#define TESTNAME "TestThisProp"

static const char *script =
"void TestThisProp()                    \n"
"{                                      \n"
"  Test t();                            \n"
"  t.Run();                             \n"
"}                                      \n"
"class Test                             \n"
"{                                      \n"
"  float a;                             \n"
"  float b;                             \n" 
"  float c;                             \n"
"  int i;                               \n"
"  void Run()                           \n"
"  {                                    \n"
"    a = 1; b = 2; c = 3;               \n"
"    i = 0;                             \n"
"    for ( i = 0; i < 10000000; i++ )   \n"
"    {                                  \n"
"      a = a + b * c;                   \n"
"      if( a == 0 )                     \n"
"        a = 100.0f;                    \n"
"      if( b == 1 )                     \n"
"        b = 2;                         \n"
"    }                                  \n"
"  }                                    \n"
"}                                      \n";

void Test()
{
	printf("---------------------------------------------\n");
	printf("%s\n\n", TESTNAME);
	printf("AngelScript 2.19.1 WIP (before) : 1.94 secs\n");
	printf("AngelScript 2.19.1 WIP          : .959 secs\n");
	printf("AS 2.20.0 (home)                : .984 secs\n");
	printf("AS 2.20.3 (home)                : .927 secs\n");


	printf("\nBuilding...\n");

 	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	COutStream out;
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(TESTNAME, script, strlen(script), 0);
	engine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true);
	mod->Build();

	asIScriptContext *ctx = engine->CreateContext();
	ctx->Prepare(mod->GetFunctionIdByDecl("void TestThisProp()"));

	printf("Executing AngelScript version...\n");

	double time = GetSystemTimer();

	int r = ctx->Execute();

	time = GetSystemTimer() - time;

	if( r != asEXECUTION_FINISHED )
	{
		printf("Execution didn't terminate with asEXECUTION_FINISHED\n", TESTNAME);
		if( r == asEXECUTION_EXCEPTION )
		{
			printf("Script exception\n");
			asIScriptFunction *func = engine->GetFunctionDescriptorById(ctx->GetExceptionFunction());
			printf("Func: %s\n", func->GetName());
			printf("Line: %d\n", ctx->GetExceptionLineNumber());
			printf("Desc: %s\n", ctx->GetExceptionString());
		}
	}
	else
		printf("Time = %f secs\n", time);

	ctx->Release();
	engine->Release();
}

} // namespace


