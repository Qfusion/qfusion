#include "utils.h"

#ifdef _MSC_VER
#pragma warning (disable:4786)
#endif
#include "../../../add_on/scriptbuilder/scriptbuilder.h"

namespace TestMetaData
{

const char *script =
// Global functions can have meta data
"[ my meta data test ] void func1() {} \n"
// meta data strings can contain any tokens, and can use nested []
"[ test['hello'] ] void func2() {} \n"
// global variables can have meta data
"[ init ] int g_var = 0; \n"
// Parts of the code can be excluded through conditional compilation
"#if DONTCOMPILE                                      \n"
"  This code should be excluded by the CScriptBuilder \n"
"  #if NESTED                                         \n"
"    Nested blocks are also possible                  \n"
"  #endif                                             \n"
"  Nested block ended                                 \n"
"#endif                                               \n"
// global object variable
"[ var of type myclass ] MyClass g_obj(); \n"
// class declarations can have meta data
"#if COMPILE \n"
"[ myclass ] class MyClass {} \n"
" #if NESTED \n"
"   dont compile this nested block \n"
" #endif \n"
"#endif \n"
// interface declarations can have meta data
"[ myintf ] interface MyIntf {} \n"
// arrays must still work
"int[] arr = {1, 2, 3}; \n"
"int[] arrayfunc(int[] a) { a.resize(1); return a; } \n"
// directives in comments should be ignored
"/* \n"
"#include \"dont_include\" \n"
"*/ \n"
;

using namespace std;



bool Test()
{
	bool fail = false;
	int r = 0;
	COutStream out;

	// TODO: Preprocessor directives should be alone on the line

	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	RegisterScriptArray(engine, true);

	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);

	// Test the parse token method
	asETokenClass t = engine->ParseToken("!is");
	if( t != asTC_KEYWORD )
		TEST_FAILED;

	// Compile a script with meta data strings
	CScriptBuilder builder;
	builder.DefineWord("COMPILE");
	r = builder.StartNewModule(engine, 0);
	r = builder.AddSectionFromMemory(script);
	r = builder.BuildModule();
#if AS_PROCESS_METADATA == 1
	if( r < 0 )
		TEST_FAILED;

	int funcId = engine->GetModule(0)->GetFunctionIdByName("func1");
	string metadata = builder.GetMetadataStringForFunc(funcId);
	if( metadata != " my meta data test " )
		TEST_FAILED;

	funcId = engine->GetModule(0)->GetFunctionIdByName("func2");
	metadata = builder.GetMetadataStringForFunc(funcId);
	if( metadata != " test['hello'] " )
		TEST_FAILED;

	int typeId = engine->GetModule(0)->GetTypeIdByDecl("MyClass");
	metadata = builder.GetMetadataStringForType(typeId);
	if( metadata != " myclass " )
		TEST_FAILED;

	typeId = engine->GetModule(0)->GetTypeIdByDecl("MyIntf");
	metadata = builder.GetMetadataStringForType(typeId);
	if( metadata != " myintf " )
		TEST_FAILED;

	int varIdx = engine->GetModule(0)->GetGlobalVarIndexByName("g_var");
	metadata = builder.GetMetadataStringForVar(varIdx);
	if( metadata != " init " )
		TEST_FAILED;

	varIdx = engine->GetModule(0)->GetGlobalVarIndexByName("g_obj");
	metadata = builder.GetMetadataStringForVar(varIdx);
	if( metadata != " var of type myclass " )
		TEST_FAILED;
#endif

	engine->Release();

	return fail;
}

} // namespace

