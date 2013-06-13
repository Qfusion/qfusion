//
// Test author: Andreas Jonsson
//

#include "utils.h"
#include "scriptstring.h"

namespace TestString2
{

#define TESTNAME "TestString2"

static const char *script =
"string BuildString2(string@ a, string@ b, string@ c)            \n"
"{                                                               \n"
"    return a + b + c;                                           \n"
"}                                                               \n"
"                                                                \n"
"void TestString2()                                              \n"
"{                                                               \n"
"    string a = \"Test\";                                        \n"
"    string b = \" \";                                           \n"
"    string c = \"string\";                                      \n"
"    string res;                                                 \n"
"    int i = 0;                                                  \n"
"                                                                \n"
"    for ( i = 0; i < 1000000; i++ )                             \n"
"    {                                                           \n"
"        res = BuildString2(a, b, c);                            \n"
"    }                                                           \n"
"}                                                               \n";

                                         
void Test()
{
	printf("---------------------------------------------\n");
	printf("%s\n\n", TESTNAME);
	printf("AngelScript 2.15.0             : 1.67 secs\n");
	printf("AngelScript 2.18.0             : 1.71 secs\n");
	printf("AngelScript 2.18.1 WIP         : 1.66 secs\n");
	printf("AngelScript 2.19.1 WIP         : 1.68 secs\n");
	printf("AS 2.20.0 (home)               : 1.97 secs\n");
	printf("AS 2.20.3 (home)               : .873 secs\n");

	printf("\nBuilding...\n");

 	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	COutStream out;
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);

	RegisterScriptString(engine);

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(TESTNAME, script, strlen(script), 0);
	mod->Build();

	asIScriptContext *ctx = engine->CreateContext();
	ctx->Prepare(mod->GetFunctionIdByDecl("void TestString2()"));

	printf("Executing AngelScript version...\n");

	double time = GetSystemTimer();

	int r = ctx->Execute();

	time = GetSystemTimer() - time;

	if( r != 0 )
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







