#include "irc_common.h"
#include "irc_listeners.h"

typedef struct irc_listener_node_s {
	irc_listener_f listener;
	struct irc_listener_node_s *next;
} irc_listener_node_t;

// numerics can only be 3 digits long, hence 999 possible commands (+1 to save offset calculations)
static irc_listener_node_t *numeric_listeners[1000];

// we keep string listeners in a trie for fast lexicographical lookup
static trie_t *string_listeners = NULL;

// called whenever no specific listeners are found
static irc_listener_node_t *generic_listeners;

// for concurrent removal
typedef struct irc_removed_listener_node_s {
	irc_command_t cmd;
	irc_listener_f listener;
	struct irc_removed_listener_node_s *next;
} irc_removed_listener_node_t;
static qboolean immutable_listeners = qfalse;
static irc_removed_listener_node_t *removed_listeners = NULL;

static void Irc_Proto_FreeListenerList(irc_listener_node_t *n) {
	if (n) {
		irc_listener_node_t *prev = NULL;
		do {
			prev = n;
			n = n->next;
			Irc_MemFree(prev);
		} while (n);
	}
}

void Irc_Proto_InitListeners(void) {
	// clear numeric_listeners array
	memset(numeric_listeners, 0, sizeof(numeric_listeners));
	// create string_listeners trie
	IRC_IMPORT.Trie_Create(TRIE_CASE_SENSITIVE, &string_listeners);
	assert(string_listeners);
}

void Irc_Proto_TeardownListeners(void) {
	trie_dump_t *dump;
	unsigned int i;
	// dump string_listeners cmd strings
	IRC_IMPORT.Trie_Dump(string_listeners, "", TRIE_DUMP_KEYS, &dump);
	// remove all listeners by string
	for (i = 0; i < dump->size; ++i) {
		irc_listener_node_t *n;
		IRC_IMPORT.Trie_Remove(string_listeners, dump->key_value_vector[i].key, (void**) &n);
		// free listener list
		Irc_Proto_FreeListenerList(n);
	}
	IRC_IMPORT.Trie_FreeDump(dump);
#if defined(DEBUG) || defined(_DEBUG)
	{
		// check whether trie is really empty
		unsigned int size;
		IRC_IMPORT.Trie_GetSize(string_listeners, &size);
		assert(!size);
	}
#endif
	// destroy trie
	IRC_IMPORT.Trie_Destroy(string_listeners);
	// free all listener lists in numeric_listeners array
	for (i = 0; i < sizeof(numeric_listeners) / sizeof(irc_listener_node_t *); ++i)
		Irc_Proto_FreeListenerList(numeric_listeners[i]);
}

void Irc_Proto_AddListener(irc_command_t cmd, irc_listener_f listener) {
	irc_listener_node_t *n = (irc_listener_node_t*) Irc_MemAlloc(sizeof(irc_listener_node_t));
	n->listener = listener;
	n->next = NULL;
	switch (cmd.type) {
		irc_listener_node_t *prev;
		case IRC_COMMAND_NUMERIC:
			// numeric command, add to numeric_listeners
			prev = numeric_listeners[cmd.numeric];
			if (prev) {
				// not the first listener of this command, append to list
				while (prev->next)
					prev = prev->next;
				prev->next = n;
			} else {
				// first listener of this command, make list head
				numeric_listeners[cmd.numeric] = n;
			}
			break;
		case IRC_COMMAND_STRING:
			// string command, add to string_listeners
			if (IRC_IMPORT.Trie_Find(string_listeners, cmd.string, TRIE_EXACT_MATCH, (void**) &prev) == TRIE_OK) {
				// not the first listener of this command, append to list
				assert(prev);
				while (prev->next)
					prev = prev->next;
				prev->next = n;
			} else {
				// first listener of this command, make list head
				IRC_IMPORT.Trie_Insert(string_listeners, cmd.string, n);
			}
			break;
	}
}

void Irc_Proto_RemoveListener(irc_command_t cmd, irc_listener_f listener) {
	if (!immutable_listeners) {
		// remove now
		irc_listener_node_t *n, *prev = NULL;
		switch (cmd.type) {
			case IRC_COMMAND_NUMERIC:
				// numeric command, remove from numeric_listeners
				for (n = numeric_listeners[cmd.numeric]; n; n = n->next) {
					if (n->listener == listener) {
						// match, remove
						if (prev)
							// not list head, cut
							prev->next = n->next;
						else
							// list head
							numeric_listeners[cmd.numeric] = n->next;
						Irc_MemFree(n);
						break;
					}
					prev = n;
				}
				break;
			case IRC_COMMAND_STRING:
				// string command, remove from string_listeners
				IRC_IMPORT.Trie_Find(string_listeners, cmd.string, TRIE_EXACT_MATCH, (void**) &n);
				for (; n; n = n->next) {
					if (n->listener == listener) {
						// match, remove
						if (prev) {
							// not list head, cut
							prev->next = n->next;
						} else {
							// list head
							if (n->next) {
								// has linked nodes, replace head with next node
								IRC_IMPORT.Trie_Replace(string_listeners, cmd.string, n->next, (void**) &prev);
							} else {
								// empty list, remove cmd.string from trie
								IRC_IMPORT.Trie_Remove(string_listeners, cmd.string, (void**) &prev);
							}
						}
						Irc_MemFree(n);
						break;
					}
					prev = n;
				}
				break;
		}
	} else {
		// prepend to removed_listeners for later removal
		irc_removed_listener_node_t * const n = (irc_removed_listener_node_t*) Irc_MemAlloc(sizeof(irc_removed_listener_node_t));
		n->cmd = cmd;
		n->listener = listener;
		n->next = removed_listeners;
		removed_listeners = n;	
	}
}

void Irc_Proto_AddGenericListener(irc_listener_f listener) {
	irc_listener_node_t *n = (irc_listener_node_t*) Irc_MemAlloc(sizeof(irc_listener_node_t));
	n->listener = listener;
	n->next = NULL;
	if (generic_listeners) {
		irc_listener_node_t *prev = generic_listeners;
		while (prev->next)
			prev = prev->next;
		prev->next = n;
	} else
		generic_listeners = n;
}

void Irc_Proto_RemoveGenericListener(irc_listener_f listener) {
	irc_listener_node_t *prev = NULL, *n = generic_listeners;
	while (n) {
		if (n->listener == listener) {
			if (prev)
				prev->next = n->next;
			else
				generic_listeners = n->next;
			Irc_MemFree(n);
			break;
		}
		prev = n;
		n = n->next;
	}
}

void Irc_Proto_CallListeners(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	irc_listener_node_t *n;
	switch (cmd.type) {
		case IRC_COMMAND_NUMERIC:
			// numeric command, search in numeric_listeners
			n = numeric_listeners[cmd.numeric];
			break;
		case IRC_COMMAND_STRING:
			// string command, search in string_listeners
			IRC_IMPORT.Trie_Find(string_listeners, cmd.string, TRIE_EXACT_MATCH, (void**) &n);
			break;
		default:
			n = NULL;
	}
	if (!n)
		// no specific listeners found, call generic listeners
		n = generic_listeners;
	// call all listeners in list
	immutable_listeners = qtrue;
	for (; n; n = n->next)
		n->listener(cmd, prefix, params, trailing);
	immutable_listeners = qfalse;
	// perform pending concurrent removals
	if (removed_listeners) {
		irc_removed_listener_node_t *prev = NULL, *n = removed_listeners;
		do {
			Irc_Proto_RemoveListener(n->cmd, n->listener);
			prev = n;
			n = n->next;
			Irc_MemFree(prev);
		} while (n);
		removed_listeners = NULL;
	}
}
