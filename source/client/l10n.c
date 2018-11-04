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

#include "client.h"
#include "../qalgo/q_trie.h"
#include "l10n.h"

typedef struct {
	trie_t *trie;
	char *buffer;
} podict_t;

typedef struct pofile_s {
	char *path;
	podict_t *dict;
	struct pofile_s *next;
} pofile_t;

typedef struct podomain_s {
	char *name;
	pofile_t *pofiles_head;
	struct podomain_s *next;
} podomain_t;

static mempool_t *pomempool;

static char posyslang[MAX_STRING_CHARS];

static podomain_t *podomains_head;

// "common" domain
static const char * podomain_common_name = "common";
static podomain_t *podomain_common;

#define L10n_Malloc( size ) _Mem_Alloc( pomempool, size, 0, 0, __FILE__, __LINE__ )
#define L10n_Free( data ) _Mem_Free( data, 0, 0, __FILE__, __LINE__ )
#define L10n_CopyString( string ) _Mem_CopyString( pomempool, string, __FILE__, __LINE__ )

// ============================================================================

/*
* L10n_ParsePOString
*/
static size_t L10n_ParsePOString( char *instr, char *outstr, bool *err ) {
	int i;
	char *q1, *q2;
	char *outstart = outstr;
	char *inend = instr + strlen( instr );

	*err = false;
	while( *instr == ' ' || *instr == '\t' ) instr++;

	// accept properly double quoted strings
	// or strings without double quotes at all
	q1 = strchr( instr, '"' );
	q2 = strrchr( instr, '"' );

	if( *instr != '"' && q1 ) {
		// do not accept string that do not start with a double
		// quote but nonetheless contain a double quote
		*err = true;
		return 0;
	}

	if( q1 && q2 ) {
		if( q2 <= q1 ) {
			*err = true;
			return 0;
		}
		q1++;
		*q2 = '\0';
	} else {
		if( ( q1 && !q2 ) || ( !q1 && q2 ) ) {
			*err = true;
			return 0;
		}
		// no quotes
		for( q1 = instr; *q1 == ' ' || *q1 == '\t'; q1++ ) ;
	}

	// skip empty lines
	if( !*q1 ) {
		return 0;
	}

	for( instr = q1; instr < inend && *instr; instr++ ) {
		char num;
		char c = *instr;

		switch( c ) {
			case '\\':
				c = *( ++instr );
				if( !c ) {
					break;
				}
				switch( c ) {
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
							c = *( ++instr );
							if( c >= '0' && c <= '9' ) {
								num = ( num << 4 ) | ( c - '0' );
							} else if( c >= 'a' && c <= 'f' ) {
								num = ( num << 4 ) | ( c - 'a' );
							} else if( c >= 'A' && c <= 'F' ) {
								num = ( num << 4 ) | ( c - 'A' );
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
						if( c >= '0' && c <= '7' ) {
							instr--;
							for( i = 0; i < 3; i++ ) {
								c = *( ++instr );
								if( c >= '0' && c <= '7' ) {
									num = ( num << 3 ) | ( c - '0' );
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
* L10n_ParsePOFile
*
* Doesn't allocate memory, writes to input buffer, which
* should NOT be freed after calling this function.
*/
static trie_t *L10n_ParsePOFile( const char *filepath, char *buffer, int length ) {
	int linenum = 0;
	char *start = buffer, *end = buffer + length;
	char *cur, *eol;
	bool have_msgid, have_msgstr, error;
	char *msgid, *msgstr, *instr, *outstr;
	size_t str_length;
	trie_t *dict;
	trie_error_t tr_err;

	tr_err = Trie_Create( TRIE_CASE_SENSITIVE, &dict );
	if( tr_err != TRIE_OK ) {
		return NULL;
	}

	have_msgid = have_msgstr = false;
	instr = outstr = buffer;
	msgid = msgstr = buffer;
	eol = end;

	for( cur = start; cur >= start && cur < end; cur = eol + 1 ) {
		if( !*cur ) {
			break;
		}

		linenum++;

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
				Trie_Insert( dict, msgid, ( void * )msgstr );
				have_msgid = have_msgstr = false;
			}
			have_msgid = true;
			instr = cur + 6;
			outstr = cur + 5;
			msgid = outstr;
			*msgid = '\0';
		} else if( have_msgid && !strncmp( cur, "msgstr ", 7 ) ) {
			have_msgstr = true;
			instr = cur + 7;
			outstr = cur + 6;
			msgstr = outstr;
			*msgstr = '\0';
		} else {
			// multiline?
			if( have_msgid || have_msgstr ) {
				if( *cur != '"' || !strrchr( cur + 1, '"' ) ) {
					if( have_msgstr ) {
						Trie_Insert( dict, msgid, ( void * )msgstr );
					}
					// no
					have_msgid = have_msgstr = false;
					goto parse_cmd;
				}
				// yes
				instr = cur;
			} else {
				continue;
			}
		}

		// parse single line of C-style string
		str_length = L10n_ParsePOString( instr, outstr, &error );
		if( !str_length ) {
			have_msgid = have_msgstr = false;
			if( error ) {
				Com_Printf( S_COLOR_YELLOW "Error parsing line %i of %s: syntax error near '%s'\n",
							linenum, filepath, instr );
			}
		} else {
			// shift the output buffer so that in case multiline string
			// is ecountered it'll be properly appended
			outstr += str_length;
		}
	}

	if( have_msgstr ) {
		Trie_Insert( dict, msgid, ( void * )msgstr );
	}

	return dict;
}

/*
* L10n_LoadPODict
*/
static podict_t *L10n_LoadPODict( const char *filepath ) {
	int file;
	int length;
	podict_t *podict;

	length = FS_FOpenFile( filepath, &file, FS_READ );
	if( length < 0 ) {
		return NULL;
	}

	podict = ( podict_t * )L10n_Malloc( sizeof( *podict ) + length + 1 );
	podict->buffer = ( char * )( ( uint8_t * )podict + sizeof( *podict ) );
	FS_Read( podict->buffer, length, file );
	podict->buffer[length] = '\0'; // safeguard
	FS_FCloseFile( file );

	podict->trie = L10n_ParsePOFile( filepath, podict->buffer, length );
	return podict;
}

/*
* L10n_DestroyPODict
*/
static void L10n_DestroyPODict( podict_t *podict ) {
	if( !podict ) {
		return;
	}
	if( podict->trie ) {
		Trie_Destroy( podict->trie );
	}
	L10n_Free( podict );
}

/*
* L10n_CreatePOFile
*/
static pofile_t *L10n_CreatePOFile( const char *filepath ) {
	size_t filepath_size = strlen( filepath ) + 1;
	pofile_t *pofile;
	pofile = ( pofile_t * )L10n_Malloc( sizeof( *pofile ) + filepath_size );
	pofile->path = ( char * )( ( uint8_t * )pofile + sizeof( *pofile ) );
	pofile->next = NULL;
	pofile->dict = NULL;
	memcpy( pofile->path, filepath, filepath_size );
	return pofile;
}

/*
* L10n_DestroyPOFile
*/
static void L10n_DestroyPOFile( pofile_t *pofile ) {
	L10n_DestroyPODict( pofile->dict );
	L10n_Free( pofile );
}

/*
* L10n_FindPOFile
*/
static pofile_t *L10n_FindPOFile( podomain_t *podomain, const char *filepath ) {
	pofile_t *pofile;

	if( !podomain ) {
		return NULL;
	}

	for( pofile = podomain->pofiles_head; pofile != NULL; pofile = pofile->next ) {
		if( !Q_stricmp( pofile->path, filepath ) ) {
			return pofile;
		}
	}

	return NULL;
}

/*
* L10n_FindPODomain
*/
static podomain_t *L10n_FindPODomain( const char *name ) {
	podomain_t *podomain;

	if( !name ) {
		return NULL;
	}

	for( podomain = podomains_head; podomain != NULL; podomain = podomain->next ) {
		if( !Q_stricmp( podomain->name, name ) ) {
			return podomain;
		}
	}

	return NULL;
}

/*
* L10n_CreatePODomain
*/
static podomain_t *L10n_CreatePODomain( const char *name ) {
	size_t name_size = strlen( name ) + 1;
	podomain_t *podomain;
	podomain = ( podomain_t * )L10n_Malloc( sizeof( *podomain ) + name_size );
	podomain->name = ( char * )( ( uint8_t * )podomain + sizeof( *podomain ) );
	podomain->next = NULL;
	podomain->pofiles_head = NULL;
	memcpy( podomain->name, name, name_size );
	return podomain;
}

/*
* L10n_ClearPODomain
*/
static void L10n_ClearPODomain( podomain_t *podomain ) {
	pofile_t *pofile, *next;

	for( pofile = podomain->pofiles_head; pofile; pofile = next ) {
		next = pofile->next;
		L10n_DestroyPOFile( pofile );
	}
	podomain->pofiles_head = NULL;
}

/*
* L10n_DestroyPODomain
*/
static void L10n_DestroyPODomain( podomain_t *podomain ) {
	L10n_ClearPODomain( podomain );
	L10n_Free( podomain );
}

/*
* L10n_GetUserLanguage
*/
const char *L10n_GetUserLanguage( void ) {
	return posyslang;
}

/*
* L10n_Init
*/
void L10n_Init( void ) {
	podomains_head = NULL;

	pomempool = Mem_AllocPool( NULL, "L10n" );

	const char *syslang = Sys_GetPreferredLanguage();
	if( !syslang || !syslang[0] ) {
		syslang = APP_DEFAULT_LANGUAGE;
	}
	Q_strncpyz( posyslang, syslang, sizeof( posyslang ) );
}

/*
* L10n_LoadLangPOFile_
*/
static bool L10n_LoadLangPOFile_( podomain_t *podomain, const char *filepath, const char *lang ) {
	pofile_t *pofile;
	podict_t *pofile_dict;
	char *tempfilename;
	const char *sep;
	size_t tempfilename_size;

	if( !filepath || !*filepath ) {
		return false;
	}

	tempfilename_size = strlen( filepath ) + 1 + strlen( lang ) + ( sizeof( ".po" ) - 1 ) + 1;
	tempfilename = ( char * )L10n_Malloc( tempfilename_size );

	sep = ( filepath[strlen( filepath ) - 1] == '/' ? "" : "/" );
	Q_snprintfz( tempfilename, tempfilename_size, "%s%s%s.po", filepath, sep, lang );

	pofile = L10n_FindPOFile( podomain, tempfilename );
	if( pofile ) {
		// already loaded
		return true;
	}

	pofile_dict = L10n_LoadPODict( tempfilename );
	if( !pofile_dict ) {
		// try to load country-independent file
		char *underscore = strchr( lang, '_' );
		if( underscore ) {
			char *tempfilename2 = L10n_Malloc( tempfilename_size );
			*underscore = '\0';
			Q_snprintfz( tempfilename2, tempfilename_size, "%s%s%s.po", filepath, sep, lang );
			pofile_dict = L10n_LoadPODict( tempfilename2 );
			L10n_Free( tempfilename2 );
		}
	}

	if( pofile_dict ) {
		pofile = L10n_CreatePOFile( tempfilename );
		pofile->dict = pofile_dict;
		pofile->next = podomain->pofiles_head;
		podomain->pofiles_head = pofile;
	}

	L10n_Free( tempfilename );

	return pofile_dict != NULL;
}

/*
* L10n_LoadLangPOFile
*/
void L10n_LoadLangPOFile( const char *domainname, const char *filepath ) {
	podomain_t *podomain;
	char lang[MAX_STRING_CHARS];

	if( !domainname || !*domainname ) {
		return;
	}
	if( !filepath || !*filepath ) {
		return;
	}

	if( !COM_ValidateFilename( filepath ) ) {
		Com_Printf( S_COLOR_YELLOW "LoadLangPOFile failed: invalid filename '%s'\n", filepath );
		return;
	}

	podomain = L10n_FindPODomain( domainname );
	if( !podomain ) {
		podomain = L10n_CreatePODomain( domainname );
		podomain->next = podomains_head;
		podomains_head = podomain;

		// store pointer to common domain for quick access
		if( podomain_common == NULL && !Q_stricmp( domainname, podomain_common_name ) ) {
			podomain_common = podomain;
		}
	}

	Q_strncpyz( lang, L10n_GetUserLanguage(), sizeof( lang ) );

	if( !L10n_LoadLangPOFile_( podomain, filepath, lang ) ) {
		// load default lang .po file
		if( strcmp( lang, APP_DEFAULT_LANGUAGE ) ) {
			L10n_LoadLangPOFile_( podomain, filepath, APP_DEFAULT_LANGUAGE );
		}
	}
}

/*
* L10n_LookupString
*/
const char *L10n_LookupString( const podomain_t *podomain, const char *string ) {
	const pofile_t *pofile;
	const podict_t *dict;
	char *result = NULL;

	for( pofile = podomain->pofiles_head; pofile; pofile = pofile->next ) {
		dict = pofile->dict;
		if( Trie_Find( dict->trie, string, TRIE_EXACT_MATCH, (void **)&result ) == TRIE_OK ) {
			return result;
		}
	}
	return result;
}

/*
* L10n_TranslateString
*/
const char *L10n_TranslateString( const char *domainname, const char *string ) {
	const podomain_t *podomain;
	const char *result;

	if( !string || !*string ) {
		return string;
	}

	podomain = L10n_FindPODomain( domainname );
	if( podomain ) {
		result = L10n_LookupString( podomain, string );
		if( result ) {
			return result;
		}
	}

	podomain = podomain_common;
	if( podomain ) {
		result = L10n_LookupString( podomain, string );
		if( result ) {
			return result;
		}
	}

	return NULL;
}

/*
* L10n_ClearDomain
*/
void L10n_ClearDomain( const char *domainname ) {
	podomain_t *podomain;

	podomain = L10n_FindPODomain( domainname );
	if( podomain ) {
		L10n_ClearPODomain( podomain );
	}
}

/*
* L10n_ClearDomains
*/
void L10n_ClearDomains( void ) {
	podomain_t *podomain;

	for( podomain = podomains_head; podomain; podomain = podomain->next ) {
		L10n_ClearPODomain( podomain );
	}
}

/*
* L10n_Shutdown
*/
void L10n_Shutdown( void ) {
	podomain_t *podomain, *next;

	for( podomain = podomains_head; podomain; podomain = next ) {
		next = podomain->next;
		L10n_DestroyPODomain( podomain );
	}

	Mem_FreePool( &pomempool );

	podomains_head = NULL;
}
