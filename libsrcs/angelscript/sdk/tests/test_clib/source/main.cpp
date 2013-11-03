#include <stdio.h>
#include <assert.h>

#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

#define ANGELSCRIPT_DLL_LIBRARY_IMPORT
#include "angelscript_c.h"

void DetectMemoryLeaks()
{
#if defined(_MSC_VER)
	_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF|_CRTDBG_ALLOC_MEM_DF);
	_CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);

	// Use _CrtSetBreakAlloc(n) to find a specific memory leak
	//_CrtSetBreakAlloc(4537);
#endif
}

void MessageCallback(asSMessageInfo *msg, void * /*obj*/)
{
	const char *msgType = 0;
	if( msg->type == 0 ) msgType = "Error  ";
	if( msg->type == 1 ) msgType = "Warning";
	if( msg->type == 2 ) msgType = "Info   ";

	printf("%s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col, msgType, msg->message);
}

void PrintSomething()
{
	printf("Called from the script\n");
}

int main(int argc, char **argv)
{
	int r;

	DetectMemoryLeaks();

	printf("AngelScript version: %s\n", asGetLibraryVersion());
	printf("AngelScript options: %s\n", asGetLibraryOptions());

	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	r = asEngine_SetMessageCallback(engine, (asFUNCTION_t)MessageCallback, 0, asCALL_CDECL); assert( r >= 0 );
	r = asEngine_RegisterGlobalFunction(engine, "void print()", (asFUNCTION_t)PrintSomething, asCALL_CDECL); assert( r >= 0 );

	printf("Initialized engine\n");

	r = asEngine_ExecuteString(engine, 0, "print()", 0, 0);
	if( r != asEXECUTION_FINISHED )
	{
		printf("Something wen't wrong with the execution\n");
	}
	else
	{
		printf("The script was executed successfully\n");
	}

	asEngine_Release(engine);

	return 0;
}
