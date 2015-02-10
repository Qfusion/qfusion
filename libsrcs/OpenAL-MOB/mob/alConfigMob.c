/**
 * This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA  02111-1307, USA.
 * Or go to http://www.gnu.org/copyleft/lgpl.html
 */
#include "config-oal.h"

#include <string.h>
#include "alConfigMob.h"
#include "alMain.h"
#include "AL/alext.h"

#ifdef MOB_IGNORE_CONFIG_FILE

#define CONFIG_KEY_NULL() { "", 0 }
#define CONFIG_BLOCK_KEY( block, key ) { #block, #key }

typedef struct 
{
	const char *block;
	const char *key;
} MOB_BlockKeyPair;


typedef struct MOB_ConfigGlobals
{
	const MOB_ConfigKeyValue *keyValues;
} MOB_ConfigGlobals;

MOB_ConfigGlobals g_mob_configGlobals;

const MOB_BlockKeyPair g_ex_configPairs[] = 
{
#include "alConfigMobDefs_inl.h"
};

#undef CONFIG_BLOCK_KEY
#undef CONFIG_KEY_NULL

AL_API void AL_APIENTRY alSetConfigMOB( const MOB_ConfigKeyValue *keyValues )
{
	g_mob_configGlobals.keyValues = keyValues;
}

void MOB_ReadALConfig(void)
{

}

void MOB_FreeALConfig(void)
{

}

int MOB_ConfigGetEnumByName(const char *blockName, const char *keyName)
{
	const int arrayLength = sizeof( g_ex_configPairs ) / sizeof( g_ex_configPairs[ 0 ] );
	const char *blockSearchName = blockName == NULL ? "root" : blockName;
	int i;
	for( i = 0; i < arrayLength; ++i )
	{
		const MOB_BlockKeyPair *curr = &g_ex_configPairs[ i ];
		if( strcmp( curr->block, blockSearchName ) == 0 && 
			strcmp( curr->key, keyName ) == 0 )
		{
			return i;
		}
	}
	return -1;
}

int MOB_ConfigValueStr_KeyStr(const char *blockName, const char *keyName, const char **ret)
{
	int configEnum = MOB_ConfigGetEnumByName(blockName, keyName);
	if( configEnum < 0 )
	{
		return 0;
	}
	return MOB_Local_ConfigValueStr( (MOB_ConfigKey)configEnum, ret );
}

int MOB_Local_FindConfigValue( MOB_ConfigKey key, MOB_ConfigValue *outValue )
{
	const MOB_ConfigKeyValue *curr = g_mob_configGlobals.keyValues;
	if( !curr )
	{
		return 0;
	}
	while( curr->key != MOB_ConfigKey_NULL )
	{
		if( curr->key == key )
		{
			*outValue = curr->value;
			return 1;
		}
		++curr;
	}
	return 0;
}

int MOB_Local_ConfigValueExists( MOB_ConfigKey key )
{
	MOB_ConfigValue value;
	return MOB_Local_FindConfigValue( key, &value );
}

const char *MOB_Local_GetConfigValue( MOB_ConfigKey key, const char *def)
{
	MOB_ConfigValue value;
	if( !MOB_Local_FindConfigValue( key, &value ) )
	{
		return def;
	}
	return value.stringVal;
}

int MOB_Local_GetConfigValueBool( MOB_ConfigKey key, int def)
{
	MOB_ConfigValue value;
	if( !MOB_Local_FindConfigValue( key, &value ) )
	{
		return def;
	}
	return value.intVal != 0;
}

int MOB_Local_ConfigValueStr( MOB_ConfigKey key, const char **ret)
{
	MOB_ConfigValue value;
	if( !MOB_Local_FindConfigValue( key, &value ) )
	{
		return 0;
	}
	*ret = value.stringVal;
	return 1;
}

int MOB_Local_ConfigValueInt( MOB_ConfigKey key, int *ret)
{
	MOB_ConfigValue value;
	if( !MOB_Local_FindConfigValue( key, &value ) )
	{
		return 0;
	}
	*ret = value.intVal;
	return 1;
}

int MOB_Local_ConfigValueUInt( MOB_ConfigKey key, unsigned int *ret)
{
	MOB_ConfigValue value;
	if( !MOB_Local_FindConfigValue( key, &value ) )
	{
		return 0;
	}
	*ret = value.uintVal;
	return 1;
}

int MOB_Local_ConfigValueFloat( MOB_ConfigKey key, float *ret)
{
	MOB_ConfigValue value;
	if( !MOB_Local_FindConfigValue( key, &value ) )
	{
		return 0;
	}
	*ret = value.floatVal;
	return 1;
}

#else // #ifdef MOB_IGNORE_CONFIG_FILE

int MOB_ConfigValueStr_KeyStr(const char *blockName, const char *keyName, const char **ret)
{
	return ConfigValueStr( blockName, keyName, ret );
}


void MOB_ReadALConfig(void)
{
	ReadALConfig();
}

void MOB_FreeALConfig(void)
{
	FreeALConfig();
}

// We translate the block name and the key name to preserve compatibility with the text files
const char *ConvertBlockName(const char *blockName )
{
	if( strcmp(blockName, "root") == 0 )
	{
		return NULL;
	}
	return blockName;
}

const char *ConvertKeyName( char *outBuffer, const char *keyName )
{
	int i;
	int stringLength = strlen( keyName );
	for( i = 0; i < stringLength; ++i )
	{
		char keyI = keyName[ i ];
		outBuffer[ i ] = keyI == '_' ? '-' : keyI;
	}
	outBuffer[ stringLength ] = '\0';
	return outBuffer;
}

int MOB_File_ConfigValueExists(const char *blockName, const char *keyName)
{
	char buffer[ 64 ];
	return ConfigValueExists( ConvertBlockName( blockName ), ConvertKeyName( buffer, keyName ) );
}
const char *MOB_File_GetConfigValue(const char *blockName, const char *keyName, const char *def)
{
	char buffer[ 64 ];
	return GetConfigValue( ConvertBlockName( blockName ), ConvertKeyName( buffer, keyName ), def );
}

int MOB_File_GetConfigValueBool(const char *blockName, const char *keyName, int def)
{
	char buffer[ 64 ];
	return GetConfigValueBool( ConvertBlockName( blockName ), ConvertKeyName( buffer, keyName ), def );
}

int MOB_File_ConfigValueStr(const char *blockName, const char *keyName, const char **ret)
{
	char buffer[ 64 ];
	return ConfigValueStr( ConvertBlockName( blockName ), ConvertKeyName( buffer, keyName ), ret );
}

int MOB_File_ConfigValueInt(const char *blockName, const char *keyName, int *ret)
{
	char buffer[ 64 ];
	return ConfigValueInt( ConvertBlockName( blockName ), ConvertKeyName( buffer, keyName ), ret );
}

int MOB_File_ConfigValueUInt(const char *blockName, const char *keyName, unsigned int *ret)
{
	char buffer[ 64 ];
	return ConfigValueUInt( ConvertBlockName( blockName ), ConvertKeyName( buffer, keyName ), ret );
}

int MOB_File_ConfigValueFloat(const char *blockName, const char *keyName, float *ret)
{
	char buffer[ 64 ];
	return ConfigValueFloat( ConvertBlockName( blockName ), ConvertKeyName( buffer, keyName ), ret );
}

#endif // #else // #ifdef MOB_IGNORE_CONFIG_FILE