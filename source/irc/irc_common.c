#include "irc_common.h"

irc_import_t IRC_IMPORT;
char IRC_ERROR_MSG[256] = { '\0' };

// global variable, const so noone can modify our list
const irc_chat_history_node_t *irc_chat_history = NULL;

// private, modifyable history
struct {
	size_t size;
	size_t total_size;
	irc_chat_history_node_t *first;
	irc_chat_history_node_t *last;
} static irc_chat_history_list = { 0, 0, NULL, NULL };

static cvar_t *irc_console = NULL;

#ifndef IRC_HARD_LINKED
// this is only here so the functions in q_shared.c and q_math.c can link
void Sys_Error( const char *format, ...) {}
void Com_Printf( const char *format, ... ) {}
#endif

void Irc_Printf( const char *format, ... )
{
	va_list		argptr;
	char		msg[1024];
	int			len;

	va_start( argptr, format );
	len = vsnprintf( msg, sizeof(msg), format, argptr );
	msg[sizeof(msg)-1] = 0;
	va_end( argptr );

	IRC_IMPORT.Printf( msg );
}

void Irc_Println_Str(const char *line) {

	// add to history
	irc_chat_history_node_t * const n = (irc_chat_history_node_t*) Irc_MemAlloc(sizeof(irc_chat_history_node_t));
	const size_t line_len = strlen(line);
	char * const line_cpy = (char*) Irc_MemAlloc((int) line_len + 1);
	n->line = memcpy(line_cpy, line, line_len + 1);
	n->line[line_len] = '\0';
	n->next = irc_chat_history_list.first;
	n->prev = NULL;
	if (irc_chat_history_list.first)
		irc_chat_history_list.first->prev = n;
	irc_chat_history_list.first = n;
	irc_chat_history = irc_chat_history_list.first;
	if (irc_chat_history_list.size == 0) {
		irc_chat_history_list.last = n;
	} else if (irc_chat_history_list.size == IRC_CHAT_HISTORY_SIZE) {
		irc_chat_history_node_t * const m = irc_chat_history_list.last;
		irc_chat_history_list.last = m->prev;
		irc_chat_history_list.last->next = NULL;
		Irc_MemFree(m->line);
		Irc_MemFree(m);
		--irc_chat_history_list.size;
	}
	++irc_chat_history_list.size;
	++irc_chat_history_list.total_size;

	// print to console
	if (!irc_console)
		irc_console = IRC_IMPORT.Cvar_Get("irc_console", "0", CVAR_ARCHIVE);
	assert(irc_console);
	if (Cvar_GetIntegerValue(irc_console))
		Irc_Printf("IRC | %s\n", line);

}

void Irc_ColorFilter(const char *pre, irc_color_filter_t filter, char *post) {

	static cvar_t *irc_colors;
	const char *in = pre;
	char *out = post;
	qboolean colorflag = qfalse;

	assert(in);
	assert(out);

	if (!irc_colors)
		irc_colors = IRC_IMPORT.Cvar_Get("irc_colors", "1", CVAR_ARCHIVE);

	switch (filter) {
		case IRC_COLOR_NONE:
			strcpy(out, in);
			break;
		case IRC_COLOR_WSW_TO_IRC:
			for (; *in; ++in) {
				if( colorflag ) {
					if( *in == Q_COLOR_ESCAPE ) {
						// second Q_COLOR_ESCAPE in a row, print it
						*out++ = *in;
					} else {
						// replace Warsow color code with IRC color code
						switch (*in) {
							case COLOR_BLACK:
								out += sprintf(out, IRC_COLOR_BLACK);
								break;
							case COLOR_RED:
								out += sprintf(out, IRC_COLOR_RED);
								break;
							case COLOR_GREEN:
								out += sprintf(out, IRC_COLOR_GREEN);
								break;
							case COLOR_YELLOW:
								out += sprintf(out, IRC_COLOR_YELLOW);
								break;
							case COLOR_BLUE:
								out += sprintf(out, IRC_COLOR_BLUE);
								break;
							case COLOR_CYAN:
								out += sprintf(out, IRC_COLOR_CYAN);
								break;
							case COLOR_MAGENTA:
								out += sprintf(out, IRC_COLOR_MAGENTA);
								break;
							case COLOR_WHITE:
								out += sprintf(out, IRC_COLOR_WHITE);
								break;
							case COLOR_ORANGE:
								out += sprintf(out, IRC_COLOR_ORANGE);
								break;
							case COLOR_GREY:
								out += sprintf(out, IRC_COLOR_GREY);
								break;
							default:
								// not a valid color code
								break;
						}
					}
					colorflag = qfalse;
				} else if(*in == Q_COLOR_ESCAPE) {
					colorflag = qtrue;
				} else if (isprint(*in)) {
					// printable character, copy as-is
					*out++ = *in;
				}
			}
			*out = '\0';
			break;
		case IRC_COLOR_IRC_TO_WSW:
			for (; *in; ++in) {
				if (*in == IRC_COLOR_ESCAPE) {
					// IRC color code found, replace with Warsow color code (best fit)
					++in;
					if (Cvar_GetIntegerValue(irc_colors)) {
						int c1, c2;

						c1 = *in;
						c2 = -1;
						if (isdigit(*(in+1))) {
							// 2 digit color code
							c2 = *(in+1);
							in++;
						}

						// \0n format
						if (c1 == '0' && c2 != -1) {
							c1 = c2;
						}

						*out++ = Q_COLOR_ESCAPE;
						switch (c1) {
							case '0':
								*out++ = COLOR_WHITE;
								break;
							case '1':
								if (c2 != -1) {
									switch (c2) {
										case '0':
											*out++ = COLOR_CYAN;
											break;
										case '1':
											*out++ = COLOR_CYAN;
											break;
										case '2':
											*out++ = COLOR_BLUE;
											break;
										case '3':
											*out++ = COLOR_MAGENTA;
											break;
										case '4':
											*out++ = COLOR_GREY;
											break;
										case '5':
											*out++ = COLOR_GREY;
											break;
										default:
											// invalid color code
											--out;	// remove Q_COLOR_ESCAPE
											break;
									}
								} else
									*out++ = COLOR_BLACK;
								break;
							case '2':
								*out++ = COLOR_BLUE;
								break;
							case '3':
								*out++ = COLOR_GREEN;
								break;
							case '4':
								*out++ = COLOR_RED;
								break;
							case '5':
								*out++ = COLOR_RED;
								break;
							case '6':
								*out++ = COLOR_MAGENTA;
								break;
							case '7':
								*out++ = COLOR_ORANGE;
								break;
							case '8':
								*out++ = COLOR_YELLOW;
								break;
							case '9':
								*out++ = COLOR_GREEN;
								break;
							default:
								// invalid color code
								--out;	// remove Q_COLOR_ESCAPE
								break;
						}
					} else if (isdigit(*(in+1))) {
						// skip 2-digit color code
						++in;
					}
					if (*(in+1) == ',' && isdigit(*(in+2))) {
						// background color code found, skip
						in += isdigit(*(in+3))
							? 3		// 2 digit color code
							: 2;	// 1 digit color code
					}
				} else if (*in == Q_COLOR_ESCAPE) {
					// Warsow color code found, escape it
					*out++ = Q_COLOR_ESCAPE;
					*out++ = *in;
				} else if (isprint(*in)) {
					// printable character, copy as-is
					*out++ = *in;
				}
			}
			*out = '\0';
			break;
	}
}

void Irc_ClearHistory(void) {
	irc_chat_history_node_t *n = irc_chat_history_list.first;
	irc_chat_history_node_t *prev = NULL;
	while (n) {
		prev = n;
		n = n->next;
		Irc_MemFree(prev);
	}
	irc_chat_history_list.first = NULL;
	irc_chat_history_list.last = NULL;
	irc_chat_history_list.size = 0;
	irc_chat_history_list.total_size = 0;
	irc_chat_history = NULL;
}

size_t Irc_HistorySize(void) {
	return irc_chat_history_list.size;
}

size_t Irc_HistoryTotalSize(void) {
	return irc_chat_history_list.total_size;
}

void Irc_ParseName(const char *mask, char *nick, irc_nick_prefix_t *prefix) {
	const char *emph;
	if (*mask == IRC_NICK_PREFIX_OP || *mask == IRC_NICK_PREFIX_VOICE) {
		*prefix = (irc_nick_prefix_t) *mask;	// read prefix
		++mask;									// crop prefix from mask
	} else
		*prefix = IRC_NICK_PREFIX_NONE;
	emph = strchr(mask, '!');
	if (emph) {
		// complete hostmask, crop anything after !
		memcpy(nick, mask, emph - mask);
		nick[emph - mask] = '\0';
	} else
		// just the nickname, use as is
		strcpy(nick, mask);
}

irc_nick_prefix_t *Irc_GetStaticPrefix(irc_nick_prefix_t transient_prefix) {
	static irc_nick_prefix_t none = IRC_NICK_PREFIX_NONE;
	static irc_nick_prefix_t op = IRC_NICK_PREFIX_OP;
	static irc_nick_prefix_t voice = IRC_NICK_PREFIX_VOICE;
	switch (transient_prefix) {
		case IRC_NICK_PREFIX_NONE:
			return &none;
		case IRC_NICK_PREFIX_OP:
			return &op;
		case IRC_NICK_PREFIX_VOICE:
			return &voice;
		default:
			assert(0);
			return 0;
	}
}
