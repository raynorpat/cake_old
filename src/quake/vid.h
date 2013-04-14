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

typedef enum renderpath_e
{
	RENDERPATH_GL11,
	RENDERPATH_GLES,
	RENDERPATH_GL30,
	RENDERPATH_D3D11,
} renderpath_t;

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

	renderpath_t	renderpath;

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
extern int vid_allowhwgamma;
extern int vid_hardwaregammasupported;
extern int vid_usinghwgamma;

extern cvar_t vid_fullscreen;
extern cvar_t vid_width;
extern cvar_t vid_height;
extern cvar_t vid_mouse;
extern cvar_t vid_vsync;

// brand of graphics chip
extern const char *gl_vendor;
// graphics chip model and other information
extern const char *gl_renderer;
// begins with 1.0.0, 1.1.0, 1.2.0, 1.2.1, 1.3.0, 1.3.1, or 1.4.0
extern const char *gl_version;
// extensions list, space separated
extern const char *gl_extensions;
// WGL, GLX, or AGL
extern const char *gl_platform;
// another extensions list, containing platform-specific extensions that are
// not in the main list
extern const char *gl_platformextensions;
// name of driver library (opengl32.dll, libGL.so.1, or whatever)
extern char gl_driver[256];

// GLX_SGI_video_sync and WGL_EXT_swap_control
extern int gl_videosyncavailable;

int		GL_OpenLibrary(const char *name);
void	GL_CloseLibrary(void);
void	*GL_GetProcAddress(const char *name);
int		GL_CheckExtension(const char *name, const dllfunction_t *funcs, char *disableparm, int silent);

void	VID_Shared_Init(void);

void	VID_CheckExtensions(void);

void	VID_Init (void);
int		VID_Mode(int fullscreen, int width, int height);
// Called at startup

void	VID_Shutdown (void);
// Called at shutdown

// sets hardware gamma correction, returns false if the device does not
// support gamma control
int		VID_SetGamma (unsigned short *ramps);
// gets hardware gamma correction, returns false if the device does not
// support gamma control
int		VID_GetGamma (unsigned short *ramps);

void	VID_UpdateGamma(qbool force);
void	VID_RestoreSystemGamma(void);

void	VID_Finish (void);
// Called at end of each frame

void	VID_GetWindowSize (int *x, int *y, int *width, int *height);

void	VID_SetCaption (char *text);

void	VID_Restart_f(void);

void	VID_Start (void);

#endif /* _VID_H_ */

