//
// Test author: Andreas Jonsson
//

#include "utils.h"

namespace TestCall2
{

#define TESTNAME "TestCall2"

static const char *script =
"void TestCall2_A()                                               \n"
"{                                                                \n"
"}                                                                \n"
"void TestCall2_B()                                               \n"
"{                                                                \n"
"}                                                                \n";


                                         
void Test()
{
	printf("---------------------------------------------\n");
	printf("%s\n\n", TESTNAME);
	printf("AngelScript 2.15.0             : 2.51 secs\n");
	printf("AngelScript 2.18.0             : 2.01 secs\n");
	printf("AngelScript 2.18.1 WIP         : 1.89 secs\n");
	printf("AngelScript 2.19.1 WIP         : 1.89 secs\n");
	printf("AS 2.20.0 (home)               : 1.65 secs\n");
	printf("AS 2.20.3 (home)               : 1.66 secs\n");

	printf("\nBuilding...\n");

 	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	COutStream out;
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(TESTNAME, script, strlen(script), 0);
	mod->Build();

	asIScriptContext *ctx = engine->CreateContext();
	int funcId_A = mod->GetFunctionIdByDecl("void TestCall2_A()");
	int funcId_B = mod->GetFunctionIdByDecl("void TestCall2_B()");

	printf("Executing AngelScript version...\n");

	double time = GetSystemTimer();
	int r;

	for( int n = 0; n < 5000000; n++ )
	{
		ctx->Prepare(funcId_A);
		r = ctx->Execute();
		if( r != 0 ) break;
		ctx->Prepare(funcId_B);
		r = ctx->Execute();
		if( r != 0 ) break;
	}

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







