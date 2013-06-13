/*
Copyright (C) 2008 Chasseur de bots

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "qcommon.h"
#include "trie.h"
#include "../client/console.h"

#include <assert.h>

static qboolean dynvar_initialized = qfalse;
static qboolean dynvar_preinitialized = qfalse;

typedef struct dynvar_listener_node_s
{
	dynvar_listener_f listener;
	struct dynvar_listener_node_s *next;
} dynvar_listener_node_t;

struct dynvar_s
{
	const char *name;               // name of the dynvar in dictionary
	const qboolean console;         // accessible from console
	dynvar_getter_f getter;         // getter function
	dynvar_setter_f setter;         // setter function
	dynvar_listener_node_t *listeners; // list of listeners
	qboolean listeners_immutable;   // Dynvar_SetValue is in call-stack
	dynvar_listener_node_t *to_remove; // list of listeners to be removed after Dynvar_SetValue
};

// we store the dynvars in a trie for dictionary lookup
static trie_t *dynvar_trie = NULL;
static const trie_casing_t DYNVAR_TRIE_CASING = CON_CASE_SENSITIVE ? TRIE_CASE_SENSITIVE : TRIE_CASE_INSENSITIVE;

// error strings used more than once
static const char DYNVAR_GET_WRITEONLY_MSG[] = "\"%s\" is write-only.\n";
static const char DYNVAR_SET_INVALID_MSG[] = "\"%s\" is an invalid value for \"%s\". Not set.\n";
static const char DYNVAR_SET_READONLY_MSG[] = "\"%s\" is read-only.\n";
static const char DYNVAR_TRANSIENT_MSG[] = "\"%s\" is currently not available.\n";
static const char DYNVAR_NOT_FOUND_MSG[] = "no such dynvar: %s\n";

// forward declarations of internal implementation

static dynvar_t *Dynvar_NewDynvar(
        const char *name,
        qboolean console,
        dynvar_getter_f getter,
        dynvar_setter_f setter
);

static dynvar_listener_node_t *Dynvar_NewListener(
        dynvar_listener_f listener,
        dynvar_listener_node_t *next
);

static void Dynvar_FreeDynvar( dynvar_t *dynvar );

static int Dynvar_Console( void *dynvar, void *pattern );

// forward declarations of commands

static void Dynvar_List_f( void );
static void Dynvar_Set_f( void );

// implementation of externalized functions

void Dynvar_PreInit( void )
{
	assert( !dynvar_initialized );
	assert( !dynvar_preinitialized );

	assert( !dynvar_trie );
	Trie_Create( DYNVAR_TRIE_CASING, &dynvar_trie );

	dynvar_preinitialized = qtrue;
}

// externalized functions

void Dynvar_Init( void )
{
	assert( !dynvar_initialized );
	assert( dynvar_preinitialized );

	// register commands
	Cmd_AddCommand( "dynvarlist", Dynvar_List_f );
	Cmd_AddCommand( "setdyn", Dynvar_Set_f );

	dynvar_initialized = qtrue;
}

void Dynvar_Shutdown( void )
{
	if( dynvar_initialized )
	{
		struct trie_dump_s *dump;
		unsigned int i;

		assert( dynvar_trie );

		Cmd_RemoveCommand( "dynvarlist" );
		Cmd_RemoveCommand( "setdyn" );

		Trie_Dump( dynvar_trie, "", TRIE_DUMP_VALUES, &dump );
		for( i = 0; i < dump->size; i++ )
		{
			Dynvar_Destroy( (dynvar_t *)dump->key_value_vector[i].value );
		}
		Trie_FreeDump( dump );

		dynvar_initialized = qfalse;
	}

	if( dynvar_preinitialized )
	{
		assert( dynvar_trie );

		Trie_Destroy( dynvar_trie );
		dynvar_trie = NULL;

		dynvar_preinitialized = qfalse;
	}
}

dynvar_t *Dynvar_Create(
        const char *name,
        qboolean console,
        dynvar_getter_f getter,
        dynvar_setter_f setter
)
{
	assert( dynvar_trie );
	if( name && getter && setter )
	{
		dynvar_t *dynvar = Dynvar_NewDynvar( name, console, getter, setter );
		if( Trie_Insert( dynvar_trie, name, dynvar ) == TRIE_OK )
			return dynvar;
		else
			Dynvar_FreeDynvar( dynvar );
	}
	return NULL;
}

void Dynvar_Destroy(
        dynvar_t *dynvar
)
{
	dynvar_t *old;
#if defined ( DEBUG ) || defined ( _DEBUG )
	trie_error_t status;
	assert( dynvar_trie );
	assert( !dynvar->listeners_immutable );
	status = Trie_Remove( dynvar_trie, Dynvar_GetName( dynvar ), (void **)&old );
	assert( status == TRIE_OK );
	assert( dynvar == old );
#else
	Trie_Remove( dynvar_trie, Dynvar_GetName( dynvar ), (void **)&old );
#endif
	Dynvar_FreeDynvar( dynvar );
}

dynvar_t *Dynvar_Lookup(
        const char *name
)
{
	dynvar_t *dynvar;
	assert( dynvar_trie );
	Trie_Find( dynvar_trie, name, TRIE_EXACT_MATCH, (void **)&dynvar );
	return dynvar;
}

const char *Dynvar_GetName(
        dynvar_t *dynvar
)
{
	assert( dynvar );
	return dynvar->name;
}

dynvar_get_status_t Dynvar_GetValue(
        dynvar_t *dynvar,
        void **value
)
{
	assert( dynvar );
	assert( dynvar->getter );
	return dynvar->getter( value );
}

dynvar_set_status_t Dynvar_SetValue(
        dynvar_t *dynvar,
        void *value
)
{
	dynvar_set_status_t status;
	assert( dynvar );
	assert( dynvar->setter );
	assert( !dynvar->listeners_immutable );
	status = dynvar->setter( value );
	if( status == DYNVAR_SET_OK )
		Dynvar_CallListeners( dynvar, value );
	return status;
}

void Dynvar_CallListeners(
        dynvar_t *dynvar,
        void *value
)
{
	dynvar_listener_node_t *n;
	dynvar->listeners_immutable = qtrue; // protect against concurrent Dynvar_RemoveListener
	// call listeners
	for( n = dynvar->listeners; n; n = n->next )
	{
		assert( n->listener );
		n->listener( value );
	}
	dynvar->listeners_immutable = qfalse; // allow Dynvar_RemoveListener to modify dynvar->listeners
	// perform pending removals
	if( dynvar->to_remove )
	{
		dynvar_listener_node_t *n, *prev = NULL;
		for( n = dynvar->to_remove; n; n = n->next )
		{
			Dynvar_RemoveListener( dynvar, n->listener );
			Mem_ZoneFree( prev );
			prev = n;
		}
		Mem_ZoneFree( prev );
		dynvar->to_remove = NULL;
	}
}

void Dynvar_AddListener(
        dynvar_t *dynvar,
        dynvar_listener_f listener
)
{
	assert( dynvar );
	assert( listener );
	if( !dynvar->listeners )
	{
		// no listeners yet, create list head
		dynvar->listeners = Dynvar_NewListener( listener, NULL );
	}
	else
	{
		// append listener
		dynvar_listener_node_t *n;
		for( n = dynvar->listeners; n->next; n = n->next )
			;
		assert( n );
		assert( !n->next );
		n->next = Dynvar_NewListener( listener, NULL );
	}
}

void Dynvar_RemoveListener(
        dynvar_t *dynvar,
        dynvar_listener_f listener
)
{
	assert( dynvar );
	assert( listener );
	if( !dynvar->listeners_immutable )
	{
		// modification of dynvar->listeners allowed
		dynvar_listener_node_t *cur, *prev;
		prev = NULL;
		cur = dynvar->listeners;
		while( cur )
		{
			if( cur->listener == listener )
			{
				if( prev )
				{
					// remove middle or tail node
					prev->next = cur->next;
				}
				else
				{
					// remove head node
					assert( cur == dynvar->listeners );
					dynvar->listeners = cur->next;
				}
				Mem_ZoneFree( cur );
				break; // remove only one instance
			}
			prev = cur;
			cur = cur->next;
		}
	}
	else
	{
		// do not modify dynvar->listeners, but prepend to dynvar->to_remove for deferred removal
		dynvar->to_remove = Dynvar_NewListener( listener, dynvar->to_remove );
	}
}

int Dynvar_CompleteCountPossible(
        const char *partial
)
{
	unsigned int matches = 0;
	assert( partial );
	if( *partial )
		Trie_NoOfMatchesIf( dynvar_trie, partial, Dynvar_Console, NULL, &matches );
	return matches;
}

const char **Dynvar_CompleteBuildList(
        const char *partial
)
{
	struct trie_dump_s *dump;
	const char **buf;
	unsigned int i;

	assert( dynvar_trie );
	assert( partial );
	Trie_DumpIf( dynvar_trie, partial, TRIE_DUMP_VALUES, Dynvar_Console, NULL, &dump );
	buf = (const char **) Mem_TempMalloc( sizeof( char * ) * ( dump->size + 1 ) );
	for( i = 0; i < dump->size; ++i )
		buf[i] = ( (dynvar_t *) ( dump->key_value_vector[i].value ) )->name;
	buf[dump->size] = NULL;
	Trie_FreeDump( dump );
	return buf;
}

const char *Dynvar_CompleteDynvar(
        const char *partial
)
{
	assert( partial );
	if( *partial )
	{
		dynvar_t *dynvar;
		if( Trie_FindIf( dynvar_trie, partial, TRIE_PREFIX_MATCH, Dynvar_Console, NULL, (void **)&dynvar ) == TRIE_OK )
			return Dynvar_GetName( dynvar );
	}
	return NULL;
}

qboolean Dynvar_Command( void )
{
	dynvar_t *dynvar = Dynvar_Lookup( Cmd_Argv( 0 ) );
	if( dynvar && dynvar->console )
	{
		// dynvar exists and is console-accessible
		if( Cmd_Argc() == 1 )
		{
			void *value;
			dynvar_get_status_t status = Dynvar_GetValue( dynvar, &value );
			switch( status )
			{
			case DYNVAR_GET_OK:
				Com_Printf( "\"%s\" is \"%s\"\n", Dynvar_GetName( dynvar ), (char *) value );
				break;
			case DYNVAR_GET_WRITEONLY:
				Com_Printf( (char *) DYNVAR_GET_WRITEONLY_MSG, Dynvar_GetName( dynvar ) );
				break;
			case DYNVAR_GET_TRANSIENT:
				Com_Printf( (char *) DYNVAR_TRANSIENT_MSG, Dynvar_GetName( dynvar ) );
				break;
			}
		}
		else
		{
			dynvar_set_status_t status = Dynvar_SetValue( dynvar, Cmd_Argv( 1 ) );
			switch( status )
			{
			case DYNVAR_SET_OK:
				break;
			case DYNVAR_SET_READONLY:
				Com_Printf( (char *) DYNVAR_SET_READONLY_MSG, Dynvar_GetName( dynvar ) );
				break;
			case DYNVAR_SET_INVALID:
				Com_Printf( (char *) DYNVAR_SET_INVALID_MSG, Cmd_Argv( 1 ), Dynvar_GetName( dynvar ) );
				break;
			case DYNVAR_SET_TRANSIENT:
				Com_Printf( (char *) DYNVAR_TRANSIENT_MSG, Dynvar_GetName( dynvar ) );
				break;
			}
		}
		return qtrue;
	}
	else
		// dynvar does not exist or is inaccessible
		return qfalse;
}

// internal implementation

static dynvar_t *Dynvar_NewDynvar(
        const char *name,
        qboolean console,
        dynvar_getter_f getter,
        dynvar_setter_f setter
)
{
	dynvar_t *dynvar = (dynvar_t *) Mem_ZoneMalloc( sizeof( dynvar_t ) );
	dynvar->name = (char *) Mem_ZoneMalloc( strlen( name ) + 1 );
	strcpy( (char *) dynvar->name, name );
	*( (qboolean *) &dynvar->console ) = console;
	dynvar->getter = getter;
	dynvar->setter = setter;
	dynvar->listeners_immutable = qfalse;
	dynvar->to_remove = NULL;
	return dynvar;
}

static dynvar_listener_node_t *Dynvar_NewListener(
        dynvar_listener_f listener,
        dynvar_listener_node_t *next
)
{
	dynvar_listener_node_t *n = (dynvar_listener_node_t *) Mem_ZoneMalloc( sizeof( dynvar_listener_node_t ) );
	n->listener = listener;
	n->next = next;
	return n;
}

static void Dynvar_FreeDynvar( dynvar_t *dynvar )
{
	dynvar_listener_node_t *next, *cur;
	cur = dynvar->listeners;
	for( cur = dynvar->listeners; cur; cur = next )
	{
		next = cur->next;
		Mem_ZoneFree( cur );
	}
	Mem_ZoneFree( (char *) dynvar->name );
	Mem_ZoneFree( dynvar );
}

static int Dynvar_Console( void *dynvar, void *pattern )
{
	const dynvar_t *const var = ( (dynvar_t *) dynvar );
	assert( var );
	return var->console &&
	       ( !pattern || Com_GlobMatch( (const char *) pattern, var->name, qfalse ) );
}

void Dynvar_List_f( void )
{
	struct trie_dump_s *dump;
	unsigned int i, size;
	char *pattern;

	Trie_GetSize( dynvar_trie, &size );
	if( !size )
	{
		Com_Printf( "No dynvars\n" );
		return;
	}

	if( Cmd_Argc() == 1 )
		pattern = NULL; // no wildcard
	else
		pattern = Cmd_Args();

	Com_Printf( "\nDynvars:\n" );
	assert( dynvar_trie );
	Trie_DumpIf( dynvar_trie, "", TRIE_DUMP_VALUES, Dynvar_Console, pattern, &dump );
	for( i = 0; i < dump->size; ++i )
	{
		dynvar_t *const dynvar = (dynvar_t *) dump->key_value_vector[i].value;
		Com_Printf( "%s\n", Dynvar_GetName( dynvar ) );
	}
	Trie_FreeDump( dump );
	Com_Printf( "%i dynvars\n", i );
}

static void Dynvar_Set_f( void )
{
	if( Cmd_Argc() == 3 )
	{
		dynvar_t *dynvar;
		dynvar = Dynvar_Lookup( Cmd_Argv( 1 ) );
		if( dynvar )
		{
			if( dynvar->console )
			{
				dynvar_set_status_t status = Dynvar_SetValue( dynvar, Cmd_Argv( 2 ) );
				switch( status )
				{
				case DYNVAR_SET_OK:
					break;
				case DYNVAR_SET_READONLY:
					Com_Printf( (char *) DYNVAR_SET_READONLY_MSG, Dynvar_GetName( dynvar ) );
					break;
				case DYNVAR_SET_INVALID:
					Com_Printf( (char *) DYNVAR_SET_INVALID_MSG, Cmd_Argv( 2 ), Dynvar_GetName( dynvar ) );
					break;
				case DYNVAR_SET_TRANSIENT:
					Com_Printf( (char *) DYNVAR_TRANSIENT_MSG, Dynvar_GetName( dynvar ) );
					break;
				}
			}
			else
				Com_Printf( (char *) DYNVAR_NOT_FOUND_MSG, Dynvar_GetName( dynvar ) );
		}
		else
			Com_Printf( (char *) DYNVAR_NOT_FOUND_MSG, Cmd_Argv( 1 ) );
	}
	else
		Com_Printf( "usage: setdyn <dynvar> <value>\n" );
}

static dynvar_get_status_t Dynvar_WriteOnly_f( void **val )
{
	return DYNVAR_GET_WRITEONLY;
}

static dynvar_set_status_t Dynvar_ReadOnly_f( void *val )
{
	return DYNVAR_SET_READONLY;
}

const dynvar_getter_f DYNVAR_WRITEONLY = Dynvar_WriteOnly_f;
const dynvar_setter_f DYNVAR_READONLY = Dynvar_ReadOnly_f;
