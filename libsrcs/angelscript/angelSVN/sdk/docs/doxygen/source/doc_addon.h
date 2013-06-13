/**

\page doc_addon Add-ons

This page gives a brief description of the add-ons that you'll find in the /sdk/add_on/ folder.

 - \subpage doc_addon_application
 - \subpage doc_addon_script

\page doc_addon_application Application modules

 - \subpage doc_addon_build
 - \subpage doc_addon_ctxmgr
 - \subpage doc_addon_debugger
 - \subpage doc_addon_serializer
 - \subpage doc_addon_helpers
 - \subpage doc_addon_autowrap
 - \subpage doc_addon_clib

\page doc_addon_script Script extensions

 - \subpage doc_addon_std_string
 - \subpage doc_addon_array
 - \subpage doc_addon_any
 - \subpage doc_addon_handle
 - \subpage doc_addon_dict
 - \subpage doc_addon_file
 - \subpage doc_addon_math


 
 
\page doc_addon_serializer Serializer

<b>Path:</b> /sdk/add_on/serializer/

The <code>CSerializer</code> implements support for serializing the values of global variables in a 
module, for example in order to reload a slightly modified version of the script without reinitializing
everything. It will resolve primitives and script classes automatically, including references and handles.
For application registered types, the application needs to implement callback objects to show how
these should be serialized.

The implementation currently has some limitations:

 - It can only serialize to memory, i.e. it is not possible to save the values to a file.
 - If the variables changed type when restoring, the serializer cannot restore the value.
 - The serializer will attempt to backup all objects, but in some cases an application may
   not want to backup the actual object, but only a reference to it, e.g. an internal application
   object referenced by the script. Currently there is no way of telling the serializer to do 
   differently in this case.
 - If the module holds references to objects from another module it will probably fail in 
   restoring the values.
 

\section doc_addon_serializer_1 Public C++ interface

\code
class CSerializer
{
public:
  CSerializer();
  ~CSerializer();

  // Add implementation for serializing user types
  void AddUserType(CUserType *ref, const std::string &name);

  // Store all global variables in the module
  int Store(asIScriptModule *mod);

  // Restore all global variables after reloading script
  int Restore(asIScriptModule *mod);
};
\endcode

\section doc_addon_serializer_2 Example usage

\code
struct CStringType;
struct CArrayType;

void RecompileModule(asIScriptEngine *engine, asIScriptModule *mod)
{
  string modName = mod->GetName();

  // Tell the serializer how the user types should be serialized
  // by adding the implementations of the CUserType interface
  CSerializer backup;
  backup.AddUserType(new CStringType(), "string");
  backup.AddUserType(new CArrayType(), "array");

  // Backup the values of the global variables
  modStore.Store(mod);
  
  // Application can now recompile the module
  CompileModule(modName);

  // Restore the values of the global variables in the new module
  mod = engine->GetModule(modName.c_str(), asGM_ONLY_IF_EXISTS);
  backup.Restore(mod);
}

// This serializes the std::string type
struct CStringType : public CUserType
{
  void Store(CSerializedValue *val, void *ptr)
  {
    val->SetUserData(new std::string(*(std::string*)ptr));
  }
  void Restore(CSerializedValue *val, void *ptr)
  {
    std::string *buffer = (std::string*)val->GetUserData();
    *(std::string*)ptr = *buffer;
  }
  void CleanupUserData(CSerializedValue *val)
  {
    std::string *buffer = (std::string*)val->GetUserData();
    delete buffer;
  }
};

// This serializes the CScriptArray type
struct CArrayType : public CUserType
{
  void Store(CSerializedValue *val, void *ptr)
  {
    CScriptArray *arr = (CScriptArray*)ptr;

    for( unsigned int i = 0; i < arr->GetSize(); i++ )
      val->m_children.push_back(new CSerializedValue(val ,"", arr->At(i), arr->GetElementTypeId()));
  }
  void Restore(CSerializedValue *val, void *ptr)
  {
    CScriptArray *arr = (CScriptArray*)ptr;
    arr->Resize(val->m_children.size());

    for( size_t i = 0; i < val->m_children.size(); ++i )
      val->m_children[i]->Restore(arr->At(i));
  }
};
\endcode







\page doc_addon_debugger Debugger

<b>Path:</b> /sdk/add_on/debugger/

The <code>CDebugger</code> implements common debugging functionality for scripts, e.g.
setting breakpoints, stepping through the code, examining values of variables, etc.

To use the debugger the line callback should be set in the context. This will allow the 
debugger to take over whenever a breakpoint is reached, so the script can be debugged.

By default the debugger uses the standard in and standard out streams to interact with the
user, but this can be easily overloaded by deriving from the <code>CDebugger</code> class and implementing
the methods <code>TakeCommands</code> and <code>Output</code>. With this it is possible to implement a graphical 
interface, or even remote debugging for an application.

The application developer may also be interested in overriding the default <code>ToString</code> method
to implement ways to visualize application registered types in an easier way.

\see The sample \ref doc_samples_asrun for a complete example of how to use the debugger

\section doc_addon_ctxmgr_1 Public C++ interface

\code
class CDebugger
{
public:
  CDebugger();
  virtual ~CDebugger();

  // User interaction
  virtual void TakeCommands(asIScriptContext *ctx);
  virtual void Output(const std::string &str);

  // Line callback invoked by context
  virtual void LineCallback(asIScriptContext *ctx);

  // Commands
  virtual void PrintHelp();
  virtual void AddFileBreakPoint(const std::string &file, int lineNbr);
  virtual void AddFuncBreakPoint(const std::string &func);
  virtual void ListBreakPoints();
  virtual void ListLocalVariables(asIScriptContext *ctx);
  virtual void ListGlobalVariables(asIScriptContext *ctx);
  virtual void ListMemberProperties(asIScriptContext *ctx);
  virtual void ListStatistics(asIScriptContext *ctx);
  virtual void PrintCallstack(asIScriptContext *ctx);
  virtual void PrintValue(const std::string &expr, asIScriptContext *ctx);

  // Helpers
  virtual bool InterpretCommand(const std::string &cmd, asIScriptContext *ctx);
  virtual bool CheckBreakPoint(asIScriptContext *ctx);
  virtual std::string ToString(void *value, asUINT typeId, bool expandMembers, asIScriptEngine *engine);
};
\endcode

\section doc_addon_debugger_1 Example usage

\code
CDebugger dbg;
int ExecuteWithDebug(asIScriptContext *ctx)
{
  // Tell the context to invoke the debugger's line callback
  ctx->SetLineCallback(asMETHOD(CDebugger, LineCallback), dbg, asCALL_THISCALL);

  // Allow the user to initialize the debugging before moving on
  dbg.TakeCommands(ctx);

  // Execute the script normally. If a breakpoint is reached the 
  // debugger will take over the control loop.
  return ctx->Execute();
}
\endcode






\page doc_addon_ctxmgr Context manager

<b>Path:</b> /sdk/add_on/contextmgr/

The <code>CContextMgr</code> is a class designed to aid the management of multiple simultaneous 
scripts executing in parallel. It supports both \ref doc_adv_concurrent "concurrent script threads" and \ref doc_adv_coroutine "co-routines". 

If the application doesn't need multiple contexts, i.e. all scripts that are executed 
always complete before the next script is executed, then this class is not necessary.

Multiple context managers can be used, for example when you have a group of scripts controlling 
ingame objects, and another group of scripts controlling GUI elements, then each of these groups
may be managed by different context managers.

Observe, that the context manager class hasn't been designed for multithreading, so you need to
be careful if your application needs to execute scripts from multiple threads.

\see The samples \ref doc_samples_concurrent and \ref doc_samples_corout for uses

\section doc_addon_ctxmgr_1 Public C++ interface

\code
class CContextMgr
{
public:
  CContextMgr();
  ~CContextMgr();

  // Set the function that the manager will use to obtain the time in milliseconds.
  void SetGetTimeCallback(TIMEFUNC_t func);

  // Registers the script function
  //
  //  void sleep(uint milliseconds)
  //
  // The application must set the get time callback for this to work
  void RegisterThreadSupport(asIScriptEngine *engine);

  // Registers the script functions
  //
  //  void createCoRoutine(const string &in functionName, any @arg)
  //  void yield()
  void RegisterCoRoutineSupport(asIScriptEngine *engine);

  // Create a new context, prepare it with the function id, then return 
  // it so that the application can pass the argument values. The context
  // will be released by the manager after the execution has completed.
  asIScriptContext *AddContext(asIScriptEngine *engine, int funcId);

  // Create a new context, prepare it with the function id, then return
  // it so that the application can pass the argument values. The context
  // will be added as a co-routine in the same thread as the currCtx.
  asIScriptContext *AddContextForCoRoutine(asIScriptContext *currCtx, int funcId);

  // Execute each script that is not currently sleeping. The function returns after 
  // each script has been executed once. The application should call this function
  // for each iteration of the message pump, or game loop, or whatever.
  void ExecuteScripts();

  // Put a script to sleep for a while
  void SetSleeping(asIScriptContext *ctx, asUINT milliSeconds);

  // Switch the execution to the next co-routine in the group.
  // Returns true if the switch was successful.
  void NextCoRoutine();

  // Abort all scripts
  void AbortAll();
};
\endcode











\page doc_addon_array array template object

<b>Path:</b> /sdk/add_on/scriptarray/

The <code>array</code> type is a \ref doc_adv_template "template object" that allow the scripts to declare arrays of any type.
Since it is a generic class it is not the most performatic due to the need to determine characteristics at 
runtime. For that reason it is recommended that the application registers a \ref doc_adv_template_2 "template specialization" for the
array types that are most commonly used.

The type is registered with <code>RegisterScriptArray(asIScriptEngine *engine, bool defaultArrayType)</code>. The second 
parameter should be set to true if you wish to allow the syntax form <code>type[]</code> to declare arrays.

\section doc_addon_array_1 Public C++ interface

\code
class CScriptArray
{
public:
  // Constructor
  CScriptArray(asUINT length, asIObjectType *ot);
  CscriptArray(asUINT length, void *defaultValue, asIObjectType *ot);
  virtual ~CScriptArray();

  // Memory management
  void AddRef() const;
  void Release() const;

  // Type information
  asIObjectType *GetArrayObjectType() const;
  int            GetArrayTypeId() const;
  int            GetElementTypeId() const;

  // Get the current size
  asUINT GetSize() const;

  // Resize the array
  void   Resize(asUINT numElements);
  
  // Get a pointer to an element. Returns 0 if out of bounds
  void  *At(asUINT index);

  // Copy the contents of one array to another (only if the types are the same)
  CScriptArray &operator=(const CScriptArray&);

  // Array manipulation
  void InsertAt(asUINT index, void *value);
  void RemoveAt(asUINT index);
  void InsertLast(void *value);
  void RemoveLast();
  void SortAsc();
  void SortAsc(asUINT index, asUINT count);
  void SortDesc();
  void SortDesc(asUINT index, asUINT count);
  void Reverse();
  int  Find(void *value);
  int  Find(asUINT index, void *value);
};
\endcode

\section doc_addon_array_2 Public script interface

<pre>
  class array<class T>
  {
    array();
    array(uint length);
    array(uint length, const T &in defaultValue);
    
    T       &opIndex(uint);
    const T &opIndex(uint) const;

    array<T> opAssign(const array<T> & in);
    
    uint length { get const; set; }
    
    void insertAt(uint index, const T& in);
    void removeAt(uint index);
    void insertLast(const T& in);
    void removeLast();
    uint length() const;
    void resize(uint);
    void sortAsc();
    void sortAsc(uint index, uint count);
    void sortDesc();
    void sortDesc(uint index, uint count);
    void reverse();
    int  find(const T& in);
    int  find(uint index, const T& in);
  }
</pre>

\section doc_addon_array_3 Script example

<pre>
  int main()
  {
    array<int> arr = {1,2,3};
    
    int sum = 0;
    for( uint n = 0; n < arr.length; n++ )
      sum += arr[n];
      
    return sum;
  }
</pre>

\section doc_addon_array_4 C++ example

This function shows how a script array can be instanciated 
from the application and then passed to the script.

\code
CScriptArray *CreateArrayOfStrings()
{
  // If called from the script, there will always be an active 
  // context, which can be used to obtain a pointer to the engine.
  asIScriptContext *ctx = asGetActiveContext();
  if( ctx )
  {
    asIScriptEngine* engine = ctx->GetEngine();

    // The script array needs to know its type to properly handle the elements
    asIObjectType* t = engine->GetObjectTypeById(engine->GetTypeIdByDecl("array<string@>"));

    CScriptArray* arr = new CScriptArray(3, t);
    for( asUINT i = 0; i < arr->GetSize(); i++ )
    {
      // Get the pointer to the element so it can be set
      CScriptString** p = static_cast<CScriptString**>(arr->At(i));
      *p = new CScriptString("test");
    }

    // The ref count for the returned handle was already set in the array's constructor
    return arr;
  }
  return 0;
}
\endcode









\page doc_addon_any any object

<b>Path:</b> /sdk/add_on/scriptany/

The <code>any</code> type is a generic container that can hold any value. It is a reference type.

The type is registered with <code>RegisterScriptAny(asIScriptEngine*)</code>.

\section doc_addon_any_1 Public C++ interface

\code
class CScriptAny
{
public:
  // Constructors
  CScriptAny(asIScriptEngine *engine);
  CScriptAny(void *ref, int refTypeId, asIScriptEngine *engine);

  // Memory management
  int AddRef() const;
  int Release() const;

  // Copy the stored value from another any object
  CScriptAny &operator=(const CScriptAny&);
  int CopyFrom(const CScriptAny *other);

  // Store the value, either as variable type, integer number, or real number
  void Store(void *ref, int refTypeId);
  void Store(asINT64 &value);
  void Store(double &value);

  // Retrieve the stored value, either as variable type, integer number, or real number
  bool Retrieve(void *ref, int refTypeId) const;
  bool Retrieve(asINT64 &value) const;
  bool Retrieve(double &value) const;

  // Get the type id of the stored value
  int GetTypeId() const;
};
\endcode

\section doc_addon_any_2 Public script interface

<pre>
  class any
  {
    any();
    any(? &in value);
  
    void store(? &in value);
    void store(int64 &in value);
    void store(double &in value);
  
    bool retrieve(? &out value) const;
    bool retrieve(int64 &out value) const;
    bool retrieve(double &out value) const;
  }
</pre>

\section doc_addon_any_3 Example usage

In the scripts it can be used as follows:

<pre>
  int value;
  obj object;
  obj \@handle;
  any a,b,c;
  a.store(value);      // store the value
  b.store(\@handle);    // store an object handle
  c.store(object);     // store a copy of the object
  
  a.retrieve(value);   // retrieve the value
  b.retrieve(\@handle); // retrieve the object handle
  c.retrieve(object);  // retrieve a copy of the object
</pre>

In C++ the type can be used as follows:

\code
CScriptAny *myAny;
int typeId = engine->GetTypeIdByDecl("string@");
CScriptString *str = new CScriptString("hello world");
myAny->Store((void*)&str, typeId);
myAny->Retrieve((void*)&str, typeId);
\endcode










\page doc_addon_handle ref object

<b>Path:</b> /sdk/add_on/scripthandle/

The <code>ref</code> type is a generic container that can hold any handle. 
It is a value type, but behaves very much like an object handle.

The type is registered with <code>RegisterScriptHandle(asIScriptEngine*)</code>.

\see \ref doc_adv_generic_handle

\section doc_addon_handle_1 Public C++ interface

\code
class CScriptHandle 
{
public:
  // Constructors
  CScriptHandle();
  CScriptHandle(const CScriptHandle &other);
  CScriptHandle(void *ref, int typeId);
  ~CScriptHandle();

  // Copy the stored reference from another handle object
  CScriptHandle &operator=(const CScriptHandle &other);
  CScriptHandle &opAssign(void *ref, int typeId);

  // Compare equalness
  bool opEquals(const CScriptHandle &o) const;
  bool opEquals(void *ref, int typeId) const;

  // Dynamic cast to desired handle type
  void opCast(void **outRef, int typeId);
};
\endcode

\section doc_addon_handle_3 Example usage

In the scripts it can be used as follows:

<pre>
  ref\@ unknown;

  // Store a handle in the ref variable
  object obj;
  \@unknown = \@obj;

  // Compare equalness
  if( unknown != null ) 
  {
    // Dynamically cast the handle to wanted type
    object \@obj2 = cast<object>(unknown);
    if( obj2 != null )
    {
      ...
    }
  }
</pre>










\page doc_addon_std_string string object

<b>Path:</b> /sdk/add_on/scriptstdstring/

This add-on registers the <code>std::string</code> type as-is with AngelScript. This gives
perfect compatibility with C++ functions that use <code>std::string</code> in parameters or
as return type.

A potential drawback is that the <code>std::string</code> type is a value type, thus may 
increase the number of copies taken when string values are being passed around
in the script code. However, this is most likely only a problem for scripts 
that perform a lot of string operations.

Register the type with <code>RegisterStdString(asIScriptEngine*)</code>. Register the optional
split method and global join function with <code>RegisterStdStringUtils(asIScriptEngine*)</code>. 
The optional functions require that the \ref doc_addon_array has been registered first.

\section doc_addon_std_string_1 Public C++ interface

Refer to the <code>std::string</code> implementation for your compiler.

\section doc_addon_std_string_2 Public script interface

<pre>
  class string
  {
    // Constructors
    string();
    string(const string &in);
    
    // Property accessor for getting and setting the length
    uint length { get const; set; }
    
    // Methods for getting and setting the length
    uint length() const;
    void resize(uint);
    
    // Assignment and concatenation
    string &opAssign(const string &in other);
    string &opAddAssign(const string &in other);
    string  opAdd(const string &in right) const;
    
    // Access individual characters
    uint8       &opIndex(uint);
    const uint8 &opIndex(uint) const;
    
    // Comparison operators
    bool opEquals(const string &in right) const;
    int  opCmp(const string &in right) const;
    
    // Substring
    string substr(uint start = 0, int count = -1) const;
    array<string>@ split(const string &in delimiter) const;
    
    // Search
    int findFirst(const string &in str, uint start = 0) const;
    int findLast(const string &in str, int start = -1) const;
    
    // Automatic conversion from primitive types to string type
    string &opAssign(double val);
    string &opAddAssign(double val);
    string  opAdd(double val) const;
    string  opAdd_r(double val) const;
    
    string &opAssign(int val);
    string &opAddAssign(int val);
    string  opAdd(int val) const;
    string  opAdd_r(int val) const;
    
    string &opAssign(uint val);
    string &opAddAssign(uint val);
    string  opAdd(uint val) const;
    string  opAdd_r(uint val) const;

    string &opAssign(bool val);
    string &opAddAssign(bool val);
    string  opAdd(bool val) const;
    string  opAdd_r(bool val) const;
  }

  // Takes an array of strings and joins them into one string separated by the specified delimiter
  string join(const array<string> &in arr, const string &in delimiter);
  
  // Formatting numbers into strings
  // The options should be informed as characters in a string
  //  l = left justify
  //  0 = pad with zeroes
  //  + = always include the sign, even if positive
  //    = add a space in case of positive number
  //  h = hexadecimal integer small letters
  //  H = hexadecimal integer capital letters
  //  e = exponent character with small e
  //  E = exponent character with capital E
  string formatInt(int64 val, const string &in options, uint width = 0);
  string formatFloat(double val, const string &in options, uint width = 0, uint precision = 0);
  
  // Parsing numbers from strings
  int64  parseInt(const string &in, uint base = 10, uint &out byteCount = 0);
  double parseFloat(const string &in, uint &out byteCount = 0);

</pre>






\page doc_addon_dict dictionary object 

<b>Path:</b> /sdk/add_on/scriptdictionary/

The dictionary object maps string values to values or objects of other types. 

Register with <code>RegisterScriptDictionary(asIScriptEngine*)</code>.

\section doc_addon_dict_1 Public C++ interface

\code
class CScriptDictionary
{
public:
  // Memory management
  CScriptDictionary(asIScriptEngine *engine);
  void AddRef() const;
  void Release() const;

  // Perform a shallow copy of the other dictionary
  CScriptDictionary &operator=(const CScriptDictionary &other);

  // Sets/Gets a variable type value for a key
  void Set(const std::string &key, void *value, int typeId);
  bool Get(const std::string &key, void *value, int typeId) const;

  // Sets/Gets an integer number value for a key
  void Set(const std::string &key, asINT64 &value);
  bool Get(const std::string &key, asINT64 &value) const;

  // Sets/Gets a real number value for a key
  void Set(const std::string &key, double &value);
  bool Get(const std::string &key, double &value) const;

  // Returns true if the key is set
  bool Exists(const std::string &key) const;
  
  // Deletes the key
  void Delete(const std::string &key);
  
  // Deletes all keys
  void DeleteAll();
};
\endcode

\section doc_addon_dict_2 Public script interface

<pre>
  class dictionary
  {
    dictionary &opAssign(const dictionary &in other);

    void set(const string &in key, ? &in value);
    bool get(const string &in value, ? &out value) const;
    
    void set(const string &in key, int64 &in value);
    bool get(const string &in key, int64 &out value) const;
    
    void set(const string &in key, double &in value);
    bool get(const string &in key, double &out value) const;
    
    bool exists(const string &in key) const;
    void delete(const string &in key);
    void deleteAll();
  }
</pre>

\section doc_addon_dict_3 Script example

<pre>
  dictionary dict;
  obj object;
  obj \@handle;
  
  dict.set("one", 1);
  dict.set("object", object);
  dict.set("handle", \@handle);
  
  if( dict.exists("one") )
  {
    bool found = dict.get("handle", \@handle);
    if( found )
    {
      dict.delete("object");
    }
  }
  
  dict.deleteAll();
</pre>





\page doc_addon_file file object 

<b>Path:</b> /sdk/add_on/scriptfile/

This object provides support for reading and writing files.

Register with <code>RegisterScriptFile(asIScriptEngine*)</code>.

If you do not want to provide write access for scripts then you can compile 
the add on with the define AS_WRITE_OPS 0, which will disable support for writing. 
This define can be made in the project settings or directly in the header.


\section doc_addon_file_1 Public C++ interface

\code
class CScriptFile
{
public:
  // Constructor
  CScriptFile();

  // Memory management
  void AddRef() const;
  void Release() const;

  // Opening and closing file handles
  // mode = "r" -> open the file for reading
  // mode = "w" -> open the file for writing (overwrites existing files)
  // mode = "a" -> open the file for appending
  int Open(const std::string &filename, const std::string &mode);
  int Close();
  
  // Returns the size of the file
  int GetSize() const;
  
  // Returns true if the end of the file has been reached
  bool IsEOF() const;

  // Reads a specified number of bytes into the string
  int ReadString(unsigned int length, std::string &str);
  
  // Reads to the next new-line character
  int ReadLine(std::string &str);

  // Reads a signed integer
  asINT64  ReadInt(asUINT bytes);

  // Reads an unsigned integer
  asQWORD  ReadUInt(asUINT bytes);

  // Reads a float
  float    ReadFloat();

  // Reads a double
  double   ReadDouble();
    
  // Writes a string to the file
  int WriteString(const std::string &str);
  
  int WriteInt(asINT64 v, asUINT bytes);
  int WriteUInt(asQWORD v, asUINT bytes);
  int WriteFloat(float v);
  int WriteDouble(double v);

  // File cursor manipulation
  int GetPos() const;
  int SetPos(int pos);
  int MovePos(int delta);

  // Determines the byte order of the binary values (default: false)
  // Big-endian = most significant byte first
  bool mostSignificantByteFirst;
};
\endcode

\section doc_addon_file_2 Public script interface

<pre>
  class file
  {
    int      open(const string &in filename, const string &in mode);
    int      close();
    int      getSize() const;
    bool     isEndOfFile() const;
    int      readString(uint length, string &out str);
    int      readLine(string &out str);
    int64    readInt(uint bytes);
    uint64   readUInt(uint bytes);
    float    readFloat();
    double   readDouble();
    int      writeString(const string &in string);
    int      writeInt(int64 value, uint bytes);
    int      writeUInt(uint64 value, uint bytes);
    int      writeFloat(float value);
    int      writeDouble(double value);
    int      getPos() const;
    int      setPos(int pos);
    int      movePos(int delta);
    bool     mostSignificantByteFirst;
  }
</pre>

\section doc_addon_file_3 Script example

<pre>
  file f;
  // Open the file in 'read' mode
  if( f.open("file.txt", "r") >= 0 ) 
  {
      // Read the whole file into the string buffer
      string str;
      f.readString(f.getSize(), str); 
      f.close();
  }
</pre>





\page doc_addon_math math functions

<b>Path:</b> /sdk/add_on/scriptmath/

This add-on registers the math functions from the standard C runtime library with the script 
engine. Use <code>RegisterScriptMath(asIScriptEngine*)</code> to perform the registration.

By defining the preprocessor word AS_USE_FLOAT=0, the functions will be registered to take 
and return doubles instead of floats.

The function <code>RegisterScriptMathComplex(asIScriptEngine*)</code> registers a type that 
represents a complex number, i.e. a number with real and imaginary parts.

\section doc_addon_math_1 Public script interface

<pre>
  // Trigonometric functions
  float cos(float rad);
  float sin(float rad);
  float tan(float rad);
  
  // Inverse trigonometric functions
  float acos(float val);
  float asin(float val);
  float atan(float val);
  float atan2(float y, float x);
  
  // Hyperbolic functions
  float cosh(float rad);
  float sinh(float rad);
  float tanh(float rad);
  
  // Logarithmic functions
  float log(float val);
  float log10(float val);
  
  // Power to
  float pow(float val, float exp);
  
  // Square root
  float sqrt(float val);

  // Absolute value
  float abs(float val);

  // Ceil and floor functions
  float ceil(float val);
  float floor(float val);
  
  // Returns the fraction
  float fraction(float val);
  
  // This type represents a complex number with real and imaginary parts
  class complex
  {
    // Constructors
    complex();
    complex(const complex &in);
    complex(float r, float i = 0);

    // Equality operator
    bool opEquals(const complex &in) const;

    // Compound assignment operators
    complex &opAddAssign(const complex &in);
    complex &opSubAssign(const complex &in);
    complex &opMulAssign(const complex &in);
    complex &opDivAssign(const complex &in);
    
    // Math operators
    complex opAdd(const complex &in) const;
    complex opSub(const complex &in) const;
    complex opMul(const complex &in) const;
    complex opDiv(const complex &in) const;
    
    // Returns the absolute value (magnitude)
    float abs() const;

    // Swizzle operators
    complex get_ri() const;
    void set_ri(const complex &in);
    complex get_ir() const;
    void set_ir(const complex &in);
    
    // The real and imaginary parts
    float r;
    float i;
  }
</pre>






\page doc_addon_build Script builder

<b>Path:</b> /sdk/add_on/scriptbuilder/

This class is a helper class for loading and building scripts, with a basic pre-processor 
that supports conditional compilation, include directives, and metadata declarations.

By default the script builder resolves include directives by loading the included file 
from the relative directory of the file it is included from. If you want to do this in another
way, then you should implement the \ref doc_addon_build_1_1 "include callback" which will
let you process the include directive in a custom way, e.g. to load the included file from 
memory, or to support multiple search paths. The include callback should call the AddSectionFromFile or
AddSectionFromMemory to include the section in the current build.

If you do not want process metadata then you can compile the add-on with the define 
AS_PROCESS_METADATA 0, which will exclude the code for processing this. This define
can be made in the project settings or directly in the header.


\section doc_addon_build_1 Public C++ interface

\code
class CScriptBuilder
{
public:
  // Start a new module
  int StartNewModule(asIScriptEngine *engine, const char *moduleName);

  // Load a script section from a file on disk
  int AddSectionFromFile(const char *filename);

  // Load a script section from memory
  int AddSectionFromMemory(const char *scriptCode, 
                           const char *sectionName = "");

  // Build the added script sections
  int BuildModule();

  // Returns the current module
  asIScriptModule *GetModule();

  // Register the callback for resolving include directive
  void SetIncludeCallback(INCLUDECALLBACK_t callback, void *userParam);

  // Add a pre-processor define for conditional compilation
  void DefineWord(const char *word);

  // Get metadata declared for class types and interfaces
  const char *GetMetadataStringForType(int typeId);

  // Get metadata declared for functions
  const char *GetMetadataStringForFunc(int funcId);

  // Get metadata declared for global variables
  const char *GetMetadataStringForVar(int varIdx);

  // Get metadata declared for a class method
  const char *GetMetadataStringForTypeMethod(int typeId, int mthdIdx);

  // Get metadata declared for a class property
  const char *GetMetadataStringForTypeProperty(int typeId, int varIdx);
};
\endcode

\subsection doc_addon_build_1_1 The include callback signature

\code
// This callback will be called for each #include directive encountered by the
// builder. The callback should call the AddSectionFromFile or AddSectionFromMemory
// to add the included section to the script. If the include cannot be resolved
// then the function should return a negative value to abort the compilation.
typedef int (*INCLUDECALLBACK_t)(const char *include, const char *from, CScriptBuilder *builder, void *userParam);
\endcode

\section doc_addon_build_2 Include directives

Example script with include directive:

<pre>
  \#include "commonfuncs.as"
  
  void main()
  {
    // Call a function from the included file
    CommonFunc();
  }
</pre>


\section doc_addon_build_condition Conditional programming

The builder supports conditional programming through the \#if/\#endif preprocessor directives.
The application may define a word with a call to DefineWord(), then the scripts may check
for this definition in the code in order to include/exclude parts of the code.

This is especially useful when scripts are shared between different binaries, for example, in a 
client/server application.

Example script with conditional compilation:

<pre>
  class CObject
  {
    void Process()
    {
  \#if SERVER
      // Do some server specific processing
  \#endif

  \#if CLIENT
      // Do some client specific processing
  \#endif 

      // Do some common processing
    }
  }
</pre>





\section doc_addon_build_metadata Metadata in scripts

Metadata can be added before script class, interface, function, and global variable 
declarations. The metadata is removed from the script by the builder class and stored
for post build lookup by the type id, function id, or variable index.

Exactly what the metadata looks like is up to the application. The builder class doesn't
impose any rules, except that the metadata should be added between brackets []. After 
the script has been built the application can obtain the metadata strings and interpret
them as it sees fit.

Example script with metadata:

<pre>
  [factory func = CreateOgre]
  class COgre
  {
    [editable] 
    vector3 myPosition;
    
    [editable [10, 100]]
    int     myStrength;
  }
  
  [factory]
  COgre \@CreateOgre()
  {
    return \@COgre();
  }
</pre>

Example usage:

\code
CScriptBuilder builder;
int r = builder.StartNewModule(engine, "my module");
if( r >= 0 )
  r = builder.AddSectionFromMemory(script);
if( r >= 0 )
  r = builder.BuildModule();
if( r >= 0 )
{
  // Find global variables that have been marked as editable by user
  asIScriptModule *mod = engine->GetModule("my module");
  int count = mod->GetGlobalVarCount();
  for( int n = 0; n < count; n++ )
  {
    string metadata = builder.GetMetadataStringForVar(n);
    if( metadata == "editable" )
    {
      // Show the global variable in a GUI
      ...
    }
  }
}
\endcode




\page doc_addon_autowrap Automatic wrapper functions

<b>Path:</b> /sdk/add_on/autowrapper/aswrappedcall.h

This header file declares some macros and template functions that will let the application developer
automatically generate wrapper functions using the \ref doc_generic "generic calling convention" with 
a simple call to a preprocessor macro. This is useful for those platforms where the native calling 
conventions are not yet supported.

The macros are defined as

\code
#define asDECLARE_FUNCTION_WRAPPER(wrapper_name,func)
#define asDECLARE_FUNCTION_WRAPPERPR(wrapper_name,func,params,rettype)
#define asDECLARE_METHOD_WRAPPER(wrapper_name,cl,func)
#define asDECLARE_METHOD_WRAPPERPR(wrapper_name,cl,func,params,rettype)
\endcode

where wrapper_name is the name of the function that you want to generate, and func is a function pointer 
to the function you want to wrap, cl is the class name, params is the parameter list, and rettype is the return type. 

Unfortunately the template functions needed to perform this generation are quite complex and older
compilers may not be able to handle them. One such example is Microsoft Visual C++ 6, though luckily 
it has no need for them as AngelScript already supports native calling conventions for it.

\section doc_addon_autowrap_1 Example usage

\code
#include "aswrappedcall.h"

// The application function that we want to register
void DoSomething(std::string param1, int param2);

// Generate the wrapper for the function
asDECLARE_FUNCTION_WRAPPER(DoSomething_Generic, DoSomething);

// Registering the wrapper with AngelScript
void RegisterWrapper(asIScriptEngine *engine)
{
  int r;

  r = engine->RegisterGlobalFunction("void DoSomething(string, int)", asFUNCTION(DoSomething_Generic), asCALL_GENERIC); assert( r >= 0 );
}
\endcode






\page doc_addon_clib ANSI C library interface

<b>Path:</b> /sdk/add_on/clib/

This add-on defines a pure C interface, that can be used in those applications that do not
understand C++ code but do understand C, e.g. Delphi, Java, and D.

To compile the AngelScript C library, you need to compile the library source files in sdk/angelscript/source together 
with the source files in sdk/add-on/clib, and link them as a shared dynamic library. In the application that will use 
the AngelScript C library, you'll include the <code>angelscript_c.h</code> header file, instead of the ordinary 
<code>%angelscript.h</code> header file. After that you can use the library much the same way that it's used in C++. 

To find the name of the C functions to call, you normally take the corresponding interface method
and give a prefix according to the following table:

<table border=0 cellspacing=0 cellpadding=0>
<tr><td><b>interface      &nbsp;</b></td><td><b>prefix&nbsp;</b></td></tr>
<tr><td>asIScriptEngine   &nbsp;</td>    <td>asEngine_</td></tr>
<tr><td>asIScriptModule   &nbsp;</td>    <td>asModule_</td></tr>
<tr><td>asIScriptContext  &nbsp;</td>    <td>asContext_</td></tr>
<tr><td>asIScriptGeneric  &nbsp;</td>    <td>asGeneric_</td></tr>
<tr><td>asIScriptObject   &nbsp;</td>    <td>asObject_</td></tr>
<tr><td>asIObjectType     &nbsp;</td>    <td>asObjectType_</td></tr>
<tr><td>asIScriptFunction &nbsp;</td>    <td>asFunction_</td></tr>
</table>

All interface methods take the interface pointer as the first parameter when in the C function format, the rest
of the parameters are the same as in the C++ interface. There are a few exceptions though, e.g. all parameters that
take an <code>asSFuncPtr</code> take a normal function pointer in the C function format. 

Example:

\code
#include <stdio.h>
#include <assert.h>
#include "angelscript_c.h"

void MessageCallback(asSMessageInfo *msg, void *);
void PrintSomething();

int main(int argc, char **argv)
{
  int r = 0;

  // Create and initialize the script engine
  asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
  r = asEngine_SetMessageCallback(engine, (asFUNCTION_t)MessageCallback, 0, asCALL_CDECL); assert( r >= 0 );
  r = asEngine_RegisterGlobalFunction(engine, "void print()", (asFUNCTION_t)PrintSomething, asCALL_CDECL); assert( r >= 0 );

  // Execute a simple script
  r = asEngine_ExecuteString(engine, 0, "print()", 0, 0);
  if( r != asEXECUTION_FINISHED )
  {
      printf("Something wen't wrong with the execution\n");
  }
  else
  {
      printf("The script was executed successfully\n");
  }

  // Release the script engine
  asEngine_Release(engine);
  
  return r;
}

void MessageCallback(asSMessageInfo *msg, void *)
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
\endcode



\page doc_addon_helpers Helper functions

<b>Path:</b> /sdk/add_on/scripthelper/

These helper functions simplify the implemention of common tasks. They can be used as is
or can serve as the starting point for your own framework.

\section doc_addon_helpers_1 Public C++ interface

\code
// Compare relation between two objects of the same type.
// Uses the object's opCmp method to perform the comparison.
// Returns a negative value if the comparison couldn't be performed.
int CompareRelation(asIScriptEngine *engine, void *leftObj, void *rightObj, int typeId, int &result);

// Compare equality between two objects of the same type.
// Uses the object's opEquals method to perform the comparison, or if that doesn't exist the opCmp method.
// Returns a negative value if the comparison couldn't be performed.
int CompareEquality(asIScriptEngine *engine, void *leftObj, void *rightObj, int typeId, bool &result);

// Compile and execute simple statements.
// The module is optional. If given the statements can access the entities compiled in the module.
// The caller can optionally provide its own context, for example if a context should be reused.
int ExecuteString(asIScriptEngine *engine, const char *code, asIScriptModule *mod = 0, asIScriptContext *ctx = 0);

// Write registered application interface to file.
// This function creates a file with the configuration for the offline compiler, asbuild, in the samples.
// If you wish to use the offline compiler you should call this function from you application after the 
// application interface has been fully registered. This way you will not have to create the configuration
// file manually.
int WriteConfigToFile(asIScriptEngine *engine, const char *filename);

// Print information on script exception to the standard output.
// Whenever the asIScriptContext::Execute method returns asEXECUTION_EXCEPTION, the application 
// can call this function to print some more information about that exception onto the standard
// output. The information obtained includes the current function, the script source section, 
// program position in the source section, and the exception description itself.
void PrintException(asIScriptContext *ctx, bool printStack = false);
\endcode

\section doc_addon_helpers_2 Example

To compare two script objects the application can execute the following code:

\code
void Compare(asIScriptObject *a, asIScriptObject *b)
{
  asIScriptEngine *engine = a->GetEngine();
  int typeId = a->GetTypeId();

  int cmp;
  int r = CompareRelation(engine, a, b, typeId, cmp);
  if( r < 0 )
  {
    cout << "The relation between a and b cannot be established b" << endl;
  }
  else
  {
    if( cmp < 0 )
      cout << "a is smaller than b" << endl;
    else if( cmp == 0 )
      cout << "a is equal to b" << endl;
    else
      cout << "a is greater than b" << endl;
  }
}
\endcode

*/  
