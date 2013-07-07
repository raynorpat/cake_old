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

#if !defined(_WIN32) || defined(__MINGW32__)
#include <unistd.h>
#endif
#include <time.h>

#include "quakedef.h"
#include "sound.h"
#include "thread.h"
#include "keys.h"

int 		con_linewidth;

float 		con_cursorspeed = 4;

#define		CON_TEXTSIZE	131072

int 		con_totallines;			// total lines in console scrollback
int 		con_backscroll;			// lines up from bottom to display
int 		con_current;			// where next message will be printed
int 		con_x;					// offset in current line for next print
char 		con_text[CON_TEXTSIZE];

qbool		con_initialized = false;
void		*con_mutex = NULL;

// seconds
cvar_t con_notifytime = {"con_notifytime", "3", CVAR_ARCHIVE};
cvar_t con_notify = {"con_notify", "4", CVAR_ARCHIVE};

cvar_t sys_specialcharactertranslation = {"sys_specialcharactertranslation", "1"};
#ifdef _WIN32
cvar_t sys_colortranslation = {"sys_colortranslation", "0"};
#else
cvar_t sys_colortranslation = {"sys_colortranslation", "1"};
#endif

#define MAX_NOTIFYLINES 32
// cl.time time the line was generated for transparent notify lines
float 		con_times[MAX_NOTIFYLINES];

int			con_vislines;

#define		MAXCMDLINE	256
extern	char	key_lines[32][MAXCMDLINE];
extern	int		edit_line;
extern	int		key_linepos;

static void Key_ClearTyping (void)
{
	key_lines[edit_line][1] = 0;	// clear any typing
	key_linepos = 1;
}

/*
==============================================================================

LOGGING

==============================================================================
*/

cvar_t log_file = {"log_file",""};
char crt_log_file [MAX_OSPATH] = "";
qfile_t* logfile = NULL;

byte* logqueue = NULL;
size_t logq_ind = 0;
size_t logq_size = 0;

void Log_ConPrint (const char *msg);

/*
====================
Log_Timestamp
====================
*/
const char* Log_Timestamp (const char *desc)
{
	static char timestamp [128];
	time_t crt_time;
	const struct tm *crt_tm;
	char timestring [64];

	// Build the time stamp (ex: "Wed Jun 30 21:49:08 1993");
	time (&crt_time);
	crt_tm = localtime (&crt_time);
	strftime (timestring, sizeof (timestring), "%a %b %d %H:%M:%S %Y", crt_tm);

	if (desc != NULL)
		Q_snprintf (timestamp, sizeof (timestamp), "====== %s (%s) ======\n", desc, timestring);
	else
		Q_snprintf (timestamp, sizeof (timestamp), "====== %s ======\n", timestring);

	return timestamp;
}


/*
====================
Log_Open
====================
*/
void Log_Open (void)
{
	if (logfile != NULL || log_file.string[0] == '\0')
		return;

	logfile = FS_Open (log_file.string, "ab", false, false);
	if (logfile != NULL)
	{
		strlcpy (crt_log_file, log_file.string, sizeof (crt_log_file));
		FS_Print (logfile, Log_Timestamp ("Log started"));
	}
}


/*
====================
Log_Close
====================
*/
void Log_Close (void)
{
	if (logfile == NULL)
		return;

	FS_Print (logfile, Log_Timestamp ("Log stopped"));
	FS_Print (logfile, "\n");
	FS_Close (logfile);

	logfile = NULL;
	crt_log_file[0] = '\0';
}


/*
====================
Log_Start
====================
*/
void Log_Start (void)
{
	Log_Open ();

	// Dump the contents of the log queue into the log file and free it
	if (logqueue != NULL)
	{
		if (logfile != NULL && logq_ind != 0)
			FS_Write (logfile, logqueue, logq_ind);
		free (logqueue);
		logqueue = NULL;
		logq_ind = 0;
		logq_size = 0;
	}
}


/*
================
Log_ConPrint
================
*/
void Log_ConPrint (const char *msg)
{
	static qbool inprogress = false;

	// don't allow feedback loops with memory error reports
	if (inprogress)
		return;
	inprogress = true;

	// Until the host is completely initialized, we maintain a log queue
	// to store the messages, since the log can't be started before
	if (logqueue != NULL)
	{
		size_t remain = logq_size - logq_ind;
		size_t len = strlen (msg);

		// If we need to enlarge the log queue
		if (len > remain)
		{
			unsigned int factor = ((logq_ind + len) / logq_size) + 1;
			byte* newqueue;

			logq_size *= factor;
			newqueue = malloc (logq_size);
			memcpy (newqueue, logqueue, logq_ind);
			free (logqueue);
			logqueue = newqueue;
			remain = logq_size - logq_ind;
		}
		memcpy (&logqueue[logq_ind], msg, len);
		logq_ind += len;

		inprogress = false;
		return;
	}

	// Check if log_file has changed
	if (strcmp (crt_log_file, log_file.string) != 0)
	{
		Log_Close ();
		Log_Open ();
	}

	// If a log file is available
	if (logfile != NULL)
		FS_Print (logfile, msg);
	inprogress = false;
}


/*
================
Log_Printf
================
*/
void Log_Printf (const char *logfilename, const char *fmt, ...)
{
	qfile_t *file;

	file = FS_Open (logfilename, "ab", true, false);
	if (file != NULL)
	{
		va_list argptr;

		va_start (argptr, fmt);
		FS_VPrintf (file, fmt, argptr);
		va_end (argptr);

		FS_Close (file);
	}
}


/*
==============================================================================

CONSOLE

==============================================================================
*/

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void)
{
	Key_ClearTyping ();

	if (key_dest == key_console) {
		if (cls.state == ca_active || cl.intermission) {
			key_dest = key_game;
			con_backscroll = 0;
		}
	} else {
		key_dest = key_console;
	}

	memset (con_times, 0, sizeof(con_times));
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	if (con_mutex)
		Thread_LockMutex(con_mutex);

	if (con_text)
		memset (con_text, ' ', CON_TEXTSIZE);

	if (con_mutex)
		Thread_UnlockMutex(con_mutex);
}


/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	int		i;

	for (i=0 ; i<MAX_NOTIFYLINES ; i++)
		con_times[i] = 0;
}


/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f (void)
{
	key_dest = key_message;
	chat_team = false;
}


/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	key_dest = key_message;
	chat_team = true;
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

	width = (vid_conwidth.integer >> 3);

	if (width == con_linewidth)
		return;

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

	memcpy (tbuf, con_text, CON_TEXTSIZE);
	memset (con_text, ' ', CON_TEXTSIZE);

	for (i=0 ; i<numlines ; i++)
	{
		for (j=0 ; j<numchars ; j++)
		{
			con_text[(con_totallines - 1 - i) * con_linewidth + j] =
					tbuf[((con_current - i + oldtotallines) %
						  oldtotallines) * oldwidth + j];
		}
	}

	Con_ClearNotify ();

	con_backscroll = 0;
	con_current = con_totallines - 1;
}

/*
================
Con_Init
================
*/
void Con_Init (void)
{
	if (Thread_HasThreads())
		con_mutex = Thread_CreateMutex();

	memset (con_text, ' ', CON_TEXTSIZE);
	con_linewidth = 80;
	con_totallines = CON_TEXTSIZE / con_linewidth;

	// Allocate a log queue
	logq_size = MAX_INPUTLINE;
	logqueue = malloc (logq_size);
	logq_ind = 0;

	// register our cvars
	Cvar_Register (&sys_colortranslation);
	Cvar_Register (&sys_specialcharactertranslation);
	Cvar_Register (&log_file);
	Cvar_Register (&con_notifytime);
	Cvar_Register (&con_notify);

	// register our commands
	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);

	con_initialized = true;
	Con_Print("Console initialized.\n");
}


/*
================
Con_Shutdown
================
*/
void Con_Shutdown (void)
{
	if (con_mutex)
		Thread_DestroyMutex(con_mutex);
	con_mutex = NULL;
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (void)
{
	if (con_backscroll)
		con_backscroll++;

	con_x = 0;
	con_current++;
	memset (&con_text[(con_current%con_totallines)*con_linewidth], ' ', con_linewidth);
}

/*
================
Con_PrintToHistory

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be displayed
If no console is visible, the notify window will pop up.
================
*/
void Con_PrintToHistory(const char *txt)
{
	int		y, l;
	unsigned char c;

	if (con_mutex)
		Thread_LockMutex(con_mutex);

	while ((c = *((unsigned char *) txt)) != 0)
	{
		// count word length
		for (l=0 ; l< con_linewidth ; l++)
			if ( txt[l] <= ' ')
				break;

		// word wrap
		if (l != con_linewidth && (con_x + l > con_linewidth) )
			con_x = 0;

		txt++;

		if (!con_x)
		{
			Con_Linefeed ();
			// mark time for transparent overlay
			if (con_current >= 0)
			{
				if (con_notify.integer < 0)
					Cvar_SetValue(&con_notify, 0);
				if (con_notify.integer > MAX_NOTIFYLINES)
					Cvar_SetValue(&con_notify, MAX_NOTIFYLINES);
				if (con_notify.integer > 0)
					con_times[con_current % con_notify.integer] = cl.time;
			}
		}

		switch (c)
		{
		case '\n':
			con_x = 0;
			break;

		case '\r':
			con_x = 0;
			break;

		default:
			// display character and advance
			y = con_current % con_totallines;
			con_text[y*con_linewidth+con_x] = c;
			con_x++;
			if (con_x >= con_linewidth)
				con_x = 0;
			break;
		}
	}

	if (con_mutex)
		Thread_UnlockMutex(con_mutex);
}

/* The translation table between the graphical font and plain ASCII  --KB */
static char qfont_table[256] = {
	'\0', '#',  '#',  '#',  '#',  '.',  '#',  '#',
	'#',  9,    10,   '#',  ' ',  13,   '.',  '.',
	'[',  ']',  '0',  '1',  '2',  '3',  '4',  '5',
	'6',  '7',  '8',  '9',  '.',  '<',  '=',  '>',
	' ',  '!',  '"',  '#',  '$',  '%',  '&',  '\'',
	'(',  ')',  '*',  '+',  ',',  '-',  '.',  '/',
	'0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',
	'8',  '9',  ':',  ';',  '<',  '=',  '>',  '?',
	'@',  'A',  'B',  'C',  'D',  'E',  'F',  'G',
	'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',
	'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',
	'X',  'Y',  'Z',  '[',  '\\', ']',  '^',  '_',
	'`',  'a',  'b',  'c',  'd',  'e',  'f',  'g',
	'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
	'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
	'x',  'y',  'z',  '{',  '|',  '}',  '~',  '<',

	'<',  '=',  '>',  '#',  '#',  '.',  '#',  '#',
	'#',  '#',  ' ',  '#',  ' ',  '>',  '.',  '.',
	'[',  ']',  '0',  '1',  '2',  '3',  '4',  '5',
	'6',  '7',  '8',  '9',  '.',  '<',  '=',  '>',
	' ',  '!',  '"',  '#',  '$',  '%',  '&',  '\'',
	'(',  ')',  '*',  '+',  ',',  '-',  '.',  '/',
	'0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',
	'8',  '9',  ':',  ';',  '<',  '=',  '>',  '?',
	'@',  'A',  'B',  'C',  'D',  'E',  'F',  'G',
	'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',
	'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',
	'X',  'Y',  'Z',  '[',  '\\', ']',  '^',  '_',
	'`',  'a',  'b',  'c',  'd',  'e',  'f',  'g',
	'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
	'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
	'x',  'y',  'z',  '{',  '|',  '}',  '~',  '<'
};

/*
================
Con_Print

Prints to all appropriate console targets, and adds timestamps
================
*/
//extern cvar_t timestamps;
//extern cvar_t timeformat;
extern cvar_t sys_nostdout;
void Con_Print(const char *msg)
{
	static int index = 0;
	static char line[MAX_INPUTLINE];

	for (;*msg;msg++)
	{
		if (index == 0)
		{
			// if this is the beginning of a new line, print timestamp
			char *timestamp = /*timestamps.integer ? Sys_TimeString(timeformat.string) :*/ "";

			// reset the color
			// FIXME: 1. perhaps we should use a terminal system 2. use a constant instead of 7!
			line[index++] = Q_COLOR_ESCAPE;
			line[index++] = 7 + '0';

			// special color codes for chat messages must always come first
			// for Con_PrintToHistory to work properly
			if (*msg == 1 || *msg == 2)
			{
				// play talk wav
				if (*msg == 1)
					S_LocalSound ("sound/misc/talk.wav");
				line[index++] = Q_COLOR_ESCAPE;
				line[index++] = '3';
				msg++;
			}

			// store timestamp
			for (;*timestamp;index++, timestamp++)
				if (index < sizeof(line) - 2)
					line[index] = *timestamp;
		}

		// append the character
		line[index++] = *msg;

		// if this is a newline character, we have a complete line to print
		if (*msg == '\n' || index >= (int)sizeof(line) / 2)
		{
			// terminate the line
			line[index] = 0;

			// send to log file
			Log_ConPrint(line);

			// send to scrollable buffer
			if (con_initialized && !dedicated)
				Con_PrintToHistory(line);

			// send to terminal or dedicated server window
			if (!sys_nostdout.integer)
			{
				unsigned char *p;
				if(sys_specialcharactertranslation.integer)
				{
					for (p = (unsigned char *) line;*p; p++)
						*p = qfont_table[*p];
				}

				if(sys_colortranslation.integer == 1) // ANSI
				{
					static char printline[MAX_INPUTLINE * 4 + 3];
					// 2 can become 7 bytes, rounding that up to 8, and 3 bytes are added at the end
					// a newline can transform into four bytes, but then prevents the three extra bytes from appearing
					int lastcolor = 0;
					const char *in;
					char *out;
					for(in = line, out = printline; *in; ++in)
					{
						switch(*in)
						{
							case Q_COLOR_ESCAPE:
								switch(in[1])
								{
									case Q_COLOR_ESCAPE:
										++in;
										*out++ = Q_COLOR_ESCAPE;
										break;
									case '0':
									case '7':
										// normal color
										++in;
										if(lastcolor == 0) break; else lastcolor = 0;
										*out++ = 0x1B; *out++ = '['; *out++ = 'm';
										break;
									case '1':
										// light red
										++in;
										if(lastcolor == 1) break; else lastcolor = 1;
										*out++ = 0x1B; *out++ = '['; *out++ = '1'; *out++ = ';'; *out++ = '3'; *out++ = '1'; *out++ = 'm';
										break;
									case '2':
										// light green
										++in;
										if(lastcolor == 2) break; else lastcolor = 2;
										*out++ = 0x1B; *out++ = '['; *out++ = '1'; *out++ = ';'; *out++ = '3'; *out++ = '2'; *out++ = 'm';
										break;
									case '3':
										// yellow
										++in;
										if(lastcolor == 3) break; else lastcolor = 3;
										*out++ = 0x1B; *out++ = '['; *out++ = '1'; *out++ = ';'; *out++ = '3'; *out++ = '3'; *out++ = 'm';
										break;
									case '4':
										// light blue
										++in;
										if(lastcolor == 4) break; else lastcolor = 4;
										*out++ = 0x1B; *out++ = '['; *out++ = '1'; *out++ = ';'; *out++ = '3'; *out++ = '4'; *out++ = 'm';
										break;
									case '5':
										// light cyan
										++in;
										if(lastcolor == 5) break; else lastcolor = 5;
										*out++ = 0x1B; *out++ = '['; *out++ = '1'; *out++ = ';'; *out++ = '3'; *out++ = '6'; *out++ = 'm';
										break;
									case '6':
										// light magenta
										++in;
										if(lastcolor == 6) break; else lastcolor = 6;
										*out++ = 0x1B; *out++ = '['; *out++ = '1'; *out++ = ';'; *out++ = '3'; *out++ = '5'; *out++ = 'm';
										break;
									// 7 handled above
									case '8':
									case '9':
										// bold normal color
										++in;
										if(lastcolor == 8) break; else lastcolor = 8;
										*out++ = 0x1B; *out++ = '['; *out++ = '0'; *out++ = ';'; *out++ = '1'; *out++ = 'm';
										break;
									default:
										*out++ = Q_COLOR_ESCAPE;
										break;
								}
								break;
							case '\n':
								if(lastcolor != 0)
								{
									*out++ = 0x1B; *out++ = '['; *out++ = 'm';
									lastcolor = 0;
								}
								*out++ = *in;
								break;
							default:
								*out++ = *in;
								break;
						}
					}
					if(lastcolor != 0)
					{
						*out++ = 0x1B;
						*out++ = '[';
						*out++ = 'm';
					}
					*out++ = 0;
					Sys_PrintToTerminal(printline);
				}
				else if(sys_colortranslation.integer == 2) // Quake
				{
					Sys_PrintToTerminal(line);
				}
				else // strip
				{
					static char printline[MAX_INPUTLINE]; // it can only get shorter here
					const char *in;
					char *out;
					for(in = line, out = printline; *in; ++in)
					{
						switch(*in)
						{
							case Q_COLOR_ESCAPE:
								switch(in[1])
								{
									case Q_COLOR_ESCAPE:
										++in;
										*out++ = Q_COLOR_ESCAPE;
										break;
									case '0':
									case '1':
									case '2':
									case '3':
									case '4':
									case '5':
									case '6':
									case '7':
									case '8':
									case '9':
										++in;
										break;
									default:
										*out++ = Q_COLOR_ESCAPE;
										break;
								}
								break;
							default:
								*out++ = *in;
								break;
						}
					}
					*out++ = 0;
					Sys_PrintToTerminal(printline);
				}
			}

			// empty the line buffer
			index = 0;
		}
	}
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
	int		y;
	int		i;
	char	temp[MAX_INPUTLINE + 1], *text;

	if (key_dest != key_console && cls.state == ca_active)
		return;

	text = strcpy(temp, key_lines[edit_line]);
	y = strlen(text);

	// fill out remainder with spaces
	for (i = y; i < 256; i++)
		text[i] = ' ';

	// add the cursor frame
	if ( (int)(curtime*con_cursorspeed) & 1 )
		text[key_linepos] = 11;

	//	prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;

	// draw input string
	Draw_ColoredString(0, vid_conheight.value - 16, text, con_linewidth, 1.0, 1.0, 1.0, 1.0, NULL, false);
}


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	int		y;
	char	*text;
	int		i, stop;
	float	time;
	char	temptext[MAX_INPUTLINE];
	int		colorindex = -1; //-1 for default

	if (con_notify.integer < 0)
		Cvar_SetValue(&con_notify, 0);
	if (con_notify.integer > MAX_NOTIFYLINES)
		Cvar_SetValue(&con_notify, MAX_NOTIFYLINES);

	if (con_mutex)
		Thread_LockMutex(con_mutex);

	y = vid_conheight.value;

	// make a copy of con_current here so that we can't get in a runaway loop printing new messages while drawing the notify text
	stop = con_current;
	for (i= stop-con_notify.integer+1 ; i<=stop ; i++)
	{
		if (i < 0)
			continue;
		time = con_times[i % con_notify.integer];
		if (time == 0)
			continue;
		time = cl.time - time;
		if (time > con_notifytime.value)
			continue;
		text = con_text + (i % con_totallines) * con_linewidth;

		Draw_ColoredString(0, y, text, con_linewidth, 1.0, 1.0, 1.0, 1.0, &colorindex, false);
		y += 8;
	}

	if (key_dest == key_message)
	{
		int colorindex = -1;

		if (chat_team)
			sprintf(temptext, "say_team:%s%c", chat_buffer, (int) 10+((int)(curtime * con_cursorspeed)&1));
		else
			sprintf(temptext, "say:%s%c", chat_buffer, (int) 10+((int)(curtime * con_cursorspeed)&1));
		
		while (strlen(temptext) >= (size_t) con_linewidth)
		{
			Draw_ColoredString(0, y, temptext, con_linewidth, 1.0, 1.0, 1.0, 1.0, &colorindex, false);
			strcpy(temptext, &temptext[con_linewidth]);
			y += 8;
		}
		if (strlen(temptext) > 0)
		{
			Draw_ColoredString(0, y, temptext, 0, 1.0, 1.0, 1.0, 1.0, &colorindex, false);
			y += 8;
		}
	}

	if (con_mutex)
		Thread_UnlockMutex(con_mutex);
}

/*
================
Con_DrawConsole

Draws console text and download bar if needed
================
*/
extern cvar_t scr_conalpha;
void Con_DrawConsole (int lines)
{
	int				i, j, y, rows;
	char			*text;
	float			alpha;
	int				colorindex = -1;

	if (lines <= 0)
		return;

	if (con_mutex)
		Thread_LockMutex(con_mutex);

	// draw the background
	if (lines == vid_conheight.integer)
		alpha = 1.0f; // non-transparent if full screen
	else
		alpha = bound (0.0f, scr_conalpha.value, 1.0f);
	R_DrawConsoleBackground (alpha);

	// draw engine version
	Draw_ColoredString (vid_conwidth.value - strlen(engineversion) * 8, vid_conheight.value - 8, engineversion, 0, 1, 0, 0, 1, NULL, true);

	// draw the text
	con_vislines = lines;

	rows = (lines + 7) / 8;					// rows of text to draw
	y = vid_conheight.value - rows * 8;		// may start slightly negative
	rows -= 2;								// minus version and input line

	for (i = con_current - rows + 1; i <= con_current; i++, y += 8)
	{
		j = max(i - con_backscroll, 0);
		text = con_text + (j % con_totallines) * con_linewidth;

		Draw_ColoredString (0, y, text, con_linewidth, 1.0, 1.0, 1.0, 1.0, &colorindex, false);
	}

	// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();

	if (con_mutex)
		Thread_UnlockMutex(con_mutex);
}
