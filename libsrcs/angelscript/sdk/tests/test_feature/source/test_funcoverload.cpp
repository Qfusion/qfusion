#include "utils.h"

static const char * const TESTNAME = "TestFuncOverload";

static const char *script1 =
"void Test()                               \n"
"{                                         \n"
"  TX.Set(\"user\", TX.Value());           \n"
"}                                         \n";

static const char *script2 =
"void ScriptFunc(void m)                   \n"
"{                                         \n"
"}                                         \n";

class Obj
{
public:
	void *p;
	void *Value() {return p;}
	void Set(const std::string&, void *) {}
};

static Obj o;

void FuncVoid()
{
}

void FuncInt(int v)
{
}

bool Test2();

bool TestFuncOverload()
{
	if( strstr(asGetLibraryOptions(), "AS_MAX_PORTABILITY") )
	{
		printf("%s: Skipped due to AS_MAX_PORTABILITY\n", TESTNAME);
		return false;
	}
	// TODO: Add Test2 again
	bool fail = false; //Test2();
	COutStream out;

	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
	RegisterScriptString(engine);

	engine->RegisterObjectType("Data", sizeof(void*), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE);

	engine->RegisterObjectType("Obj", sizeof(Obj), asOBJ_REF | asOBJ_NOHANDLE);
	engine->RegisterObjectMethod("Obj", "Data &Value()", asMETHOD(Obj, Value), asCALL_THISCALL);
	engine->RegisterObjectMethod("Obj", "void Set(string &in, Data &in)", asMETHOD(Obj, Set), asCALL_THISCALL);
	engine->RegisterObjectMethod("Obj", "void Set(string &in, string &in)", asMETHOD(Obj, Set), asCALL_THISCALL);
	engine->RegisterGlobalProperty("Obj TX", &o);

	engine->RegisterGlobalFunction("void func()", asFUNCTION(FuncVoid), asCALL_CDECL);
	engine->RegisterGlobalFunction("void func(int)", asFUNCTION(FuncInt), asCALL_CDECL);

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(TESTNAME, script1, strlen(script1), 0);
	int r = mod->Build();
	if( r < 0 )
		TEST_FAILED;

	ExecuteString(engine, "func(func(3));", mod);

	CBufferedOutStream bout;
	engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);
	mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(TESTNAME, script2, strlen(script2), 0);
	r = mod->Build();
	if( r >= 0 )
		TEST_FAILED;
	if( bout.buffer != "TestFuncOverload (1, 1) : Info    : Compiling void ScriptFunc(void)\n"
                       "TestFuncOverload (1, 17) : Error   : Parameter type can't be 'void'\n" )
	{
		printf("%s", bout.buffer.c_str());
		TEST_FAILED;
	}

	// Permit void parameter list
	r = engine->RegisterGlobalFunction("void func2(void)", asFUNCTION(FuncVoid), asCALL_CDECL); assert( r >= 0 );

	// Don't permit void parameters
	r = engine->RegisterGlobalFunction("void func3(void n)", asFUNCTION(FuncVoid), asCALL_CDECL); assert( r < 0 );

	engine->Release();

	return fail;
}

// TODO: Implement this support
// (It currently doesn't work because the first argument gives an exact match for another function.
// I need to weigh this limitation against the possibility of increasing multiple matches)
//
// This test verifies that it is possible to find a best match even if the first argument
// may give a better match for another function. Also the order of the function declarations
// should not affect the result.
bool Test2()
{
	bool fail = false;
	COutStream out;
	int r;

	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

	const char *script1 =
		"class A{}  \n"
		"class B{}  \n"
		"void func(A&in, A&in) {} \n"
		"void func(const A&in, const B&in) {} \n"
		"void test()  \n"
		"{ \n"
		"  A a; B b; \n"
		"  func(a,b); \n"
		"}\n";

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	r = mod->AddScriptSection("test", script1, strlen(script1));
	r = mod->Build();
	if( r < 0 )
		TEST_FAILED;

	const char *script2 =
		"class A{}  \n"
		"class B{}  \n"
		"void func(const A&in, const B&in) {} \n"
		"void func(A&in, A&in) {} \n"
		"void test()  \n"
		"{ \n"
		"  A a; B b; \n"
		"  func(a,b); \n"
		"}\n";

	mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	r = mod->AddScriptSection("test", script2, strlen(script2));
	r = mod->Build();
	if( r < 0 )
		TEST_FAILED;

	engine->Release();

	return fail;
}
