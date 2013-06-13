//
// Tests releasing a suspended context
//
// Test author: Andreas Jonsson
//

#include "utils.h"

namespace TestSuspend
{

static const char * const TESTNAME = "TestSuspend";

static int loopCount = 0;

static const char *script1 =
"string g_str = \"test\";    \n" // variable that must be released when module is released
"void TestSuspend()          \n"
"{                           \n"
"  string str = \"hello\";   \n" // variable that must be released before exiting the function
"  while( true )             \n" // never ending loop
"  {                         \n"
"    string a = str + g_str; \n" // variable inside the loop
"    Suspend();              \n"
"    loopCount++;            \n"
"  }                         \n"
"}                           \n";

static const char *script2 = 
"void TestSuspend2()         \n"
"{                           \n"
"  loopCount++;              \n"
"  loopCount++;              \n"
"  loopCount++;              \n"
"}                           \n";

bool doSuspend = false;
void Suspend(asIScriptGeneric * /*gen*/)
{
	asIScriptContext *ctx = asGetActiveContext();
	if( ctx ) ctx->Suspend();
	doSuspend = true;
}

bool doAbort = true;
void Abort(asIScriptGeneric *)
{
	asIScriptContext *ctx = asGetActiveContext();
	if( ctx ) ctx->Abort();
	doAbort = true;
}

void STDCALL LineCallback(asIScriptContext *ctx, void * /*param*/)
{
	// Suspend immediately
	ctx->Suspend();
}

bool Test()
{
	bool fail = false;

	//---
 	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	
	// Verify that the function doesn't crash when the stack is empty
	asIScriptContext *ctx = asGetActiveContext();
	assert( ctx == 0 );

	RegisterScriptString_Generic(engine);
	
	engine->RegisterGlobalFunction("void Suspend()", asFUNCTION(Suspend), asCALL_GENERIC);
	engine->RegisterGlobalFunction("void Abort()", asFUNCTION(Abort), asCALL_GENERIC);
	engine->RegisterGlobalProperty("int loopCount", &loopCount);

	COutStream out;
	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(":1", script1);

	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
	mod->Build();

	ctx = engine->CreateContext();
	ctx->SetLineCallback(asFUNCTION(LineCallback), 0, asCALL_STDCALL);
	if( ctx->Prepare(mod->GetFunctionIdByDecl("void TestSuspend()")) >= 0 )
	{
		while( loopCount < 5 && !doSuspend )
			ctx->Execute();
	}
	else
		TEST_FAILED;

	// Make sure the Execute method returns proper status on abort
	int r = ExecuteString(engine, "Abort()", 0, 0);
	if( r != asEXECUTION_ABORTED )
	{
		TEST_FAILED;
	}

	// Release the engine first
	engine->Release();

	// Now release the context
	ctx->Release();
	//---
	// If the library was built with the flag BUILD_WITH_LINE_CUES the script
	// will return after each increment of the loopCount variable.
	engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	engine->RegisterGlobalProperty("int loopCount", &loopCount);
	mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(":2", script2);
	mod->Build();

	ctx = engine->CreateContext();
	ctx->SetLineCallback(asFUNCTION(LineCallback), 0, asCALL_STDCALL);
	ctx->Prepare(engine->GetModule(0)->GetFunctionIdByDecl("void TestSuspend2()"));
	loopCount = 0;
	while( ctx->GetState() != asEXECUTION_FINISHED )
		ctx->Execute();
	if( loopCount != 3 )
	{
		printf("%s: failed\n", TESTNAME);
		TEST_FAILED;
	}

	ctx->Prepare(asPREPARE_PREVIOUS);
	loopCount = 0;
	while( ctx->GetState() != asEXECUTION_FINISHED )
		ctx->Execute();
	if( loopCount != 3 )
	{
		printf("%s: failed\n", TESTNAME);
		TEST_FAILED;
	}

	ctx->Release();
	engine->Release();

	// Success
	return fail;
}

} // namespace

