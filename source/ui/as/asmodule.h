#pragma once
#ifndef __UI_ASMODULE_H__
#define __UI_ASMODULE_H__

// forward declare some angelscript stuff without including angelscript
class asIScriptEngine;
class asIScriptContext;
class asIScriptModule;
class asITypeInfo;
class asIScriptFunction;

class CScriptArrayInterface;
class CScriptDictionaryInterface;

struct asstring_s;
typedef struct asstring_s asstring_t;

namespace ASUI
{

// base class for AS
class ASInterface
{
public:
	virtual bool Init( void ) = 0;
	virtual void Shutdown( void ) = 0;

	virtual asIScriptEngine *getEngine( void ) const = 0;
	virtual asIScriptContext *getContext( void ) const = 0;
	virtual asIScriptModule *getModule( const char *name ) const = 0;

	virtual void *setModuleUserData( asIScriptModule *m, void *data, unsigned type = 0 ) = 0;
	virtual void *getModuleUserData( asIScriptModule *m, unsigned type = 0 ) const = 0;
	virtual const char *getModuleName( asIScriptModule *m ) const = 0;

	// only valid during execution of script functions
	virtual asIScriptContext *getActiveContext( void ) const = 0;
	virtual asIScriptModule *getActiveModule( void ) const = 0;

	virtual asITypeInfo *getStringObjectType( void ) const = 0;

	// called to start a building round
	// note that temporary name assigned to the build (module)
	// may be changed in the finishBuilding call
	virtual asIScriptModule *startBuilding( const char *moduleName ) = 0;

	// compile all added scripts, set final module name
	virtual bool finishBuilding( asIScriptModule *module ) = 0;

	// adds a script either to module, or the following.
	// If no name is provided, script_XXX is used
	virtual bool addScript( asIScriptModule *module, const char *name, const char *code ) = 0;

	// adds a function to module, despite if finishBuilding has been called
	virtual bool addFunction( asIScriptModule *module, const char *name, const char *code, asIScriptFunction **outFunction ) = 0;

	// testing, dumapi command
	virtual void dumpAPI( const char *path, bool markdown, bool singleFile, unsigned andMask, unsigned notMask ) = 0;

	// reset all potentially referenced global vars
	// (used for releasing reference-counted Rocket objects)
	virtual void buildReset( asIScriptModule *module ) = 0;
	virtual void buildReset( const char *name ) = 0;

	// garbage collector interfaces
	virtual void garbageCollectOneStep( void ) = 0;
	virtual void garbageCollectFullCycle( void ) = 0;

	// creates a new array object, which can be natively passed on to scripts
	virtual CScriptArrayInterface *createArray( unsigned int size, asITypeInfo *ot ) = 0;

	// AS string functions
	virtual asstring_t *createString( const char *buffer, unsigned int length ) = 0;
	virtual asstring_t *assignString( asstring_t *self, const char *string, unsigned int strlen ) = 0;
	virtual void releaseString( asstring_t *str ) = 0;

	// creates a new dictionary object, which can be natively passed on to scripts
	virtual CScriptDictionaryInterface *createDictionary( void ) = 0;

	// caching, TODO!
	// void *getBytecode(size_t &size);
	// void setByteCode(void *bytecode, size_t size);

	// void saveBytecode(const char *filename);
	// void loadBytecode(const char *filename);
};

ASInterface * GetASModule( WSWUI::UI_Main *main );

}
#endif
