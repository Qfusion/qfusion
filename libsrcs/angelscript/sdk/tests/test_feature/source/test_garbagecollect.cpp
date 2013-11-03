

#include "utils.h"

namespace TestGarbageCollect
{

int called = 0;
void PrintString_Generic(asIScriptGeneric *gen)
{
	std::string *str = (std::string*)gen->GetArgAddress(0);
	UNUSED_VAR(str);
//	printf("%s",str->c_str());
	called++;
}




class CFoo 
{ 
public:     
	CFoo() : m_Ref(1) { m_pObject = 0; }
	~CFoo() { if( m_pObject ) m_pObject->Release(); }
	void SetScriptObject(asIScriptObject* _pObject) { m_pObject = _pObject; }
	void AddRef() { m_Ref++; }
	void Release() { if( --m_Ref == 0 ) { delete this; } }
	static CFoo* CreateObject() { return new CFoo; }
private:
	asIScriptObject* m_pObject;
	asUINT m_Ref;
};





bool Test()
{
	bool fail = false;

    // Create the script engine
    asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
 
    // Register Function
    RegisterScriptString(engine);
    engine->RegisterGlobalFunction("void Print(string &in)", asFUNCTION(PrintString_Generic), asCALL_GENERIC);
 
    // Compile
    asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
    mod->AddScriptSection("script", 
        "class Obj{};"
        "class Hoge"
        "{"
        "    Hoge(){ Print('ctor\\n'); }"
        "    ~Hoge(){ Print('dtor\\n'); }"
        "    Obj@ obj;"
        "};"
        "void main()"
        "{"
        "    Hoge hoge;"
        "};"
        , 0);
    mod->Build();
 
    // Context Create
    asIScriptContext *ctx = engine->CreateContext();
 
    // Loop
    for( asUINT n = 0; n < 3; n++ )
    {
        // Execute
        //printf("----- execute\n");
        ctx->Prepare(mod->GetFunctionIdByDecl("void main()"));
        ctx->Execute();
 
        // GC
        const int GC_STEP_COUNT_PER_FRAME = 100;
        for ( int i = 0; i < GC_STEP_COUNT_PER_FRAME; ++i )
        {
            engine->GarbageCollect(asGC_ONE_STEP);
        }
        
        // Check status
        {
            asUINT currentSize = asUINT();
            asUINT totalDestroyed = asUINT();
            asUINT totalDetected = asUINT();
            engine->GetGCStatistics(&currentSize , &totalDestroyed , &totalDetected );
			if( currentSize    != 8 ||
				totalDestroyed != n+1 ||
				totalDetected  != 0 )
				TEST_FAILED;
            //printf("(%lu,%lu,%lu)\n" , currentSize , totalDestroyed , totalDetected );
        }
    }

    // Release 
    ctx->Release();
    engine->Release();

	// Test 
	{	
		COutStream out;
		int r;

		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		RegisterScriptArray(engine, true);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection(0, 
			"interface ITest\n"
			"{\n"
			"}\n"
			"class Test : ITest\n"
			"{\n"
			"	ITest@[] arr;\n"
			"	void Set(ITest@ e)\n"
			"	{\n"
			"		arr.resize(1);\n"
			"		@arr[0]=e;\n"
			"	}\n"
			"}\n"
			"void main()\n"
			"{\n"
			"	Test@ t=Test();\n"
			"	t.Set(t);\n"
			"}\n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		asUINT currentSize;
		engine->GetGCStatistics(&currentSize);
		
		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->GetGCStatistics(&currentSize);

		engine->Release();
	}

	// Test attempted access of global variable after it has been destroyed
	{	
		COutStream out;
		int r;

		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		RegisterScriptArray(engine, true);
		RegisterStdString(engine);
		engine->RegisterGlobalFunction("void Log(const string &in)", asFUNCTION(PrintString_Generic), asCALL_GENERIC);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection(0, 
			"class Big \n"
			"{ \n"
			"  Big() \n"
			"  { \n"
			"    Log('Big instance created\\n'); \n"
			"  } \n"
			"  ~Big() \n"
			"  { \n"
			"    Log('Big instance being destroyed\\n'); \n"
			"  } \n"
			"  void exec() \n"
			"  { \n"
			"    Log('executed\\n'); \n"
			"  } \n"
			"} \n"
			"Big big; \n" // Global object
			"class SomeClass \n"
			"{ \n"
			"  SomeClass@ handle; \n" // Make sure SomeClass is garbage collected
			"  SomeClass() {} \n"
			"  ~SomeClass() \n"
			"  { \n"
			"    Log('Before attempting access to global var\\n'); \n"
			"    big.exec(); \n" // As the module has already been destroyed, the global variable won't exist anymore, thus raising a null pointer exception here
			"    Log('SomeClass instance being destroyed\\n'); \n" // This won't be called
			"  } \n"
			"} \n"
			"void test_main() \n"
			"{ \n"
			"  SomeClass @something = @SomeClass(); \n" // Instanciate the object. It will only be destroyed by the GC
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "test_main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// The global variables in the module will be destroyed first. The objects in the GC that 
		// tries to access them should throw exception, but should not cause the application to crash
		called = 0;
		engine->Release();
		if( called != 2 )
			TEST_FAILED;
	}
/*
	{
		// This test forces a memory leak due to not registering the GC behaviours for the CFoo class
		COutStream out;
		int r;

		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		engine->RegisterInterface("IMyInterface"); 
		engine->RegisterObjectType("CFoo", sizeof(CFoo), asOBJ_REF); 
		engine->RegisterObjectBehaviour("CFoo", asBEHAVE_ADDREF, "void f()", asMETHOD(CFoo, AddRef), asCALL_THISCALL); 
		engine->RegisterObjectBehaviour("CFoo", asBEHAVE_RELEASE, "void f()", asMETHOD(CFoo, Release), asCALL_THISCALL); 
		engine->RegisterObjectBehaviour("CFoo", asBEHAVE_FACTORY, "CFoo@ f()", asFUNCTION(&CFoo::CreateObject), asCALL_CDECL);       
		engine->RegisterObjectMethod("CFoo", "void SetObject(IMyInterface@)", asMETHOD(CFoo, SetScriptObject), asCALL_THISCALL); 

		const char *script = 
			"CBar test; \n"
			"class CBase : IMyInterface \n"
			"{ \n"
			"  IMyInterface@ m_dummy; \n" // Comment only this and everything is ok
			"} \n"
			"class CBar : CBase \n"
			"{ \n"
			"  CBar() \n"
			"  { \n"
			"    m_foo.SetObject(this); \n" // Comment only this and everything is ok
			"  } \n"
			"  CFoo m_foo; \n"
			"}; ";

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script);
		r = mod->Build();

		engine->Release();
	}
*/
	return fail;
}


} // namespace

