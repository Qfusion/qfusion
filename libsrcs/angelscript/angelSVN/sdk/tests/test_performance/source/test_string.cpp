//
// Test author: Andreas Jonsson
//

#include "utils.h"
#include "scriptstring.h"

namespace TestString
{

#define TESTNAME "TestString"

static const char *script =
"string BuildString(string &in a, string &in b, string &in c)    \n"
"{                                                               \n"
"    return a + b + c;                                           \n"
"}                                                               \n"
"                                                                \n"
"void TestString()                                               \n"
"{                                                               \n"
"    string a = \"Test\";                                        \n"
"    string b = \"string\";                                      \n"
"    int i = 0;                                                  \n"
"                                                                \n"
"    for ( i = 0; i < 1000000; i++ )                             \n"
"    {                                                           \n"
"        string res = BuildString(a, \" \", b);                  \n"
"    }                                                           \n"
"}                                                               \n";

                                         
void Test()
{
	printf("---------------------------------------------\n");
	printf("%s\n\n", TESTNAME);
	printf("AngelScript 2.15.0             : 3.17 secs\n");
	printf("AngelScript 2.18.0             : 3.26 secs\n");
	printf("AngelScript 2.18.1 WIP         : 3.02 secs\n");
	printf("AngelScript 2.19.1 WIP         : 3.22 secs\n");
	printf("AS 2.20.0 (home)               : 4.84 secs\n");
	printf("AS 2.20.3 (home)               : 1.64 secs\n");

	printf("\nBuilding...\n");

 	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	COutStream out;
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);

	RegisterScriptString(engine);

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(TESTNAME, script, strlen(script), 0);
	mod->Build();

	asIScriptContext *ctx = engine->CreateContext();
	ctx->Prepare(mod->GetFunctionIdByDecl("void TestString()"));

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







