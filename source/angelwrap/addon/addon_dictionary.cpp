#include "../qas_precompiled.h"
#include "addon_dictionary.h"
#include "addon_scriptarray.h"
#include "addon_string.h"

BEGIN_AS_NAMESPACE

using namespace std;

static void RegisterScriptDictionary_Native(asIScriptEngine *engine);

static asIObjectType *DictionaryType;
static asIObjectType *StringsArrayObjectType;

//--------------------------------------------------------------------------
// CScriptDictionary implementation

CScriptDictionary::CScriptDictionary(asIScriptEngine *engine)
{
	assert( DictionaryType != NULL );

    // We start with one reference
    refCount = 1;

    // Keep a reference to the engine for as long as we live
	// We don't increment the reference counter, because the 
	// engine will hold a pointer to the object. 
    this->engine = engine;

	// Notify the garbage collector of this object
	engine->NotifyGarbageCollectorOfNewObject(this, DictionaryType);
}

CScriptDictionary::~CScriptDictionary()
{
    // Delete all keys and values
    Clear();
}

void CScriptDictionary::AddRef() const
{
	// We need to clear the GC flag
	refCount = (refCount & 0x7FFFFFFF) + 1;
}

void CScriptDictionary::Release() const
{
	// We need to clear the GC flag
	refCount = (refCount & 0x7FFFFFFF) - 1;
	if( refCount == 0 )
        QAS_DELETE(const_cast<CScriptDictionary*>(this), CScriptDictionary);
}

int CScriptDictionary::GetRefCount()
{
	return refCount & 0x7FFFFFFF;
}

void CScriptDictionary::SetGCFlag()
{
	refCount |= 0x80000000;
}

bool CScriptDictionary::GetGCFlag()
{
	return (refCount & 0x80000000) ? true : false;
}

void CScriptDictionary::EnumReferences(asIScriptEngine *engine)
{
	// Call the gc enum callback for each of the objects
    map<string, valueStruct>::iterator it;
    for( it = dict.begin(); it != dict.end(); it++ )
    {
		if( it->second.typeId & asTYPEID_MASK_OBJECT )
			engine->GCEnumCallback(it->second.valueObj);
    }
}

void CScriptDictionary::ReleaseAllReferences(asIScriptEngine * /*engine*/)
{
	// We're being told to release all references in 
	// order to break circular references for dead objects
	Clear();
}

CScriptDictionary &CScriptDictionary::operator =(const CScriptDictionary &other)
{
	// Clear everything we had before
	Clear();

	// Do a shallow copy of the dictionary
    map<string, valueStruct>::const_iterator it;
    for( it = other.dict.begin(); it != other.dict.end(); it++ )
    {
		if( it->second.typeId & asTYPEID_OBJHANDLE )
			_Set(it->first.c_str(), (void*)&it->second.valueObj, it->second.typeId);
		else if( it->second.typeId & asTYPEID_MASK_OBJECT )
			_Set(it->first.c_str(), (void*)it->second.valueObj, it->second.typeId);
		else
			_Set(it->first.c_str(), (void*)&it->second.valueInt, it->second.typeId);
    }

    return *this;
}

void CScriptDictionary::_Set(const char *key, void *value, int typeId)
{
	valueStruct valStruct = {{0},0};
	valStruct.typeId = typeId;
	if( typeId & asTYPEID_OBJHANDLE )
	{
		// We're receiving a reference to the handle, so we need to dereference it
		valStruct.valueObj = *(void**)value;
		engine->AddRefScriptObject(valStruct.valueObj, typeId);
	}
	else if( typeId & asTYPEID_MASK_OBJECT )
	{
		// Create a copy of the object
		valStruct.valueObj = engine->CreateScriptObjectCopy(value, typeId);
	}
	else
	{
		// Copy the primitive value
		// We receive a pointer to the value.
		int size = engine->GetSizeOfPrimitiveType(typeId);
		memcpy(&valStruct.valueInt, value, size);
	}

    map<string, valueStruct>::iterator it;
	it = dict.find(key);
    if( it != dict.end() )
    {
        FreeValue(it->second);

        // Insert the new value
        it->second = valStruct;
    }
    else
    {
		dict.insert(map<string, valueStruct>::value_type(key, valStruct));
    }
}

void CScriptDictionary::Set(const asstring_t *key, void *value, int typeId)
{
	_Set(key->buffer, value, typeId);
}

// This overloaded method is implemented so that all integer and
// unsigned integers types will be stored in the dictionary as int64
// through implicit conversions. This simplifies the management of the
// numeric types when the script retrieves the stored value using a 
// different type.
void CScriptDictionary::Set(const asstring_t *key, qint64 &value)
{
	_Set(key->buffer, &value, asTYPEID_INT64);
}

// This overloaded method is implemented so that all floating point types 
// will be stored in the dictionary as double through implicit conversions. 
// This simplifies the management of the numeric types when the script 
// retrieves the stored value using a different type.
void CScriptDictionary::Set(const asstring_t *key, double &value)
{
	_Set(key->buffer, &value, asTYPEID_DOUBLE);
}

// Returns true if the value was successfully retrieved
bool CScriptDictionary::_Get(const char *key, void *value, int typeId) const
{
    map<string, valueStruct>::const_iterator it;
	it = dict.find(key);
    if( it != dict.end() )
    {
        // Return the value
		if( typeId & asTYPEID_OBJHANDLE )
		{
			// A handle can be retrieved if the stored type is a handle of same or compatible type
			// or if the stored type is an object that implements the interface that the handle refer to.
			if( (it->second.typeId & asTYPEID_MASK_OBJECT) && 
				engine->IsHandleCompatibleWithObject(it->second.valueObj, it->second.typeId, typeId) )
			{
				engine->AddRefScriptObject(it->second.valueObj, it->second.typeId);
				*(void**)value = it->second.valueObj;

				return true;
			}
		}
		else if( typeId & asTYPEID_MASK_OBJECT )
		{
			// Verify that the copy can be made
			bool isCompatible = false;
			if( it->second.typeId == typeId )
				isCompatible = true;

			// Copy the object into the given reference
			if( isCompatible )
			{
				engine->CopyScriptObject(value, it->second.valueObj, typeId);

				return true;
			}
		}
		else
		{
			if( it->second.typeId == typeId )
			{
				int size = engine->GetSizeOfPrimitiveType(typeId);
				memcpy(value, &it->second.valueInt, size);
				return true;
			}

			// We know all numbers are stored as either int64 or double, since we register overloaded functions for those
			if( it->second.typeId == asTYPEID_INT64 && typeId == asTYPEID_DOUBLE )
			{
				*(double*)value = double(it->second.valueInt);
				return true;
			}
			else if( it->second.typeId == asTYPEID_DOUBLE && typeId == asTYPEID_INT64 )
			{
				*(asINT64*)value = asINT64(it->second.valueFlt);
				return true;
			}
		}
    }

    // AngelScript has already initialized the value with a default value,
    // so we don't have to do anything if we don't find the element, or if 
	// the element is incompatible with the requested type.

	return false;
}

bool CScriptDictionary::Get(const asstring_t *key, void *value, int typeId) const
{
	return _Get(key->buffer, value, typeId);
}

bool CScriptDictionary::Get(const asstring_t *key, qint64 &value) const
{
	return _Get(key->buffer, &value, asTYPEID_INT64);
}

bool CScriptDictionary::Get(const asstring_t *key, double &value) const
{
	return _Get(key->buffer, &value, asTYPEID_DOUBLE);
}

bool CScriptDictionary::Exists(const asstring_t *key) const
{
    map<string, valueStruct>::const_iterator it;
	it = dict.find(key->buffer);
    if( it != dict.end() )
    {
        return true;
    }

    return false;
}

void CScriptDictionary::Delete(const asstring_t *key)
{
    map<string, valueStruct>::iterator it;
    it = dict.find(key->buffer);
    if( it != dict.end() )
    {
        FreeValue(it->second);

        dict.erase(it);
    }
}

void CScriptDictionary::Clear()
{
    map<string, valueStruct>::iterator it;
    for( it = dict.begin(); it != dict.end(); it++ )
    {
        FreeValue(it->second);
    }

    dict.clear();
}

bool CScriptDictionary::Empty() const
{
    return dict.empty();
}

void CScriptDictionary::FreeValue(valueStruct &value)
{
    // If it is a handle or a ref counted object, call release
	if( value.typeId & asTYPEID_MASK_OBJECT )
	{
		// Let the engine release the object
		engine->ReleaseScriptObject(value.valueObj, value.typeId);
		value.valueObj = 0;
		value.typeId = 0;
	}

    // For primitives, there's nothing to do
}

CScriptArrayInterface *CScriptDictionary::GetKeys()
{
	CScriptArrayInterface *arr = QAS_NEW(CScriptArray)(dict.size(), StringsArrayObjectType);

	int n = 0;
    map<string, valueStruct>::iterator it;
    for( it = dict.begin(); it != dict.end(); it++ )
    {
		const char *c_str = it->first.c_str();
		asstring_t *str = objectString_FactoryBuffer( c_str, strlen( c_str ) );
		*((asstring_t **)arr->At(n++)) = str;
	}

	return arr;
}

static void ScriptDictionaryFactory(asIScriptGeneric *gen)
{
    *(CScriptDictionary**)gen->GetAddressOfReturnLocation() = QAS_NEW(CScriptDictionary)(gen->GetEngine());
}

//--------------------------------------------------------------------------
// Register the type

static void CacheObjectTypes(asIScriptEngine *engine)
{
	DictionaryType = engine->GetObjectTypeByName("Dictionary");
	StringsArrayObjectType = engine->GetObjectTypeById(engine->GetTypeIdByDecl("array<String @>"));
}

void PreRegisterDictionaryAddon(asIScriptEngine *engine)
{
	int r;

    r = engine->RegisterObjectType("Dictionary", sizeof(CScriptDictionary), asOBJ_REF | asOBJ_GC); assert( r >= 0 );
}

void RegisterDictionaryAddon(asIScriptEngine *engine)
{
	RegisterScriptDictionary_Native(engine);

	CacheObjectTypes(engine);
}

static void RegisterScriptDictionary_Native(asIScriptEngine *engine)
{
	int r;

	// Use the generic interface to construct the object since we need the engine pointer, we could also have retrieved the engine pointer from the active context
    r = engine->RegisterObjectBehaviour("Dictionary", asBEHAVE_FACTORY, "Dictionary@ f()", asFUNCTION(ScriptDictionaryFactory), asCALL_GENERIC); assert( r>= 0 );
    r = engine->RegisterObjectBehaviour("Dictionary", asBEHAVE_ADDREF, "void f()", asMETHOD(CScriptDictionary,AddRef), asCALL_THISCALL); assert( r >= 0 );
    r = engine->RegisterObjectBehaviour("Dictionary", asBEHAVE_RELEASE, "void f()", asMETHOD(CScriptDictionary,Release), asCALL_THISCALL); assert( r >= 0 );

	r = engine->RegisterObjectMethod("Dictionary", "Dictionary &opAssign(const Dictionary &in)", asMETHODPR(CScriptDictionary, operator=, (const CScriptDictionary &), CScriptDictionary&), asCALL_THISCALL); assert( r >= 0 );

    r = engine->RegisterObjectMethod("Dictionary", "void set(const String &in, ?&in)", asMETHODPR(CScriptDictionary,Set,(const asstring_t *,void*,int),void), asCALL_THISCALL); assert( r >= 0 );
    r = engine->RegisterObjectMethod("Dictionary", "bool get(const String &in, ?&out) const", asMETHODPR(CScriptDictionary,Get,(const asstring_t *,void*,int) const,bool), asCALL_THISCALL); assert( r >= 0 );

    r = engine->RegisterObjectMethod("Dictionary", "void set(const String &in, int64&in)", asMETHODPR(CScriptDictionary,Set,(const asstring_t *,qint64&),void), asCALL_THISCALL); assert( r >= 0 );
    r = engine->RegisterObjectMethod("Dictionary", "bool get(const String &in, int64&out) const", asMETHODPR(CScriptDictionary,Get,(const asstring_t *,qint64&) const,bool), asCALL_THISCALL); assert( r >= 0 );

    r = engine->RegisterObjectMethod("Dictionary", "void set(const String &in, double&in)", asMETHODPR(CScriptDictionary,Set,(const asstring_t *,double&),void), asCALL_THISCALL); assert( r >= 0 );
    r = engine->RegisterObjectMethod("Dictionary", "bool get(const String &in, double&out) const", asMETHODPR(CScriptDictionary,Get,(const asstring_t *,double&) const,bool), asCALL_THISCALL); assert( r >= 0 );
    
	r = engine->RegisterObjectMethod("Dictionary", "bool exists(const String &in) const", asMETHOD(CScriptDictionary,Exists), asCALL_THISCALL); assert( r >= 0 );
    r = engine->RegisterObjectMethod("Dictionary", "void delete(const String &in)", asMETHOD(CScriptDictionary,Delete), asCALL_THISCALL); assert( r >= 0 );
    r = engine->RegisterObjectMethod("Dictionary", "void clear()", asMETHOD(CScriptDictionary,Clear), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectMethod("Dictionary", "bool empty() const", asMETHOD(CScriptDictionary,Empty), asCALL_THISCALL); assert( r >= 0 );

    r = engine->RegisterObjectMethod("Dictionary", "array<String @> @getKeys() const", asMETHOD(CScriptDictionary,GetKeys), asCALL_THISCALL); assert( r >= 0 );

	// Register GC behaviours
	r = engine->RegisterObjectBehaviour("Dictionary", asBEHAVE_GETREFCOUNT, "int f()", asMETHOD(CScriptDictionary,GetRefCount), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("Dictionary", asBEHAVE_SETGCFLAG, "void f()", asMETHOD(CScriptDictionary,SetGCFlag), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("Dictionary", asBEHAVE_GETGCFLAG, "bool f()", asMETHOD(CScriptDictionary,GetGCFlag), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("Dictionary", asBEHAVE_ENUMREFS, "void f(int&in)", asMETHOD(CScriptDictionary,EnumReferences), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("Dictionary", asBEHAVE_RELEASEREFS, "void f(int&in)", asMETHOD(CScriptDictionary,ReleaseAllReferences), asCALL_THISCALL); assert( r >= 0 );
}

END_AS_NAMESPACE
