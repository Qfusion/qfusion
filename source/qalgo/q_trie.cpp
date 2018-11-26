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

#include "../gameshared/q_arch.h"
#include "q_trie.h"

#include <assert.h>
#include <string.h>

/* Trie structure definitions */

struct trie_node_s {
	int depth;
	char letter;
	struct trie_node_s *child;
	struct trie_node_s *sibling;
	int data_is_set;
	void *data;
};

struct trie_s {
	struct trie_node_s *root;
	unsigned int size;
	trie_casing_t casing;
};

typedef enum trie_remove_result_t {
	TRIE_REMOVE_NO_CHILDREN_OR_DATA_LEFT = 0,
	TRIE_REMOVE_CHILDREN_OR_DATA_LEFT = 1,
	TRIE_REMOVE_DATA_LEFT,
	TRIE_REMOVE_KEY_NOT_FOUND
} trie_remove_result_t;

/* Forward declarations of internal implementation */

static struct trie_node_s *Trie_CreateNode(
	int depth,
	char letter,
	struct trie_node_s *child,
	struct trie_node_s *sibling,
	int data_is_set,
	void *data
	);

static void Trie_Destroy_Rec(
	struct trie_node_s *node
	);

static struct trie_node_s *TRIE_Find_Rec(
	struct trie_node_s *node,
	const char *key,
	trie_find_mode_t mode,
	trie_casing_t casing,
	int ( *predicate )( void *value, void *cookie ),
	void *cookie
	);

static int Trie_Insert_Rec(
	struct trie_node_s *node,
	const char *key,
	trie_casing_t casing,
	void *data
	);

static trie_remove_result_t Trie_Remove_Rec(
	struct trie_node_s *node,
	const char *key,
	trie_casing_t casing,
	void **data
	);

static unsigned int Trie_NoOfKeys(
	const struct trie_node_s *node,
	trie_casing_t casing,
	int ( *predicate )( void *value, void *cookie ),
	void *cookie,
	int addSiblings
	);

static void Trie_Dump_Rec(
	const struct trie_node_s *node,
	trie_dump_what_t what,
	trie_casing_t casing,
	int ( *predicate )( void *value, void *cookie ),
	void *cookie,
	int dumpSiblings,
	const char *key_prev,
	struct trie_key_value_s **key_value_vector
	);

static int Trie_AlwaysTrue(
	void *,
	void *
	);

static inline int Trie_LetterCompare(
	char left,
	char right,
	trie_casing_t casing
	);

/* External trie functions */

trie_error_t Trie_Create(
	trie_casing_t casing,
	struct trie_s **trie
	) {
	if( trie ) {
		*trie = (struct trie_s *) malloc( sizeof( struct trie_s ) );
		( *trie )->root = Trie_CreateNode( 0, '\0', NULL, NULL, 0, NULL );
		( *trie )->size = 0;
		( *trie )->casing = casing;
		return TRIE_OK;
	} else {
		return TRIE_INVALID_ARGUMENT;
	}
}

trie_error_t Trie_Destroy(
	struct trie_s *trie
	) {
	if( trie ) {
		Trie_Destroy_Rec( trie->root );
		free( trie );
		return TRIE_OK;
	} else {
		return TRIE_INVALID_ARGUMENT;
	}
}

trie_error_t Trie_Clear(
	struct trie_s *trie
	) {
	if( trie ) {
		Trie_Destroy_Rec( trie->root );
		trie->root = Trie_CreateNode( 0, '\0', NULL, NULL, 0, NULL );
		trie->size = 0;
		return TRIE_OK;
	} else {
		return TRIE_INVALID_ARGUMENT;
	}
}

trie_error_t Trie_GetSize(
	struct trie_s *trie,
	unsigned int *size
	) {
	if( trie && size ) {
		*size = trie->size;
		return TRIE_OK;
	} else {
		return TRIE_INVALID_ARGUMENT;
	}
}

trie_error_t Trie_Insert(
	struct trie_s *trie,
	const char *key,
	void *data
	) {
	if( trie && key ) {
		if( !Trie_Insert_Rec( trie->root, key, trie->casing, data ) ) {
			// insertion successful
			++trie->size;
			return TRIE_OK;
		} else {
			// key already in trie
			return TRIE_DUPLICATE_KEY;
		}
	} else {
		return TRIE_INVALID_ARGUMENT;
	}
}

trie_error_t Trie_Remove(
	struct trie_s *trie,
	const char *key,
	void **data
	) {
	if( trie && key && data ) {
		if( Trie_Remove_Rec( trie->root, key, trie->casing, data ) != TRIE_REMOVE_KEY_NOT_FOUND ) {
			// removal successful
			--trie->size;
			return TRIE_OK;
		} else {
			return TRIE_KEY_NOT_FOUND;
		}
	} else {
		return TRIE_INVALID_ARGUMENT;
	}
}

trie_error_t Trie_Replace(
	struct trie_s *trie,
	const char *key,
	void *data_new,
	void **data_old
	) {
	if( trie && key ) {
		struct trie_node_s *result = TRIE_Find_Rec( trie->root, key, TRIE_EXACT_MATCH, trie->casing, Trie_AlwaysTrue, NULL );
		if( result ) {
			// key found, replace data pointer
			*data_old = result->data;
			result->data = data_new;
			return TRIE_OK;
		} else {
			return TRIE_KEY_NOT_FOUND;
		}
	} else {
		return TRIE_INVALID_ARGUMENT;
	}
}

trie_error_t Trie_Find(
	const struct trie_s *trie,
	const char *key,
	trie_find_mode_t mode,
	void **data
	) {
	return Trie_FindIf( trie, key, mode, Trie_AlwaysTrue, NULL, data );
}

trie_error_t Trie_FindIf(
	const struct trie_s *trie,
	const char *key,
	trie_find_mode_t mode,
	int ( *predicate )( void *value, void *cookie ),
	void *cookie,
	void **data
	) {
	if( trie && key && data ) {
		const struct trie_node_s *result = TRIE_Find_Rec( trie->root, key, mode, trie->casing, predicate, cookie );
		if( result ) {
			while( result->child && !result->data_is_set ) {
				const struct trie_node_s *sibling;
				for( sibling = result; sibling->sibling && !sibling->data_is_set; sibling = sibling->sibling )
					/* search for sibling with data */;
				if( sibling->data_is_set ) {
					// sibling found, make it the result
					result = sibling;
					break;
				}
				// descend
				result = result->child;
			}
			assert( result->data_is_set );
			*data = result->data;
			return TRIE_OK;
		} else {
			*data = NULL;
			return TRIE_KEY_NOT_FOUND;
		}
	} else {
		return TRIE_INVALID_ARGUMENT;
	}
}

trie_error_t Trie_NoOfMatches(
	const struct trie_s *trie,
	const char *prefix,
	unsigned int *matches
	) {
	return Trie_NoOfMatchesIf( trie, prefix, Trie_AlwaysTrue, NULL, matches );
}

trie_error_t Trie_NoOfMatchesIf(
	const struct trie_s *trie,
	const char *prefix,
	int ( *predicate )( void *value, void *cookie ),
	void *cookie,
	unsigned int *matches
	) {
	if( trie && prefix && matches ) {
		struct trie_node_s *node = TRIE_Find_Rec( trie->root, prefix, TRIE_PREFIX_MATCH, trie->casing, predicate, cookie );
		*matches = node
				   ? Trie_NoOfKeys( node, trie->casing, predicate, cookie, 0 )
				   : 0;
		return TRIE_OK;
	} else {
		return TRIE_INVALID_ARGUMENT;
	}
}

trie_error_t Trie_Dump(
	const struct trie_s *trie,
	const char *prefix,
	trie_dump_what_t what,
	struct trie_dump_s **dump
	) {
	return Trie_DumpIf( trie, prefix, what, Trie_AlwaysTrue, NULL, dump );
}

trie_error_t Trie_DumpIf(
	const struct trie_s *trie,
	const char *prefix,
	trie_dump_what_t what,
	int ( *predicate )( void *value, void *cookie ),
	void *cookie,
	struct trie_dump_s **dump
	) {
	if( prefix && dump && predicate ) {
		struct trie_node_s *result = TRIE_Find_Rec( trie->root, prefix, TRIE_PREFIX_MATCH, trie->casing, predicate, cookie );
		*dump = (struct trie_dump_s *) malloc( sizeof( struct trie_dump_s ) );
		// prefix matches some nodes, begin dump
		if( result ) {
			( *dump )->size = Trie_NoOfKeys( result, trie->casing, predicate, cookie, 0 );
			( *dump )->what = what;
			( *dump )->key_value_vector = (struct trie_key_value_s *) malloc( sizeof( struct trie_key_value_s ) * ( ( *dump )->size + 1 ) );
			Trie_Dump_Rec( result, what, trie->casing, predicate, cookie, 0, prefix, &( *dump )->key_value_vector );
			( *dump )->key_value_vector -= ( *dump )->size;
		} else {
			( *dump )->key_value_vector = NULL;
			( *dump )->size = 0;
		}
		return TRIE_OK;
	} else {
		return TRIE_INVALID_ARGUMENT;
	}
}

trie_error_t Trie_FreeDump(
	struct trie_dump_s *dump
	) {
	if( dump ) {
		unsigned int i;
		for( i = 0; i < dump->size; ++i )
			if( dump->key_value_vector[i].key ) {
				free( (char *) dump->key_value_vector[i].key );
			}
		free( dump->key_value_vector );
		free( dump );
	}
	return TRIE_OK;
}

/* Internal implementations */

static struct trie_node_s *Trie_CreateNode(
	int depth,
	char letter,
	struct trie_node_s *child,
	struct trie_node_s *sibling,
	int data_is_set,
	void *data
	) {
	struct trie_node_s *result = (struct trie_node_s *) malloc( sizeof( struct trie_node_s ) );
	assert( result );
	result->depth = depth;
	result->letter = letter;
	result->child = child;
	result->sibling = sibling;
	result->data_is_set = data_is_set;
	result->data = data;
	return result;
}

static void Trie_Destroy_Rec(
	struct trie_node_s *node
	) {
	assert( node );
	if( node->sibling ) {
		Trie_Destroy_Rec( node->sibling );
	}
	if( node->child ) {
		Trie_Destroy_Rec( node->child );
	}
	free( node );
}

static struct trie_node_s *TRIE_Find_Rec(
	struct trie_node_s *node,
	const char *key,
	trie_find_mode_t mode,
	trie_casing_t casing,
	int ( *predicate )( void *value, void *cookie ),
	void *cookie
	) {
	assert( key );
	assert( node );
	if( !Trie_LetterCompare( *key, node->letter, casing ) ) {
		// prefix matches
		if( !*key || !*( key + 1 ) ) {
			// end of key reached, see if node contains data
			if( mode == TRIE_PREFIX_MATCH || node->data_is_set ) {
				// node contains data or prefix match only, key is valid
				return node;
			} else {
				// no data supplied, key only matches prefix of some node in trie
				return NULL;
			}
		} else if( node->child ) {
			// end of key not reached, continue with child
			return TRIE_Find_Rec( node->child, key + 1, mode, casing, predicate, cookie );
		} else {
			// end of key not reached, but current node is a leaf
			return NULL;
		}
	} else if( node->sibling && Trie_LetterCompare( node->sibling->letter, *key, casing ) <= 0 ) {
		// prefix does not match, but we might have a matching sibling
		return TRIE_Find_Rec( node->sibling, key, mode, casing, predicate, cookie );
	} else if( !node->depth ) {
		// node is root
		if( !*key ) {
			if( mode == TRIE_PREFIX_MATCH || node->data_is_set ) {
				// key is "", return root
				return node;
			} else {
				// key is "", but root does not contain data
				return NULL;
			}
		} else if( node->child ) {
			return TRIE_Find_Rec( node->child, key, mode, casing, predicate, cookie );
		} else {
			return NULL;
		}
	} else {
		// prefix does not match, no matching siblings
		return NULL;
	}
}

static int Trie_Insert_Rec(
	struct trie_node_s *node,
	const char *key,
	trie_casing_t casing,
	void *data
	) {
	assert( key );
	assert( node );
	if( !node->depth || !Trie_LetterCompare( *key, node->letter, casing ) ) {
		// node is root or prefix matches
		if( ( !node->depth && !*key ) || ( node->depth && !*( key + 1 ) ) ) {
			// end of key reached, set data
			if( !node->data_is_set ) {
				node->data = data;
				node->data_is_set = 1;
				return TRIE_OK;
			} else {
				return TRIE_DUPLICATE_KEY;
			}
		} else {
			// not end of key, descend to child
			const char *const nextKey = node->depth
										? key + 1
										: key;
			if( !node->child || Trie_LetterCompare( node->child->letter, *nextKey, casing ) > 0 ) {
				// no matching child, create one
				node->child = Trie_CreateNode( node->depth + 1, *nextKey, NULL, node->child, 0, NULL );
			}
			// descend to matching child
			return Trie_Insert_Rec( node->child, nextKey, casing, data );
		}
	} else {
		assert( node->depth );
		if( !node->sibling || Trie_LetterCompare( node->sibling->letter, *key, casing ) > 0 ) {
			node->sibling = Trie_CreateNode( node->depth, *key, NULL, node->sibling, 0, NULL );
		}
		return Trie_Insert_Rec( node->sibling, key, casing, data );
	}
}

static trie_remove_result_t Trie_Remove_Rec(
	struct trie_node_s *node,
	const char *key,
	trie_casing_t casing,
	void **data
	) {
	trie_remove_result_t status;
	assert( node );
	assert( key );
	if( node->depth && Trie_LetterCompare( node->letter, *key, casing ) < 0 ) {
		// node is not root and prefix does not match
		if( node->sibling ) {
			// call recursively for sibling
			status = Trie_Remove_Rec( node->sibling, key, casing, data );
			if( status == TRIE_REMOVE_NO_CHILDREN_OR_DATA_LEFT ) {
				// sibling node has no children or data, free it and preserve siblings
				struct trie_node_s *sibling = node->sibling->sibling;
				free( node->sibling );
				node->sibling = sibling;
				// ch : is this right?
				// return ( node->child != NULL ) || ( node->data_is_set );
				return ( node->child != NULL ) || ( node->data_is_set ) ? TRIE_REMOVE_CHILDREN_OR_DATA_LEFT : TRIE_REMOVE_NO_CHILDREN_OR_DATA_LEFT;
			} else {
				return status;
			}
		} else {
			// key not found
			return TRIE_REMOVE_KEY_NOT_FOUND;
		}
	} else if( !node->depth || !Trie_LetterCompare( node->letter, *key, casing ) ) {
		// prefix matches or node is root, check for end of key
		if( !( !node->depth && !*key ) && !( node->depth && !*( key + 1 ) ) ) {
			// not end of key, descend
			if( node->child ) {
				status = Trie_Remove_Rec( node->child, node->depth ? key + 1 : key, casing, data );
				if( !status ) {
					// child node has no children, free it and preserve siblings
					struct trie_node_s *sibling = node->child->sibling;
					free( node->child );
					node->child = sibling;
					// ch : is this right?
					// return ( node->child != NULL ) || ( node->data_is_set );
					return ( node->child != NULL ) || ( node->data_is_set ) ? TRIE_REMOVE_CHILDREN_OR_DATA_LEFT : TRIE_REMOVE_NO_CHILDREN_OR_DATA_LEFT;
				} else {
					return status;
				}
			} else {
				// key not found
				return TRIE_REMOVE_KEY_NOT_FOUND;
			}
		} else {
			// end of key
			*data = node->data;
			node->data = NULL;
			node->data_is_set = 0;
			// ch : is this right?
			// return ( node->child != 0 );
			return ( node->child != 0 ) ? TRIE_REMOVE_CHILDREN_OR_DATA_LEFT : TRIE_REMOVE_NO_CHILDREN_OR_DATA_LEFT;
		}
	} else {
		// key not found
		return TRIE_REMOVE_KEY_NOT_FOUND;
	}
}

static unsigned int Trie_NoOfKeys(
	const struct trie_node_s *node,
	trie_casing_t casing,
	int ( *predicate )( void *value, void *cookie ),
	void *cookie,
	int addSiblings
	) {
	unsigned int noOfKeys;
	assert( node );
	assert( predicate );
	// if data is set, we have a data node, otherwise just a prefix node
	if( node->data_is_set && predicate( node->data, cookie ) ) {
		noOfKeys = 1;
	} else {
		noOfKeys = 0;
	}
	// recursively add siblings and children
	if( addSiblings && node->sibling ) {
		noOfKeys += Trie_NoOfKeys( node->sibling, casing, predicate, cookie, 1 );
	}
	if( node->child ) {
		noOfKeys += Trie_NoOfKeys( node->child, casing, predicate, cookie, 1 );
	}
	return noOfKeys;
}

static void Trie_Dump_Rec(
	const struct trie_node_s *node,
	trie_dump_what_t what,
	trie_casing_t casing,
	int ( *predicate )( void *value, void *cookie ),
	void *cookie,
	int dumpSiblings,
	const char *key_prev,
	struct trie_key_value_s **key_value_vector
	) {
	char *key = NULL;
	int keyDumped = 0;
	if( what & TRIE_DUMP_KEYS ) {
		key = (char *) malloc( sizeof( char ) * ( node->depth + 1 ) );
		strncpy( key, key_prev, node->depth ); // copy previous key
		if( node->depth ) {
			key[node->depth - 1] = node->letter; // append/replace letter
		}
		key[node->depth] = '\0';        // terminate key string
	}
	if( node->data_is_set && predicate( node->data, cookie ) ) {
		// dump key and values if requested
		if( what & TRIE_DUMP_KEYS ) {
			keyDumped = 1;
			( *key_value_vector )->key = key;
		} else {
			( *key_value_vector )->key = NULL;
		}
		( *key_value_vector )->value = ( what & TRIE_DUMP_VALUES )
									   ? node->data
									   : NULL;
		// increment key_vector
		++( *key_value_vector );
	}
	// dump children
	if( node->child ) {
		Trie_Dump_Rec( node->child, what, casing, predicate, cookie, 1, key, key_value_vector );
	}
	// dump siblings
	if( dumpSiblings && node->sibling ) {
		Trie_Dump_Rec( node->sibling, what, casing, predicate, cookie, 1, key, key_value_vector );
	}
	if( ( what & TRIE_DUMP_KEYS ) && !keyDumped ) {
		assert( key );
		free( key );
	}
}

static int Trie_AlwaysTrue(
	void *value,
	void *cookie
	) {
	return 1;
}

static inline int Trie_LetterCompare(
	char left,
	char right,
	trie_casing_t casing
	) {
	if( casing == TRIE_CASE_SENSITIVE ) {
		return ( (int) left ) - ( (int) right );
	} else {
		return ( (int) tolower( left ) ) - ( (int) tolower( right ) );
	}
}
