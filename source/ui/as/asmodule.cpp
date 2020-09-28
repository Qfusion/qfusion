#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"

#include "as/asui.h"
#include "as/asui_local.h"

#include <list>

#define UI_AS_MODULE "UI_AS_MODULE"

namespace ASUI
{

typedef WSWUI::UI_Main UI_Main;

//=======================================

class BinaryBufferStream : public asIBinaryStream
{
	// TODO: use vector!
	unsigned char *data;
	size_t size;        // size of data stored
	size_t allocated;   // actual allocated size
	size_t offset;      // read-head

public:
	BinaryBufferStream() : data( 0 ), size( 0 ), allocated( 0 ), offset( 0 ) {
	}

	BinaryBufferStream( void *_data, size_t _size )
		: data( 0 ), size( _size ), allocated( _size ), offset( 0 ) {
		data = __newa__( unsigned char, _size );
		memcpy( data, _data, _size );
	}

	~BinaryBufferStream() {
		if( data ) {
			__delete__( data );
		}
	}

	size_t getDataSize() { return size; }
	void *getData() { return data; }

	// asIBinaryStream implementation
	int Read( void *ptr, asUINT _size ) {
		if( !data || !ptr ) {
			trap::Error( "BinaryBuffer::Read null pointer" );
			return -1;
		}
		if( ( offset + _size ) > allocated ) {
			trap::Error( "BinaryBuffer::Read tried to read more bytes than available" );
			return -1;
		}

		memcpy( ptr, data + offset, _size );
		offset += _size;
		return (int)size;
	}

	int Write( const void *ptr, asUINT _size ) {
		if( !data || !ptr ) {
			trap::Error( "BinaryBuffer::Write null pointer" );
			return -1;
		}

		if( ( size + _size ) > allocated ) {
			// reallocate
			allocated = size + _size;
			unsigned char *tmp = __newa__( unsigned char, allocated );    // allocate little more?
			memcpy( tmp, data, size );
			__delete__( data );   // note! regular delete works for unsigned char*
			data = tmp;
		}

		memcpy( data + size, ptr, _size );
		size += _size;
		return (int)size;
	}
};

//=======================================

class BinaryFileStream : public asIBinaryStream
{
	int fh;

public:
	BinaryFileStream() : fh( 0 ) {
	}

	BinaryFileStream( const char *filename, int mode ) {
		if( trap::FS_FOpenFile( filename, &fh, mode ) == -1 ) {
			trap::Error( "BinaryFileStream: failed to open file" );
		}
	}

	~BinaryFileStream() {
		Close();
	}

	bool Open( const char *filename, int mode ) {
		Close();
		return trap::FS_FOpenFile( filename, &fh, mode ) == -1;
	}

	void Close() {
		if( fh ) {
			trap::FS_FCloseFile( fh );
		}
	}

	// asIBinaryStream implementation
	int Read( void *ptr, asUINT size ) {
		if( !fh ) {
			trap::Error( "BinaryFileStream::Read tried to read from closed file" );
			return -1;
		}

		return trap::FS_Read( ptr, size, fh );
	}

	int Write( const void *ptr, asUINT size ) {
		if( !fh ) {
			trap::Error( "BinaryFileStream::Write tried to write to closed file" );
			return -1;
		}

		return trap::FS_Write( ptr, size, fh );
	}
};

//=======================================

class ASModule : public ASInterface
{
	UI_Main *ui_main;

	asIScriptEngine *engine;
	struct angelwrap_api_s *as_api;
	asITypeInfo *stringObjectType;

	static const asDWORD accessMask = 0x1;

// private class, its ok to have everything as public :)

public:
	ASModule()
		: ui_main( 0 ), engine( 0 ),
		as_api( 0 ),
		stringObjectType( 0 ) {
	}

	virtual ~ASModule( void ) {
	}

	virtual bool Init( void ) {
		bool as_max_portability = false;

		as_api = trap::asGetAngelExport();
		if( !as_api ) {
			return false;
		}

		engine = as_api->asCreateEngine( &as_max_portability );
		if( engine == NULL ) {
			return false;
		}

		if( as_max_portability != false ) {
			return false;
		}

		engine->SetDefaultAccessMask( accessMask );

		stringObjectType = engine->GetTypeInfoById( engine->GetTypeIdByDecl( "String" ) );

		/*
		module = engine->GetModule( UI_AS_MODULE, asGM_ALWAYS_CREATE );
		if( !module )
		    return false;
		*/

		return true;
	}

	virtual void Shutdown( void ) {
		//module = 0;

		if( as_api && engine != NULL ) {
			as_api->asReleaseEngine( engine );
		}

		engine = 0;

		as_api = NULL;

		stringObjectType = 0;
	}

	virtual void setUI( UI_Main *ui ) { ui_main = ui; }

	virtual asIScriptEngine *getEngine( void ) const {
		return engine;
	}

	virtual asIScriptModule *getModule( const char *name ) const {
		return engine->GetModule( name, asGM_ONLY_IF_EXISTS );
	}

	virtual void *setModuleUserData( asIScriptModule *module, void *data, unsigned type ) {
		return module ? module->SetUserData( data, type ) : NULL;
	}

	virtual void *getModuleUserData( asIScriptModule *module, unsigned type ) const {
		return module ? module->GetUserData( type ) : NULL;
	}

	virtual const char *getModuleName( asIScriptModule *module ) const {
		return module ? module->GetName() : NULL;
	}

	virtual asIScriptContext *getContext( void ) const {
		return as_api ? as_api->asAcquireContext( engine ) : NULL;
	}

	virtual asIScriptContext *getActiveContext( void ) const {
		return as_api ? as_api->asGetActiveContext() : NULL;
	}

	virtual asIScriptModule *getActiveModule( void ) const {
		if( engine == nullptr ) {
			return NULL;
		}
		asIScriptContext *activeContext = UI_Main::Get()->getAS()->getActiveContext();
		asIScriptFunction *currentFunction = activeContext != nullptr ? activeContext->GetFunction( 0 ) : nullptr;
		return currentFunction != nullptr ? currentFunction->GetModule() : nullptr;
	}

	virtual asITypeInfo *getStringObjectType( void ) const {
		return stringObjectType;
	}

	virtual asIScriptModule *startBuilding( const char *moduleName ) {
		asIScriptModule *module = engine->GetModule( moduleName, asGM_CREATE_IF_NOT_EXISTS );
		return module;
	}

	virtual bool finishBuilding( asIScriptModule *module ) {
		if( !module ) {
			return false;
		}
		return module->Build() >= 0;
	}

	virtual bool addScript( asIScriptModule *module, const char *name, const char *code ) {
		// TODO: precache SaveByteCode/LoadByteCode
		// (this can be done on a global .rml level too)
		// TODO: figure out if name can be NULL, or otherwise create
		// temp name from NULL argument to differentiate <script> tags
		// without source
		if( !module ) {
			return false;
		}

		return module->AddScriptSection( name, code ) >= 0;
	}

	virtual bool addFunction( asIScriptModule *module, const char *name, const char *code, asIScriptFunction **outFunction ) {
		// note that reference count for outFunction is increased here!
		return module ? ( module->CompileFunction( name, code, 0, asCOMP_ADD_TO_MODULE, outFunction ) >= 0 ) : false;
	}

	// TODO: disk/mem-cache fully compiled set (use Binary*Stream)

	// testing, dumpapi, note that path has to end with '/'
	virtual void dumpAPI( const char *path, bool markdown, bool singleFile, unsigned andMask, unsigned notMask ) {
		if( andMask == 0 ) {
			andMask = accessMask;
		}
		if( notMask == 0 ) {
			notMask = ~notMask;
		}

		as_api->asWriteEngineDocsToFile( engine, path, "ui", markdown, singleFile, andMask, notMask );
	}

	virtual void buildReset( asIScriptModule *module ) {
		if( engine && module ) {
			module->Discard();
		}
		garbageCollectFullCycle();
	}

	virtual void buildReset( const char *name ) {
		if( engine && name ) {
			buildReset( engine->GetModule( name, asGM_ONLY_IF_EXISTS ) );
		}
	}

	virtual void garbageCollectOneStep( void ) {
		if( engine ) {
			engine->GarbageCollect( asGC_ONE_STEP );
		}
	}

	virtual void garbageCollectFullCycle( void ) {
		if( engine ) {
			engine->GarbageCollect( asGC_FULL_CYCLE );
		}
	}

	// array factory
	virtual CScriptArrayInterface *createArray( unsigned int size, asITypeInfo *ot ) {
		if( as_api ) {
			return static_cast<CScriptArrayInterface *>( as_api->asCreateArrayCpp( size, static_cast<void *>( ot ) ) );
		}
		return NULL;
	}

	// AS string functions
	virtual asstring_t *createString( const char *buffer, unsigned int length ) {
		if( as_api ) {
			return as_api->asStringFactoryBuffer( buffer, length );
		}
		return NULL;
	}

	void releaseString( asstring_t *str ) {
		if( as_api ) {
			as_api->asStringRelease( str );
		}
	}

	virtual asstring_t *assignString( asstring_t *self, const char *string, unsigned int strlen ) {
		if( as_api ) {
			return as_api->asStringAssignString( self, string, strlen );
		}
		return NULL;
	}

	// dictionary factory
	virtual CScriptDictionaryInterface *createDictionary( void ) {
		if( as_api ) {
			return static_cast<CScriptDictionaryInterface *>( as_api->asCreateDictionaryCpp( engine ) );
		}
		return NULL;
	}
};

//=======================================

// TODO: __new__ this one out
ASModule asmodule;

ASInterface * GetASModule( UI_Main *ui ) {
	asmodule.setUI( ui );
	return &asmodule;
}

}
