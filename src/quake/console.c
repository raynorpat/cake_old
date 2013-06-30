/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// console.c

#include "quakedef.h"
#include "keys.h"

#define		CON_TEXTSIZE	65536
typedef struct
{
	char	text[CON_TEXTSIZE];
	int		current;		// line where next message will be printed
	int		display;		// bottom of console displays this line
	int		numlines;		// number of non-blank text lines, used for backscroling
	int 	backscroll;		// lines up from bottom to display
} console_t;

static console_t	con;

int			con_x;			// offset in current line for next print
int			con_ormask;
int 		con_linewidth;	// characters across screen
int			con_totallines;		// total lines in console scrollback
float		con_cursorspeed = 4;

cvar_t		_con_notifylines = {"con_notifylines","4"};
cvar_t		con_notifytime = {"con_notifytime","3"};		//seconds

#define	NUM_CON_TIMES 16
float		con_times[NUM_CON_TIMES];	// cls.realtime time the line was generated
								// for transparent notify lines

int			con_vislines;
int			con_notifylines;		// scan lines to clear for notify lines

#define		MAXCMDLINE	256
extern	char	key_lines[32][MAXCMDLINE];
extern	int		edit_line;
extern	int		key_linepos;


qbool		con_initialized = false;


void Key_ClearTyping (void)
{
	key_lines[edit_line][1] = 0;	// clear any typing
	key_linepos = 1;
}

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void)
{
	Key_ClearTyping ();

	if (key_dest == key_console)
	{
		if (cls.state == ca_active || cl.intermission)
		{
			key_dest = key_game;
			con.backscroll = 0;
		}
	}
	else
		key_dest = key_console;

	SCR_EndLoadingPlaque ();
	Con_ClearNotify ();
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	con.numlines = 0;
	memset (con.text, ' ', CON_TEXTSIZE);
	con.display = con.current;
	con.backscroll = 0;
}


/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	int		i;

	for (i=0 ; i<NUM_CON_TIMES ; i++)
		con_times[i] = 0;
}


/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f (void)
{
	if (cls.state != ca_active)
		return;

	chat_team = false;
	key_dest = key_message;
	chat_buffer[0] = 0;
	chat_linepos = 0;
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	if (cls.state != ca_active)
		return;

	chat_team = true;
	key_dest = key_message;
	chat_buffer[0] = 0;
	chat_linepos = 0;
}

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char	tbuf[CON_TEXTSIZE];

	width = ((int)vid_conwidth.value >> 3) - 2;

	if (width == con_linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = 78;
		con_linewidth = width;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		memset (con.text, ' ', CON_TEXTSIZE);
	}
	else
	{
		oldwidth = con_linewidth;
		con_linewidth = width;
		oldtotallines = con_totallines;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		numlines = oldtotallines;

		if (con_totallines < numlines)
			numlines = con_totallines;

		numchars = oldwidth;

		if (con_linewidth < numchars)
			numchars = con_linewidth;

		memcpy (tbuf, con.text, CON_TEXTSIZE);
		memset (con.text, ' ', CON_TEXTSIZE);

		for (i=0 ; i<numlines ; i++)
		{
			for (j=0 ; j<numchars ; j++)
			{
				con.text[(con_totallines - 1 - i) * con_linewidth + j] =
						tbuf[((con.current - i + oldtotallines) %
							  oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con.current = con_totallines - 1;
	con.display = con.current;
	con.backscroll = 0;
}


/*
================
Con_ConDump_f
================
*/
void Con_ConDump_f (void)
{
	char	buffer[1024];
	qfile_t	*f;
	int		i, x, linewidth;
	char	*line;

	if (Cmd_Argc() < 2) {
		Com_Printf ("condump <filename> : dump console text to file\n");
		return;
	}

	if (strstr(Cmd_Argv(1), ".."))
		return;

	f = FS_Open (Cmd_Argv(1), "wb", true, false);
	if (!f)
		return;

	linewidth = min (con_linewidth, sizeof(buffer)-1);
	buffer[linewidth] = 0;

	for (i = con.numlines - 1; i >= 0 ; i--)
	{
		line = con.text + ((con.current - i + con_totallines) % con_totallines)*con_linewidth;
		strncpy (buffer, line, linewidth);
		for (x = linewidth-1; x >= 0; x--)
		{
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}
		for (x=0; buffer[x]; x++)
			if ((unsigned char)buffer[x] >= 128 + 32)
				buffer[x] &= 0x7f;	// strip high bit off ASCII chars

		FS_Printf (f, "%s\n", buffer);
	}

	FS_Close (f);
	Com_Printf ("Dumped console text to %s.\n", Cmd_Argv(1));
}


/*
================
Con_Init
================
*/
void Con_Init (void)
{
	if (dedicated)
		return;

	con_linewidth = -1;
	Con_CheckResize ();

	Com_Printf ("Console initialized.\n");

//
// register our commands
//
	Cvar_Register (&_con_notifylines);
	Cvar_Register (&con_notifytime);

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("condump", Con_ConDump_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
	con_initialized = true;
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (void)
{
	if (con.backscroll)
		con.backscroll++;
	if (con.backscroll > con_totallines - (vid.height>>3) - 1)
		con.backscroll = con_totallines - (vid.height>>3) - 1;

	con_x = 0;
	con.current++;
	memset (&con.text[(con.current%con_totallines)*con_linewidth], ' ', con_linewidth);
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
================
*/
void Con_Print (char *txt)
{
	int		y;
	int		c, l;
	static int	cr;
	int		mask;

	if (!con_initialized)
		return;


	if (txt[0] == 1 || txt[0] == 2)
	{
		mask = 128;		// go to colored text
		txt++;
	}
	else
		mask = 0;


	while ( (c = *txt) != 0 )
	{
	// count word length
		for (l=0 ; l< con_linewidth ; l++)
			if ( txt[l] <= ' ')
				break;

	// word wrap
		if (l != con_linewidth && (con_x + l > con_linewidth) )
			con_x = 0;

		txt++;

		if (cr)
		{
			con.current--;
			cr = false;
		}


		if (!con_x)
		{
			Con_Linefeed ();
		// mark time for transparent overlay
			if (con.current >= 0)
				con_times[con.current % NUM_CON_TIMES] = cls.realtime;
		}

		switch (c)
		{
		case '\n':
			con_x = 0;
			break;

		case '\r':
			con_x = 0;
			cr = 1;
			break;

		default:	// display character and advance
			y = con.current % con_totallines;
			con.text[y*con_linewidth+con_x] = c | mask | con_ormask;
			con_x++;
			if (con_x >= con_linewidth)
				con_x = 0;
			break;
		}

	}
}

// scroll the (visible area of the) console up or down
void Con_Scroll (int count)
{
	con.backscroll += count;
	if (con.backscroll > con_totallines - (vid.height>>3) - 1)
		con.backscroll = con_totallines - (vid.height>>3) - 1;
	if (con.backscroll < 0)
		con.backscroll = 0;
}

void Con_ScrollToTop (void)
{
	int i, x;
	char *line;

	for (i = con.current - con_totallines + 1; i <= con.current; i++)
	{
		line = con.text + (i % con_totallines) * con_linewidth;
		for (x = 0; x < con_linewidth; x++)
		{
			// skip initial empty lines
			if (line[x] != ' ')
				break;
		}
		if (x != con_linewidth)
			break;
	}
	con.backscroll = clamp(0, con.current-i%con_totallines-2, con_totallines-(vid.height>>3)-1);
}


void Con_ScrollToBottom (void)
{
	con.backscroll = 0;
}


/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
void Con_DrawInput (void)
{
	int		len;
	char	*text;
	char	temp[MAXCMDLINE + 1];       //+ 1 for cursor if stlen(key_lines[edit_line]) == 255

	if (key_dest != key_console && cls.state == ca_active)
		return;

	len = strlcpy (temp, key_lines[edit_line], MAXCMDLINE);
	text = temp;

	memset(text + len, ' ', MAXCMDLINE - len);              // fill out remainder with spaces
	text[MAXCMDLINE] = 0;

	// add the cursor frame
	if ( (int)(curtime*con_cursorspeed) & 1 )
		text[key_linepos] = 11;

	//	prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;

	// draw input string
	R_DrawString (8, vid_conheight.value - 16, text);
}


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	int		x, v;
	char	*text;
	int		i;
	float	time;
	char	*s;
	int		skip;
	int		maxlines;

	maxlines = _con_notifylines.value;
	if (maxlines > NUM_CON_TIMES)
		maxlines = NUM_CON_TIMES;
	if (maxlines < 0)
		maxlines = 0;

	v = vid_conheight.value;

	for (i = con.current-maxlines+1 ; i<=con.current ; i++)
	{
		if (i < 0)
			continue;
		time = con_times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = cls.realtime - time;
		if (time > con_notifytime.value)
			continue;
		text = con.text + (i % con_totallines)*con_linewidth;

		clearnotify = 0;
		scr_copytop = 1;

		for (x = 0 ; x < con_linewidth ; x++)
			R_DrawChar ((x+1)<<3, v, text[x]);

		v += 8;
	}

	if (key_dest == key_message)
	{
		char	temp[MAXCMDLINE+1];

		clearnotify = 0;
		scr_copytop = 1;

		if (chat_team) {
			R_DrawString (8, v, "say_team:");
			skip = 11;
		} else {
			R_DrawString (8, v, "say:");
			skip = 5;
		}

		// FIXME: clean this up

		s = strcpy (temp, chat_buffer);

		// add the cursor frame
		if ( (int)(curtime*con_cursorspeed) & 1 ) {
			if (chat_linepos == strlen(s))
				s[chat_linepos+1] = '\0';
			s[chat_linepos] = 11;
		}

		// prestep if horizontally scrolling
		if (chat_linepos + skip >= (vid.width>>3))
			s += 1 + chat_linepos + skip - (vid.width>>3);

		x = 0;
		while (s[x] && x+skip < (vid.width>>3))
		{
			R_DrawChar ((x+skip)<<3, v, s[x]);
			x++;
		}
		v += 8;
	}

	if (v > con_notifylines)
		con_notifylines = v;
}

/*
================
Con_DrawConsole

Draws console text and download bar if needed
================
*/
void Con_DrawConsole (int lines)
{
	int				i, j, x, y, sb, n, rows;
	char			*text;
	char			dlbar[1024];

	if (lines <= 0)
		return;

	con_vislines = lines;

// draw the buffer text
	rows = (con_vislines +7)/8;
	y = vid_conheight.value - rows*8;
	rows -= 2; // for input and version lines
	sb = (con.backscroll) ? 2 : 0;

	for (i = con.current - rows + 1; i <= con.current - sb; i++, y += 8)
	{
		j = i - con.backscroll;
		if (j < 0)
			j = 0;
		text = con.text + (j % con_totallines)*con_linewidth;

		for (x=0 ; x<con_linewidth ; x++)
			R_DrawChar ((x+1)<<3, y, text[x]);
	}

// draw scrollback arrows
	if (con.backscroll)
	{
		y += 8; // blank line
		for (x=0 ; x<con_linewidth ; x += 4)
			R_DrawChar ((x+1)<<3, y, '^');
		y += 8;
	}

	// draw the download bar
	// figure out width
	if (cls.download) {
		if ((text = strrchr(cls.downloadname, '/')) != NULL)
			text++;
		else
			text = cls.downloadname;

		x = con_linewidth - ((con_linewidth * 7) / 40);
		y = x - strlen(text) - 8;
		i = con_linewidth/3;
		if (strlen(text) > i) {
			y = x - i - 11;
			strlcpy (dlbar, text, i+1);
			strcat(dlbar, "...");
		} else
			strcpy(dlbar, text);
		strcat(dlbar, ": ");
		i = strlen(dlbar);
		dlbar[i++] = '\x80';
		// where's the dot go?
		if (cls.downloadpercent == 0)
			n = 0;
		else
			n = y * cls.downloadpercent / 100;

		for (j = 0; j < y; j++)
			if (j == n)
				dlbar[i++] = '\x83';
			else
				dlbar[i++] = '\x81';
		dlbar[i++] = '\x82';
		dlbar[i] = 0;

		sprintf(dlbar + strlen(dlbar), " %02d%%", cls.downloadpercent);

		// draw it
		y = con_vislines-22 + 8;
		R_DrawString (8, y, dlbar);
	}

// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();
}
