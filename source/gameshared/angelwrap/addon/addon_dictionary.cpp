#include "../qas_local.h"
#include "addon_dictionary.h"
#include "addon_scriptarray.h"
#include "addon_string.h"

BEGIN_AS_NAMESPACE

using namespace std;

//--------------------------------------------------------------------------
// CScriptDictionary implementation

CScriptDictionary::CScriptDictionary( asIScriptEngine *engine ) {
	Initialize( engine );
}

CScriptDictionary::CScriptDictionary( asBYTE *buffer ) {
	// We start with one reference
	refCount = 1;
	gcFlag = false;

	// This constructor will always be called from a script
	// so we can get the engine from the active context
	asIScriptContext *ctx = asGetActiveContext();
	engine = ctx->GetEngine();

	// Notify the garbage collector of this object
	// TODO: The type id should be cached
	engine->NotifyGarbageCollectorOfNewObject( this, engine->GetObjectTypeByName( "Dictionary" ) );

	// Initialize the dictionary from the buffer
	asUINT length = *(asUINT*)buffer;
	buffer += 4;

	while( length-- ) {
		// Align the buffer pointer on a 4 byte boundary in
		// case previous value was smaller than 4 bytes
		if( asPWORD( buffer ) & 0x3 ) {
			buffer += 4 - ( asPWORD( buffer ) & 0x3 );
		}

		// Get the name value pair from the buffer and insert it in the dictionary
		asstring_t name = *(asstring_t*)buffer;
		buffer += sizeof( asstring_t );

		// Get the type id of the value
		int typeId = *(int*)buffer;
		buffer += sizeof( int );

		// Depending on the type id, the value will inline in the buffer or a pointer
		void *ref = (void*)buffer;

		if( typeId >= asTYPEID_INT8 && typeId <= asTYPEID_DOUBLE ) {
			// Convert primitive values to either int64 or double, so we can use the overloaded Set methods
			int64_t i64;
			double d;
			switch( typeId ) {
				case asTYPEID_INT8: i64 = *(char*)ref; break;
				case asTYPEID_INT16: i64 = *(short*)ref; break;
				case asTYPEID_INT32: i64 = *(int*)ref; break;
				case asTYPEID_INT64: i64 = *(int64_t*)ref; break;
				case asTYPEID_UINT8: i64 = *(unsigned char*)ref; break;
				case asTYPEID_UINT16: i64 = *(unsigned short*)ref; break;
				case asTYPEID_UINT32: i64 = *(unsigned int*)ref; break;
				case asTYPEID_UINT64: i64 = *(int64_t*)ref; break;
				case asTYPEID_FLOAT: d = *(float*)ref; break;
				case asTYPEID_DOUBLE: d = *(double*)ref; break;
			}

			if( typeId >= asTYPEID_FLOAT ) {
				Set( name, d );
			} else {
				Set( name, i64 );
			}
		} else {
			if( ( typeId & asTYPEID_MASK_OBJECT ) &&
				!( typeId & asTYPEID_OBJHANDLE ) &&
				( engine->GetObjectTypeById( typeId )->GetFlags() & asOBJ_REF ) ) {
				// Dereference the pointer to get the reference to the actual object
				ref = *(void**)ref;
			}

			Set( name, ref, typeId );
		}

		// Advance the buffer pointer with the size of the value
		if( typeId & asTYPEID_MASK_OBJECT ) {
			asIObjectType *ot = engine->GetObjectTypeById( typeId );
			if( ot->GetFlags() & asOBJ_VALUE ) {
				buffer += ot->GetSize();
			} else {
				buffer += sizeof( void* );
			}
		} else if( typeId == 0 ) {
			// null pointer
			buffer += sizeof( void* );
		} else {
			buffer += engine->GetSizeOfPrimitiveType( typeId );
		}
	}
}

CScriptDictionary::CScriptDictionary( const CScriptDictionary &other ) {
	Initialize( other.engine );
	this->operator=( other );
}

void CScriptDictionary::Initialize( asIScriptEngine *engine ) {
	// We start with one reference
	refCount = 1;
	gcFlag = false;

	// Keep a reference to the engine for as long as we live
	// We don't increment the reference counter, because the
	// engine will hold a pointer to the object.
	this->engine = engine;

	// Notify the garbage collector of this object
	// TODO: The type id should be cached
	engine->NotifyGarbageCollectorOfNewObject( this, engine->GetObjectTypeByName( "Dictionary" ) );
}

CScriptDictionary::~CScriptDictionary() {
	// Delete all keys and values
	DeleteAll();
}

void CScriptDictionary::AddRef() const {
	// We need to clear the GC flag
	gcFlag = false;
	asAtomicInc( refCount );
}

void CScriptDictionary::Release() const {
	// We need to clear the GC flag
	gcFlag = false;
	if( asAtomicDec( refCount ) == 0 ) {
		QAS_DELETE( const_cast<CScriptDictionary*>( this ), CScriptDictionary );
	}
}

int CScriptDictionary::GetRefCount() {
	return refCount;
}

void CScriptDictionary::SetGCFlag() {
	gcFlag = true;
}

bool CScriptDictionary::GetGCFlag() {
	return gcFlag;
}

void CScriptDictionary::EnumReferences( asIScriptEngine *engine ) {
	// Call the gc enum callback for each of the objects
	map<std::string, valueStruct>::iterator it;
	for( it = dict.begin(); it != dict.end(); it++ ) {
		if( it->second.typeId & asTYPEID_MASK_OBJECT ) {
			engine->GCEnumCallback( it->second.valueObj );
		}
	}
}

void CScriptDictionary::ReleaseAllReferences( asIScriptEngine * /*engine*/ ) {
	// We're being told to release all references in
	// order to break circular references for dead objects
	DeleteAll();
}

CScriptDictionary &CScriptDictionary::operator =( const CScriptDictionary &other ) {
	// Clear everything we had before
	DeleteAll();

	// Do a shallow copy of the dictionary
	map<std::string, valueStruct>::const_iterator it;
	for( it = other.dict.begin(); it != other.dict.end(); it++ ) {
		if( it->second.typeId & asTYPEID_OBJHANDLE ) {
			Set_( it->first.c_str(), (void*)&it->second.valueObj, it->second.typeId );
		} else if( it->second.typeId & asTYPEID_MASK_OBJECT ) {
			Set_( it->first.c_str(), (void*)it->second.valueObj, it->second.typeId );
		} else {
			Set_( it->first.c_str(), (void*)&it->second.valueInt, it->second.typeId );
		}
	}

	return *this;
}

void CScriptDictionary::Set_( const char *key, void *value, int typeId ) {
	valueStruct valStruct = {{0},0};
	valStruct.typeId = typeId;
	if( typeId & asTYPEID_OBJHANDLE ) {
		// We're receiving a reference to the handle, so we need to dereference it
		valStruct.valueObj = *(void**)value;
		engine->AddRefScriptObject( valStruct.valueObj, engine->GetObjectTypeById( typeId ) );
	} else if( typeId & asTYPEID_MASK_OBJECT ) {
		// Create a copy of the object
		valStruct.valueObj = engine->CreateScriptObjectCopy( value, engine->GetObjectTypeById( typeId ) );
	} else {
		// Copy the primitive value
		// We receive a pointer to the value.
		int size = engine->GetSizeOfPrimitiveType( typeId );
		memcpy( &valStruct.valueInt, value, size );
	}

	map<std::string, valueStruct>::iterator it;
	it = dict.find( key );
	if( it != dict.end() ) {
		FreeValue( it->second );

		// Insert the new value
		it->second = valStruct;
	} else {
		dict.insert( map<std::string, valueStruct>::value_type( key, valStruct ) );
	}
}

void CScriptDictionary::Set( const asstring_t &key, void *value, int typeId ) {
	Set_( key.buffer, value, typeId );
}

void CScriptDictionary::Set( const asstring_t &key, asstring_t *value ) {
	return Set_( key.buffer, value, engine->GetTypeIdByDecl( "String" ) );
}

// This overloaded method is implemented so that all integer and
// unsigned integers types will be stored in the dictionary as int64
// through implicit conversions. This simplifies the management of the
// numeric types when the script retrieves the stored value using a
// different type.
void CScriptDictionary::Set( const asstring_t &key, int64_t &value ) {
	Set( key, &value, asTYPEID_INT64 );
}

// This overloaded method is implemented so that all floating point types
// will be stored in the dictionary as double through implicit conversions.
// This simplifies the management of the numeric types when the script
// retrieves the stored value using a different type.
void CScriptDictionary::Set( const asstring_t &key, double &value ) {
	Set( key, &value, asTYPEID_DOUBLE );
}

// Returns true if the value was successfully retrieved
bool CScriptDictionary::Get( const asstring_t &key, void *value, int typeId ) const {
	map<std::string, valueStruct>::const_iterator it;
	it = dict.find( key.buffer );
	if( it != dict.end() ) {
		// Return the value
		if( typeId & asTYPEID_OBJHANDLE ) {
			// A handle can be retrieved if the stored type is a handle of same or compatible type
			// or if the stored type is an object that implements the interface that the handle refer to.
			if( ( it->second.typeId & asTYPEID_MASK_OBJECT ) &&
				engine->IsHandleCompatibleWithObject( it->second.valueObj, it->second.typeId, typeId ) ) {
				engine->AddRefScriptObject( it->second.valueObj, engine->GetObjectTypeById( it->second.typeId ) );
				*(void**)value = it->second.valueObj;

				return true;
			}
		} else if( typeId & asTYPEID_MASK_OBJECT ) {
			// Verify that the copy can be made
			bool isCompatible = false;
			if( it->second.typeId == typeId ) {
				isCompatible = true;
			}

			// Copy the object into the given reference
			if( isCompatible ) {
				engine->AssignScriptObject( value, it->second.valueObj, engine->GetObjectTypeById( typeId ) );

				return true;
			}
		} else {
			if( it->second.typeId == typeId ) {
				int size = engine->GetSizeOfPrimitiveType( typeId );
				memcpy( value, &it->second.valueInt, size );
				return true;
			}

			// We know all numbers are stored as either int64 or double, since we register overloaded functions for those
			if( it->second.typeId == asTYPEID_INT64 && typeId == asTYPEID_DOUBLE ) {
				*(double*)value = double(it->second.valueInt);
				return true;
			} else if( it->second.typeId == asTYPEID_DOUBLE && typeId == asTYPEID_INT64 ) {
				*(asINT64*)value = asINT64( it->second.valueFlt );
				return true;
			}
		}
	}

	// AngelScript has already initialized the value with a default value,
	// so we don't have to do anything if we don't find the element, or if
	// the element is incompatible with the requested type.

	return false;
}

bool CScriptDictionary::Get( const asstring_t &key, int64_t &value ) const {
	return Get( key, &value, asTYPEID_INT64 );
}

bool CScriptDictionary::Get( const asstring_t &key, double &value ) const {
	return Get( key, &value, asTYPEID_DOUBLE );
}

bool CScriptDictionary::Get( const asstring_t &key, asstring_t *value ) const {
	return Get( key, value, engine->GetTypeIdByDecl( "String" ) );
}

bool CScriptDictionary::Exists( const asstring_t &key ) const {
	map<std::string, valueStruct>::const_iterator it;
	it = dict.find( key.buffer );
	if( it != dict.end() ) {
		return true;
	}

	return false;
}

bool CScriptDictionary::IsEmpty() const {
	if( dict.size() == 0 ) {
		return true;
	}

	return false;
}

asUINT CScriptDictionary::GetSize() const {
	return asUINT( dict.size() );
}

void CScriptDictionary::Delete( const asstring_t &key ) {
	map<std::string, valueStruct>::iterator it;
	it = dict.find( key.buffer );
	if( it != dict.end() ) {
		FreeValue( it->second );
		dict.erase( it );
	}
}

void CScriptDictionary::DeleteAll() {
	map<std::string, valueStruct>::iterator it;
	for( it = dict.begin(); it != dict.end(); it++ )
		FreeValue( it->second );

	dict.clear();
}

void CScriptDictionary::FreeValue( valueStruct &value ) {
	// If it is a handle or a ref counted object, call release
	if( value.typeId & asTYPEID_MASK_OBJECT ) {
		// Let the engine release the object
		engine->ReleaseScriptObject( value.valueObj, engine->GetObjectTypeById( value.typeId ) );
		value.valueObj = 0;
		value.typeId = 0;
	}

	// For primitives, there's nothing to do
}

CScriptArrayInterface * CScriptDictionary::GetKeys() const {
	// TODO: optimize: The string array type should only be determined once.
	//                 It should be recomputed when registering the dictionary class.
	//                 Only problem is if multiple engines are used, as they may not
	//                 share the same type id. Alternatively it can be stored in the
	//                 user data for the dictionary type.
	int stringArrayType = engine->GetTypeIdByDecl( "array<String @>" );
	asIObjectType *ot = engine->GetObjectTypeById( stringArrayType );

	// Create the array object
	CScriptArrayInterface *arr = QAS_NEW( CScriptArray )( dict.size(), ot );
	int n = 0;
	std::map<std::string, valueStruct>::const_iterator it;
	for( it = dict.begin(); it != dict.end(); it++ ) {
		const char *c_str = it->first.c_str();
		asstring_t *str = objectString_FactoryBuffer( c_str, strlen( c_str ) );
		*( (asstring_t **)arr->At( n++ ) ) = str;
	}

	return arr;
}

//--------------------------------------------------------------------------
// Generic wrappers

void ScriptDictionaryFactory_Generic( asIScriptGeneric *gen ) {
	*(CScriptDictionary**)gen->GetAddressOfReturnLocation() = QAS_NEW( CScriptDictionary )( gen->GetEngine() );
}

void ScriptDictionaryListFactory_Generic( asIScriptGeneric *gen ) {
	asBYTE *buffer = (asBYTE*)gen->GetArgAddress( 0 );
	*(CScriptDictionary**)gen->GetAddressOfReturnLocation() = QAS_NEW( CScriptDictionary )( buffer );
}

void ScriptDictionaryAddRef_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *dict = (CScriptDictionary*)gen->GetObject();
	dict->AddRef();
}

void ScriptDictionaryRelease_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *dict = (CScriptDictionary*)gen->GetObject();
	dict->Release();
}

void ScriptDictionaryAssign_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *dict = (CScriptDictionary*)gen->GetObject();
	CScriptDictionary *other = *(CScriptDictionary**)gen->GetAddressOfArg( 0 );
	*dict = *other;
	*(CScriptDictionary**)gen->GetAddressOfReturnLocation() = dict;
}

void ScriptDictionarySet_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *dict = (CScriptDictionary*)gen->GetObject();
	asstring_t *key = *(asstring_t**)gen->GetAddressOfArg( 0 );
	void *ref = *(void**)gen->GetAddressOfArg( 1 );
	int typeId = gen->GetArgTypeId( 1 );
	dict->Set( *key, ref, typeId );
}

void ScriptDictionarySetInt_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *dict = (CScriptDictionary*)gen->GetObject();
	asstring_t *key = *(asstring_t**)gen->GetAddressOfArg( 0 );
	void *ref = *(void**)gen->GetAddressOfArg( 1 );
	dict->Set( *key, *(int64_t*)ref );
}

void ScriptDictionarySetFlt_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *dict = (CScriptDictionary*)gen->GetObject();
	asstring_t *key = *(asstring_t**)gen->GetAddressOfArg( 0 );
	void *ref = *(void**)gen->GetAddressOfArg( 1 );
	dict->Set( *key, *(double*)ref );
}

void ScriptDictionarySetString_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *dict = (CScriptDictionary*)gen->GetObject();
	asstring_t *key = *(asstring_t**)gen->GetAddressOfArg( 0 );
	asstring_t *value = *(asstring_t**)gen->GetAddressOfArg( 1 );
	dict->Set( *key, value );
}

void ScriptDictionaryGet_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *dict = (CScriptDictionary*)gen->GetObject();
	asstring_t *key = *(asstring_t**)gen->GetAddressOfArg( 0 );
	void *ref = *(void**)gen->GetAddressOfArg( 1 );
	int typeId = gen->GetArgTypeId( 1 );
	*(bool*)gen->GetAddressOfReturnLocation() = dict->Get( *key, ref, typeId );
}

void ScriptDictionaryGetInt_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *dict = (CScriptDictionary*)gen->GetObject();
	asstring_t *key = *(asstring_t**)gen->GetAddressOfArg( 0 );
	void *ref = *(void**)gen->GetAddressOfArg( 1 );
	*(bool*)gen->GetAddressOfReturnLocation() = dict->Get( *key, *(int64_t*)ref );
}

void ScriptDictionaryGetFlt_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *dict = (CScriptDictionary*)gen->GetObject();
	asstring_t *key = *(asstring_t**)gen->GetAddressOfArg( 0 );
	void *ref = *(void**)gen->GetAddressOfArg( 1 );
	*(bool*)gen->GetAddressOfReturnLocation() = dict->Get( *key, *(double*)ref );
}

void ScriptDictionaryGetString_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *dict = (CScriptDictionary*)gen->GetObject();
	asstring_t *key = *(asstring_t**)gen->GetAddressOfArg( 0 );
	void *ref = *(void**)gen->GetAddressOfArg( 1 );
	*(bool*)gen->GetAddressOfReturnLocation() = dict->Get( *key, (asstring_t *)ref );
}

void ScriptDictionaryExists_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *dict = (CScriptDictionary*)gen->GetObject();
	asstring_t *key = *(asstring_t**)gen->GetAddressOfArg( 0 );
	bool ret = dict->Exists( *key );
	*(bool*)gen->GetAddressOfReturnLocation() = ret;
}

void ScriptDictionaryDelete_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *dict = (CScriptDictionary*)gen->GetObject();
	asstring_t *key = *(asstring_t**)gen->GetAddressOfArg( 0 );
	dict->Delete( *key );
}

void ScriptDictionaryDeleteAll_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *dict = (CScriptDictionary*)gen->GetObject();
	dict->DeleteAll();
}

static void ScriptDictionaryGetRefCount_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *self = (CScriptDictionary*)gen->GetObject();
	*(int*)gen->GetAddressOfReturnLocation() = self->GetRefCount();
}

static void ScriptDictionarySetGCFlag_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *self = (CScriptDictionary*)gen->GetObject();
	self->SetGCFlag();
}

static void ScriptDictionaryGetGCFlag_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *self = (CScriptDictionary*)gen->GetObject();
	*(bool*)gen->GetAddressOfReturnLocation() = self->GetGCFlag();
}

static void ScriptDictionaryEnumReferences_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *self = (CScriptDictionary*)gen->GetObject();
	asIScriptEngine *engine = *(asIScriptEngine**)gen->GetAddressOfArg( 0 );
	self->EnumReferences( engine );
}

static void ScriptDictionaryReleaseAllReferences_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *self = (CScriptDictionary*)gen->GetObject();
	asIScriptEngine *engine = *(asIScriptEngine**)gen->GetAddressOfArg( 0 );
	self->ReleaseAllReferences( engine );
}

static void CScriptDictionaryGetKeys_Generic( asIScriptGeneric *gen ) {
	CScriptDictionary *self = (CScriptDictionary*)gen->GetObject();
	*(CScriptArray**)gen->GetAddressOfReturnLocation() = (CScriptArray*)self->GetKeys();
}

//--------------------------------------------------------------------------
// Register the type

static void RegisterScriptDictionary_Native( asIScriptEngine *engine ) {
	int r;

	// Use the generic interface to construct the object since we need the engine pointer, we could also have retrieved the engine pointer from the active context
	r = engine->RegisterObjectBehaviour( "Dictionary", asBEHAVE_FACTORY, "Dictionary@ f()", asFUNCTION( ScriptDictionaryFactory_Generic ), asCALL_GENERIC ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Dictionary", asBEHAVE_LIST_FACTORY, "Dictionary @f(int &in) {repeat {String, ?}}", asFUNCTION( ScriptDictionaryListFactory_Generic ), asCALL_GENERIC ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Dictionary", asBEHAVE_ADDREF, "void f()", asMETHOD( CScriptDictionary,AddRef ), asCALL_THISCALL ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Dictionary", asBEHAVE_RELEASE, "void f()", asMETHOD( CScriptDictionary,Release ), asCALL_THISCALL ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Dictionary", "Dictionary &opAssign(const Dictionary &in)", asMETHODPR( CScriptDictionary, operator=, ( const CScriptDictionary & ), CScriptDictionary& ), asCALL_THISCALL ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Dictionary", "void set(const String &in, ?&in)", asMETHODPR( CScriptDictionary,Set,( const asstring_t&,void*,int ),void ), asCALL_THISCALL ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Dictionary", "bool get(const String &in, ?&out) const", asMETHODPR( CScriptDictionary,Get,( const asstring_t&,void*,int ) const,bool ), asCALL_THISCALL ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Dictionary", "void set(const String &in, int64&in)", asMETHODPR( CScriptDictionary,Set,( const asstring_t&,int64_t & ),void ), asCALL_THISCALL ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Dictionary", "bool get(const String &in, int64&out) const", asMETHODPR( CScriptDictionary,Get,( const asstring_t&,int64_t & ) const,bool ), asCALL_THISCALL ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Dictionary", "void set(const String &in, double&in)", asMETHODPR( CScriptDictionary,Set,( const asstring_t&,double& ),void ), asCALL_THISCALL ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Dictionary", "bool get(const String &in, double&out) const", asMETHODPR( CScriptDictionary,Get,( const asstring_t&,double& ) const,bool ), asCALL_THISCALL ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Dictionary", "void set(const String &in, const String &in)", asMETHODPR( CScriptDictionary,Set,( const asstring_t&,asstring_t* ),void ), asCALL_THISCALL ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Dictionary", "bool get(const String &in, String &out) const",asMETHODPR( CScriptDictionary,Get,( const asstring_t&,asstring_t* ) const,bool ), asCALL_THISCALL ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Dictionary", "bool exists(const String &in) const", asMETHOD( CScriptDictionary,Exists ), asCALL_THISCALL ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Dictionary", "bool isEmpty() const", asMETHOD( CScriptDictionary, IsEmpty ), asCALL_THISCALL ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Dictionary", "uint getSize() const", asMETHOD( CScriptDictionary, GetSize ), asCALL_THISCALL ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Dictionary", "void delete(const String &in)", asMETHOD( CScriptDictionary,Delete ), asCALL_THISCALL ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Dictionary", "void deleteAll()", asMETHOD( CScriptDictionary,DeleteAll ), asCALL_THISCALL ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Dictionary", "array<String @> @getKeys() const", asMETHOD( CScriptDictionary,GetKeys ), asCALL_THISCALL ); assert( r >= 0 );

	// Register GC behaviours
	r = engine->RegisterObjectBehaviour( "Dictionary", asBEHAVE_GETREFCOUNT, "int f()", asMETHOD( CScriptDictionary,GetRefCount ), asCALL_THISCALL ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Dictionary", asBEHAVE_SETGCFLAG, "void f()", asMETHOD( CScriptDictionary,SetGCFlag ), asCALL_THISCALL ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Dictionary", asBEHAVE_GETGCFLAG, "bool f()", asMETHOD( CScriptDictionary,GetGCFlag ), asCALL_THISCALL ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Dictionary", asBEHAVE_ENUMREFS, "void f(int&in)", asMETHOD( CScriptDictionary,EnumReferences ), asCALL_THISCALL ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Dictionary", asBEHAVE_RELEASEREFS, "void f(int&in)", asMETHOD( CScriptDictionary,ReleaseAllReferences ), asCALL_THISCALL ); assert( r >= 0 );

#if AS_USE_STLNAMES == 1

	// Same as isEmpty
	r = engine->RegisterObjectMethod( "Dictionary", "bool empty() const", asMETHOD( CScriptDictionary, IsEmpty ), asCALL_THISCALL ); assert( r >= 0 );

	// Same as getSize
	r = engine->RegisterObjectMethod( "Dictionary", "uint size() const", asMETHOD( CScriptDictionary, GetSize ), asCALL_THISCALL ); assert( r >= 0 );

	// Same as delete
	r = engine->RegisterObjectMethod( "Dictionary", "void erase(const String &in)", asMETHOD( CScriptDictionary,Delete ), asCALL_THISCALL ); assert( r >= 0 );

	// Same as deleteAll
	r = engine->RegisterObjectMethod( "Dictionary", "void clear()", asMETHOD( CScriptDictionary,DeleteAll ), asCALL_THISCALL ); assert( r >= 0 );
#endif

	(void)sizeof( r ); // hush the compiler
}

static void RegisterScriptDictionary_Generic( asIScriptEngine *engine ) {
	int r;

	r = engine->RegisterObjectBehaviour( "Dictionary", asBEHAVE_FACTORY, "Dictionary@ f()", asFUNCTION( ScriptDictionaryFactory_Generic ), asCALL_GENERIC ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Dictionary", asBEHAVE_LIST_FACTORY, "Dictionary @f(int &in) {repeat {String, ?}}", asFUNCTION( ScriptDictionaryListFactory_Generic ), asCALL_GENERIC ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Dictionary", asBEHAVE_ADDREF, "void f()", asFUNCTION( ScriptDictionaryAddRef_Generic ), asCALL_GENERIC ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Dictionary", asBEHAVE_RELEASE, "void f()", asFUNCTION( ScriptDictionaryRelease_Generic ), asCALL_GENERIC ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Dictionary", "Dictionary &opAssign(const Dictionary &in)", asFUNCTION( ScriptDictionaryAssign_Generic ), asCALL_GENERIC ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Dictionary", "void set(const String &in, ?&in)", asFUNCTION( ScriptDictionarySet_Generic ), asCALL_GENERIC ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Dictionary", "bool get(const String &in, ?&out) const", asFUNCTION( ScriptDictionaryGet_Generic ), asCALL_GENERIC ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Dictionary", "void set(const String &in, int64&in)", asFUNCTION( ScriptDictionarySetInt_Generic ), asCALL_GENERIC ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Dictionary", "bool get(const String &in, int64&out) const", asFUNCTION( ScriptDictionaryGetInt_Generic ), asCALL_GENERIC ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Dictionary", "void set(const String &in, double&in)", asFUNCTION( ScriptDictionarySetFlt_Generic ), asCALL_GENERIC ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Dictionary", "bool get(const String &in, double&out) const", asFUNCTION( ScriptDictionaryGetFlt_Generic ), asCALL_GENERIC ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Dictionary", "void set(const String &in, const String &in)", asFUNCTION( ScriptDictionarySetString_Generic ), asCALL_GENERIC ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Dictionary", "bool get(const String &in, String &out) const", asFUNCTION( ScriptDictionaryGetString_Generic ), asCALL_GENERIC ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Dictionary", "bool exists(const String &in) const", asFUNCTION( ScriptDictionaryExists_Generic ), asCALL_GENERIC ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Dictionary", "void delete(const String &in)", asFUNCTION( ScriptDictionaryDelete_Generic ), asCALL_GENERIC ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Dictionary", "void deleteAll()", asFUNCTION( ScriptDictionaryDeleteAll_Generic ), asCALL_GENERIC ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Dictionary", "array<String @> @getKeys() const", asFUNCTION( CScriptDictionaryGetKeys_Generic ), asCALL_GENERIC ); assert( r >= 0 );

	// Register GC behaviours
	r = engine->RegisterObjectBehaviour( "Dictionary", asBEHAVE_GETREFCOUNT, "int f()", asFUNCTION( ScriptDictionaryGetRefCount_Generic ), asCALL_GENERIC ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Dictionary", asBEHAVE_SETGCFLAG, "void f()", asFUNCTION( ScriptDictionarySetGCFlag_Generic ), asCALL_GENERIC ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Dictionary", asBEHAVE_GETGCFLAG, "bool f()", asFUNCTION( ScriptDictionaryGetGCFlag_Generic ), asCALL_GENERIC ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Dictionary", asBEHAVE_ENUMREFS, "void f(int&in)", asFUNCTION( ScriptDictionaryEnumReferences_Generic ), asCALL_GENERIC ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Dictionary", asBEHAVE_RELEASEREFS, "void f(int&in)", asFUNCTION( ScriptDictionaryReleaseAllReferences_Generic ), asCALL_GENERIC ); assert( r >= 0 );

	(void)sizeof( r ); // hush the compiler
}

void PreRegisterScriptDictionary( asIScriptEngine *engine ) {
	int r;
	r = engine->RegisterObjectType( "Dictionary", sizeof( CScriptDictionary ), asOBJ_REF | asOBJ_GC ); assert( r >= 0 );
	(void)sizeof( r ); // hush the compiler
}

void RegisterScriptDictionary( asIScriptEngine *engine ) {
	if( strstr( asGetLibraryOptions(), "AS_MAX_PORTABILITY" ) ) {
		RegisterScriptDictionary_Generic( engine );
	} else {
		RegisterScriptDictionary_Native( engine );
	}
}

END_AS_NAMESPACE
