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
// vid.h -- video driver defs
#ifndef _VID_H_
#define _VID_H_

#define VID_CBITS	6
#define VID_GRADES	(1 << VID_CBITS)

// a pixel can be one, two, or four bytes
typedef byte pixel_t;

typedef struct vrect_s
{
	int				x,y,width,height;
	struct vrect_s	*pnext;
} vrect_t;

typedef struct
{
	pixel_t			*colormap;		// 256 * VID_GRADES size
	int				numpages;

	int				width;
	int				height;

	// these are set with VID_GetWindowSize and can change from frame to frame
	int				realx;
	int				realy;
	int				realwidth;
	int				realheight;
} viddef_t;

extern	viddef_t	vid;				// global video state
extern	unsigned	d_8to24table[256];

extern int vid_hidden;
extern int vid_activewindow;

extern cvar_t vid_fullscreen;
extern cvar_t vid_width;
extern cvar_t vid_height;
extern cvar_t vid_vsync;

void	VID_Shared_Init(void);

void	VID_Init (void);
int		VID_Mode(int fullscreen, int width, int height);
// Called at startup

void	VID_Shutdown (void);
// Called at shutdown

void	VID_Finish (void);
// Called at end of each frame

void VID_GetWindowSize (int *x, int *y, int *width, int *height);

void VID_SetCaption (char *text);

void VID_Restart_f(void);

void VID_Start (void);

// oldman: gamma variables for glx linux
void VID_SetDeviceGammaRamp (unsigned short *ramps);
extern qbool vid_hwgamma_enabled;

#ifdef _WIN32
void VID_SetDeviceGammaRamp (unsigned short *ramps);
extern qbool vid_hwgamma_enabled;
#endif

#endif /* _VID_H_ */

