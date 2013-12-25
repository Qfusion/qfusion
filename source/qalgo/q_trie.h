#ifndef WSW_TRIE_H
#define WSW_TRIE_H

/* Forward declaration of trie structures (layout hidden) */
struct trie_s;
struct trie_node_s;
typedef struct trie_s trie_t;
typedef struct trie_node_s trie_node_t;

/* Definitions of externalized types */

typedef enum trie_error_t
{
	TRIE_OK = 0,
	TRIE_DUPLICATE_KEY,
	TRIE_KEY_NOT_FOUND,
	TRIE_INVALID_ARGUMENT
} trie_error_t;

typedef enum trie_find_mode_t
{
	TRIE_PREFIX_MATCH,
	TRIE_EXACT_MATCH
} trie_find_mode_t;

typedef enum trie_dump_what_t
{
	TRIE_DUMP_KEYS = 1,
	TRIE_DUMP_VALUES = 2,
	TRIE_DUMP_BOTH = TRIE_DUMP_KEYS | TRIE_DUMP_VALUES
} trie_dump_what_t;

typedef enum trie_casing_t
{
	TRIE_CASE_SENSITIVE,
	TRIE_CASE_INSENSITIVE
} trie_casing_t;

typedef struct trie_key_value_s
{
	const char *key;
	void *value;
} trie_key_value_t;

typedef struct trie_dump_s
{
	unsigned int size;
	trie_dump_what_t what;
	struct trie_key_value_s	*key_value_vector;
} trie_dump_t;

/* Trie life-cycle functions */

trie_error_t Trie_Create(
        trie_casing_t casing,   // case sensitive or not
        struct trie_s **trie    // output parameter, the created trie
);

trie_error_t Trie_Destroy(
        struct trie_s *trie     // the trie to destroy
);

trie_error_t Trie_Clear(
        struct trie_s *trie     // the trie to clear from keys and values
);

trie_error_t Trie_GetSize(
        struct trie_s *trie,
        unsigned int *size      // output parameter, size of trie
);

/* Key/data insertion and removal */

trie_error_t Trie_Insert(
        struct trie_s *trie,
        const char *key,        // key to insert
        void *data              // data to insert
);

trie_error_t Trie_Remove(
        struct trie_s *trie,
        const char *key,        // key to match
        void **data             // output parameter, data of removed node
);

trie_error_t Trie_Replace(
        struct trie_s *trie,
        const char *key,        // key to match
        void *data_new,         // data to set
        void **data_old         // output parameter, replaced data
);

trie_error_t Trie_Find(
        const struct trie_s *trie,
        const char *key,        // key to match
        trie_find_mode_t mode,  // mode (exact or prefix only)
        void **data             // output parameter, data of node found
);

trie_error_t Trie_FindIf(
        const struct trie_s *trie,
        const char *key,        // key to match
        trie_find_mode_t mode,  // mode (exact or prefix only)
        int ( *predicate )( void *value, void *cookie ), // predicate function to be true
        void *cookie,               // the cookie passed to predicate
        void **data             // output parameter, data of node found
);

trie_error_t Trie_NoOfMatches(
        const struct trie_s *trie,
        const char *prefix,     // key prefix to match
        unsigned int *matches   // output parameter, number of matches
);

trie_error_t Trie_NoOfMatchesIf(
        const struct trie_s *trie,
        const char *prefix,     // key prefix to match
        int ( *predicate )( void *value, void *cookie ), // predicate function to be true
        void *cookie,           // the cookie passed to predicate
        unsigned int *matches   // output parameter, number of matches
);

/* Dump by prefix */

trie_error_t Trie_Dump(
        const struct trie_s *trie,
        const char *prefix,         // prefix to match
        trie_dump_what_t what,      // what to dump
        struct trie_dump_s **dump   // output parameter, deallocate with Trie_FreeDump
);

trie_error_t Trie_DumpIf(
        const struct trie_s *trie,
        const char *prefix,         // prefix to match
        trie_dump_what_t what,      // what to dump
        int ( *predicate )( void *value, void *cookie ), // predicate function to be true
        void *cookie,               // the cookie passed to predicate
        struct trie_dump_s **dump   // output parameter, deallocate with Trie_FreeDump
);

trie_error_t Trie_FreeDump(
        struct trie_dump_s *dump    // allocated by Trie_Dump or Trie_DumpIf
);

#endif
