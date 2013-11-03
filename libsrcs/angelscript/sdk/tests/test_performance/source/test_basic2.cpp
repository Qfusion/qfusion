//
// Test author: Andreas Jonsson
//

#include "utils.h"

namespace TestBasic2
{

#define TESTNAME "TestBasic2"

static const char *script =
"void TestBasic2()                      \n"
"{                                      \n"
"    float a = 1, b = 2, c = 3;         \n"
"    int i = 0;                         \n"
"                                       \n"
"    for ( i = 0; i < 10000000; i++ )   \n"
"    {                                  \n"
"       a = a + b * c;                  \n"
"       if( a == 0 )                    \n"
"         a = 100.0f;                   \n"
"       if( b == 1 )                    \n"
"         b = 2;                        \n"
"    }                                  \n"
"}                                      \n";

void Test()
{
	printf("---------------------------------------------\n");
	printf("%s\n\n", TESTNAME);
	printf("AngelScript 2.15.0             : .857 secs\n");
	printf("AngelScript 2.18.0             : .594 secs\n");
	printf("AngelScript 2.18.1 WIP         : .361 secs\n");
	printf("AngelScript 2.19.1 WIP         : .302 secs\n");
	printf("AS 2.20.0 (home)               : .289 secs\n");
	printf("AS 2.20.3 (home)               : .291 secs\n");


	printf("\nBuilding...\n");

 	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	COutStream out;
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(TESTNAME, script, strlen(script), 0);
	engine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true);
	mod->Build();

	asIScriptContext *ctx = engine->CreateContext();
	ctx->Prepare(mod->GetFunctionIdByDecl("void TestBasic2()"));

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


