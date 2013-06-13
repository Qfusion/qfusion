//
// Test author: Andreas Jonsson
//

#include "utils.h"

namespace TestFib
{

#define TESTNAME "TestFib"

static const char *script =
"int fibR(int n)                      \n"
"{                                    \n"
"    if (n < 2) return n;             \n"
"    return (fibR(n-2) + fibR(n-1));  \n"
"}                                    \n"
"                                     \n"
"int fibI(int n)                      \n"
"{                                    \n"
"    int last = 0;                    \n"
"    int cur = 1;                     \n"
"    --n;                             \n"
"    while(n > 0)                     \n"
"    {                                \n"
"        --n;                         \n"
"        int tmp = cur;               \n"
"        cur = last + cur;            \n"
"        last = tmp;                  \n"
"    }                                \n"
"    return cur;                      \n"
"}                                    \n";

void Test()
{
	printf("---------------------------------------------\n");
	printf("%s\n\n", TESTNAME);
	printf("AngelScript 2.18.1 WIP         : 2.25 secs\n");
	printf("AngelScript 2.19.1 WIP         : 2.09 secs\n");
	printf("AS 2.20.0 (home)               : 2.11 secs\n");
	printf("AS 2.20.3 (home)               : 1.97 secs\n");

	printf("\nBuilding...\n");

 	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	engine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true);

	COutStream out;
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(TESTNAME, script, strlen(script), 0);
	mod->Build();

	asIScriptContext *ctx = engine->CreateContext();

	int fibI = mod->GetFunctionIdByDecl("int fibI(int)");
	int fibR = mod->GetFunctionIdByDecl("int fibR(int)");

	ctx->Prepare(fibR);
	ctx->SetArgDWord(0, 35); // 43

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

	// Verify the result
	int fib = ctx->GetReturnDWord();
	if( fib != 9227465 ) 
		printf("Didn't get the expected fibonacci value, got %d\n", fib);

	ctx->Release();
	engine->Release();
}

} // namespace







