/**

\page doc_finetuning Fine tuning

Here's a few recommendations to get the most performance out of AngelScript.

\section doc_finetuning_1 Cache the functions and types

Doing searches by function declaration or name is rather time consuming and 
should not be done more than once per function that will be called. The same 
goes for the types that you might use it.

Also try to use the actual \ref asIScriptFunction or \ref asIObjectType pointers
instead of the ids where possible. This will save the engine from translating the  
id to the actual object.

You may use the user data in the various engine interfaces to store the cached 
information. For example, store a structure with the commonly used class methods
as \ref asIObjectType::SetUserData "user data" in the \ref asIObjectType interface. 
This way you will have quick access to the functions when they need to be called.



\section doc_finetuning_2 Reuse the context object

The context object is a heavy weight object and you should avoid allocating new
instances for each call. The object has been designed to be reused for multiple 
calls.

Ideally the application will keep a simple memory pool of allocated context 
objects, where new objects are only allocated if the is no free objects in the 
pool.

\code
std::vector<asIScriptContext *> pool;
asIScriptContext *GetContextFromPool()
{
  // Get a context from the pool, or create a new
  asIScriptContext *ctx = 0;
  if( pool.size() )
  {
    ctx = *pool.rbegin();
    pool.pop_back();
  }
  else
    ctx = engine->CreateContext();

  return ctx;
}

void ReturnContextToPool(asIScriptContext *ctx)
{
  pool.push_back(ctx);
  
  // Unprepare the context to free non-reusable resources
  ctx->Unprepare();
}
\endcode




\section doc_finetuning_3 Compile scripts without line cues

The line cues are normally placed in the bytecode between each script 
statement. These are where the VM will allow the execution to be suspended, and
also where the line callback is invoked.

If you do not use the line callback then you may get a little more performance 
out of the script by compiling without the line cues.

\code
engine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES);
\endcode




\section doc_finetuning_4 Disable thread safety

If your application only uses one thread to invoke the script engine, then it 
may be worth it to compile the library without the thread safety to gain a 
little more performance.

To do this, define the AS_NO_THREADS flag in the as_config.h header or in the 
project settings.

*/