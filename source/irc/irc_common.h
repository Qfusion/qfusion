#ifndef IRC_COMMON_H
#define IRC_COMMON_H

#include "../qcommon/qcommon.h"
#include "irc_interface.h"

// undef existing Mem_* macros to avoid name clashes
#undef Mem_Alloc
#undef Mem_Free
#undef Mem_AllocPool
#undef Mem_FreePool
#undef Mem_EmptyPool

// add __FILE__ and __LINE__ to Mem_ functions in IRC_IMPORT
#define Irc_MemAlloc(size)		IRC_IMPORT.Mem_Alloc(size,__FILE__,__LINE__)
#define Irc_MemFree(data)		IRC_IMPORT.Mem_Free(data,__FILE__,__LINE__)
#define Irc_MemAllocPool(name)	IRC_IMPORT.Mem_AllocPool(name,__FILE__,__LINE__)
#define Irc_MemFreePool()		IRC_IMPORT.Mem_FreePool(__FILE__,__LINE__)
#define Irc_MemEmptyPool()		IRC_IMPORT.Mem_EmptyPool(__FILE__,__LINE__)

#define Irc_Println(format, color_filter, ...) \
	do { \
		char buf[4096]; \
		char buf2[4096]; \
		snprintf(buf, sizeof(buf), format, ##__VA_ARGS__); \
		Irc_ColorFilter(buf, color_filter, buf2); \
		Irc_Println_Str(buf2); \
	} while (0)

#define IRC_CHAT_HISTORY_SIZE 128

// history is in reverse order (newest line first)
typedef struct irc_chat_history_node_s {
	char *line;
	struct irc_chat_history_node_s *next;
	struct irc_chat_history_node_s *prev;
} irc_chat_history_node_t;

#define IRC_CTCP_MARKER_CHR '\001'
#define IRC_CTCP_MARKER_STR "\001"

#define IRC_COLOR_ESCAPE	3
#define IRC_BOLD_ESCAPE		2

#ifdef _WIN32
#	pragma warning (disable : 4125)		// decimal digit terminates octal escape sequence
#endif

#define IRC_COLOR_WHITE		"\00300"
#define IRC_COLOR_BLACK		"\00301"
#define IRC_COLOR_RED		"\00304"
#define IRC_COLOR_ORANGE	"\00307"
#define IRC_COLOR_YELLOW	"\00308"
#define IRC_COLOR_GREEN		"\00309"
#define IRC_COLOR_CYAN		"\00311"
#define IRC_COLOR_BLUE		"\00312"
#define IRC_COLOR_MAGENTA	"\00313"
#define IRC_COLOR_GREY		"\00314"

typedef enum irc_color_filter_e {
	IRC_COLOR_NONE,
	IRC_COLOR_WSW_TO_IRC,
	IRC_COLOR_IRC_TO_WSW
} irc_color_filter_t;

typedef enum irc_nick_prefix_e {
	IRC_NICK_PREFIX_NONE = ' ',
	IRC_NICK_PREFIX_OP = '@',
	IRC_NICK_PREFIX_VOICE = '+'
} irc_nick_prefix_t;

// this function will add str to the irc_chat_history and print to console if cvar "irc_console" is 1
// no color filtering is performed, use Irc_Println() or explicitly filter the line if necessary
void Irc_Println_Str(const char *line);

// reads the characters from pre, performs color-code replacement, and writes the result to post
void Irc_ColorFilter(const char *pre, irc_color_filter_t filter, char *post);

// clear all lines in irc_chat_history_node_t
void Irc_ClearHistory(void);

size_t Irc_HistorySize(void);
size_t Irc_HistoryTotalSize(void);

// parses usermask for nick and chanmode prefix
void Irc_ParseName(const char *mask, char *nick, irc_nick_prefix_t *prefix);

// returns pointer to equivalent prefix in static memory
irc_nick_prefix_t *Irc_GetStaticPrefix(irc_nick_prefix_t transient_prefix);

static inline bool Irc_IsChannel(const char *target) {
	assert(target);
	return (*target == '#' || *target == '&');
}

void Irc_Printf( const char *format, ... );

extern irc_import_t IRC_IMPORT;
extern char IRC_ERROR_MSG[256];
extern const irc_chat_history_node_t *irc_chat_history;

#endif
