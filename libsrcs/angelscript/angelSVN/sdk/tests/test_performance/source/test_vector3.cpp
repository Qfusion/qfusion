//
// Test author: Andreas Jonsson
//

#include "utils.h"
#include "../../add_on/scriptmath3d/scriptmath3d.h"

namespace TestVector3
{

#define TESTNAME "TestVector3"

static const char *script =
"void TestVector3()                                              \n"
"{                                                               \n"
"    for ( uint i = 0; i < 1000000; i++ )                        \n"
"    {                                                           \n"
"        vector3 a, b, c;                                        \n"
"        a = b + c;                                              \n"
"        b = a*2;                                                \n"
"    }                                                           \n"
"}                                                               \n";

                                         
void Test()
{
	printf("---------------------------------------------\n");
	printf("%s\n\n", TESTNAME);
	// If this test is run alone, it gets 0.631 secs. I believe this has to do with
	// the memory allocations for the return type that are faster due to less fractioned memory
	printf("AS 2.20.1 (home)               : 1.65 secs\n");
	printf("AS 2.20.3 (home)               : .511 secs\n");


	printf("\nBuilding...\n");

 	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	COutStream out;
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);

	RegisterScriptMath3D(engine);

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(TESTNAME, script, strlen(script), 0);
	mod->Build();

	asIScriptContext *ctx = engine->CreateContext();
	ctx->Prepare(mod->GetFunctionIdByDecl("void TestVector3()"));

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







