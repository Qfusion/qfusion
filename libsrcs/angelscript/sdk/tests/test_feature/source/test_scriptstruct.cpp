#include "utils.h"

namespace TestScriptStruct
{

static const char * const TESTNAME = "TestScriptStruct";

// Normal structure
static const char *script1 =
"class Test                   \n"
"{                            \n"
"   int a;                    \n"
"   bool b;                   \n"
"};                           \n"
"void TestStruct()            \n"
"{                            \n"
"   Test a;                   \n"
"   a.a = 3;                  \n"
"   a.b = false;              \n"
"   Test b;                   \n"
"   Test @c = @a;             \n"
"   a = b;                    \n"
"   TestStruct2(c);           \n"
"   Test[] d(1);              \n"
"   d[0] = a;                 \n"
"   a = Test();               \n"
"}                            \n"
"void TestStruct2(Test a)     \n"
"{                            \n"
"}                            \n";

// Do not allow const in properties
static const char *script2 = 
"class Test                   \n"
"{                            \n"
"   const int a;              \n"
"};                           \n";

// Test arrays in struct
static const char *script3 =
"class Test                   \n"
"{                            \n"
"   int[] a;                  \n"
"};                           \n"
"class Test2                  \n"
"{                            \n"
"   Test2@[][] a;             \n"
"};                           \n"
"void TestArrayInStruct()     \n"
"{                            \n"
"   Test a;                   \n"
"   a.a.resize(10);           \n"
"   Test2 b;                  \n"
"   b.a.resize(1);            \n"
"   b.a[0].resize(1);         \n"
"   // Circular reference     \n"
"   @b.a[0][0] = b;           \n"
"}                            \n";

// Only allow primitives (at first)
static const char *script4 =
"class B                      \n"
"{                            \n"
"   A a;                      \n"
"   string b;                 \n"
"   int c;                    \n"
"};                           \n"
"void Test()                  \n"
"{                            \n"
"  B a, b;                    \n"
"  b.a.a = 5;                 \n"
"  b.b = \"Test\";            \n"
"  b.c = 6;                   \n"
"  a = b;                     \n"
"  b.a.a = 6;                 \n"
"  b.b = \"1\";               \n"
"  b.c = 2;                   \n"
"  Assert(a.a.a == 5);        \n"
"  Assert(a.b == \"Test\");   \n"
"  Assert(a.c == 6);          \n"
"}                            \n"
"class A                      \n"
"{                            \n"
"   uint a;                   \n"
"};                           \n";

// Verify that the struct names cannot conflict with one another
static const char *script5 = 
"class A {};                  \n"
"class A {};                  \n"
"class B                      \n"
"{                            \n"
"  int a;                     \n"
"  float a;                   \n"
"};                           \n";

// Verify that a structure cannot have itself as local member (directly or indirectly)
static const char *script6 = 
"class A                      \n"
"{                            \n"
"  A a;                       \n"
"};                           \n"
"class B                      \n"
"{                            \n"
"  C c;                       \n"
"};                           \n"
"class C                      \n"
"{                            \n"
"  B b;                       \n"
"};                           \n";

static const char *script7 =
"class A                      \n"
"{                            \n"
"  string@ s;                 \n"
"};                           \n"
"void TestHandleInStruct()    \n"
"{                            \n"
"  A a;                       \n"
"  Assert(@a.s == null);      \n"
"  a = a;                     \n"
"  @a.s = \"Test\";           \n"
"  Assert(a.s == \"Test\");   \n"
"}                            \n";

// Verify that circular references are handled by the GC
static const char *script8 = 
"class A                      \n"
"{                            \n"
"  A@ next;                   \n"
"};                           \n"
"class B                      \n"
"{                            \n"
"  D@ next;                   \n"
"};                           \n"
"class C                      \n"
"{                            \n"
"  B b;                       \n"
"};                           \n"
"class D                      \n"
"{                            \n"
"  C c;                       \n"
"};                           \n"
"void TestHandleInStruct2()   \n"
"{                            \n"
// Simple circular reference
"  A a;                       \n"
"  @a.next = a;               \n"
// More complex circular reference
"  D d1;                      \n"
"  D d2;                      \n"
"  @d1.c.b.next = d2;         \n"
"  @d2.c.b.next = d1;         \n"
"}                            \n";


static const char *script9 = 
"class MyStruct               \n"
"{                            \n"
"  uint myBits;               \n"
"};                           \n"
"uint MyFunc(uint a)          \n"
"{                            \n"
"  return a;                  \n"
"}                            \n"
"void MyFunc(string@) {}      \n"
"void Test()                  \n"
"{                            \n"
"  uint val = 0x0;            \n"
"  MyStruct s;                \n"
"  s.myBits = 0x5;            \n"
"  val = MyFunc(s.myBits);    \n"
"}                            \n";

// Don't allow arrays of the struct type as members (unless it is handles)
static const char *script10 = 
"class Test2                  \n"
"{                            \n"
"   Test2[] a;                \n"
"};                           \n";

// Test array constness in members
static const char *script11 = 
"class A                      \n"
"{                            \n"
"   int[] a;                  \n"
"};                           \n"
"void Test()                  \n"
"{                            \n"
"   const A a;                \n"
"   // Should not compile     \n"
"   a.a[0] = 23;              \n"
"}                            \n";

// Test order independence with declarations
static const char *script12 =
"A Test()                     \n"
"{                            \n"
"  A a;                       \n"
"  return a;                  \n"
"}                            \n";

static const char *script13 =
"class A                      \n"
"{                            \n"
"  B b;                       \n"
"};                           \n"
"class B                      \n"
"{                            \n"
"  int val;                   \n"
"};                           \n";

static const char *script14 =
"class A                     \n"
"{                           \n"
"  B @b;                     \n"
"}                           \n"
"class B                     \n"
"{                           \n"
"  int val;                  \n"
"}                           \n";


bool Test2();

bool Test()
{
	bool fail = Test2();
	int r;

	asIScriptModule *mod = 0;

	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	RegisterScriptArray(engine, true);
	RegisterScriptString(engine);

	engine->RegisterGlobalFunction("void Assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

	COutStream out;
	CBufferedOutStream bout;
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);

	mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(TESTNAME, script1);
	r = mod->Build();
	if( r < 0 ) TEST_FAILED;

	// Verify that GetObjectTypeByIndex recognizes the script class
	if( mod->GetObjectTypeCount() != 1 )
		TEST_FAILED;
	asIObjectType *type = mod->GetObjectTypeByIndex(0);
	if( strcmp(type->GetName(), "Test") != 0 )
		TEST_FAILED;

	asIScriptContext *ctx = engine->CreateContext();
	r = ExecuteString(engine, "TestStruct()", mod, ctx);
	if( r != asEXECUTION_FINISHED ) 
	{
		if( r == asEXECUTION_EXCEPTION ) PrintException(ctx);
		TEST_FAILED;
	}
	if( ctx ) ctx->Release();

	bout.buffer = "";
	mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(TESTNAME, script2);
	engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);
	r = mod->Build();
	if( r >= 0 || bout.buffer != "TestScriptStruct (3, 4) : Error   : Class properties cannot be declared as const\n" ) TEST_FAILED;

	mod->AddScriptSection(TESTNAME, script3);
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
	r = mod->Build();
	if( r < 0 ) TEST_FAILED;
	r = ExecuteString(engine, "TestArrayInStruct()", mod);
	if( r != 0 ) TEST_FAILED;

	mod->AddScriptSection(TESTNAME, script4, strlen(script4), 0);
	r = mod->Build();
	if( r < 0 ) TEST_FAILED;
	r = ExecuteString(engine, "Test()", mod);
	if( r != 0 ) TEST_FAILED;

	bout.buffer = "";
	mod->AddScriptSection(TESTNAME, script5, strlen(script5), 0);
	engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);
	r = mod->Build();
	if( r >= 0 || bout.buffer != 
		"TestScriptStruct (2, 7) : Error   : Name conflict. 'A' is a class.\n"
		"TestScriptStruct (6, 9) : Error   : Name conflict. 'a' is an object property.\n" ) TEST_FAILED;

	bout.buffer = "";
	mod->AddScriptSection(TESTNAME, script6, strlen(script6), 0);
	r = mod->Build();
	if( r >= 0 || bout.buffer !=
		"TestScriptStruct (1, 7) : Error   : Illegal member type\n"
		"TestScriptStruct (5, 7) : Error   : Illegal member type\n" ) TEST_FAILED;

	mod->AddScriptSection(TESTNAME, script7, strlen(script7), 0);
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
	r = mod->Build();
	if( r < 0 ) TEST_FAILED;
	ctx = engine->CreateContext();
	r = ExecuteString(engine, "TestHandleInStruct()", mod, ctx);
	if( r != 0 )
	{
		if( r == asEXECUTION_EXCEPTION )
		{
			printf("%s\n", ctx->GetExceptionString());
		}
		TEST_FAILED;
	}
	if( ctx ) ctx->Release();

	mod->AddScriptSection(TESTNAME, script8, strlen(script8), 0);
	r = mod->Build();
	if( r < 0 ) TEST_FAILED;
	r = ExecuteString(engine, "TestHandleInStruct2()", mod);
	if( r != 0 ) TEST_FAILED;

	mod->AddScriptSection(TESTNAME, script9, strlen(script9), 0);
	r = mod->Build();
	if( r < 0 ) TEST_FAILED;
	r = ExecuteString(engine, "Test()", mod);
	if( r != 0 ) TEST_FAILED;

	bout.buffer = "";
	mod->AddScriptSection(TESTNAME, script10, strlen(script10), 0);
	engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);
	r = mod->Build();
	if( r >= 0 ) TEST_FAILED;
	if( bout.buffer != "TestScriptStruct (1, 7) : Error   : Illegal member type\n" ) TEST_FAILED;

	bout.buffer = "";
	mod->AddScriptSection(TESTNAME, script11, strlen(script11), 0);
	r = mod->Build();
	if( r >= 0 ) TEST_FAILED;
	if( bout.buffer != "TestScriptStruct (5, 1) : Info    : Compiling void Test()\nTestScriptStruct (9, 11) : Error   : Reference is read-only\n" ) TEST_FAILED;

	mod->AddScriptSection(TESTNAME, script12, strlen(script12), 0);
	mod->AddScriptSection(TESTNAME, script13, strlen(script13), 0);
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
	r = mod->Build();
	if( r < 0 ) TEST_FAILED;

	// The garbage collection doesn't have to be invoked immediately. Modules
	// can even be discarded before calling the garbage collector.
	engine->GarbageCollect();
	
	// Make sure it is possible to copy a script class that contains an object handle
	mod->AddScriptSection(TESTNAME, script14, strlen(script14), 0);
	r = mod->Build();
	if( r < 0 ) TEST_FAILED;
	r = ExecuteString(engine, "A a; B b; @a.b = @b; b.val = 1; A a2; a2 = a; Assert(a2.b.val == 1);", mod);
	if( r != asEXECUTION_FINISHED )
		TEST_FAILED;

	// Make sure it is possible to copy a script class that contains an array
	const char *script15 = 
		"class Some \n"
        "{ \n"
        "    int[] i; // need be array \n"
        "} \n"
        "void main() \n"
        "{ \n"
        "    Some e; \n"
        "    e=some(e); // crash \n"
        "} \n"
        "Some@ some(Some@ e) \n"
        "{ \n"
        "    return e; \n"
        "} \n";

	mod->AddScriptSection(TESTNAME, script15);
	r = mod->Build();
	if( r < 0 ) TEST_FAILED;
	r = ExecuteString(engine, "main()", mod);
	if( r != asEXECUTION_FINISHED )
		TEST_FAILED;

	engine->Release();

	// A script class must be able to have a registered ref type as a local member
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);
		RegisterScriptString(engine);

		const char *script = "class C { string s; }";
		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("s", script);

		bout.buffer = "";
		r = mod->Build();
		if( r < 0 ) 
			TEST_FAILED;

		if( bout.buffer != "" )
		{
			printf("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		if( !fail )
		{
			r = ExecuteString(engine, "C c; c.s = 'test';", mod);
			if( r != asEXECUTION_FINISHED )
			{
				TEST_FAILED;
			}
		}

		engine->Release();
	}

	// Test private properties
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);

		const char *script = "class C { \n"
			                 "private int a; \n"
							 "void func() { a = 1; } }\n"
							 "void main() { C c; c.a = 2; }";
		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("s", script);

		bout.buffer = "";
		r = mod->Build();
		if( r >= 0 ) 
			TEST_FAILED;

		if( bout.buffer != "s (4, 1) : Info    : Compiling void main()\n"
		                   "s (4, 21) : Error   : Illegal access to private property 'a'\n" )
		{
			printf("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Test private methods
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		const char *script = "class C { \n"
							 "  private void func() {} \n"
							 "  void func2() { func(); } } \n"
							 "void main() { C c; c.func(); } \n";
		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("s", script);

		bout.buffer = "";
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != "s (4, 1) : Info    : Compiling void main()\n"
		                   "s (4, 20) : Error   : Illegal call to private method 'void C::func()'\n" )
		{
			printf("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Test private methods with inheritance
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		const char *script = 
			"bool alreadyCalled = false; \n"
			"class CBar \n"
			"{ \n"
			" CBar() \n"
			" { \n"
			"  assert(alreadyCalled == false); \n"
			"  alreadyCalled = true; \n"
			" } \n"
			" void Foo() \n"
			" { \n"
			" } \n"
			"}; \n"
			"class CDerivedBar : CBar \n"
			"{ \n"
			" CDerivedBar() \n"
			" { \n"
			" } \n"
			" private void ImNotAnOverrideOfTheBaseClass() \n"
			" { \n"
			" } \n"
			" private void Foo() \n" 
			" { \n"
			" } \n"
			"}; \n";

		asIScriptModule *mod = engine->GetModule("t", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);
		mod->Build();

		asIScriptContext *ctx = engine->CreateContext();
		r = ExecuteString(engine, "CDerivedBar bar; bar.Foo(); ", mod, ctx);
		if( r != asEXECUTION_FINISHED )
		{
			if( r == asEXECUTION_EXCEPTION )
				PrintException(ctx, true);
			TEST_FAILED;
		}
		ctx->Release();
 
		engine->Release();
	}

	// Default constructor and opAssign shouldn't be provided if a non-default constructor is implemented
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		const char *script = 
			"class CBar \n"
			"{ \n"
			" CBar(int a) {}\n"
			"}; \n"
			"void func() \n"
			"{ \n"
			"  CBar a; \n" // not ok
			"  CBar b(1); \n" // ok
			"  CBar c = CBar(1); \n" // not ok
			"  b = b; \n" // not ok
			"  CBar d(CBar()); \n" // not ok
			"}; \n";

		asIScriptModule *mod = engine->GetModule("t", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);

		bout.buffer = "";
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != "script (5, 1) : Info    : Compiling void func()\n"
						   "script (7, 8) : Error   : No default constructor for object of type 'CBar'.\n"
						   "script (9, 8) : Error   : No default constructor for object of type 'CBar'.\n"
						   "script (9, 8) : Error   : There is no copy operator for the type 'CBar' available.\n"
						   "script (10, 5) : Error   : There is no copy operator for the type 'CBar' available.\n"
						   "script (11, 10) : Error   : No matching signatures to 'CBar()'\n"
						   "script (11, 10) : Info    : Candidates are:\n"
						   "script (11, 10) : Info    : CBar@ CBar(int)\n" )
		{
			printf("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
 
		engine->Release();
	}

	// Success
	return fail;
}


//--------------------------------
// Test reported by SiCrane
// 
// Doing an assignment of a temporary object would give an incorrect result, even crashing the application
bool Test2()
{
	bool fail = false;
	COutStream out;
	int r;


	const char *script1 = 
		"class MyClass {                  \n"
		"  int a;                         \n"
		"  MyClass(int a) { this.a = a; } \n"
		"  MyClass &opAssign(const MyClass &in o) { a = o.a; return this; } \n"
		"  int foo() { return a; }        \n"
		"}                                \n"
		"                                 \n"
		"void main() {                    \n"
		"  int i;                         \n"
		"  MyClass m(5);                  \n"
		"  MyClass t(10);                 \n"
		"  i = (m = t).a;                 \n"
		"  assert(i == 10);               \n"
		"  i = (m = MyClass(10)).a;       \n"
		"  assert(i == 10);               \n"
		"  MyClass n(10);                 \n"
		"  MyClass o(15);                 \n"
		"  m = n = o;                     \n"
		"  m = n = MyClass(20);           \n"
		"  (m = n).foo();                 \n"
		"  (m = MyClass(20)).foo();       \n"
		"}                                \n";

 	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
	engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection("script", script1, strlen(script1), 0);
	r = mod->Build();
	if( r < 0 )
	{
		TEST_FAILED;
	}

	r = ExecuteString(engine, "main()", mod);
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}

	engine->Release();

	return fail;
}

} // namespace

