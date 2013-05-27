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
// sys_win.c

#include <windows.h>

#ifdef SERVERONLY
#include "common.h"
#include <winsock.h>
#else
#include "quakedef.h"
#include "resource.h"
#endif
#include <limits.h>
#include <direct.h>		// _mkdir

#ifndef SERVERONLY
static HANDLE	qwclsemaphore;
#endif
static HANDLE	tevent;
static HANDLE	hinput, houtput;

#ifdef SERVERONLY
cvar_t	sys_sleep = {"sys_sleep", "8"};
cvar_t	sys_nostdout = {"sys_nostdout","0"};
#endif


/*
===============================================================================

DLL MANAGEMENT

===============================================================================
*/

dllhandle_t Sys_LoadLibrary (const char* name)
{
	return LoadLibrary (name);
}

void Sys_UnloadLibrary (dllhandle_t handle)
{
	FreeLibrary (handle);
}

void* Sys_GetProcAddress (dllhandle_t handle, const char* name)
{
	return (void *)GetProcAddress (handle, name);
}


/*
===============================================================================

FILE IO

===============================================================================
*/


void Sys_mkdir (char *path)
{
	_mkdir (path);
}


/*
===============================================================================

SYSTEM IO

===============================================================================
*/

void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

#ifndef SERVERONLY	// FIXME
	extern HWND	hwnd_dialog;

	Host_Shutdown ();
#endif

	va_start (argptr, error);
	_vsnprintf (text, sizeof(text) - 1, error, argptr);
	text[sizeof(text) - 1] = '\0';
	va_end (argptr);

#ifdef SERVERONLY
	printf ("ERROR: %s\n", text);
#else
	if (hwnd_dialog) {
		DestroyWindow (hwnd_dialog);
		hwnd_dialog = NULL;
	}
	MessageBox(NULL, text, "Error", 0 /* MB_OK */ );

	if (qwclsemaphore)
		CloseHandle (qwclsemaphore);
#endif

	exit (1);
}

void Sys_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];
	DWORD		dummy;

	if (!dedicated)
		return;

	va_start (argptr,fmt);
	_vsnprintf (text, sizeof(text) - 1, fmt, argptr);
	text[sizeof(text) - 1] = '\0';
	va_end (argptr);

	WriteFile (houtput, text, strlen(text), &dummy, NULL);
}

void Sys_Quit (void)
{
#ifndef SERVERONLY
	if (tevent)
		CloseHandle (tevent);

	if (qwclsemaphore)
		CloseHandle (qwclsemaphore);

	if (dedicated)
		FreeConsole ();
#endif

	exit (0);
}

double Sys_DoubleTime (void)
{
	static int first = true;
	static double oldtime = 0.0, curtime = 0.0;
	double newtime;

	// make sure the timer is high precision, otherwise different versions of windows have varying accuracy
	if (first)
		timeBeginPeriod (1);

	newtime = (double) timeGetTime () / 1000.0;

	if (first)
	{
		first = false;
		oldtime = newtime;
	}

	if (newtime < oldtime)
	{
		// warn if it's significant
		if (newtime - oldtime < -0.01)
			Com_Printf("Sys_DoubleTime: time stepped backwards (went from %f to %f, difference %f)\n", oldtime, newtime, newtime - oldtime);
	}
	else if (newtime > oldtime + 1800)
	{
		Com_Printf("Sys_DoubleTime: time stepped forward (went from %f to %f, difference %f)\n", oldtime, newtime, newtime - oldtime);
	}
	else
		curtime += newtime - oldtime;

	oldtime = newtime;

	return newtime;
}

// if successful, returns Q_malloc'ed string (make sure to free it afterwards)
// returns NULL if the operation failed for some reason
char *Sys_GetClipboardText (void)
{
	HANDLE	h;
	char	*text = NULL, *p;

	if (OpenClipboard(NULL)) {
		if ((h = GetClipboardData(CF_TEXT)) != NULL) {
			if ((p = GlobalLock(h)) != NULL) {
				text = Q_strdup (p);
				GlobalUnlock(h);
			}
		}
		CloseClipboard();
	}

	return text;
}

char *Sys_ConsoleInput (void)
{
	static char	text[256];
	static int		len;
	INPUT_RECORD	rec;
	int		i, ch;
	DWORD   dummy, numread, numevents;
	HANDLE	hclipdata;
	char	*clipText;

	while (1)
	{
		if (!GetNumberOfConsoleInputEvents (hinput, &numevents))
			Sys_Error ("Error getting # of console events");

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput(hinput, &rec, 1, &numread))
			Sys_Error ("Error reading console input");

		if (numread != 1)
			Sys_Error ("Couldn't read console input");

		if (rec.EventType == KEY_EVENT)
		{
			if (rec.Event.KeyEvent.bKeyDown)
			{
				ch = rec.Event.KeyEvent.uChar.AsciiChar;

				switch (ch)
				{
					case '\r':
						WriteFile(houtput, "\r\n", 2, &dummy, NULL);

						if (len)
						{
							text[len] = 0;
							len = 0;
							return text;
						}
						break;

					case '\b':
						WriteFile(houtput, "\b \b", 3, &dummy, NULL);
						if (len)
							len--;
						break;

					default:
						if ((ch == ('V'&31) /* Ctrl-V */) ||
							((rec.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED)
							&& (rec.Event.KeyEvent.wVirtualKeyCode == VK_INSERT))) {
							if (OpenClipboard(NULL)) {
								hclipdata = GetClipboardData(CF_TEXT);
								if (hclipdata) {
									clipText = GlobalLock(hclipdata);
									if (clipText) {
										for (i=0; clipText[i]; i++)
											if (clipText[i]=='\n' || clipText[i]=='\r' || clipText[i]=='\b')
												break;
										if (i + len >= sizeof(text))
											i = sizeof(text) - 1 - len;
										if (i > 0) {
											memcpy (text + len, clipText, i);
											len += i;
											WriteFile(houtput, clipText, i, &dummy, NULL);
										}
									}
									GlobalUnlock(hclipdata);
								}
								CloseClipboard();
							}
						} else if (ch >= ' ' && len < sizeof(text)-1)
						{
							WriteFile(houtput, &ch, 1, &dummy, NULL);	
							text[len++] = ch;
						}

						break;

				}
			}
		}
	}

	return NULL;
}


#ifndef SERVERONLY
void Sys_SendKeyEvents (void)
{
    MSG        msg;

	while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		if (!GetMessage (&msg, NULL, 0, 0))
			Host_Quit ();
      	TranslateMessage (&msg);
      	DispatchMessage (&msg);
	}
}
#endif


BOOL WINAPI HandlerRoutine (DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
		case CTRL_C_EVENT:
			return true;	// ignore
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			Cbuf_AddText ("quit\n");
			return true;
	}

	return false;
}


/*
================
Sys_Init

Quake calls this so the system can register variables before host_hunklevel
is marked
================
*/
void Sys_Init (void)
{
#ifdef SERVERONLY
	Cvar_Register (&sys_sleep);
	Cvar_Register (&sys_nostdout);

	if (COM_CheckParm ("-nopriority"))
	{
		Cvar_Set (&sys_sleep, "0");
	}
	else
	{
		if ( ! SetPriorityClass (GetCurrentProcess(), HIGH_PRIORITY_CLASS))
			Com_Printf ("SetPriorityClass() failed\n");
		else
			Com_Printf ("Process priority class set to HIGH\n");
	}
#endif
}


/*
==============================================================================

 WINDOWS CRAP

==============================================================================
*/

#define MAX_NUM_ARGVS	50

int		argc;
char	*argv[MAX_NUM_ARGVS];
static char	*empty_string = "";

void ParseCommandLine (char *lpCmdLine)
{
	argc = 1;
	argv[0] = empty_string;

	while (*lpCmdLine && (argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine)
		{
			argv[argc] = lpCmdLine;
			argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
				lpCmdLine++;

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
			
		}
	}
}


/*
==================
WinMain
==================
*/
#ifndef SERVERONLY
HINSTANCE	global_hInstance;
HWND		hwnd_dialog;	// startup dialog box

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	double			time, oldtime, newtime;
	RECT			rect;

	global_hInstance = hInstance;

	ParseCommandLine (lpCmdLine);

	// we need to check some parms before Host_Init is called
	COM_InitArgv (argc, argv);

#if !defined(CLIENTONLY)
	dedicated = COM_CheckParm ("-dedicated");
#endif

	if (dedicated)
	{
		if (!AllocConsole())
			Sys_Error ("Couldn't allocate dedicated server console");
		SetConsoleCtrlHandler (HandlerRoutine, TRUE);
		hinput = GetStdHandle (STD_INPUT_HANDLE);
		houtput = GetStdHandle (STD_OUTPUT_HANDLE);
	}
	else
	{
		hwnd_dialog = CreateDialog (hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, NULL);
		if (hwnd_dialog)
		{
			if (GetWindowRect (hwnd_dialog, &rect))
			{
				if (rect.left > (rect.top * 2))
				{
					SetWindowPos (hwnd_dialog, 0,
						(rect.left / 2) - ((rect.right - rect.left) / 2),
						rect.top, 0, 0,
						SWP_NOZORDER | SWP_NOSIZE);
				}
			}

			ShowWindow (hwnd_dialog, SW_SHOWDEFAULT);
			UpdateWindow (hwnd_dialog);
			SetForegroundWindow (hwnd_dialog);
		}
	}

	tevent = CreateEvent (NULL, FALSE, FALSE, NULL);
	if (!tevent)
		Sys_Error ("Couldn't create event");

	// allocate a named semaphore on the client so the front end can tell if it is alive
	if (!dedicated && !COM_CheckParm("-allowmultiple"))
	{
		// mutex will fail if semaphore already exists
		qwclsemaphore = CreateMutex(
			NULL,         /* Security attributes */
			0,            /* owner       */
			"qwcl"); /* Semaphore name      */
		if (!qwclsemaphore)
			Sys_Error (PROGRAM "is already running on this system");
		CloseHandle (qwclsemaphore);
		
		qwclsemaphore = CreateSemaphore(
			NULL,         /* Security attributes */
			0,            /* Initial count       */
			1,            /* Maximum count       */
			"qwcl"); /* Semaphore name      */
	}

	Host_Init (argc, argv);

	oldtime = Sys_DoubleTime ();

    /* main window message loop */
	while (1)
	{
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;
		Host_Frame (time);
		oldtime = newtime;
	}

    /* return success of application */
    return TRUE;
}

#else

/*
==================
main

==================
*/
int main (int argc, char **argv)
{
	double			newtime, time, oldtime;
	int				sleep_msec;

	SetConsoleCtrlHandler (HandlerRoutine, TRUE);
	hinput = GetStdHandle (STD_INPUT_HANDLE);
	houtput = GetStdHandle (STD_OUTPUT_HANDLE);

	Host_Init (argc, argv);

//
// main loop
//
	oldtime = Sys_DoubleTime () - 0.1;
	while (1)
	{
		sleep_msec = sys_sleep.value;
		if (sleep_msec > 0)
		{
			if (sleep_msec > 13)
				sleep_msec = 13;
			Sleep (sleep_msec);
		}

		NET_Sleep (1);

	// find time passed since last cycle
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;
		oldtime = newtime;
		
		Host_Frame (time);				
	}	

	return true;
}

#endif	// !SERVERONLY
