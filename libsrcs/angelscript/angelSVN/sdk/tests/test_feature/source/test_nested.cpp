#include "utils.h"
using namespace std;

static const char * const TESTNAME = "TestNested";

static const char *script1 =
"void TestNested()                         \n"
"{                                         \n"
"  CallExecuteString(\"i = 2\");           \n"
"  i = i + 2;                              \n"
"}                                         \n";

static void CallExecuteString(string &str)
{
	asIScriptContext *ctx = asGetActiveContext();
	asIScriptEngine *engine = ctx->GetEngine();
	if( ExecuteString(engine, str.c_str()) < 0 )
		ctx->SetException("ExecuteString() failed\n");
}

static void CallExecuteString_gen(asIScriptGeneric *gen)
{
	string str = ((CScriptString*)gen->GetArgAddress(0))->buffer;
	CallExecuteString(str);
}

static int i = 0;

bool TestNested()
{
	bool fail = false;

	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

	RegisterScriptString_Generic(engine);

	engine->RegisterGlobalProperty("int i", &i);
	if( strstr(asGetLibraryOptions(), "AS_MAX_PORTABILITY") )
		engine->RegisterGlobalFunction("void CallExecuteString(string &in)", asFUNCTION(CallExecuteString_gen), asCALL_GENERIC);
	else
		engine->RegisterGlobalFunction("void CallExecuteString(string &in)", asFUNCTION(CallExecuteString), asCALL_CDECL);

	COutStream out;	

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(TESTNAME, script1, strlen(script1), 0);
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
	mod->Build();

	// Make the call with a separate context (should work)
	asIScriptContext *ctx = engine->CreateContext();
	ctx->Prepare(engine->GetModule(0)->GetFunctionIdByIndex(0));
	ctx->Execute();

	if( i != 4 )
	{
		printf("%s: Failed to call nested ExecuteString() from other context\n", TESTNAME);
		TEST_FAILED;
	}

	ctx->Release();

	// Make the call with ExecuteString 
	i = 0;
	int r = ExecuteString(engine, "TestNested()", mod);
	if( r != asEXECUTION_FINISHED )
	{
		printf("%s: ExecuteString() didn't succeed\n", TESTNAME);
		TEST_FAILED;
	}

	if( i != 4 )
	{
		printf("%s: Failed to call nested ExecuteString() from ExecuteString()\n", TESTNAME);
		TEST_FAILED;
	}

	engine->Release();

	// Success
	return fail;
}
