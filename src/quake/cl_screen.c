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
// screen.c -- master for refresh, status bar, console, chat, notify, etc

#include "gl_local.h"
#include "keys.h"
#include "rc_image.h"
#include "menu.h"
#include "cl_sbar.h"
#include "sound.h"
#include "version.h"
#include <time.h>

/*

background clear
rendering
turtle/net/ram icons
sbar
centerprint / slow centerprint
notify lines
intermission / finale overlay
loading plaque
console
menu

required background clears
required update regions


syncronous draw mode or async
One off screen buffer, with updates either copied or xblited
Need to double buffer?


async draw will require the refresh area to be cleared, because it will be
xblited, but sync draw can just ignore it.

sync
draw

CenterPrint ()
SlowPrint ()
Screen_Update ();
Com_Printf ();

net 
turn off messages option

the refresh is always rendered, unless the console is full screen


console is:
	notify lines
	half
	full
	

*/


// only the refresh window will be updated unless these variables are flagged 
int			scr_copytop;
int			scr_copyeverything;

float		scr_con_current;
float		scr_conlines;		// lines of console to display

cvar_t		scr_drawall = {"scr_drawall", "0"};
cvar_t		scr_viewsize = {"viewsize","100",CVAR_ARCHIVE};
cvar_t		scr_fov = {"fov","90",CVAR_ARCHIVE};	// 10 - 170
cvar_t		scr_consize = {"scr_consize","0.5"};
cvar_t		scr_conspeed = {"scr_conspeed","1000"};
cvar_t		scr_conalpha = {"scr_conalpha", "0.8"};
cvar_t		scr_centertime = {"scr_centertime","2"};
cvar_t		scr_showram = {"showram","1"};
cvar_t		scr_showturtle = {"showturtle","0"};
cvar_t		scr_showpause = {"showpause","1"};
cvar_t		scr_printspeed = {"scr_printspeed","8"};
cvar_t		scr_clock = {"cl_clock","0"};
cvar_t		scr_clock_x = {"cl_clock_x","0"};
cvar_t		scr_clock_y = {"cl_clock_y","-1"};
cvar_t		show_speed = {"show_speed","0"};
cvar_t		show_fps = {"show_fps", "0"};

cvar_t		gl_triplebuffer = {"gl_triplebuffer", "1", CVAR_ARCHIVE};

qbool		scr_initialized;		// ready to draw

mpic_t		*scr_ram;
mpic_t		*scr_net;
mpic_t		*scr_turtle;

static int	scr_fullupdate;		// set to 0 to force full redraw

int			clearconsole;
int			clearnotify;

viddef_t	vid;				// global video state

vrect_t		scr_vrect;


/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char		scr_centerstring[2048];
float		scr_centertime_start;	// for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_lines;
int			scr_erase_center;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (char *str)
{
	strlcpy (scr_centerstring, str, sizeof(scr_centerstring));
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

// count the number of lines for centering
	scr_center_lines = 1;
	while (*str)
	{
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}

void SCR_DrawCenterString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;
	int		remaining;

// the finale prints the characters one at a time
	if (cl.intermission)
		remaining = scr_printspeed.value * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = vid.height*0.35;
	else
		y = 48;

	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l*8)/2;
		for (j=0 ; j<l ; j++, x+=8)
		{
			R_DrawChar (x, y, start[j]);	
			if (!remaining--)
				return;
		}
			
		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

void SCR_CheckDrawCenterString (void)
{
	scr_copytop = 1;
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= cls.frametime;
	
	if (scr_centertime_off <= 0 && !cl.intermission)
		return;
	if (key_dest != key_game)
		return;

	SCR_DrawCenterString ();
}

//=============================================================================

/*
====================
CalcFov
====================
*/
float CalcFov (float fov_x, float width, float height)
{
	float   a;
	float   x;

/*
FIXME: this function is broken in GL (doesn't take screen aspect into account)
Probably in software also, but software doesn't care about fov_y
*/

	if (fov_x < 1 || fov_x > 179)
		Sys_Error ("Bad fov: %f", fov_x);
	
	x = width / tan(fov_x/360*M_PI);
	
	a = atan (height/x);
	
	a = a*360/M_PI;
	
	return a;
}


/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
void SCR_CalcRefdef (void)
{
	float		size;

// bound viewsize
	if (scr_viewsize.value < 30)
		Cvar_Set (&scr_viewsize, "30");
	if (scr_viewsize.value > 120)
		Cvar_Set (&scr_viewsize, "120");

// bound field of view
	if (scr_fov.value < 10)
		Cvar_Set (&scr_fov, "10");
	if (scr_fov.value > (r_refdef2.allowCheats ? 170 : 140))
		Cvar_SetValue (&scr_fov, r_refdef2.allowCheats ? 170 : 140);

// intermission is always full screen	
	if (cl.intermission) {
		size = 100.0;
		sb_lines = 0;
		sb_drawinventory = sb_drawmain = false;
	}
	else
	{
		size = scr_viewsize.value;

		// decide how much of the status bar to draw
		if (size >= 120) {
			sb_lines = 0;		// no status bar at all
			sb_drawinventory = sb_drawmain = false;
		}
		else if (size >= 110) {
			sb_lines = 24;		// no inventory
			sb_drawinventory = false;
			sb_drawmain = true;
		}
		else {
			sb_lines = 24+16+8;	// full status bar
			sb_drawinventory = sb_drawmain = true;
		}

		sb_oldmanssbar = (cl_sbar.value >= 2 && vid.width > 320);
		sb_oldmanssbar2 = (cl_sbar.value == 3);

		if (!cl_sbar.value || sb_oldmanssbar)
			sb_lines = 0;

		if (size > 100.0)
			size = 100.0;
	}

	size /= 100.0;

	VID_GetWindowSize (&vid.realx, &vid.realy, &vid.realwidth, &vid.realheight);

	scr_vrect.width = (int)(vid.width * size + 1.0) & ~1;
	if (scr_vrect.width < 96) {
		size = 96.0 / scr_vrect.width;
		scr_vrect.width = 96;      // min for icons
	}

	scr_vrect.height = (int)(vid.height * size + 1.0) & ~1;
	if (scr_vrect.height > vid.height - sb_lines)
		scr_vrect.height = vid.height - sb_lines;

	scr_vrect.x = (vid.width - scr_vrect.width)/2;
	scr_vrect.y = (vid.height - sb_lines - scr_vrect.height)/2;

	r_refdef2.vrect = scr_vrect;

	r_refdef2.fov_x = scr_fov.value;
	r_refdef2.fov_y = CalcFov (r_refdef2.fov_x, scr_vrect.width, scr_vrect.height);
}


//============================================================================

/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f (void)
{
	Cvar_SetValue (&scr_viewsize, scr_viewsize.value+10);
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f (void)
{
	Cvar_SetValue (&scr_viewsize, scr_viewsize.value-10);
	scr_fullupdate = 0;
}

static void scr_start (void)
{
	scr_ram = R_CacheWadPic ("ram");
	scr_net = R_CacheWadPic ("net");
	scr_turtle = R_CacheWadPic ("turtle");
}

static void scr_shutdown(void)
{
}

static void scr_newmap(void)
{
}

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{
	Cvar_Register (&scr_drawall);
	Cvar_Register (&scr_fov);
	Cvar_Register (&scr_viewsize);
	Cvar_Register (&scr_consize);
	Cvar_Register (&scr_conspeed);
	Cvar_Register (&scr_conalpha);
	Cvar_Register (&scr_showram);
	Cvar_Register (&scr_showturtle);
	Cvar_Register (&scr_showpause);
	Cvar_Register (&scr_centertime);
	Cvar_Register (&scr_printspeed);
	Cvar_Register (&scr_clock_x);
	Cvar_Register (&scr_clock_y);
	Cvar_Register (&scr_clock);
	Cvar_Register (&show_speed);
	Cvar_Register (&show_fps);

	Cvar_Register (&gl_triplebuffer);

//
// register our commands
//
	Cmd_AddCommand ("sizeup", SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown", SCR_SizeDown_f);

	R_RegisterModule("scr", scr_start, scr_shutdown, scr_newmap);

	scr_initialized = true;
}


/*
==============
SCR_DrawRam
==============
*/
void SCR_DrawRam (void)
{
	if (!scr_showram.value)
		return;

	if (!r_cache_thrash)
		return;

	R_DrawPic (scr_vrect.x+32, scr_vrect.y, scr_ram);
}

/*
==============
SCR_DrawTurtle
==============
*/
void SCR_DrawTurtle (void)
{
	static int	count;
	
	if (!scr_showturtle.value)
		return;

	if (cls.frametime < 0.1)
	{
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	R_DrawPic (scr_vrect.x, scr_vrect.y, scr_turtle);
}

/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged < UPDATE_BACKUP-1)
		return;
	if (cls.demoplayback)
		return;

	R_DrawPic (scr_vrect.x+64, scr_vrect.y, scr_net);
}

void SCR_DrawFPS (void)
{
	static double lastframetime;
	double t;
	extern int fps_count;
	static int lastfps;
	int x;
	char st[80];

	if (!show_fps.value)
		return;

	t = Sys_DoubleTime();
	if ((t - lastframetime) >= 1.0) {
		lastfps = fps_count / (t - lastframetime) + 0.5;
		fps_count = 0;
		lastframetime = t;
	}

	sprintf(st, "%3d FPS", lastfps);
	x = vid.width - strlen(st) * 8 - 8;
	R_DrawString(x, 0, st);
}


void SCR_DrawSpeed (void)
{
	int x, y;
	char st[80];
	vec3_t vel;
	float speed;
	static float maxspeed = 0;
	static float display_speed = -1;
	static double lastrealtime = 0;

	if (!show_speed.value)
		return;

	if (lastrealtime > cls.realtime)
	{
		lastrealtime = 0;
		display_speed = -1;
		maxspeed = 0;
	}

	if (show_speed.value == 2) {
		VectorCopy (cl.simvel, vel);	// predicted velocity
	} else if (cl.validsequence)
		VectorCopy (cl.frames[cl.validsequence & UPDATE_MASK].playerstate[cl.playernum].velocity, vel);
	else
		VectorClear (vel);
	vel[2] = 0;
	speed = VectorLength(vel);

	if (speed > maxspeed)
		maxspeed = speed;

	if (display_speed >= 0)
	{
		sprintf(st, "%3d", (int)display_speed);
		x = vid.width - strlen(st) * 8 - 8;
		y = 8;
		R_DrawString(x, y, st);
	}

	if (cls.realtime - lastrealtime >= 0.1)
	{
		lastrealtime = cls.realtime;
		display_speed = maxspeed;
		maxspeed = 0;
	}
}

void SCR_DrawClock (void)
{
	char	str[80] = "";
	int		hours, minutes, seconds;
	int		tens_hours, tens_minutes, tens_seconds;

	if (!scr_clock.value)
		return;

	if (scr_clock.value == 2)
	{
		time_t		t;
		struct tm	*ptm;

		time (&t);
		ptm = localtime (&t);
		if (!ptm)
			strcpy (str, "#bad date#");
		else
			strftime (str, sizeof(str)-1, "%H:%M:%S", ptm);
	}
	else
	{
		float	time;
		if (cl.servertime_works)
			time = cl.servertime;
		else
			time = cls.realtime;
		tens_hours = fmod (time / 36000, 10);
		hours = fmod (time / 3600, 10);
		tens_minutes = fmod (time / 600, 6);
		minutes = fmod (time / 60, 10);
		tens_seconds = fmod (time / 10, 6);
		seconds = fmod (time, 10);
		sprintf (str, "%i%i:%i%i:%i%i", tens_hours, hours, tens_minutes, minutes,
			tens_seconds, seconds);
	}

	if (scr_clock_y.value < 0)
		R_DrawString (8 * scr_clock_x.value, vid.height - sb_lines + 8*scr_clock_y.value, str);
	else
		R_DrawString (8 * scr_clock_x.value, 8*scr_clock_y.value, str);
}


/*
==============
DrawPause
==============
*/
void SCR_DrawPause (void)
{
	extern cvar_t sv_paused;
	mpic_t	*pic;

	if (!scr_showpause.value)		// turn off for screenshots
		return;

	if (!cl.paused)
		return;

#ifndef CLIENTONLY
	if (sv_paused.value == 2)
		return;		// auto-paused in single player
#endif

	pic = R_CachePic ("gfx/pause.lmp");
	R_DrawPic ( (vid.width - pic->width)/2, (vid.height - 48 - pic->height)/2, pic);
}

extern void Host_StartVideo (void);

/*
===============
SCR_BeginLoadingPlaque
================
*/
void SCR_BeginLoadingPlaque (void)
{
	Host_StartVideo();
	S_StopAllSounds (true);
	R_LoadingScreen();
}

/*
===============
SCR_EndLoadingPlaque
================
*/
void SCR_EndLoadingPlaque (void)
{
}


//=============================================================================


/*
** SCR_RunConsole
**
** Scroll it up or down
*/
void SCR_RunConsole (void)
{
	Con_CheckResize ();

	// decide on the height of the console
	if (cls.state != ca_active && !cl.intermission)
	{
		scr_conlines = vid.height;		// full screen
		scr_con_current = scr_conlines;
	}
	else if (key_dest == key_console)
	{
		scr_conlines = vid.height * scr_consize.value;
		if (scr_conlines < 30)
			scr_conlines = 30;
		if (scr_conlines > vid.height - 10)
			scr_conlines = vid.height - 10;
	}
	else
		scr_conlines = 0;				// none visible

	// scroll it up or down
	if (scr_con_current > scr_conlines) {
		scr_con_current -= scr_conspeed.value*cls.trueframetime*vid.height/320;
		scr_con_current = max (scr_con_current, scr_conlines);
	}
	else if (scr_con_current < scr_conlines) {
		scr_con_current += scr_conspeed.value*cls.trueframetime*vid.height/320;
		scr_con_current = min (scr_con_current, scr_conlines);
	}
}


/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole (void)
{
	if (clearconsole++ < vid.numpages)
	{
		Sbar_Changed ();
	}
	else if (clearnotify++ < vid.numpages)
	{
	}
	else
		con_notifylines = 0;
}

/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole (void)
{
	const char *ver = PROGRAM " " PROGRAM_VERSION;
	float	alpha;
	mpic_t	*conback;

	if (!scr_con_current)
	{
		// console is up, draw notify instead
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify ();	// only draw notify in game
		return;
	}

	// guard against going from one mode to another that's less than half the
	// vertical resolution
	if (scr_con_current > vid.height)
		scr_con_current = vid.height;

	// draw the background
	if (scr_con_current == vid.height)
		alpha = 1.0f;	// non-transparent if full screen
	else
		alpha = bound (0.0f, scr_conalpha.value, 1.0f);

	conback = R_CachePic ("gfx/conback.lmp");
	R_DrawStretchPic (0, scr_con_current - vid.height, vid.width, vid.height, conback, alpha);

	// draw version string
	Draw_Alt_String (vid.width - strlen(ver)*8 - 8, scr_con_current - 10, ver);

	// draw console text
	if (key_dest != key_menu)
		Con_DrawConsole (scr_con_current);

	clearconsole = 0;
	scr_copyeverything = 1;
}


/*
==================
SCR_InvalidateScreen

Force full redraw next time SCR_UpdateScreen is called
==================
*/
void SCR_InvalidateScreen (void)
{
	scr_fullupdate = 0;
}


/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

WARNING: be very careful calling this from elsewhere, because the refresh
needs almost the entire 256k of stack space!
==================
*/
void SCR_UpdateScreen (void)
{
	VID_UpdateGamma(false);

	if (!scr_initialized)
		return;

	if (vid_hidden)
		return;

	vid.numpages = 2 + gl_triplebuffer.value;

	scr_copytop = 0;
	scr_copyeverything = 0;

	SCR_CalcRefdef ();

	//
	// Do 3D drawing first, followed by 2D, and then finally a finish command
	//

	SCR_SetUpToDrawConsole ();
	
	V_RenderView ();

	RB_Set2DProjections ();

	R_PolyBlend ();

	if (scr_fullupdate++ < vid.numpages || scr_drawall.value)
		Sbar_Changed ();

	if (r_netgraph.value)
		R_NetGraph ();

	if (cl.intermission == 1 && key_dest != key_menu)
	{
		Sbar_IntermissionOverlay ();
		Con_ClearNotify ();
	}
	else if (cl.intermission == 2 && key_dest != key_menu)
	{
		Sbar_FinaleOverlay ();
		SCR_CheckDrawCenterString ();
		Con_ClearNotify ();
	}
		
	if (cls.state == ca_active)
	{
		SCR_DrawRam ();
		SCR_DrawNet ();
		SCR_DrawTurtle ();
		SCR_DrawPause ();
		if (!cl.intermission)
		{
			if (key_dest != key_menu)
				Draw_Crosshair ();
			SCR_CheckDrawCenterString ();
			SCR_DrawSpeed ();
			SCR_DrawClock ();
			SCR_DrawFPS ();
			Sbar_Draw ();
		}
	}
	
	SCR_DrawConsole ();	
	M_Draw ();

	VID_Finish ();
}
