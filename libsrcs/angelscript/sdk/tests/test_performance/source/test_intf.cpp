//
// Test author: Andreas Jonsson
//

#include "utils.h"

namespace TestIntf
{

#define TESTNAME "TestIntf"

static const char *script =
"interface intf                                                  \n"
"{                                                               \n"
"    void func0();                                               \n"
"    void func1(int);                                            \n"
"    void func2(int,int);                                        \n"
"    void func3(int,int,int);                                    \n"
"    void func4(int,int,int,int);                                \n"
"    void func5(int,int,int,int,int);                            \n"
"    void func6(int,int,int,int,int,int);                        \n"
"    void func7(int,int,int,int,int,int,int);                    \n"
"    void func8(int,int,int,int,int,int,int,int);                \n"
"    void func9(int,int,int,int,int,int,int,int,int);            \n"
"}                                                               \n"
"class cls : intf                                                \n"
"{                                                               \n"
"    void func0()                                    {}          \n"
"    void func1(int)                                 {}          \n"
"    void func2(int,int)                             {}          \n"
"    void func3(int,int,int)                         {}          \n"
"    void func4(int,int,int,int)                     {}          \n"
"    void func5(int,int,int,int,int)                 {}          \n"
"    void func6(int,int,int,int,int,int)             {}          \n"
"    void func7(int,int,int,int,int,int,int)         {}          \n"
"    void func8(int,int,int,int,int,int,int,int)     {}          \n"
"    void func9(int,int,int,int,int,int,int,int,int) {}          \n"
"}                                                               \n"
"                                                                \n"
"void TestIntf()                                                 \n"
"{                                                               \n"
"    cls obj;                                                    \n"
"    intf @i = obj;                                              \n"
"                                                                \n"
"    for( int n = 0; n < 1000000; n++ )                          \n"
"    {                                                           \n"
"        i.func0();                                              \n"
"        i.func1(1);                                             \n"
"        i.func2(1,2);                                           \n"
"        i.func3(1,2,3);                                         \n"
"        i.func4(1,2,3,4);                                       \n"
"        i.func5(1,2,3,4,5);                                     \n"
"        i.func6(1,2,3,4,5,6);                                   \n"
"        i.func7(1,2,3,4,5,6,7);                                 \n"
"        i.func8(1,2,3,4,5,6,7,8);                               \n"
"        i.func9(1,2,3,4,5,6,7,8,9);                             \n"
"    }                                                           \n"
"}                                                               \n";

                                         
void Test()
{
	printf("---------------------------------------------\n");
	printf("%s\n\n", TESTNAME);
	printf("AngelScript 2.15.0             : 2.63 secs\n");
	printf("AngelScript 2.18.0             : 2.51 secs\n");
	printf("AngelScript 2.18.1 WIP         : 2.34 secs\n");
	printf("AngelScript 2.19.1 WIP         : 2.25 secs\n");
	printf("AS 2.20.0 (home)               : 1.72 secs\n");
	printf("AS 2.20.3 (home)               : 1.60 secs\n");

	printf("\nBuilding...\n");

 	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	COutStream out;
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(TESTNAME, script, strlen(script), 0);
	mod->Build();

	asIScriptContext *ctx = engine->CreateContext();
	ctx->Prepare(mod->GetFunctionIdByDecl("void TestIntf()"));

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







