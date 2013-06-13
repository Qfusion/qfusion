#include "irc_common.h"
#include "irc_client.h"

#include "../client/console.h"

#define IRC_WINDOW_WIDTH (IRC_IMPORT.viddef->width * max(min(Cvar_GetFloatValue(irc_windowWidth), 1.0f), 0.0f))
#define IRC_WINDOW_HEIGHT (lines * font_height)
#define IRC_WINDOW_BORDER 2

cvar_t *irc_windowWidth = NULL;

static vec4_t IRC_WINDOW_BG_COLOR = {0.5f, 0.5f, 0.5f, 0.25f};
static vec4_t IRC_WINDOW_TXT_COLOR = {0.0f, 0.8f, 0.0f, 1.0f};
static vec4_t IRC_TXT_COLOR_WHITE = {1.0f, 1.0f, 1.0f, 1.0f};
static const char IRC_WINDOW_BG_PIC[] = "$whiteimage";

struct mufont_s;

static cvar_t *con_fontSystemSmall = NULL;

static void Irc_Client_DrawWindow(struct mufont_s *font, int x, int y, int width, int height, int lines, struct shader_s *shaderBg);
static int Irc_Client_DrawLine(int lines_avail, int off, int *x, int *y, const char *s, struct mufont_s *font, int font_height, vec4_t default_color, int last_color);
static int Irc_Client_LastColor(const char *msg, size_t msg_len);

void Irc_Client_DrawNotify(const char *target, const char *chat_buffer, size_t chat_bufferlen) {
	
	struct mufont_s *font;
	unsigned int font_height = 0;
	unsigned int charbuffer_width, target_width;
	const char *s;
	int vskip, hskip;

	if (!con_fontSystemSmall)
		con_fontSystemSmall = IRC_IMPORT.Cvar_Get("con_fontSystemSmall", "", 0);

	// get font and font size
	font = IRC_IMPORT.SCR_RegisterFont((char*) Cvar_GetStringValue(con_fontSystemSmall));
	font_height = (unsigned int) IRC_IMPORT.SCR_strHeight(font);
	target_width = (unsigned int) IRC_IMPORT.SCR_strWidth(target, font, 0);

	hskip = 8 + (int) target_width + (int) IRC_IMPORT.SCR_strWidth(": ", font, 0);
	vskip = font_height;

	// print prompt
	IRC_IMPORT.SCR_DrawString(8, vskip, ALIGN_LEFT_TOP, target, font, IRC_TXT_COLOR_WHITE);
	IRC_IMPORT.SCR_DrawRawChar(8 + (int) target_width, vskip, ':', font, IRC_TXT_COLOR_WHITE);
	// print what the user typed so far
	s = chat_buffer;
	charbuffer_width = (unsigned int) IRC_IMPORT.SCR_strWidth(s, font, (int) chat_bufferlen + 1);
	while (charbuffer_width > IRC_IMPORT.viddef->width - (hskip + 72)) {
		++s;
		charbuffer_width = (unsigned int) IRC_IMPORT.SCR_strWidth(s, font, (int) chat_bufferlen + 1);
	}
	IRC_IMPORT.SCR_DrawString(hskip, vskip, ALIGN_LEFT_TOP, s, font, IRC_TXT_COLOR_WHITE);
	hskip += (int) IRC_IMPORT.SCR_strWidth(s, font, 0);
	// print blinking '_'
	IRC_IMPORT.SCR_DrawRawChar(hskip, vskip, ((IRC_IMPORT.Milliseconds()>>8)&1)?'_':' ', font, IRC_TXT_COLOR_WHITE);

}

void Irc_Client_DrawIngameWindow() {
	
	static struct shader_s *shaderBg = NULL;
	struct mufont_s *font;
	const int lines = Cvar_GetIntegerValue(irc_windowLines);
	unsigned int font_height;

	// read cvars
	if (!con_fontSystemSmall)
		con_fontSystemSmall = IRC_IMPORT.Cvar_Get("con_fontSystemSmall", "", 0);
	if (!irc_windowWidth)
		irc_windowWidth = IRC_IMPORT.Cvar_Get("irc_windowWidth", "0.4", CVAR_ARCHIVE);
	if (!shaderBg)
		shaderBg = IRC_IMPORT.R_RegisterPic((char*) IRC_WINDOW_BG_PIC);

	// get font and font height
	font = IRC_IMPORT.SCR_RegisterFont((char*) Cvar_GetStringValue(con_fontSystemSmall));
	font_height = (unsigned int) IRC_IMPORT.SCR_strHeight(font);

	// draw the window
	Irc_Client_DrawWindow(
		font,
		8 - IRC_WINDOW_BORDER, (NUM_CON_TIMES + 1) * font_height - 2,	// upper-left corner
		(int) IRC_WINDOW_WIDTH + IRC_WINDOW_BORDER * 2,					// width
		(int) IRC_WINDOW_HEIGHT + IRC_WINDOW_BORDER * 2,				// height
		lines, shaderBg
	);

}

static void Irc_Client_DrawWindow(
	struct mufont_s *font,
	int x, int y,
	int width, int height,
	int lines,
	struct shader_s *shaderBg
) {

	unsigned int font_height = 0;
	const irc_chat_history_node_t *n = irc_chat_history;
	int vskip;
	int i = 0;

	// get font size
	font_height = (unsigned int) IRC_IMPORT.SCR_strHeight(font);
	vskip = (NUM_CON_TIMES + lines) * font_height;

	// draw background box
	IRC_IMPORT.R_DrawStretchPic(x, y, width, height, 0, 0, 1, 1, IRC_WINDOW_BG_COLOR, shaderBg);

	// print the last irc_windowLines lines in chat history
	while (n && i < lines) {
		int x = 8;
		int y = vskip - i * font_height;
		const int linesDrawn = Irc_Client_DrawLine(lines - i, 0, &x, &y, n->line, font, font_height, IRC_WINDOW_TXT_COLOR, -1);
		if (linesDrawn > 0) {
			i += linesDrawn;
			n = n->next;
		} else
			break;	// error
	}

}

static int Irc_Client_DrawLine(int lines_avail, int off, int *x, int *y, const char *s, struct mufont_s *font, int font_height, vec4_t default_color, int last_color) {

	int lines_used = 0;
	size_t s_len = strlen(s);
	const char *rest = s + s_len;
	char *buf;
	unsigned int s_width = (unsigned int) IRC_IMPORT.SCR_strWidth(s, font, (int) s_len);

	// perform binary search for optimum s_len
	int w = off + (int) s_width;
	if (w > (int) IRC_WINDOW_WIDTH) {
		int l = s_len;
		int l_delta = l >> 1;
		while (l_delta) {
			if (w > (int) IRC_WINDOW_WIDTH)
				l -= l_delta;
			else if (w < (int) IRC_WINDOW_WIDTH)
				l += l_delta;
			else
				break;
			w = off + (int) IRC_IMPORT.SCR_strWidth(s, font, l);
			l_delta >>= 1;
		}
		// as good as it gets
		l -= (w > (int) IRC_WINDOW_WIDTH);
		rest -= s_len - l;
		s_len = l;
	}

	if (s_len) {

		// at least one character has space, print

		if (last_color >= 0) {
			// prepend color sequence
			buf = Irc_MemAlloc((int) s_len + 3);
			memcpy(buf + 2, s, s_len);
			buf[0] = Q_COLOR_ESCAPE;
			buf[1] = last_color;
			s_len += 2;
		} else {
			// just duplicate s
			buf = Irc_MemAlloc((int) s_len + 1);
			memcpy(buf, s, s_len);
		}
		buf[s_len] = '\0';

		// recursively print rest
		if (*rest)
			lines_used += Irc_Client_DrawLine(lines_avail, (int) IRC_IMPORT.SCR_strWidth("  ", font, 2), x, y, rest, font, font_height, default_color, Irc_Client_LastColor(buf, s_len));

		// print line if still space left
		if (lines_used < lines_avail) {
			IRC_IMPORT.SCR_DrawString(off + *x, *y, ALIGN_LEFT_TOP, buf, font, default_color);
			++lines_used;
			*y -= font_height;
		}

		Irc_MemFree(buf);
		return lines_used;

	} else {

		// no space, return error
		return 0;

	}
	
}

static int Irc_Client_LastColor(const char *msg, size_t msg_len) {
	const char *c;
	qboolean colorflag = qfalse;
	int last_color = -1;
	for (c = msg; c < msg + msg_len; ++c) {
		if (colorflag) {
			if (isdigit(*c))
				last_color = *c;
			colorflag = qfalse;
		} else if (*c == Q_COLOR_ESCAPE)
			colorflag = qtrue;
	}
	return last_color;
}
