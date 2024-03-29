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

#include "common.h"
#include "pmove.h"
#include "version.h"
#include "thread.h"
#include <setjmp.h>


#if !defined(CLIENTONLY) && !defined(SERVERONLY)
qbool		dedicated = false;
#endif

cvar_t		host_mapname = {"mapname", "", CVAR_ROM};

double		curtime;

qbool		host_initialized;		// true if into command execution
int			host_hunklevel;

jmp_buf 	host_abort;

char		engineversion[128];


/*
================
Host_Abort
================
*/
void Host_Abort (void)
{
	longjmp (host_abort, 1);
}

/*
================
Host_EndGame
================
*/
void Host_EndGame (void)
{
	SCR_EndLoadingPlaque ();

	SV_Shutdown ("Server was killed");
	CL_Disconnect ();

	// clear disconnect messages from loopback
	NET_ClearLoopback ();
}

/*
================
Host_Error

This shuts down both the client and server
================
*/
void Host_Error (char *error, ...)
{
	va_list		argptr;
	char		string[1024];
	static qbool inerror = false;
	
	if (inerror)
		Sys_Error ("Host_Error: recursively entered");
	inerror = true;

	Com_EndRedirect ();

	SCR_EndLoadingPlaque ();

	va_start (argptr,error);
#ifdef _WIN32
	_vsnprintf (string, sizeof(string) - 1, error, argptr);
	string[sizeof(string) - 1] = '\0';
#else
	vsnprintf (string, sizeof(string), error, argptr);
#endif // _WIN32
	va_end (argptr);

	Com_Printf ("\n===========================\n");
	Com_Printf ("Host_Error: %s\n", string);
	Com_Printf ("===========================\n\n");
	
	SV_Shutdown (va("server crashed: %s\n", string));
	CL_Disconnect ();
	CL_HandleHostError ();		// stop demo loop

	if (dedicated)
	{
		NET_Shutdown ();
		COM_Shutdown ();
		Sys_Error ("%s", string);
	}

	if (!host_initialized)
		Sys_Error ("Host_Error: %s", string);

	inerror = false;

	Host_Abort ();
}

extern void VID_Start (void);
extern void SCR_BeginLoadingPlaque (void);

qbool vid_opened = false;
void Host_StartVideo(void)
{
	if (!vid_opened && !dedicated)
	{
		vid_opened = true;
		VID_Start();
	}
}

// init whatever commands/cvars we need
// not many, really
void Host_InitLocal (void)
{
	Cvar_Register (&host_mapname);
}

extern void Mod_ClearAll (void);

/*
===============
Host_ClearMemory

Free hunk memory up to host_hunklevel
Can only be called when changing levels!
===============
*/
void Host_ClearMemory ()
{
	Com_DPrintf ("Clearing memory\n");

	// FIXME, move to CL_ClearState
#ifndef SERVERONLY
	if (!dedicated)
		Mod_ClearAll ();
#endif

	CM_InvalidateMap ();

	// any data previously allocated on hunk is no longer valid
	Hunk_FreeToLowMark (host_hunklevel);
}


/*
===============
Host_Frame
===============
*/
void Host_Frame (double time)
{
	if (setjmp (host_abort))
		return;			// something bad happened, or the server disconnected

	curtime += time;

	if (dedicated)
		SV_Frame (time);
	else
		CL_Frame (time, SV_IsThreaded());	// will also call SV_Frame
}

/*
====================
Host_Init
====================
*/
void Host_Init (int argc, char **argv)
{
	const char* os;

	COM_InitArgv (argc, argv);

#if !defined(CLIENTONLY) && !defined(SERVERONLY)
	if (COM_CheckParm("-dedicated"))
		dedicated = true;
#endif
	
	// initialize console command/cvar/alias/command execution systems
	Cbuf_Init ();
	Cmd_Init ();
	Cvar_Init ();
	COM_Init ();
	Key_Init ();
	Thread_Init ();

	// initialize various cvars/cmds that could not be initialized earlier
	FS_Init_Commands ();

	// detect gamemode from commandline options or executable name
	COM_InitGameType();

	// initialize console and logging and its cvars/commands
	Con_Init ();

	Cbuf_AddEarlyCommands ();
	Cbuf_Execute ();

	// initialize filesystem (including fs_basedir, fs_gamedir, -game, scr_screenshot_name)
	FS_Init();

	NET_Init ();
	Netchan_Init ();
	Sys_Init ();
	CM_Init ();
	PM_Init ();
	Host_InitLocal ();

	// construct a version string for the corner of the console
	os = QUAKE_OS_NAME;
	Q_snprintf (engineversion, sizeof (engineversion), "%s %s %s", gamename, os, buildstring);
	Com_Printf("%s\n", engineversion);

	// initialize client and server
	SV_Init ();
	CL_Init ();

	Cvar_CleanUpTempVars ();

	Hunk_AllocName (0, "-HOST_HUNKLEVEL-");
	host_hunklevel = Hunk_LowMark ();

	host_initialized = true;

	if (dedicated)
	{
		Cbuf_AddText ("exec server.cfg\n");
		Cmd_StuffCmds_f (); // process command line arguments
		Cbuf_Execute ();

		// if a map wasn't specified on the command line, spawn start map
		if (!com_serveractive)
			Cmd_ExecuteString ("map start", false);
		if (!com_serveractive)
			Host_Error ("Couldn't spawn a server");
	}
	else
	{
		Cbuf_AddText ("exec quake.rc\n");
		Cbuf_Execute ();
	}

	Com_DPrintf ("\n========= " PROGRAM " Initialized =========\n");

	if (!dedicated)
		SV_StartThread ();
}


/*
===============
Host_Shutdown

FIXME: this is a callback from Sys_Quit and Sys_Error.  It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void Host_Shutdown (void)
{
	static qbool isdown = false;
	
	if (isdown)
	{
		printf ("recursive shutdown\n");
		return;
	}
	isdown = true;

	SV_Shutdown ("Server quit\n");
	CL_Shutdown ();
	NET_Shutdown ();
	COM_Shutdown ();
	Cmd_Shutdown ();
	Con_Shutdown ();
	FS_Shutdown ();
	SV_StopThread ();
	Thread_Shutdown ();
}

/*
===============
Host_Quit
===============
*/
void Host_Quit (void)
{
	Host_Shutdown ();
	Sys_Quit ();
}

