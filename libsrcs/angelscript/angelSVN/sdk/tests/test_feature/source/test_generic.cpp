
#include <stdarg.h>
#include "utils.h"

using std::string;

namespace TestGeneric
{


int obj;

void GenFunc1(asIScriptGeneric *gen)
{
	assert(gen->GetObject() == 0);

//	printf("GenFunc1\n");

	int arg1 = (int)gen->GetArgDWord(0);
	double arg2 = gen->GetArgDouble(1);
	string arg3 = *(string*)gen->GetArgObject(2);

	assert(arg1 == 23);
	assert(arg2 == 23);
	assert(arg3 == "test");

	gen->SetReturnDouble(23);
}

void GenMethod1(asIScriptGeneric *gen)
{
	assert(gen->GetObject() == &obj);

//	printf("GenMethod1\n");

	int arg1 = (int)gen->GetArgDWord(0);
	double arg2 = gen->GetArgDouble(1);

	assert(arg1 == 23);
	assert(arg2 == 23);

	string s("Hello");
	gen->SetReturnObject(&s);
}

void GenAssign(asIScriptGeneric *gen)
{
//	assert(gen->GetObject() == &obj);

	int *obj2 = (int*)gen->GetArgObject(0);
	UNUSED_VAR(obj2);

//	assert(obj2 == &obj);

	gen->SetReturnObject(&obj);
}

void TestDouble(asIScriptGeneric *gen)
{
	double d = gen->GetArgDouble(0);

	assert(d == 23);
}

void TestString(asIScriptGeneric *gen)
{
	string s = *(string*)gen->GetArgObject(0);

	assert(s == "Hello");
}

void GenericString_Construct(asIScriptGeneric *gen)
{
	string *s = (string*)gen->GetObject();

	new(s) string;
}

void GenericString_Destruct(asIScriptGeneric *gen)
{
	string *s = (string*)gen->GetObject();

	s->~string();
}

void GenericString_Assignment(asIScriptGeneric *gen)
{
	string *other = (string*)gen->GetArgObject(0);
	string *self = (string*)gen->GetObject();

	*self = *other;

	gen->SetReturnObject(self);
}

void GenericString_Factory(asIScriptGeneric *gen)
{
	asUINT length = gen->GetArgDWord(0);
	UNUSED_VAR(length);
	const char *s = (const char *)gen->GetArgAddress(1);

	string str(s);

	gen->SetReturnObject(&str);
}

void nullPtr(asIScriptGeneric *gen)
{
	asIScriptObject **intf = (asIScriptObject**)gen->GetAddressOfArg(0);
	assert( *intf == 0 );

	assert(gen->GetArgCount() == 1);

	*(asIScriptObject **)gen->GetAddressOfReturnLocation() = *intf;

	assert(gen->GetReturnTypeId() == gen->GetEngine()->GetTypeIdByDecl("intf@"));
}

bool Test2();

bool Test()
{
	bool fail = Test2();

	int r;

	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

	r = engine->RegisterObjectType("string", sizeof(string), asOBJ_VALUE | asOBJ_APP_CLASS_CDA); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("string", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(GenericString_Construct), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("string", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(GenericString_Destruct), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectMethod("string", "string &opAssign(string &in)", asFUNCTION(GenericString_Assignment), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterStringFactory("string", asFUNCTION(GenericString_Factory), asCALL_GENERIC); assert( r >= 0 );

	r = engine->RegisterGlobalFunction("void test(double)", asFUNCTION(TestDouble), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterGlobalFunction("void test(string)", asFUNCTION(TestString), asCALL_GENERIC); assert( r >= 0 );

	r = engine->RegisterGlobalFunction("double func1(int, double, string)", asFUNCTION(GenFunc1), asCALL_GENERIC); assert( r >= 0 );

	r = engine->RegisterObjectType("obj", 4, asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE); assert( r >= 0 );
	r = engine->RegisterObjectMethod("obj", "string mthd1(int, double)", asFUNCTION(GenMethod1), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectMethod("obj", "obj &opAssign(obj &in)", asFUNCTION(GenAssign), asCALL_GENERIC); assert( r >= 0 );

	r = engine->RegisterGlobalProperty("obj o", &obj);

	r = engine->RegisterInterface("intf");
	r = engine->RegisterGlobalFunction("intf @nullPtr(intf @)", asFUNCTION(nullPtr), asCALL_GENERIC); assert( r >= 0 );

	COutStream out;
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
	ExecuteString(engine, "test(func1(23, 23, \"test\"))");

	ExecuteString(engine, "test(o.mthd1(23, 23))");

	ExecuteString(engine, "o = o");

	ExecuteString(engine, "nullPtr(null)");

	engine->Release();

	// Success
	return fail;
}

//--------------------------------------------------------
// This part is going to test the auto-generated wrappers
//--------------------------------------------------------

// This doesn't work on MSVC6. The template implementation isn't good enough.
// It also doesn't work on MSVC2005, it gets confused on const methods that return void. Reported by Jeff Slutter.
// TODO: Need to fix implementation for MSVC2005.
#if !defined(_MSC_VER) || (_MSC_VER > 1200 && _MSC_VER != 1400) 

#include "../../../add_on/autowrapper/aswrappedcall.h"

void TestWrapNoArg() {}
asDECLARE_FUNCTION_WRAPPER(TestNoArg_Generic, TestWrapNoArg);


void TestWrapStringByVal(std::string val) {
	assert(val == "test");
}
asDECLARE_FUNCTION_WRAPPER(TestStringByVal_Generic, TestWrapStringByVal);


void TestWrapStringByRef(std::string &ref) {
	assert(ref == "test");
}
asDECLARE_FUNCTION_WRAPPER(TestStringByRef_Generic, TestWrapStringByRef);


void TestWrapIntByVal(int val) {
	assert(val == 42);
}
asDECLARE_FUNCTION_WRAPPER(TestIntByVal_Generic, TestWrapIntByVal);


void TestWrapIntByRef(int &ref) {
	assert(ref == 42);
}
asDECLARE_FUNCTION_WRAPPER(TestIntByRef_Generic, TestWrapIntByRef);


int TestWrapRetIntByVal() {
	return 42;
}
asDECLARE_FUNCTION_WRAPPER(TestRetIntByVal_Generic, TestWrapRetIntByVal);


int &TestWrapRetIntByRef() {
	static int val = 42;
	return val;
}
asDECLARE_FUNCTION_WRAPPER(TestRetIntByRef_Generic, TestWrapRetIntByRef);


std::string TestWrapRetStringByVal() {
	return "test";
}
asDECLARE_FUNCTION_WRAPPER(TestRetStringByVal_Generic, TestWrapRetStringByVal);


std::string &TestWrapRetStringByRef() {
	static std::string val = "test";
	return val;
}
asDECLARE_FUNCTION_WRAPPER(TestRetStringByRef_Generic, TestWrapRetStringByRef);

void TestWrapOverload(int) {}
asDECLARE_FUNCTION_WRAPPERPR(TestWrapOverload_Generic, TestWrapOverload, (int), void);

void TestWrapOverload(float) {}
asDECLARE_FUNCTION_WRAPPERPR(TestWrapOverload2_Generic, TestWrapOverload, (float), void);

class A
{
public:
	A() {id = 0;}
	virtual void a() const {assert(id == 2);}
	int id;
};

class B
{
public:
	B() {}
	virtual void b() {}
};

class C : public A, B
{
public:
	C() {id = 2;}
	virtual void c(int) {assert(id == 2);}
	virtual void c(float) const {assert(id == 2);}
};

asDECLARE_METHOD_WRAPPER(A_a_generic, A, a);
asDECLARE_METHOD_WRAPPER(B_b_generic, B, b);
asDECLARE_METHOD_WRAPPERPR(C_c_generic, C, c, (int), void);
asDECLARE_METHOD_WRAPPERPR(C_c2_generic, C, c, (float) const, void);

void Construct_C(asIScriptGeneric *gen)
{
	void *mem = gen->GetObject();
	new(mem) C();
}
// TODO: The wrapper doesn't work for the constructor behaviour, as the 
//       generic interface passes the memory pointer as the object pointer, 
//       but the wrapper tries to access it as a parameter
//asDECLARE_FUNCTION_WRAPPER(Construct_C_Generic, Construct_C);

bool Test2()
{
	bool fail = false;
	COutStream out;

	int r;
	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
	engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);
	RegisterStdString(engine);

	r = engine->RegisterGlobalFunction("void TestNoArg()", asFUNCTION(TestNoArg_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = ExecuteString(engine, "TestNoArg()");
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}

	r = engine->RegisterGlobalFunction("void TestStringByVal(string val)", asFUNCTION(TestStringByVal_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = ExecuteString(engine, "TestStringByVal('test')");
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}

	r = engine->RegisterGlobalFunction("void TestStringByRef(const string &in ref)", asFUNCTION(TestStringByRef_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = ExecuteString(engine, "TestStringByRef('test')");
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}

	r = engine->RegisterGlobalFunction("void TestIntByVal(int val)", asFUNCTION(TestIntByVal_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = ExecuteString(engine, "TestIntByVal(42)");
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}

	r = engine->RegisterGlobalFunction("void TestIntByRef(int &in ref)", asFUNCTION(TestIntByRef_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = ExecuteString(engine, "TestIntByRef(42)");
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}

	r = engine->RegisterGlobalFunction("int TestRetIntByVal()", asFUNCTION(TestRetIntByVal_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = ExecuteString(engine, "assert(TestRetIntByVal() == 42)");
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}

	r = engine->RegisterGlobalFunction("int &TestRetIntByRef()", asFUNCTION(TestRetIntByRef_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = ExecuteString(engine, "assert(TestRetIntByRef() == 42)");
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}

	r = engine->RegisterGlobalFunction("string TestRetStringByVal()", asFUNCTION(TestRetStringByVal_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = ExecuteString(engine, "assert(TestRetStringByVal() == 'test')");
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}

	r = engine->RegisterGlobalFunction("string &TestRetStringByRef()", asFUNCTION(TestRetStringByRef_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = ExecuteString(engine, "assert(TestRetStringByRef() == 'test')");
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}

	r = engine->RegisterObjectType("C", sizeof(C), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("C", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Construct_C), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectMethod("C", "void a() const", asFUNCTION(A_a_generic), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectMethod("C", "void b()", asFUNCTION(B_b_generic), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectMethod("C", "void c(int)", asFUNCTION(C_c_generic), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectMethod("C", "void c(float) const", asFUNCTION(C_c2_generic), asCALL_GENERIC); assert( r >= 0 );

	r = ExecuteString(engine, "C c; c.a(); c.b(); c.c(1); c.c(1.1f);");
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}

	engine->Release();

	return fail;
}
#else
bool Test2()
{
	printf("The test of the autowrapper was skipped due to lack of proper template support\n");
	return false;
}
#endif

} // namespace

