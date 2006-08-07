/**
 * @file console.c
 * @brief Console related code.
 */

/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

15/06/06, Eddy Cullen (ScreamingWithNoSound):
	Reformatted to agreed style.
	Added doxygen file comment.
	Updated copyright notice.

Original file from Quake 2 v3.21: quake2-2.31/client/console.c
Copyright (C) 1997-2001 Id Software, Inc.

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

console_t con;

static cvar_t *con_notifytime;

extern char key_lines[32][MAXCMDLINE];
extern int edit_line;
extern int key_linepos;

/**
 * @brief
 */
static void DisplayString(int x, int y, char *s)
{
	while (*s) {
		re.DrawChar(x, y, *s);
		x += 8;
		s++;
	}
}

/**
 * @brief
 */
static void Key_ClearTyping(void)
{
	key_lines[edit_line][1] = 0;	/* clear any typing */
	key_linepos = 1;
}

/**
 * @brief
 */
void Con_ToggleConsole_f(void)
{
	if (cl.attractloop) {
		Cbuf_AddText("killserver\n");
		return;
	}
	Key_ClearTyping();
	Con_ClearNotify();

	if (cls.key_dest == key_console) {
		cls.key_dest = key_game;
		Cvar_Set("paused", "0");
	} else {
		cls.key_dest = key_console;

		if (Cvar_VariableValue("maxclients") == 1 && Com_ServerState())
			Cvar_Set("paused", "1");
	}
}

/**
 * @brief
 */
void Con_ToggleChat_f(void)
{
	Key_ClearTyping();

	if (cls.key_dest == key_console) {
		if (cls.state == ca_active)
			cls.key_dest = key_game;
	} else
		cls.key_dest = key_console;

	Con_ClearNotify();
}

/**
 * @brief Clears the console buffer
 */
static void Con_Clear_f(void)
{
	memset(con.text, ' ', CON_TEXTSIZE);
}


/**
 * @brief Save the console contents out to a file
 */
static void Con_Dump_f(void)
{
	int l, x;
	char *line;
	FILE *f;
	char buffer[MAX_STRING_CHARS];
	char name[MAX_OSPATH];

	if (Cmd_Argc() != 2) {
		Com_Printf("usage: condump <filename>\n");
		return;
	}

	Com_sprintf(name, sizeof(name), "%s/%s.txt", FS_Gamedir(), Cmd_Argv(1));

	Com_Printf("Dumped console text to %s.\n", name);
	FS_CreatePath(name);
	f = fopen(name, "w");
	if (!f) {
		Com_Printf("ERROR: couldn't open.\n");
		return;
	}

	/* skip empty lines */
	for (l = con.current - con.totallines + 1; l <= con.current; l++) {
		line = con.text + (l % con.totallines) * con.linewidth;
		for (x = 0; x < con.linewidth; x++)
			if (line[x] != ' ')
				break;
		if (x != con.linewidth)
			break;
	}

	/* write the remaining lines */
	buffer[con.linewidth] = 0;
	for (; l <= con.current; l++) {
		line = con.text + (l % con.totallines) * con.linewidth;
		strncpy(buffer, line, con.linewidth);
		for (x = con.linewidth - 1; x >= 0; x--) {
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}
		for (x = 0; buffer[x]; x++)
			buffer[x] &= SCHAR_MAX;

		fprintf(f, "%s\n", buffer);
	}

	fclose(f);
}


/**
 * @brief
 */
void Con_ClearNotify(void)
{
	int i;

	for (i = 0; i < NUM_CON_TIMES; i++)
		con.times[i] = 0;
}


/**
 * @brief
 */
void Con_MessageModeSay_f(void)
{
	msg_mode = MSG_SAY;
	cls.key_dest = key_message;
}

/**
 * @brief
 */
static void Con_MessageModeSayTeam_f(void)
{
	msg_mode = MSG_SAY_TEAM;
	cls.key_dest = key_message;
}

/**
 * @brief
 */
static void Con_MessageModeMenu_f(void)
{
	msg_mode = MSG_MENU;
	cls.key_dest = key_message;
}

/**
 * @brief If the line width has changed, reformat the buffer.
 */
void Con_CheckResize(void)
{
	int i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char tbuf[CON_TEXTSIZE];

	width = (viddef.width >> 3) - 2;

	if (width == con.linewidth)
		return;

	if (width < 1) {	/* video hasn't been initialized yet */
		width = 80;
		con.linewidth = width;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		memset(con.text, ' ', CON_TEXTSIZE);
	} else {
		oldwidth = con.linewidth;
		con.linewidth = width;
		oldtotallines = con.totallines;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		numlines = oldtotallines;

		if (con.totallines < numlines)
			numlines = con.totallines;

		numchars = oldwidth;

		if (con.linewidth < numchars)
			numchars = con.linewidth;

		memcpy(tbuf, con.text, CON_TEXTSIZE);
		memset(con.text, ' ', CON_TEXTSIZE);

		for (i = 0; i < numlines; i++) {
			for (j = 0; j < numchars; j++) {
				con.text[(con.totallines - 1 - i) * con.linewidth + j] = tbuf[((con.current - i + oldtotallines) % oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify();
	}

	con.current = con.totallines - 1;
	con.display = con.current;
}


/**
 * @brief
 */
void Con_Init(void)
{
	con.linewidth = -1;

	Con_CheckResize();

	Com_Printf("Console initialized.\n");

	/* register our commands */
	con_notifytime = Cvar_Get("con_notifytime", "3", 0);

	Cmd_AddCommand("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand("togglechat", Con_ToggleChat_f);
	Cmd_AddCommand("messagesay", Con_MessageModeSay_f);
	Cmd_AddCommand("messagesayteam", Con_MessageModeSayTeam_f);
	Cmd_AddCommand("messagemenu", Con_MessageModeMenu_f);
	Cmd_AddCommand("clear", Con_Clear_f);
	Cmd_AddCommand("condump", Con_Dump_f);
	con.initialized = qtrue;
}


/**
 * @brief
 */
static void Con_Linefeed(void)
{
	con.x = 0;
	if (con.display == con.current)
		con.display++;
	con.current++;
	memset(&con.text[(con.current % con.totallines) * con.linewidth],' ', con.linewidth);
}

/**
 * @brief Handles cursor positioning, line wrapping, etc
 * All console printing must go through this in order to be logged to disk
 * If no console is visible, the text will appear at the top of the game window
 * @sa Sys_ConsoleOutput
 */
void Con_Print(char *txt)
{
	int y;
	int c, l;
	static int cr;
	int mask;

	if (!con.initialized)
		return;

	if (txt[0] == 1 || txt[0] == 2) {
		mask = 128; /* go to colored text */
		txt++;
	} else
		mask = 0;


	while ( ( c = *txt ) != 0 ) {
		/* count word length */
		for (l = 0; l < con.linewidth; l++)
			if (txt[l] <= ' ')
				break;

		/* word wrap */
		if (l != con.linewidth && (con.x + l > con.linewidth))
			con.x = 0;

		txt++;

		if (cr) {
			con.current--;
			cr = qfalse;
		}

		if (!con.x) {
			Con_Linefeed();
			/* mark time for transparent overlay */
			if (con.current >= 0)
				con.times[con.current % NUM_CON_TIMES] = cls.realtime;
		}

		switch (c) {
		case '\n':
			con.x = 0;
			break;

		case '\r':
			con.x = 0;
			cr = 1;
			break;

		default:	/* display character and advance */
#if 0
			if (!isprint(c))
				continue;
#endif
			y = con.current % con.totallines;
			con.text[y * con.linewidth + con.x] = c | mask | con.ormask;
			con.x++;
			if (con.x >= con.linewidth)
				con.x = 0;
			break;
		}
	}
}


/**
 * @brief Centers the text to print on console
 * @param[in] text
 * @sa Con_Print
 */
void Con_CenteredPrint(char *text)
{
	int l;
	char buffer[MAX_STRING_CHARS];

	l = strlen(text);
	l = (con.linewidth - l) / 2;
	if (l < 0)
		l = 0;
	memset(buffer, ' ', l);
	Q_strcat(buffer, text, sizeof(buffer));
	Q_strcat(buffer, "\n", sizeof(buffer));
	Con_Print(buffer);
}

/*
==============================================================================

DRAWING

==============================================================================
*/


/**
 * @brief The input line scrolls horizontally if typing goes beyond the right edge
 */
static void Con_DrawInput(void)
{
	int y;
	int i;
	char *text;

	if (cls.key_dest != key_console && cls.state == ca_active)
		return;					/* don't draw anything (always draw if not active) */

	text = key_lines[edit_line];

	/* add the cursor frame */
	text[key_linepos] = 10 + ((int) (cls.realtime >> 8) & 1);

	/* fill out remainder with spaces */
	for (i = key_linepos + 1; i < con.linewidth; i++)
		text[i] = ' ';

	/* prestep if horizontally scrolling */
	if (key_linepos >= con.linewidth)
		text += 1 + key_linepos - con.linewidth;

	/* draw it */
	y = con.vislines - 8;

	for (i = 0; i < con.linewidth; i++)
		re.DrawChar((i + 1) << 3, con.vislines - 22, text[i]);

	/* remove cursor */
	key_lines[edit_line][key_linepos] = 0;
}


/**
 * @brief Draws the last few lines of output transparently over the game top
 * @sa SCR_DrawConsole
 */
void Con_DrawNotify(void)
{
	int x, l, v;
	char *text;
	int i;
	int time;
	char *s;
	int skip;

	v = 60 * viddef.rx;
	l = 120 * viddef.ry;
	for (i = con.current - NUM_CON_TIMES + 1; i <= con.current; i++) {
		if (i < 0)
			continue;
		time = con.times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = cls.realtime - time;
		if (time > con_notifytime->value * 1000)
			continue;
		text = con.text + (i % con.totallines) * con.linewidth;

		for (x = 0; x < con.linewidth; x++)
			re.DrawChar(l + (x << 3), v, text[x]);

		v += 8;
	}

	if (cls.key_dest == key_message && (msg_mode == MSG_SAY_TEAM || msg_mode == MSG_SAY)) {
		if (msg_mode == MSG_SAY) {
			DisplayString(l, v, "say:");
			skip = 4;
		} else {
			DisplayString(l, v, "say_team:");
			skip = 10;
		}

		s = msg_buffer;
		if (msg_bufferlen > (viddef.width >> 3) - (skip + 1))
			s += msg_bufferlen - ((viddef.width >> 3) - (skip + 1));
		x = 0;
		while (s[x]) {
			re.DrawChar(l + ((x + skip) << 3), v, s[x]);
			x++;
		}
		re.DrawChar(l + ((x + skip) << 3), v, 10 + ((cls.realtime >> 8) & 1));
		v += 8;
	}

	if (v) {
		SCR_AddDirtyPoint(0, 0);
		SCR_AddDirtyPoint(viddef.width - 1, v);
	}
}

/**
 * @brief Draws the console with the solid background
 * @param[in] frac
 */
void Con_DrawConsole(float frac)
{
	int i, x, y;
	int rows;
	char *text;
	int row;
	int lines;
	char version[MAX_VAR];

	lines = viddef.height * frac;
	if (lines <= 0)
		return;

	if (lines > viddef.height)
		lines = viddef.height;

	/* draw the background */
	re.DrawStretchPic(0, lines - (int) viddef.height, viddef.width, viddef.height, "conback");
	SCR_AddDirtyPoint(0, 0);
	SCR_AddDirtyPoint(viddef.width - 1, lines - 1);

	Com_sprintf(version, sizeof(version), "v%s", UFO_VERSION);
	for (x = 0; x < 5; x++)
		re.DrawChar(viddef.width - 44 + x * 8, lines - 12, 128 + version[x]);

	/* draw the text */
	con.vislines = lines;

#if 0
	rows = (lines - 8) >> 3;	/* rows of text to draw */

	y = lines - 24;
#else
	rows = (lines - 22) >> 3;	/* rows of text to draw */

	y = lines - 30;
#endif

	/* draw from the bottom up */
	if (con.display != con.current) {
		/* draw arrows to show the buffer is backscrolled */
		for (x = 0; x < con.linewidth; x += 4)
			re.DrawChar((x + 1) << 3, y, '^');

		y -= 8;
		rows--;
	}

	row = con.display;
	for (i = 0; i < rows; i++, y -= 8, row--) {
		if (row < 0)
			break;
		if (con.current - row >= con.totallines)
			break;				/* past scrollback wrap point */

		text = con.text + (row % con.totallines) * con.linewidth;

		for (x = 0; x < con.linewidth; x++)
			re.DrawChar((x + 1) << 3, y, text[x]);
	}

	/* draw the input prompt, user text, and cursor if desired */
	Con_DrawInput();
}
