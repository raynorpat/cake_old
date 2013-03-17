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
int vid_hidden = false;
// if window is not the active window, don't hog as much CPU time,
// let go of the mouse, turn off sound, and restore system gamma ramps...
int vid_activewindow = true;

int current_vid_fullscreen;
int current_vid_width;
int current_vid_height;

cvar_t vid_fullscreen = {"vid_fullscreen", "1"};
cvar_t vid_width = {"vid_width", "1024"};
cvar_t vid_height = {"vid_height", "768"};

extern int VID_InitMode (int fullscreen, int width, int height);
int VID_Mode(int fullscreen, int width, int height)
{
	if (fullscreen)
		Com_Printf("Video: %dx%d fullscreen\n", width, height);
	else
		Com_Printf("Video: %dx%d windowed\n", width, height);

	if (VID_InitMode(fullscreen, width, height))
		return true;
	else
		return false;
}

void VID_InitCvars(void)
{
	int i;

	Cvar_Register(&vid_fullscreen);
	Cvar_Register(&vid_width);
	Cvar_Register(&vid_height);
	
	// interpret command-line parameters
	if ((i = COM_CheckParm("-window")) != 0)
		Cvar_SetValue(&vid_fullscreen, 0);
	if ((i = COM_CheckParm("-fullscreen")) != 0)
		Cvar_SetValue(&vid_fullscreen, 1);
	if ((i = COM_CheckParm("-width")) != 0)
		Cvar_Set(&vid_width, com_argv[i+1]);
	if ((i = COM_CheckParm("-height")) != 0)
		Cvar_Set(&vid_height, com_argv[i+1]);

	current_vid_fullscreen = vid_fullscreen.value;
	current_vid_width = vid_width.value;
	current_vid_height = vid_height.value;
}
