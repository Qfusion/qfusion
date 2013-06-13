#ifndef __ADDON_SCRIPTDICTIONARY_H__
#define __ADDON_SCRIPTDICTIONARY_H__

// The dictionary class relies on the script string object, thus the script
// string type must be registered with the engine before registering the
// dictionary type

#if defined ( __APPLE__ )
#include <angelscript/angelscript.h>
#else
#include <angelscript.h>
#endif
#include <string>

#ifdef _MSC_VER
// Turn off annoying warnings about truncated symbol names
#pragma warning (disable:4786)
#endif

#include <map>

BEGIN_AS_NAMESPACE

struct asstring_s;
typedef struct asstring_s asstring_t;

class CScriptDictionary : public CScriptDictionaryInterface
{
public:
    // Memory management
    CScriptDictionary(asIScriptEngine *engine);
    void AddRef() const;
    void Release() const;

    CScriptDictionary &operator =(const CScriptDictionary &other);

    // Sets/Gets a variable type value for a key
    void Set(const asstring_t *key, void *value, int typeId);
    bool Get(const asstring_t *key, void *value, int typeId) const;

    // Sets/Gets an integer number value for a key
    void Set(const asstring_t *key, qint64 &value);
    bool Get(const asstring_t *key, qint64 &value) const;

    // Sets/Gets a real number value for a key
    void Set(const asstring_t *key, double &value);
    bool Get(const asstring_t *key, double &value) const;

    // Returns true if the key is set
    bool Exists(const asstring_t *key) const;
    bool Empty() const;

    // Deletes the key
    void Delete(const asstring_t *key);

    // Deletes all keys
    void Clear();

    // Returns all keys in array
	CScriptArrayInterface *GetKeys();

	// Garbage collections behaviours
	int GetRefCount();
	void SetGCFlag();
	bool GetGCFlag();
	void EnumReferences(asIScriptEngine *engine);
	void ReleaseAllReferences(asIScriptEngine *engine);

protected:
	// The structure for holding the values
    struct valueStruct
    {
        union
        {
            asINT64 valueInt;
            double  valueFlt;
            void   *valueObj;
        };
        int   typeId;
    };
    
	// We don't want anyone to call the destructor directly, it should be called through the Release method
	virtual ~CScriptDictionary();

    // Sets/Gets a variable type value for a key
    void _Set(const char *key, void *value, int typeId);
    bool _Get(const char *key, void *value, int typeId) const;

	// Helper methods
    void FreeValue(valueStruct &value);
	
	// Our properties
    asIScriptEngine *engine;
    mutable int refCount;
    std::map<std::string, valueStruct> dict;
};

void PreRegisterDictionaryAddon(asIScriptEngine *engine);
void RegisterDictionaryAddon(asIScriptEngine *engine);

END_AS_NAMESPACE

#endif
