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
// vid_shared.c -- shared video functionality

#include "gl_local.h"

// if window is hidden, don't update screen
int vid_hidden = true;
// if window is not the active window, don't hog as much CPU time,
// let go of the mouse, turn off sound, and restore system gamma ramps...
int vid_activewindow = true;

cvar_t vid_fullscreen = {"vid_fullscreen", "0"};
cvar_t vid_width = {"vid_width", "1024"};
cvar_t vid_height = {"vid_height", "768"};

int current_vid_fullscreen;
int current_vid_width;
int current_vid_height;
extern int VID_InitMode (int fullscreen, int width, int height);
int VID_Mode(int fullscreen, int width, int height)
{
	if (fullscreen)
		Com_Printf("Video: %dx%d fullscreen\n", width, height);
	else
		Com_Printf("Video: %dx%d windowed\n", width, height);

	if (VID_InitMode(fullscreen, width, height))
	{
		current_vid_fullscreen = fullscreen;
		current_vid_width = width;
		current_vid_height = height;
		Cvar_SetValue(&vid_fullscreen, fullscreen);
		Cvar_SetValue(&vid_width, width);
		Cvar_SetValue(&vid_height, height);
		return true;
	}
	else
	{
		return false;
	}
}

void VID_Shared_Init(void)
{
	int i;

	Cvar_Register(&vid_fullscreen);
	Cvar_Register(&vid_width);
	Cvar_Register(&vid_height);

	Cmd_AddCommand("vid_restart", VID_Restart_f);
	
	// interpret command-line parameters
	if ((i = COM_CheckParm("-window")) != 0)
		Cvar_SetValue(&vid_fullscreen, 0);
	if ((i = COM_CheckParm("-fullscreen")) != 0)
		Cvar_SetValue(&vid_fullscreen, 1);
	if ((i = COM_CheckParm("-width")) != 0)
		Cvar_Set(&vid_width, com_argv[i+1]);
	if ((i = COM_CheckParm("-height")) != 0)
		Cvar_Set(&vid_height, com_argv[i+1]);
}

static void VID_OpenSystems(void)
{
	R_Modules_Start();
}

static void VID_CloseSystems(void)
{
	R_Modules_Shutdown();
}

void VID_Restart_f(void)
{
	Com_Printf("VID_Restart: changing from %s %dx%d, to %s %dx%d.\n",
		current_vid_fullscreen ? "fullscreen" : "window", current_vid_width, current_vid_height, 
		vid_fullscreen.value ? "fullscreen" : "window", vid_width.value, vid_height.value);

	VID_Close();
	if (!VID_Mode(vid_fullscreen.value, vid_width.value, vid_height.value))
	{
		Com_Printf("Video mode change failed\n");
		if (!VID_Mode(current_vid_fullscreen, current_vid_width, current_vid_height))
			Sys_Error("Unable to restore to last working video mode\n");
	}
	VID_OpenSystems();

	TexMgr_ReloadImages();
}

void VID_Open(void)
{
	Com_Printf("Starting video system\n");
	if (!VID_Mode(vid_fullscreen.value, vid_width.value, vid_height.value))
	{
		Com_Printf("Desired video mode failed, trying fallbacks...\n");
		if (vid_fullscreen.value)
		{
			if (!VID_Mode(true, 640, 480))
				if (!VID_Mode(false, 640, 480))
					Sys_Error("Video modes failed\n");
		}
		else
		{
			Sys_Error("Windowed video failed\n");
		}
	}

	VID_OpenSystems();
}

void VID_Close(void)
{
	VID_CloseSystems();
	VID_Shutdown();
}
