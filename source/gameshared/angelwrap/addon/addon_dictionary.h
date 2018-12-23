#ifndef SCRIPTDICTIONARY_H
#define SCRIPTDICTIONARY_H

// The dictionary class relies on the script string object, thus the script
// string type must be registered with the engine before registering the
// dictionary type

#ifndef ANGELSCRIPT_H
// Avoid having to inform include path if header is already include before
#include <angelscript.h>
#endif

#include <string>

#ifdef _MSC_VER
// Turn off annoying warnings about truncated symbol names
#pragma warning (disable:4786)
#endif

#include <map>

// Sometimes it may be desired to use the same method names as used by C++ STL.
// This may for example reduce time when converting code from script to C++ or
// back.
//
//  0 = off
//  1 = on

#ifndef AS_USE_STLNAMES
#define AS_USE_STLNAMES 0
#endif


BEGIN_AS_NAMESPACE

struct asstring_s;
typedef struct asstring_s asstring_t;

class CScriptDictionary : public CScriptDictionaryInterface
{
public:
	// Memory management
	CScriptDictionary( asIScriptEngine *engine );
	CScriptDictionary( asBYTE *buffer );
	CScriptDictionary( const CScriptDictionary &other );
	void AddRef() const;
	void Release() const;

	CScriptDictionary &operator =( const CScriptDictionary &other );

	// Sets/Gets a variable type value for a key
	void Set( const asstring_t &key, void *value, int typeId );
	bool Get( const asstring_t &key, void *value, int typeId ) const;

	// Sets/Gets an integer number value for a key
	void Set( const asstring_t &key, int64_t &value );
	bool Get( const asstring_t &key, int64_t &value ) const;

	// Sets/Gets a real number value for a key
	void Set( const asstring_t &key, double &value );
	bool Get( const asstring_t &key, double &value ) const;

	void Set( const asstring_t &key, asstring_t *value );
	bool Get( const asstring_t &key, asstring_t *value ) const;

	// Returns true if the key is set
	bool Exists( const asstring_t &key ) const;
	bool IsEmpty() const;
	asUINT GetSize() const;

	// Deletes the key
	void Delete( const asstring_t &key );

	// Deletes all keys
	void DeleteAll();

	// Get an array of all keys
	CScriptArrayInterface *GetKeys() const;

	// Garbage collections behaviours
	int GetRefCount();
	void SetGCFlag();
	bool GetGCFlag();
	void EnumReferences( asIScriptEngine *engine );
	void ReleaseAllReferences( asIScriptEngine *engine );

protected:
	// The structure for holding the values
	struct valueStruct {
		union {
			asINT64 valueInt;
			double valueFlt;
			void   *valueObj;
		};
		int typeId;
	};

	// We don't want anyone to call the destructor directly, it should be called through the Release method
	virtual ~CScriptDictionary();

	// Helper methods
	void FreeValue( valueStruct &value );
	void Initialize( asIScriptEngine *engine );

	void Set_( const char *key, void *value, int typeId );

	// Our properties
	asIScriptEngine *engine;
	mutable int refCount;
	mutable bool gcFlag;

	// TODO: optimize: Use C++11 std::unordered_map instead
	std::map<std::string, valueStruct> dict;
};

// This function will determine the configuration of the engine
// and use one of the two functions below to register the dictionary object
void RegisterScriptDictionary( asIScriptEngine *engine );
void PreRegisterScriptDictionary( asIScriptEngine *engine );

END_AS_NAMESPACE

#endif
