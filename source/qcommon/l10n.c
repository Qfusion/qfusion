/*
Copyright (C) 2013 Victor Luchits

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
#include "../gameshared/q_trie.h"
#include "l10n.h"

typedef struct pofile_s
{
	char *path;
	trie_t *dict;
	struct pofile_s *next;
} pofile_t;

typedef struct podomain_s
{
	char *name;
	pofile_t *pofiles_head;
	struct podomain_s *next;
} podomain_t;

static podomain_t *podomains_head;

// ============================================================================

/*
* Com_ParsePOString
*/
static char *Com_ParsePOString( char *str )
{
}

/*
* Com_ParsePOFile
*/
static trie_t *Com_ParsePOFile( char *buffer, int length )
{
	char *start = buffer, *end = buffer + length;
	char *cur, *eol;
	char *msgid;
	char *q1, *q2;
	trie_t *dict;
	trie_error_t tr_err;

	tr_err = Trie_Create( TRIE_CASE_SENSITIVE, &dict );
	if( tr_err != TRIE_OK ) {
		return NULL;
	}

	for( cur = start; cur && *cur && cur < end; cur++ ) {
		if( !*cur ) {
			break;
		}

		// skip whitespaces
		while( *cur == ' ' || *cur == '\t' ) {
			cur++;
		}

		// find the end of line
		eol = strchr( cur, '\n' );
		if( eol ) {
			*eol = '\0';
		}

		if( *cur == '#' ) {
			cur = eol;
			continue;
		}

		// search for msgid "id"
		if( !msgid ) {
			if( !strncmp( cur, "msgid ", 6 ) ) {
				cur = eol;
				continue;
			}

			// "abc"
			// 01234
			q1 = strchr( cur, '"' );
			q2 = strrchr( cur, '"' );
			if( q1 && q2 && (q2 > q1) ) {
				continue;
			}

			*q2 = '\0';
			Com_ParsePOString( q1 + 1 );
		}
	}

	return dict;
}

/*
* Com_LoadPOFile
*/
static trie_t *Com_LoadPOFile( const char *filepath )
{
	int length;
	char *buffer;
	trie_t *dict;

	length = FS_LoadFile( filepath, &buffer, NULL, 0 );
	if( length < 0 ) {
		return NULL;
	}

	dict = Com_ParsePOFile( buffer, length );

	FS_FreeFile( buffer );
	
	return dict;
}

/*
* Com_CreatePOFile
*/
static pofile_t *Com_CreatePOFile( const char *filepath )
{
	pofile_t *pofile;
	pofile = Mem_ZoneMalloc( sizeof( *pofile ) );
	pofile->path = ZoneCopyString( filepath );
	pofile->next = NULL;
	pofile->dict = NULL;
	return pofile;
}

/*
* Com_DestroyPOFile
*/
static void Com_DestroyPOFile( pofile_t *pofile )
{
	Trie_Destroy( pofile->dict );
	Mem_ZoneFree( pofile->path );
	Mem_ZoneFree( pofile );
}

/*
* Com_FindPODomain
*/
static podomain_t *Com_FindPODomain( const char *name )
{
	podomain_t *podomain;

	for( podomain = podomains_head; podomain != NULL; podomain = podomain->next ) {
		if( !Q_stricmp( podomain->name, name ) ) {
			return podomain;
		}
	}

	return NULL;
}

/*
* Com_CreatePODomain
*/
static podomain_t *Com_CreatePODomain( const char *name )
{
	podomain_t *podomain;
	podomain = Mem_ZoneMalloc( sizeof( *podomain ) );
	podomain->name = ZoneCopyString( name );
	podomain->next = NULL;
	podomain->pofiles_head = NULL;
	return podomain;
}

/*
* Com_DestroyPODomain
*/
static void Com_DestroyPODomain( podomain_t *podomain )
{
	pofile_t *pofile, *next;
	
	for( pofile = podomain->pofiles_head; pofile; pofile = next )
	{
		next = pofile->next;
		Com_DestroyPOFile( pofile );
	}

	Mem_ZoneFree( podomain->name );
	Mem_ZoneFree( podomain );
}

/*
* Com_l10n_Init
*/
void Com_l10n_Init( void )
{
	podomains_head = NULL;
}

/*
* Com_l10n_LoadPOFile
*/
void Com_l10n_LoadPOFile( const char *domainname, const char *filepath )
{
	podomain_t *podomain;
	pofile_t *pofile;
	trie_t *pofile_dict;

	podomain = Com_FindPODomain( domainname );
	if( !podomain ) {
		podomain = Com_CreatePODomain( domainname );
		podomain->next = podomains_head;
		podomains_head = podomain;
	}

	pofile_dict = Com_LoadPOFile( filepath );
	if( !pofile_dict ) {
		return;
	}

	pofile = Com_CreatePOFile( filepath );
	pofile->dict = pofile_dict;
	pofile->next = podomain->pofiles_head;

	podomain->pofiles_head = pofile;
}

/*
* Com_l10n_Shutdown
*/
void Com_l10n_Shutdown( void )
{
	podomain_t *podomain, *next;

	for( podomain = podomains_head; podomain; podomain = next )
	{
		next = podomain->next;
		Com_DestroyPODomain( podomain );
	}

	podomains_head = NULL;
}
