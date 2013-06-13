#include "utils.h"

using namespace std;

namespace TestDefaultArg
{

bool Test()
{
	bool fail = false;
	int r;
	CBufferedOutStream bout;
	COutStream out;
	asIScriptModule *mod;
 	asIScriptEngine *engine;
	
	// Test calling a function with default argument
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
		RegisterScriptString(engine);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);

		const char *script =
			"void func(int b, const string &in a = 'default') \n"
			"{ \n"
			"  if( b == 0 ) \n"
			"    assert( a == 'default' ); \n"
			"  else  \n"
			"    assert( a == 'test' ); \n"
			"} \n" 
			"void main() \n"
			"{ \n"
			"  func(0); \n"
			"  func(0, 'default'); \n"
			"  func(1, 'test'); \n"
			"} \n";

		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	// Must be possible to register functions with default args as well
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
		r = engine->RegisterGlobalFunction("void defarg(bool, int a = 34 + /* comments will be removed */ 45, int b = 23)", asFUNCTION(0), asCALL_GENERIC);
		if( r < 0 )
			TEST_FAILED;
		asIScriptFunction *func = engine->GetFunctionById(r);
		string decl = func->GetDeclaration();
		if( decl != "void defarg(bool, int arg1 = 34 + 45, int arg2 = 23)" )
		{
			printf("%s\n", decl.c_str());
			TEST_FAILED;
		}
		engine->Release();
	}

	// When default arg is used, all other args after that must have default args
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);
		r = engine->RegisterGlobalFunction("void defarg(bool, int a = 34+45, int)", asFUNCTION(0), asCALL_GENERIC);
		if( r >= 0 )
			TEST_FAILED;
		if( bout.buffer != "System function (1, 1) : Error   : All subsequent parameters after the first default value must have default values in function 'void defarg(bool, int arg1 = 34 + 45, int)'\n"
			               " (0, 0) : Error   : Failed in call to function 'RegisterGlobalFunction' with 'void defarg(bool, int a = 34+45, int)'\n" )
		{
			printf("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
		engine->Release();
	}

	// Shouldn't be possible to write default arg expressions that access local variables, globals are ok though
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);

		const char *script =
			"void func(int a = n) {} \n"
			"void main() \n"
			"{ \n"
			"  int n; \n"
			"  func(); \n"
			"} \n";

		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;

		// TODO: The first line in the error message should show the real script name
		if( bout.buffer != "default arg (2, 1) : Info    : Compiling void main()\n"
		                   "default arg (1, 1) : Error   : 'n' is not declared\n"
		                   "script (5, 3) : Error   : Failed while compiling default arg for parameter 0 in function 'void func(int arg0 = n)'\n" )
		{
			printf("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Default args in script class constructors
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);

		const char *script =
			"class T \n"
			"{ \n"
			"  T(int a, int b = 25) \n"
			"  { \n"
			"    assert(a == 10); \n"
			"    assert(b == 25); \n"
			"  } \n"
			"} \n" 
			"T g(10); \n"
			"void main() \n"
			"{ \n"
			"  T(10); \n"
			"  T l(10); \n"
			"} \n";

		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	// Default arg must not end up using variables that are used 
	// in previously compiled variables as temporaries
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);
		RegisterStdString(engine);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);

		const char *script =
			"void func(uint8 a, string b = 'b') \n"
			"{ \n"
			"  assert( a == 97 ); \n"
			"  assert( b == 'b' ); \n"
			"} \n" 
			"void main() \n"
			"{ \n"
			"  uint8 a; \n"
			"  func(a = 'a'[0]); \n"
			"} \n";

		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	// Shouldn't crash if attempting to call incorrect function
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);

		const char *script =
			"void myFunc( float f, int a=0, int b ) {} \n"
			"void main() \n"
			"{ \n"
			"  int n; \n"
			"  myFunc( 1.2, 6 ); \n"
			"} \n";

		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != "script (1, 1) : Error   : All subsequent parameters after the first default value must have default values in function 'void myFunc(float, int arg1 = 0, int)'\n" )
		{
			printf("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}


	// The test to make sure the saved bytecode keeps the default args is done in test_saveload.cpp
	// A test to make sure script class methods with default args work is done in test_saveload.cpp

	// TODO: The compilation of the default args must not add any LINE instructions in the byte code, because they wouldn't match the real script

	// Success
	return fail;
}



} // namespace

