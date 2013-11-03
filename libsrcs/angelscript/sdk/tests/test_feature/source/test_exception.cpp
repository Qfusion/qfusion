#include "utils.h"
using namespace std;

static const char * const TESTNAME = "TestException";

// This script will cause an exception inside a class method
const char *script1 =
"class A               \n"
"{                     \n"
"  void Test(string c) \n"
"  {                   \n"
"    int a = 0, b = 0; \n"
"    a = a/b;          \n"
"  }                   \n"
"}                     \n";

static void print(asIScriptGeneric *gen)
{
	std::string *s = (std::string*)gen->GetArgAddress(0);
	UNUSED_VAR(s);
}

bool TestException()
{
	bool fail = false;
	int r;

	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

	COutStream out;
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);

	RegisterScriptString(engine);
	engine->RegisterGlobalFunction("void print(const string &in)", asFUNCTION(print), asCALL_GENERIC);


	asIScriptContext *ctx = engine->CreateContext();
	r = ExecuteString(engine, "int a = 0;\na = 10/a;", 0, ctx); // Throws an exception
	if( r == asEXECUTION_EXCEPTION )
	{
		int func = ctx->GetExceptionFunction();
		int line = ctx->GetExceptionLineNumber();
		const char *desc = ctx->GetExceptionString();

		const asIScriptFunction *function = engine->GetFunctionById(func);
		if( strcmp(function->GetName(), "ExecuteString") != 0 )
		{
			printf("%s: Exception function name is wrong\n", TESTNAME);
			TEST_FAILED;
		}
		if( strcmp(function->GetDeclaration(), "void ExecuteString()") != 0 )
		{
			printf("%s: Exception function declaration is wrong\n", TESTNAME);
			TEST_FAILED;
		}

		if( line != 2 )
		{
			printf("%s: Exception line number is wrong\n", TESTNAME);
			TEST_FAILED;
		}
		if( strcmp(desc, "Divide by zero") != 0 )
		{
			printf("%s: Exception string is wrong\n", TESTNAME);
			TEST_FAILED;
		}
	}
	else
	{
		printf("%s: Failed to raise exception\n", TESTNAME);
		TEST_FAILED;
	}

	ctx->Release();

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection("script", script1, strlen(script1));
	mod->Build();
	r = ExecuteString(engine, "A a; a.Test(\"test\");", mod);
	if( r != asEXECUTION_EXCEPTION )
	{
		TEST_FAILED;
	}

	// A test to validate Unprepare without execution
	{
		asIObjectType *type = mod->GetObjectTypeByIndex(0);
		int funcId = type->GetMethodIdByDecl("void Test(string c)");
		ctx = engine->CreateContext();
		ctx->Prepare(funcId);
		asIScriptContext *obj = (asIScriptContext*)engine->CreateScriptObject(type->GetTypeId());
		ctx->SetObject(obj); // Just sets the address
		CScriptString *str = new CScriptString();
		ctx->SetArgObject(0, str); // Makes a copy of the object
		str->Release();
		ctx->Unprepare(); // Must release the string argument, but not the object
		ctx->Release();
		obj->Release();
	}

	// A test to verify behaviour when exception occurs in script class constructor
	const char *script2 = "class SomeClassA \n"
	"{ \n"
	"	int A; \n"
	" \n"
	"	~SomeClassA() \n"
	"	{ \n"
	"		print('destruct'); \n"
	"	} \n"
	"} \n"
	"class SomeClassB \n"
	"{ \n"
	"	SomeClassA@ nullptr; \n"
	"	SomeClassB(SomeClassA@ aPtr) \n"
	"	{ \n"
	"		this.nullptr.A=100; // Null pointer access. After this class a is destroyed. \n"
	"	} \n"
	"} \n"
	"void test() \n"
	"{ \n"
	"	SomeClassA a; \n"
	"	SomeClassB(a); \n"
	"} \n";
	mod->AddScriptSection("script2", script2);
	r = mod->Build();
	if( r < 0 ) TEST_FAILED;
	r = ExecuteString(engine, "test()", mod);
	if( r != asEXECUTION_EXCEPTION )
	{
		TEST_FAILED;
	}

	engine->GarbageCollect();

	engine->Release();

	// Success
	return fail;
}
