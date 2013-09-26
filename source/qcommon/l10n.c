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

static cvar_t *com_lang;

// ============================================================================

/*
* Com_ParsePOString
*/
static size_t Com_ParsePOString( char *instr, char *outstr )
{
	int i;
	char *q1, *q2;
	char *outstart = outstr;
	char *inend = instr + strlen( instr );

	// accept properly double quoted strings
	// or strings without double quotes at all
	q1 = strchr( instr, '"' );
	q2 = strrchr( instr, '"' );
	if( q1 && q2 ) {
		if( q2 <= q1 ) {
			// TODO: print error message
			return 0;
		}
		q1++;
		*q2 = '\0';
	}
	else {
		if( q1 && !q2 || !q1 && q2 ) {
			// TODO: print error message
			return 0;
		}
		// no quotes
		for( q1 = instr; *q1 == ' ' || *q1 == '\t'; q1++ );
	}

	// skip empty lines
	if( !*q1 ) {
		// TODO: print error message
		return 0;
	}

	for( instr = q1; instr < inend && *instr; instr++ ) {
		char num;
		char c = *instr;

		switch (c) {
			case '\\':
				c = *(++instr);
				if (!c)
					break;
				switch (c) {
					case 'a':
						*outstr++ = '\a';
						break;
					case 'b':
						*outstr++ = '\b';
						break;
					case 'f':
						*outstr++ = '\f';
						break;
					case 'n':
						*outstr++ = '\n';
						break;
					case 'r':
						*outstr++ = '\r';
						break;
					case 't':
						*outstr++ = '\t';
						break;
					case 'v':
						*outstr++ = '\v';
						break;
					case 'x':
						// hexadecimals
						num = 0;
						for( i = 0; i < 2; i++ ) {
							c = *(++instr);
							if( c >= '0' && c <= '9' ) {
								num = (num << 4) | (c - '0');
							} else if( c >= 'a' && c <= 'f' ) {
								num = (num << 4) | (c - 'a');
							} else if( c >= 'A' && c <= 'F' ) {
								num = (num << 4) | (c - 'A');
							} else {
								instr -= 2;
								break;
							}
						}
						*outstr++ = num ? num : 'x';
						break;
					default:
						// octals
						num = 0;
						instr--;
						if( c >= '0' && c <= '7' ) {
							for( i = 0; i < 3; i++ ) {
								c = *(++instr);
								if( c >= '0' && c <= '7' ) {
									num = (num << 3) | (c - '0');
								} else {
									instr -= 2;
									break;
								}
							}
						}
						*outstr++ = num ? num : c;
						break;
				}
				break;
			default:
				*outstr++ = c;
				break;
		}
	}
	*outstr++ = '\0';
	return outstr - outstart - 1;
}

/*
* Com_ParsePOFile
*/
static trie_t *Com_ParsePOFile( char *buffer, int length )
{
	char *start = buffer, *end = buffer + length;
	char *cur, *eol;
	qboolean have_msgid, have_msgstr;
	char *msgid, *msgstr, *instr, *outstr;
	size_t str_length;
	trie_t *dict;
	trie_error_t tr_err;

	tr_err = Trie_Create( TRIE_CASE_SENSITIVE, &dict );
	if( tr_err != TRIE_OK ) {
		return NULL;
	}

	have_msgid = have_msgstr = qfalse;
	instr = outstr = buffer;
	msgid = msgstr = buffer;

	for( cur = start; cur >= start && cur < end; cur = eol + 1 ) {
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
			char *prev = eol - 1;
			*eol = '\0';
			if( *prev == '\r' ) {
				*prev = '\0';
			}
		}

		if( *cur == '#' ) {
			continue;
		}

		if( !*cur ) {
			// empty line
			continue;
		}

parse_cmd:
		// search for msgid "id"
		if( !strncmp( cur, "msgid ", 6 ) ) {
			if( have_msgstr ) {
				Trie_Insert( dict, msgid, ( void * )ZoneCopyString( msgstr ) );
			}
			have_msgid = qtrue;
			instr = cur + 6;
			outstr = cur + 5;
			msgid = outstr;
			*msgid = '\0';
		}
		else if( have_msgid && !strncmp( cur, "msgstr ", 7 ) ) {
			have_msgstr = qtrue;
			instr = cur + 7;
			outstr = cur + 6;
			msgstr = outstr;
			*msgstr = '\0';
		}
		else {
			// multiline?
			if( have_msgid || have_msgstr ) {
				if( *cur != '"' || !strrchr( cur+1, '"') ) {
					if( have_msgstr ) {
						Trie_Insert( dict, msgid, ( void * )ZoneCopyString( msgstr ) );
					}
					// no
					have_msgid = have_msgstr = qfalse;
					goto parse_cmd;
				}
				// yes
				instr = cur;
			} else {
				continue;
			}
		}

		str_length = Com_ParsePOString( instr, outstr );
		if( !str_length ) {
			have_msgid = have_msgstr = qfalse;			
		}
		else {
			outstr += str_length;
		}
	}

	if( have_msgstr ) {
		Trie_Insert( dict, msgid, ( void * )ZoneCopyString( msgstr ) );
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

	length = FS_LoadFile( filepath, ( void ** )&buffer, NULL, 0 );
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
	pofile = ( pofile_t * )Mem_ZoneMalloc( sizeof( *pofile ) );
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
	podomain = ( podomain_t * )Mem_ZoneMalloc( sizeof( *podomain ) );
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
* Com_l10n_CheckUserLanguage
*/
void Com_l10n_CheckUserLanguage( void )
{
	if( com_lang->modified ) {
		if( !com_lang->string[0] ) {
			const char *lang;

			lang = Sys_GetPreferredLanguage();
			if( !lang || !lang[0] ) {
				lang = APP_DEFAULT_LANGUAGE;
			}
			Cvar_ForceSet( com_lang->name, lang );
		}
		com_lang->modified = qfalse;
	}
}

/*
* Com_l10n_Init
*/
void Com_l10n_Init( void )
{
	podomains_head = NULL;

	com_lang = Cvar_Get( "com_lang", "", CVAR_NOSET );
	com_lang->modified = qtrue;

	Com_l10n_CheckUserLanguage();
}

/*
* Com_l10n_LoadLangPOFile
*/
void Com_l10n_LoadLangPOFile( const char *domainname, const char *filepath )
{
	podomain_t *podomain;
	pofile_t *pofile;
	trie_t *pofile_dict;
	char *tempfilename;
	const char *sep;
	size_t tempfilename_size;

	if( !domainname || !*domainname ) {
		return;
	}
	if( !filepath || !*filepath ) {
		return;
	}

	podomain = Com_FindPODomain( domainname );
	if( !podomain ) {
		podomain = Com_CreatePODomain( domainname );
		podomain->next = podomains_head;
		podomains_head = podomain;
	}

	tempfilename_size = strlen( filepath ) + strlen( "/" ) + strlen( com_lang->string ) + strlen( ".po" ) + 1;
	tempfilename = ( char * )Mem_TempMalloc( tempfilename_size );

	sep = ( filepath[strlen( filepath ) - 1] == '/' ? "" : "/" );
	Q_snprintfz( tempfilename, tempfilename_size, "%s%s%s.po", filepath, sep, com_lang->string );

	pofile_dict = Com_LoadPOFile( tempfilename );
	if( pofile_dict ) {
		pofile = Com_CreatePOFile( filepath );
		pofile->dict = pofile_dict;
		pofile->next = podomain->pofiles_head;
		podomain->pofiles_head = pofile;
	}

	Mem_TempFree( tempfilename );
}

/*
* Com_l10n_LookupString
*/
const char *Com_l10n_LookupString( const podomain_t *podomain, const char *string )
{
	const pofile_t *pofile;
	const trie_t *dict;
	char *result = NULL;

	for( pofile = podomain->pofiles_head; pofile; pofile = pofile->next ) {
		dict = pofile->dict;
		if( Trie_Find( dict, string, TRIE_EXACT_MATCH, (void **)&result ) == TRIE_OK ) {
			return result;
		}
	}
	return result;
}

/*
* Com_l10n_TranslateString
*/
const char *Com_l10n_TranslateString( const char *domainname, const char *string )
{
	podomain_t *podomain;
	const char *result;
	
	if( !string || !*string ) {
		return string;
	}

	podomain = Com_FindPODomain( domainname );
	if( podomain ) {
		result = Com_l10n_LookupString( podomain, string );
		if( result ) {
			return result;
		}
	}

	podomain = Com_FindPODomain( "common" );
	if( podomain ) {
		result = Com_l10n_LookupString( podomain, string );
		if( result ) {
			return result;
		}
	}

	return NULL;
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
