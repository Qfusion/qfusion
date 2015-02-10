#ifndef AL_CONFIG_MOB_H
#define AL_CONFIG_MOB_H

#include "AL/al.h"
#include "AL/alc.h"
#include "AL/alext.h"

/* Define if we should ignore the config file and use local settings */
#define MOB_IGNORE_CONFIG_FILE

void MOB_ReadALConfig(void);
void MOB_FreeALConfig(void);
int MOB_ConfigValueStr_KeyStr(const char *blockName, const char *keyName, const char **ret);

#ifdef MOB_IGNORE_CONFIG_FILE

#define CONFIG_KEY_NULL() MOB_ConfigKey_NULL
#define CONFIG_BLOCK_KEY( block, key ) MOB_ConfigKey_##block##_##key
typedef enum 
{
#include "alConfigMobDefs_inl.h"
} MOB_ConfigKey;

typedef union
{
	const char  *stringVal;
	int          intVal;
	unsigned int uintVal;
	float        floatVal;
} MOB_ConfigValue;

struct MOB_ConfigKeyValue_Struct
{
	MOB_ConfigKey   key;
	MOB_ConfigValue value;
};

typedef struct MOB_ConfigKeyValue_Struct MOB_ConfigKeyValue;

int MOB_Local_ConfigValueExists( MOB_ConfigKey key );
const char *MOB_Local_GetConfigValue( MOB_ConfigKey key, const char *def);
int MOB_Local_GetConfigValueBool( MOB_ConfigKey key, int def);
int MOB_Local_ConfigValueStr( MOB_ConfigKey key, const char **ret);
int MOB_Local_ConfigValueInt( MOB_ConfigKey key, int *ret);
int MOB_Local_ConfigValueUInt( MOB_ConfigKey key, unsigned int *ret);
int MOB_Local_ConfigValueFloat( MOB_ConfigKey key, float *ret);

// Type safe ways of constructing your config
static AL_INLINE MOB_ConfigValue MOB_ConfigValue_Str( const char *stringVal )
{
	MOB_ConfigValue value;
	value.stringVal = stringVal;
	return value;
}

static AL_INLINE MOB_ConfigValue MOB_ConfigValue_Int( int intVal )
{
	MOB_ConfigValue value;
	value.intVal = intVal;
	return value;
}

static AL_INLINE MOB_ConfigValue MOB_ConfigValue_Uint( unsigned int uintVal )
{
	MOB_ConfigValue value;
	value.uintVal = uintVal;
	return value;
}


static AL_INLINE MOB_ConfigValue MOB_ConfigValue_Float( float floatVal )
{
	MOB_ConfigValue value;
	value.floatVal = floatVal;
	return value;
}



#define MOB_ConfigValueExists( blockName, keyName ) MOB_Local_ConfigValueExists( MOB_ConfigKey_##blockName##_##keyName )
#define MOB_GetConfigValue( blockName, keyName, def ) MOB_Local_GetConfigValue(MOB_ConfigKey_##blockName##_##keyName, (def))
#define MOB_GetConfigValueBool( blockName, keyName, def ) MOB_Local_GetConfigValueBool(MOB_ConfigKey_##blockName##_##keyName, (def))
#define MOB_ConfigValueStr( blockName, keyName, ret ) MOB_Local_ConfigValueStr(MOB_ConfigKey_##blockName##_##keyName, (ret))
#define MOB_ConfigValueInt( blockName, keyName, ret ) MOB_Local_ConfigValueInt(MOB_ConfigKey_##blockName##_##keyName, (ret))
#define MOB_ConfigValueUInt( blockName, keyName, ret ) MOB_Local_ConfigValueUInt(MOB_ConfigKey_##blockName##_##keyName, (ret))
#define MOB_ConfigValueFloat( blockName, keyName, ret ) MOB_Local_ConfigValueFloat(MOB_ConfigKey_##blockName##_##keyName, (ret))

#undef CONFIG_BLOCK_KEY
#undef CONFIG_KEY_NULL

#else // #ifdef MOB_IGNORE_CONFIG_FILE

#define MOB_ConfigValueExists( blockName, keyName ) MOB_File_ConfigValueExists(#blockName, #keyName)
#define MOB_GetConfigValue( blockName, keyName, def ) MOB_File_GetConfigValue(#blockName, #keyName, (def))
#define MOB_GetConfigValueBool( blockName, keyName, def ) MOB_File_GetConfigValueBool(#blockName, #keyName, (def))
#define MOB_ConfigValueStr( blockName, keyName, ret ) MOB_File_ConfigValueStr(#blockName, #keyName, (ret))
#define MOB_ConfigValueInt( blockName, keyName, ret ) MOB_File_ConfigValueInt(#blockName, #keyName, (ret))
#define MOB_ConfigValueUInt( blockName, keyName, ret ) MOB_File_ConfigValueUInt(#blockName, #keyName, (ret))
#define MOB_ConfigValueFloat( blockName, keyName, ret ) MOB_File_ConfigValueFloat(#blockName, #keyName, (ret))


int MOB_File_ConfigValueExists(const char *blockName, const char *keyName);
const char *MOB_File_GetConfigValue(const char *blockName, const char *keyName, const char *def);
int MOB_File_GetConfigValueBool(const char *blockName, const char *keyName, int def);
int MOB_File_ConfigValueStr(const char *blockName, const char *keyName, const char **ret);
int MOB_File_ConfigValueInt(const char *blockName, const char *keyName, int *ret);
int MOB_File_ConfigValueUInt(const char *blockName, const char *keyName, unsigned int *ret);
int MOB_File_ConfigValueFloat(const char *blockName, const char *keyName, float *ret);

#endif // #else // #ifdef MOB_IGNORE_CONFIG_FILE

#endif // #ifndef AL_CONFIG_MOB_H
