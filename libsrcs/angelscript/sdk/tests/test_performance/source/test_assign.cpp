//
// Test author: Andreas Jonsson
//

#include "utils.h"
#include "../../add_on/scriptmath3d/scriptmath3d.h"

namespace TestAssign
{

#define TESTNAME "TestAssign"

static const char *script =
"void test1() \n"
"{ \n"
"  float x = 0; \n"
"  float y = 0; \n"
"  float xx = 1; \n"
"  float yy = 2; \n"
"  for(int i=0;i<100000;++i) \n"
"  { \n"
"    x = xx; \n"
"    y = yy; \n"
"    xx = x; \n"
"    yy = y; \n"
"  } \n"
"} \n"
"void test2() \n"
"{ \n"
"  vector2 a; \n"
"  vector2 b; \n"
"  for(int i=0;i<100000;++i) \n"
"  { \n"
"    a.x = b.x; \n"
"    b.x = a.x; \n"
"    a.y = b.y; \n"
"    b.y = a.y; \n"
"  } \n"
"} \n"
"void test3() \n"
"{ \n"
"  vector2 a; \n"
"  vector2 b; \n"
"  for(int i=0;i<100000;++i) \n"
"  { \n"
"    a = b; \n"
"    b = a; \n"
"  } \n"
"} \n"
"class test  \n"
"{  \n"
"  vector2 a;  \n"
"  vector2 b;  \n"
"  void run()  \n"
"  {  \n"
"    for(int i=0;i<100000;++i)  \n"
"    {  \n"
"      a = b;  \n"
"      b = a;  \n"
"    }  \n"
"  }  \n"
"} \n"
"void test4() \n"
"{ \n"
"  test t; t.run(); \n"
"} \n"
"class V \n"
"{ \n"
"  vector2 v; \n"
"} \n"
"void test5() \n"
"{ \n"
"  V a; \n"
"  V b; \n"
"  for(int i=0;i<100000;++i) \n"
"  { \n"
"    a.v = b.v; \n"
"    b.v = a.v; \n"
"  } \n"
"} \n";

                                       
void Test()
{
	printf("---------------------------------------------\n");
	printf("%s\n\n", TESTNAME);
	printf("AS 2.20.3 (home)               : .00665 secs\n");
	printf("Original...\n");
	printf("Time = 0.010541 secs\n"
		   "Time = 0.024077 secs\n"
		   "Time = 0.009444 secs\n"
		   "Time = 0.022281 secs\n"
		   "Time = 0.019826 secs\n");
	printf("Current...\n");
	printf("Time = 0.004959 secs\n"
		   "Time = 0.010941 secs\n"
		   "Time = 0.004622 secs\n"
		   "Time = 0.009473 secs\n"
		   "Time = 0.009446 secs\n");

	printf("\nBuilding...\n");

 	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	COutStream out;
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);

	int r;
	r = engine->RegisterObjectType("vector2", sizeof(float)*2, asOBJ_VALUE | asOBJ_POD); assert( r >= 0 );
	r = engine->RegisterObjectProperty("vector2", "float x", 0); assert( r >= 0 );
	r = engine->RegisterObjectProperty("vector2", "float y", 4); assert( r >= 0 );

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(TESTNAME, script, strlen(script), 0);
	mod->Build();

	double time;
	asIScriptContext *ctx = engine->CreateContext();

	printf("Executing AngelScript version...\n");

	ctx->Prepare(mod->GetFunctionIdByDecl("void test1()"));
	time = GetSystemTimer();
	r = ctx->Execute();
	time = GetSystemTimer() - time;
	printf("Time = %f secs\n", time);

	// TODO: optimize: PSF, ADDSi, PopRPtr -> LoadObjR
	ctx->Prepare(mod->GetFunctionIdByDecl("void test2()"));
	time = GetSystemTimer();
	r = ctx->Execute();
	time = GetSystemTimer() - time;
	printf("Time = %f secs\n", time);

	ctx->Prepare(mod->GetFunctionIdByDecl("void test3()"));
	time = GetSystemTimer();
	r = ctx->Execute();
	time = GetSystemTimer() - time;
	printf("Time = %f secs\n", time);

	// TODO: optimize: This will benefit from when value types are inlined in script classes
	ctx->Prepare(mod->GetFunctionIdByDecl("void test4()"));
	time = GetSystemTimer();
	r = ctx->Execute();
	time = GetSystemTimer() - time;
	printf("Time = %f secs\n", time);

	// TODO: optimize: This will benefit from when value types are inlined in script classes
	ctx->Prepare(mod->GetFunctionIdByDecl("void test5()"));
	time = GetSystemTimer();
	r = ctx->Execute();
	time = GetSystemTimer() - time;
	printf("Time = %f secs\n", time);

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

	ctx->Release();
	engine->Release();
}

} // namespace







