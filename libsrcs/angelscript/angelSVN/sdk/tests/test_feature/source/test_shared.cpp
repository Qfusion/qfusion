#include "utils.h"

namespace TestShared
{

bool Test()
{
	bool fail = false;
	CBufferedOutStream bout;
	asIScriptEngine *engine;
	int r;

	{
		int reg;
 		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);
		engine->RegisterGlobalProperty("int reg", &reg);

		RegisterScriptArray(engine, true);

		engine->RegisterEnum("ESHARED");
		engine->RegisterEnumValue("ESHARED", "ES1", 1);

		asIScriptModule *mod = engine->GetModule("", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("a", 
			"interface badIntf {} \n"
			"shared interface sintf {} \n"
			"shared class T : sintf, badIntf \n" // Allow shared interface, but not non-shared interface
			"{ \n"
			"  void test() \n"
			"  { \n"
			"    var = 0; \n" // Don't allow accessing non-shared global variables
			"    gfunc(); \n" // Don't allow calling non-shared global functions
			"    reg = 1; \n" // Allow accessing registered variables
			"    assert( reg == 1 ); \n" // Allow calling registered functions
			"    badIntf @intf; \n" // Do not allow use of non-shared types in parameters/return type
			"    int cnst = g_cnst; \n" // Allow using literal consts, even if they are declared in global scope
			"    ESHARED es = ES1; \n" // Allow
			"    ENOTSHARED ens = ENS1; \n" // Do not allow. The actual value ENS1 is allowed though, as it is a literal constant
			"    cast<badIntf>(null); \n" // do not allow casting to non-shared types
			"    assert !is null; \n" // Allow taking address of registered functions
			"    gfunc !is null; \n" // Do not allow taking address of non-shared script functions
			"    nonShared(); \n" // Do not allow constructing objects of non-shared type
			"    impfunc(); \n" // Do not allow calling imported function
			"  } \n"
			"  T @dup() const \n" // It must be possible for the shared class to use its own type
			"  { \n"
			"    T d; \n" // Calling the global factory as a shared function
			"    return d; \n" 
			"  } \n"
			"  T() {} \n"
			"  T(int a) \n"
			"  { \n"
			"     var = a; \n" // Constructor of shared class must not access non-shared code
			"  } \n"
			"  void f(badIntf @) {} \n" // Don't allow use of non-shared types in parameters/return type
			"  ESHARED _es; \n" // allow
			"  ENOTSHARED _ens; \n" // Don't allow
			"  void f() \n"
			"  { \n"
			"    array<int> a; \n"
			"  } \n"
			"} \n"
			"int var; \n"
			"void gfunc() {} \n"
			"enum ENOTSHARED { ENS1 = 1 } \n"
			"const int g_cnst = 42; \n"
			"class nonShared {} \n"
			"import void impfunc() from 'mod'; \n"
			);
		bout.buffer = "";
		r = mod->Build();
		if( r >= 0 ) 
			TEST_FAILED;
		if( bout.buffer != "a (31, 3) : Error   : Shared code cannot use non-shared type 'badIntf'\n"
						   "a (3, 25) : Error   : Shared class cannot implement non-shared interface 'badIntf'\n"
						   "a (33, 3) : Error   : Shared code cannot use non-shared type 'ENOTSHARED'\n"
						   "a (5, 3) : Info    : Compiling void T::test()\n"
						   "a (7, 5) : Error   : Shared code cannot access non-shared global variable 'var'\n"
						   "a (8, 5) : Error   : Shared code cannot call non-shared function 'void gfunc()'\n"
						   "a (11, 5) : Error   : Shared code cannot use non-shared type 'badIntf'\n"
						   "a (14, 5) : Error   : Shared code cannot use non-shared type 'ENOTSHARED'\n"
						   "a (15, 5) : Error   : Shared code cannot use non-shared type 'badIntf'\n"
						   "a (17, 5) : Error   : Shared code cannot call non-shared function 'void gfunc()'\n"
						   "a (18, 5) : Error   : Shared code cannot use non-shared type 'nonShared'\n"
						   "a (18, 5) : Error   : Shared code cannot call non-shared function 'nonShared@ nonShared()'\n"
						   "a (19, 5) : Error   : Shared code cannot call non-shared function 'void impfunc()'\n"
						   "a (27, 3) : Info    : Compiling T::T(int)\n"
		                   "a (29, 6) : Error   : Shared code cannot access non-shared global variable 'var'\n" )
		{
			printf("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
		engine->DiscardModule("");

		const char *validCode =
			"shared interface I {} \n"
			"shared class T : I \n"
			"{ \n"
			"  void func() {} \n"
			"  int i; \n"
			"} \n";
		mod = engine->GetModule("1", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("a", validCode);
		bout.buffer = "";
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		int t1 = mod->GetTypeIdByDecl("T");
		if( t1 < 0 )
			TEST_FAILED;

		asIScriptModule *mod2 = engine->GetModule("2", asGM_ALWAYS_CREATE);
		mod2->AddScriptSection("b", validCode);
		r = mod2->Build();
		if( r < 0 )
			TEST_FAILED;

		if( bout.buffer != "" )
		{
			printf("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		int t2 = mod2->GetTypeIdByDecl("T");
		if( t1 != t2 )
			TEST_FAILED;

		CBytecodeStream stream(__FILE__"1");
		mod->SaveByteCode(&stream);

		bout.buffer = "";
		asIScriptModule *mod3 = engine->GetModule("3", asGM_ALWAYS_CREATE);
		r = mod3->LoadByteCode(&stream);
		if( r < 0 )
			TEST_FAILED;

		int t3 = mod3->GetTypeIdByDecl("T");
		if( t1 != t3 )
			TEST_FAILED;
		if( bout.buffer != "" )
		{
			printf("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		bout.buffer = "";
		r = ExecuteString(engine, "T t; t.func();", mod3);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;
		if( bout.buffer != "" )
		{
			printf("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Success
	return fail;
}



} // namespace

