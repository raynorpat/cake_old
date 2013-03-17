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
// vid_wgl.c -- Windows OpenGL driver

#include "gl_local.h"
#include "cdaudio.h"
#include "input.h"
#include "keys.h"
#include "msvc\resource.h"
#include "sound.h"
#include "winquake.h"

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

qbool			DDActive;

static DEVMODE	gdevmode;
static qbool	vid_initialized = false;
static qbool	vid_canalttab = false;
static qbool	vid_wassuspended = false;
static int		windowed_mouse;
extern qbool	in_mouseactive;  // from in_win.c
static HICON	hIcon;

HWND mainwindow;

static int vid_isfullscreen;

float vid_gamma = 1.0;

cvar_t	gl_strings = {"gl_strings", "", CVAR_ROM};
cvar_t	vid_hwgammacontrol = {"vid_hwgammacontrol","1"};
cvar_t	gl_swapinterval = {"gl_swapinterval", "1"};
cvar_t	gl_ext_swapinterval = {"gl_ext_swapinterval", "1"};

qbool	vid_gammaworks = false;
qbool	vid_hwgamma_enabled = false;
unsigned short *currentgammaramp = NULL;
void RestoreHWGamma (void);

unsigned	d_8to24table[256];
unsigned	d_8to24table2[256];
unsigned char d_15to8table[65536];

float		gldepthmin, gldepthmax;

LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void AppActivate(BOOL fActive, BOOL minimize);

void GL_Init (void);

PROC glArrayElementEXT;
PROC glColorPointerEXT;
PROC glTexCoordPointerEXT;
PROC glVertexPointerEXT;

typedef void (APIENTRY *lp3DFXFUNC) (int, int, int, int, int, const void*);
lp3DFXFUNC glColorTableEXT;
qbool is8bit = false;
qbool isPermedia = false;
qbool gl_mtexable = false;
qbool gl_mtexfbskins = false;

//====================================

cvar_t		_windowed_mouse = {"_windowed_mouse","1",CVAR_ARCHIVE};
cvar_t		vid_displayfrequency = {"vid_displayfrequency", "0"};

int window_x, window_y, window_width, window_height;
int window_center_x, window_center_y;

void IN_ShowMouse (void);
void IN_DeactivateMouse (void);
void IN_HideMouse (void);
void IN_ActivateMouse (void);
void IN_MouseEvent (int mstate);

qbool mouseinitialized;
static qbool dinput;

/*
================
VID_SetCaption
================
*/
void VID_SetCaption (char *text)
{
	if (vid_initialized)
		SetWindowText (mainwindow, text);
}

/*
================
VID_UpdateWindowStatus
================
*/
void VID_UpdateWindowStatus (void)
{
	window_center_x = window_x + window_width / 2;
	window_center_y = window_y + window_height / 2;

	if (mouseinitialized && in_mouseactive && !dinput)
	{
		RECT window_rect;
		window_rect.left = window_x;
		window_rect.top = window_y;
		window_rect.right = window_x + window_width;
		window_rect.bottom = window_y + window_height;
		ClipCursor (&window_rect);
	}
}

/*
=================
VID_GetWindowSize
=================
*/
void VID_GetWindowSize (int *x, int *y, int *width, int *height)
{
	*x = 0;
	*y = 0;
	*width = window_width;
	*height = window_height;
}

//====================================

BINDTEXFUNCPTR bindTexFunc;

#define TEXTURE_EXT_STRING "GL_EXT_texture_object"


void CheckTextureExtensions (void)
{
	unsigned char	*tmp;
	qbool			 texture_ext;
	HINSTANCE		 hInstGL;

	texture_ext = FALSE;
	/* check for texture extension */
	tmp = (unsigned char *)glGetString(GL_EXTENSIONS);
	while (tmp && *tmp)
	{
		if (strncmp((const char*)tmp, TEXTURE_EXT_STRING, strlen(TEXTURE_EXT_STRING)) == 0)
			texture_ext = TRUE;
		tmp++;
	}

	if (!texture_ext || COM_CheckParm ("-gl11") )
	{
		hInstGL = LoadLibrary("opengl32.dll");

		if (hInstGL == NULL)
			Sys_Error ("Couldn't load opengl32.dll");

		bindTexFunc = (void *)GetProcAddress(hInstGL,"glBindTexture");

		if (!bindTexFunc)
			Sys_Error ("No texture objects!");
		return;
	}

/* load library and get procedure adresses for texture extension API */
	if ((bindTexFunc = (BINDTEXFUNCPTR)
		wglGetProcAddress((LPCSTR) "glBindTextureEXT")) == NULL)
	{
		Sys_Error ("GetProcAddress for BindTextureEXT failed");
		return;
	}
}

void CheckArrayExtensions (void)
{
	unsigned char		*tmp;

	/* check for texture extension */
	tmp = (unsigned char *)glGetString(GL_EXTENSIONS);
	while (tmp && *tmp)
	{
		if (strncmp((const char*)tmp, "GL_EXT_vertex_array", strlen("GL_EXT_vertex_array")) == 0)
		{
			if ( ((glArrayElementEXT = wglGetProcAddress("glArrayElementEXT")) == NULL) ||
			     ((glColorPointerEXT = wglGetProcAddress("glColorPointerEXT")) == NULL) ||
			     ((glTexCoordPointerEXT = wglGetProcAddress("glTexCoordPointerEXT")) == NULL) ||
			     ((glVertexPointerEXT = wglGetProcAddress("glVertexPointerEXT")) == NULL) )
			{
				Sys_Error ("GetProcAddress for vertex extension failed");
				return;
			}
			return;
		}
		tmp++;
	}

	Sys_Error ("Vertex array extension not present");
}

void CheckMultiTextureExtensions (void)
{
	if (strstr(gl_extensions, "GL_ARB_multitexture ") && !COM_CheckParm("-nomtex")) {
		qglMultiTexCoord2f = (void *) wglGetProcAddress("glMultiTexCoord2fARB");
		qglActiveTexture = (void *) wglGetProcAddress("glActiveTextureARB");
		if (!qglMultiTexCoord2f || !qglActiveTexture)
			return;
		Com_Printf ("Multitexture extensions found.\n");
		gl_mtexable = true;
	}

	if (gl_mtexable && !COM_CheckParm("-nomtexfbskins"))
		gl_mtexfbskins = true;
}

BOOL ( WINAPI * qwglSwapIntervalEXT)( int interval ) = NULL;

void CheckSwapIntervalExtension (void)
{
	if (gl_ext_swapinterval.value && strstr(gl_extensions, "WGL_EXT_swap_control")) {
		qwglSwapIntervalEXT = (void *) wglGetProcAddress("wglSwapIntervalEXT");
	}
}

void UpdateSwapInterval (void)
{
	static float old_gl_swapinterval = 0;

	if (qwglSwapIntervalEXT) {
		if (gl_swapinterval.value != old_gl_swapinterval) {
			old_gl_swapinterval = gl_swapinterval.value;
			qwglSwapIntervalEXT (gl_swapinterval.value != 0);
		}
	}
}

qbool VID_Is8bit() {
	return is8bit;
}

#define GL_SHARED_TEXTURE_PALETTE_EXT 0x81FB

static void VID_Build15to8table (void)
{
	byte	*pal;
	unsigned r,g,b;
	unsigned v;
	int     r1,g1,b1;
	int		j,k,l;
	int		i;

	// 3D distance calcs - k is last closest, l is the distance.
	// FIXME: Precalculate this and cache to disk ?
	for (i=0; i < (1<<15); i++) {
		/* Maps
			000000000000000
			000000000011111 = Red  = 0x1F
			000001111100000 = Blue = 0x03E0
			111110000000000 = Grn  = 0x7C00
		*/
		r = ((i & 0x1F) << 3)+4;
		g = ((i & 0x03E0) >> 2)+4;
		b = ((i & 0x7C00) >> 7)+4;
		pal = (unsigned char *)d_8to24table;
		for (v=0,k=0,l=10000*10000; v<256; v++,pal+=4) {
			r1 = r-pal[0];
			g1 = g-pal[1];
			b1 = b-pal[2];
			j = (r1*r1)+(g1*g1)+(b1*b1);
			if (j<l) {
				k=v;
				l=j;
			}
		}
		d_15to8table[i]=k;
	}
}

void VID_Init8bitPalette()
{
	// Check for 8bit Extensions and initialize them.
	int i;
	char thePalette[256*3];
	char *oldPalette, *newPalette;

	glColorTableEXT = (void *)wglGetProcAddress("glColorTableEXT");
    if (!glColorTableEXT || !strstr(gl_extensions, "GL_EXT_shared_texture_palette") ||
		!COM_CheckParm("-use8bit"))
		return;

	Com_Printf ("8-bit GL extensions enabled.\n");
    glEnable( GL_SHARED_TEXTURE_PALETTE_EXT );
	oldPalette = (char *) d_8to24table;
	newPalette = thePalette;
	for (i=0;i<256;i++) {
		*newPalette++ = *oldPalette++;
		*newPalette++ = *oldPalette++;
		*newPalette++ = *oldPalette++;
		oldPalette++;
	}
	glColorTableEXT(GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGB, 256, GL_RGB, GL_UNSIGNED_BYTE,
		(void *) thePalette);
	is8bit = TRUE;

	VID_Build15to8table ();
}

/*
===============
GL_Init
===============
*/
void GL_Init (void)
{
	gl_vendor = (const char *) glGetString (GL_VENDOR);
	Com_Printf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = (const char *) glGetString (GL_RENDERER);
	Com_Printf ("GL_RENDERER: %s\n", gl_renderer);
	gl_version = (const char *) glGetString (GL_VERSION);
	Com_Printf ("GL_VERSION: %s\n", gl_version);
	gl_extensions = (const char *) glGetString (GL_EXTENSIONS);
//	Com_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);

	Cvar_Register (&gl_strings);
	Cvar_ForceSet (&gl_strings, va("GL_VENDOR: %s\nGL_RENDERER: %s\n"
		"GL_VERSION: %s\nGL_EXTENSIONS: %s", gl_vendor, gl_renderer, gl_version, gl_extensions));

    if (strnicmp(gl_renderer,"Permedia",8)==0)
         isPermedia = true;

	CheckTextureExtensions ();
	CheckMultiTextureExtensions ();
	CheckSwapIntervalExtension ();

	// Check for 3DFX Extensions and initialize them.
	VID_Init8bitPalette();

	glClearColor (1,0,0,0);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.666);

	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel (GL_FLAT);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

//	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}


void GL_EndRendering (void)
{
	HDC hdc;

	/*
	static qbool old_hwgamma_enabled;

	vid_hwgamma_enabled = vid_hwgammacontrol.value && vid_gammaworks && vid_activewindow && !vid_hidden && modestate == MS_FULLDIB;
	if (vid_hwgamma_enabled != old_hwgamma_enabled) {
		old_hwgamma_enabled = vid_hwgamma_enabled;
		if (vid_hwgamma_enabled && currentgammaramp)
			VID_SetDeviceGammaRamp (currentgammaramp);
		else
			RestoreHWGamma ();
	}
	*/

	UpdateSwapInterval ();

	if (!scr_skipupdate || block_drawing)
	{
		glFinish();
		hdc = GetDC(mainwindow);
		SwapBuffers(hdc);
		ReleaseDC(mainwindow, hdc);
	}

// handle the mouse state when windowed if that's changed
	if (!vid_isfullscreen)
	{
		if (!_windowed_mouse.value) {
			if (windowed_mouse)	{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
				windowed_mouse = false;
			}
		} else {
			windowed_mouse = true;
			if (key_dest == key_game && !in_mouseactive && vid_activewindow) {
				IN_ActivateMouse ();
				IN_HideMouse ();
			} else if (in_mouseactive && key_dest != key_game) {
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}
		}
	}
}

void VID_SetPalette (unsigned char *palette)
{
	int			i;
	byte		*pal;
	unsigned	r,g,b;
	unsigned	v;
	unsigned	*table;

//
// 8 8 8 encoding
//
	pal = palette;
	table = d_8to24table;
	for (i=0 ; i<256 ; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;

//		v = (255<<24) + (r<<16) + (g<<8) + (b<<0);
//		v = (255<<0) + (r<<8) + (g<<16) + (b<<24);
		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		*table++ = v;
	}
	d_8to24table[255] = 0;	// 255 is transparent

// Tonik: create a brighter palette for bmodel textures
	pal = palette;
	table = d_8to24table2;

	for (i=0 ; i<256 ; i++)
	{
		r = pal[0] * (2.0 / 1.5); if (r > 255) r = 255;
		g = pal[1] * (2.0 / 1.5); if (g > 255) g = 255;
		b = pal[2] * (2.0 / 1.5); if (b > 255) b = 255;
		pal += 3;
		*table++ = (255<<24) + (r<<0) + (g<<8) + (b<<16);
	}
	d_8to24table2[255] = 0;	// 255 is transparent
}

static byte	systemgammaramp[3][256][2];
static qbool customgamma = false;

/*
======================
VID_SetDeviceGammaRamp

Note: ramps must point to a static array
======================
*/
void VID_SetDeviceGammaRamp (unsigned short *ramps)
{
	/*
	if (vid_gammaworks) {
		currentgammaramp = ramps;
		if (vid_hwgamma_enabled) {
			SetDeviceGammaRamp (maindc, ramps);
			customgamma = true;
		}
	}
	*/
}

void InitHWGamma (void)
{
	/*
	if (COM_CheckParm("-nohwgamma"))
		return;
	vid_gammaworks = GetDeviceGammaRamp (maindc, systemgammaramp);
	vid_hwgamma_enabled = vid_hwgammacontrol.value && vid_gammaworks
		&& vid_activewindow && !vid_hidden && modestate == MS_FULLDIB;
	*/
}

void RestoreHWGamma (void)
{
	/*
	if (vid_gammaworks && customgamma) {
		customgamma = false;
		SetDeviceGammaRamp (maindc, systemgammaramp);
	}
	*/
}

/*
===================================================================

MAIN WINDOW

===================================================================
*/

/*
================
ClearAllStates
================
*/
void ClearAllStates (void)
{
	extern void IN_ClearStates (void);
	extern qbool keydown[256];
	int		i;

// send an up event for each key, to make sure the server clears them all
	for (i=0 ; i<256 ; i++)
	{
		if (keydown[i])
			Key_Event (i, false);
	}

	Key_ClearStates ();
	IN_ClearStates ();
}

/****************************************************************************
*
* Function:     AppActivate
* Parameters:   fActive - True if app is activating
*
* Description:  If the application is activating, then swap the system
*               into SYSPAL_NOSTATIC mode so that our palettes will display
*               correctly.
*
****************************************************************************/
void AppActivate(BOOL fActive, BOOL minimize)
{
	static BOOL	sound_active;

	vid_activewindow = fActive;
	vid_hidden = minimize;

// enable/disable sound on focus gain/loss
	if (!vid_activewindow && sound_active)
	{
		S_BlockSound ();
		sound_active = false;
	}
	else if (vid_activewindow && !sound_active)
	{
		S_UnblockSound ();
		sound_active = true;
	}

	if (fActive)
	{
		if (vid_isfullscreen)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();

			if (vid_canalttab && !vid_hidden && currentgammaramp)
				VID_SetDeviceGammaRamp (currentgammaramp);

			if (vid_canalttab && vid_wassuspended) {
				vid_wassuspended = false;
				if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
					Sys_Error ("Couldn't set fullscreen DIB mode");
				ShowWindow (mainwindow, SW_SHOWNORMAL);

				// Fix for alt-tab bug in NVidia drivers
				MoveWindow (mainwindow, 0, 0, gdevmode.dmPelsWidth, gdevmode.dmPelsHeight, false);

				SCR_InvalidateScreen ();
			}
		}
		else if (!vid_isfullscreen && vid_hidden)
			ShowWindow (mainwindow, SW_RESTORE);
		else if (!vid_isfullscreen && _windowed_mouse.value && key_dest == key_game)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
	}

	if (!fActive)
	{
		if (vid_isfullscreen)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			RestoreHWGamma ();
			if (vid_canalttab) {
				ChangeDisplaySettings (NULL, 0);
				vid_wassuspended = true;
			}
		}
		else if (!vid_isfullscreen && _windowed_mouse.value)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
	}
}


/*
===================================================================

MAIN WINDOW

===================================================================
*/

LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void IN_TranslateKeyEvent (int lKeyData, qbool down);

/* main window procedure */
LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM  wParam, LPARAM lParam)
{
    LONG    lRet = 1;
	int		fActive, fMinimized, temp;
	extern unsigned int uiWheelMessage;

	if ( uMsg == uiWheelMessage ) {
		uMsg = WM_MOUSEWHEEL;
		wParam <<= 16;
	}

    switch (uMsg)
    {
		case WM_KILLFOCUS:
			if (vid_isfullscreen)
				ShowWindow(mainwindow, SW_SHOWMINNOACTIVE);
			break;

		case WM_CREATE:
			break;

		case WM_MOVE:
			window_x = (int) LOWORD(lParam);
			window_y = (int) HIWORD(lParam);
			VID_UpdateWindowStatus ();
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			IN_TranslateKeyEvent (lParam, true);
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			IN_TranslateKeyEvent (lParam, false);
			break;

		case WM_SYSCHAR:
		// keep Alt-Space from happening
			break;

	// this is complicated because Win32 seems to pack multiple mouse events into
	// one update sometimes, so we always check all states and look for events
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
		case WM_MOUSEMOVE:
			temp = 0;

			if (wParam & MK_LBUTTON)
				temp |= 1;

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

			// Win2k/XP let us bind button4 and button5
			if (wParam & MK_XBUTTON1)
				temp |= 8;

			if (wParam & MK_XBUTTON2)
				temp |= 16;

			IN_MouseEvent (temp);

			break;

		// JACK: This is the mouse wheel with the Intellimouse
		// Its delta is either positive or neg, and we generate the proper
		// Event.
		case WM_MOUSEWHEEL:
			if (in_mwheeltype != MWHEEL_DINPUT)
			{
				in_mwheeltype = MWHEEL_WINDOWMSG;

				if ((short) HIWORD(wParam) > 0) {
					Key_Event(K_MWHEELUP, true);
					Key_Event(K_MWHEELUP, false);
				} else {
					Key_Event(K_MWHEELDOWN, true);
					Key_Event(K_MWHEELDOWN, false);
				}
			}
			break;

    	case WM_SIZE:
            break;

   	    case WM_CLOSE:
			if (MessageBox (mainwindow, "Are you sure you want to quit?", "Confirm Exit", MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
				Host_Quit ();
	        break;

		case WM_ACTIVATE:
			fActive = LOWORD(wParam);
			fMinimized = (BOOL) HIWORD(wParam);
			AppActivate(!(fActive == WA_INACTIVE), fMinimized);

		// fix the leftover Alt from any Alt-Tab or the like that switched us away
			ClearAllStates ();

			break;

   	    case WM_DESTROY:
        {
			if (mainwindow)
				DestroyWindow (mainwindow);

            PostQuitMessage (0);
        }
        break;

		case MM_MCINOTIFY:
            lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
			break;

    	default:
            /* pass all unhandled messages to DefWindowProc */
            lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
        break;
    }

    /* return 1 if handled message, 0 if not */
    return lRet;
}

static void Check_Gamma (unsigned char *pal)
{
	float	f, inf;
	unsigned char	palette[768];
	int		i;

	if ((i = COM_CheckParm("-gamma")) != 0 && i+1 < com_argc)
		vid_gamma = bound (0.3, Q_atof(com_argv[i+1]), 1);
	else
		vid_gamma = 1;

	Cvar_SetValue (&gl_gamma, vid_gamma);

	for (i=0 ; i<768 ; i++)
	{
		f = powf ( (pal[i]+1)/256.0 , vid_gamma );
		inf = f*255 + 0.5;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		palette[i] = inf;
	}

	memcpy (pal, palette, sizeof(palette));
}

/*
===================
VID_Init
===================
*/
void VID_Init (void)
{
	WNDCLASS wc;

	Cvar_Register (&_windowed_mouse);
	Cvar_Register (&vid_hwgammacontrol);
	Cvar_Register (&vid_displayfrequency);
	Cvar_Register (&gl_swapinterval);
	Cvar_Register (&gl_ext_swapinterval);

	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_APPICON));

	// Register the frame class
	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC)MainWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = global_hInstance;
	wc.hIcon         = 0;
	wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName  = 0;
	wc.lpszClassName = "QuakeWindowClass";

	if (!RegisterClass (&wc))
		Sys_Error("Couldn't register window class\n");
}

int VID_InitMode (int fullscreen, int width, int height)
{
	int i;
	HDC hdc;
	RECT rect;
	MSG msg;
	PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
		1,						// version number
		PFD_DRAW_TO_WINDOW 		// support window
		|  PFD_SUPPORT_OPENGL 	// support OpenGL
		|  PFD_DOUBLEBUFFER ,	// double buffered
		PFD_TYPE_RGBA,			// RGBA type
		24,						// 24-bit color depth
		0, 0, 0, 0, 0, 0,		// color bits ignored
		0,						// no alpha buffer
		0,						// shift bit ignored
		0,						// no accumulation buffer
		0, 0, 0, 0, 			// accum bits ignored
		24,						// 24-bit z-buffer
		8,						// 8-bit stencil buffer
		0,						// no auxiliary buffer
		PFD_MAIN_PLANE,			// main layer
		0,						// reserved
		0, 0, 0					// layer masks ignored
	};
	int pixelformat;
	DWORD WindowStyle, ExWindowStyle;
	HGLRC baseRC;
	int CenterX, CenterY;

	if (vid_initialized)
		Sys_Error("VID_InitMode called when video is already initialised\n");

	memset(&gdevmode, 0, sizeof(gdevmode));

	if (hwnd_dialog) {
		DestroyWindow (hwnd_dialog);
		hwnd_dialog = NULL;
	}

	Check_Gamma(host_basepal);
	VID_SetPalette (host_basepal);

	InitHWGamma ();

	vid_isfullscreen = false;
	if (fullscreen)
	{
		gdevmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
		gdevmode.dmBitsPerPel = 32;
		gdevmode.dmPelsWidth = width;
		gdevmode.dmPelsHeight = height;
		gdevmode.dmSize = sizeof (gdevmode);
		if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
		{
			VID_Shutdown();
			Com_Printf("Unable to change to requested mode %dx%dx\n", width, height);
			return false;
		}

		vid_isfullscreen = true;
		WindowStyle = WS_POPUP;
		ExWindowStyle = 0;
	}
	else
	{
		hdc = GetDC (NULL);
		i = GetDeviceCaps(hdc, RASTERCAPS);
		ReleaseDC (NULL, hdc);
		if (i & RC_PALETTE)
		{
			VID_Shutdown();
			Com_Printf ("Can't run in non-RGB mode\n");
			return false;
		}

		WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
		ExWindowStyle = 0;
	}

	rect.top = 0;
	rect.left = 0;
	rect.right = width;
	rect.bottom = height;
	AdjustWindowRectEx(&rect, WindowStyle, false, 0);

	if (fullscreen)
	{
		CenterX = 0;
		CenterY = 0;
	}
	else
	{
		CenterX = (GetSystemMetrics(SM_CXSCREEN) - (rect.right - rect.left)) / 2;
		CenterY = (GetSystemMetrics(SM_CYSCREEN) - (rect.bottom - rect.top)) / 2;
	}
	CenterX = max(0, CenterX);
	CenterY = max(0, CenterY);

	// x and y may be changed by WM_MOVE messages
	window_x = CenterX;
	window_y = CenterY;
	window_width = width;
	window_height = height;
	rect.left += CenterX;
	rect.right += CenterX;
	rect.top += CenterY;
	rect.bottom += CenterY;

	mainwindow = CreateWindowEx (ExWindowStyle, "QuakeWindowClass", "WinQuake", WindowStyle, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, global_hInstance, NULL);
	if (!mainwindow)
	{
		VID_Shutdown();
		Com_Printf("CreateWindowEx(%d, %s, %s, %d, %d, %d, %d, %d, %p, %p, %d, %p) failed\n", ExWindowStyle, "WinQuake", "WinQuake", WindowStyle, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, global_hInstance, NULL);
		return false;
	}

	/*
	if (!fullscreen)
		SetWindowPos (mainwindow, NULL, CenterX, CenterY, 0, 0,SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
	*/

	ShowWindow (mainwindow, SW_SHOWDEFAULT);
	UpdateWindow (mainwindow);

	SendMessage (mainwindow, WM_SETICON, (WPARAM)true, (LPARAM)hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)false, (LPARAM)hIcon);

	VID_UpdateWindowStatus ();

	// now we try to make sure we get the focus on the mode switch, because
	// sometimes in some systems we don't.  We grab the foreground, then
	// finish setting up, pump all our messages, and sleep for a little while
	// to let messages finish bouncing around the system, then we put
	// ourselves at the top of the z order, then grab the foreground again,
	// Who knows if it helps, but it probably doesn't hurt
	SetForegroundWindow (mainwindow);

	while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	Sleep (100);

	SetWindowPos (mainwindow, HWND_TOP, 0, 0, 0, 0, SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOCOPYBITS);

	SetForegroundWindow (mainwindow);

	// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	vid.width = width;
	vid.height = height;
	vid.numpages = 2;
	
	hdc = GetDC(mainwindow);

	if ((pixelformat = ChoosePixelFormat(hdc, &pfd)) == 0)
	{
		VID_Shutdown();
		Com_Printf("ChoosePixelFormat(%d, %p) failed\n", hdc, &pfd);
		return false;
	}

	if (SetPixelFormat(hdc, pixelformat, &pfd) == false)
	{
		VID_Shutdown();
		Com_Printf("SetPixelFormat(%d, %d, %p) failed\n", hdc, pixelformat, &pfd);
		return false;
	}
	
	baseRC = wglCreateContext(hdc);
	if (!baseRC)
	{
		VID_Shutdown();
		Com_Printf("Could not initialize GL (wglCreateContext failed).\n\nMake sure you are in 65536 color mode, and try running -window.\n");
		return false;
	}
	if (!wglMakeCurrent(hdc, baseRC))
	{
		VID_Shutdown();
		Com_Printf("wglMakeCurrent(%d, %d) failed\n", hdc, baseRC);
		return false;
	}

	ReleaseDC(mainwindow, hdc);

	GL_Init ();

	vid_canalttab = true;
	vid_hidden = false;
	vid_initialized = true;

	vid.colormap = host_colormap;

	return true;
}

void VID_Shutdown (void)
{
	HGLRC hRC = 0;
	HDC hDC = 0;

	if (vid_initialized)
	{
		vid_initialized = false;
		if (wglGetCurrentContext)
			hRC = wglGetCurrentContext();
		if (wglGetCurrentDC)
			hDC = wglGetCurrentDC();

		if (wglMakeCurrent)
			wglMakeCurrent(NULL, NULL);

		if (hRC && wglDeleteContext)
			wglDeleteContext(hRC);

		if (hDC && mainwindow)
			ReleaseDC(mainwindow, hDC);

		if (vid_isfullscreen)
			ChangeDisplaySettings (NULL, 0);

		AppActivate(false, false);
	}
}

// in_win.c -- windows 95 mouse and joystick code
// 02/21/97 JCB Added extended DirectInput code to support external controllers.

#define DIRECTINPUT_VERSION 0x0300
#include <dinput.h>

#define DINPUT_BUFFERSIZE           16
#define iDirectInputCreate(a,b,c,d)	pDirectInputCreate(a,b,c,d)

HRESULT (WINAPI *pDirectInputCreate)(HINSTANCE hinst, DWORD dwVersion,
	LPDIRECTINPUT * lplpDirectInput, LPUNKNOWN punkOuter);

// mouse variables
cvar_t	m_filter = {"m_filter", "0"};
cvar_t	in_dinput = {"in_dinput", "1", CVAR_ARCHIVE};

// compatibility with old Quake -- setting to 0 disables KP_* codes
cvar_t	cl_keypad = {"cl_keypad","1"};

static int		mouse_buttons = 0;
static int		mouse_oldbuttonstate = 0;
static POINT	current_pos;
static double	mouse_x = 0.0, mouse_y = 0.0;
static int		old_mouse_x = 0, old_mouse_y = 0;

static qbool	restore_spi = false;
static int		originalmouseparms[3], newmouseparms[3] = {0, 0, 0};
static qbool	mouseparmsvalid = false, mouseactivatetoggle = false;
static qbool	mouseshowtoggle = true;
static qbool	dinput_acquired = false;
static unsigned int		mstate_di = (unsigned int)0;
unsigned int	uiWheelMessage = (unsigned int)0;

qbool			in_mouseactive = false;

// joystick defines and variables
// where should defines be moved?
#define JOY_ABSOLUTE_AXIS	0x00000000		// control like a joystick
#define JOY_RELATIVE_AXIS	0x00000010		// control like a mouse, spinner, trackball
#define	JOY_MAX_AXES		6				// X, Y, Z, R, U, V
#define JOY_AXIS_X			0
#define JOY_AXIS_Y			1
#define JOY_AXIS_Z			2
#define JOY_AXIS_R			3
#define JOY_AXIS_U			4
#define JOY_AXIS_V			5

enum _ControlList
{
	AxisNada = 0, AxisForward, AxisLook, AxisSide, AxisTurn
};

DWORD	dwAxisFlags[JOY_MAX_AXES] =
{
	JOY_RETURNX, JOY_RETURNY, JOY_RETURNZ, JOY_RETURNR, JOY_RETURNU, JOY_RETURNV
};

DWORD	dwAxisMap[JOY_MAX_AXES];
DWORD	dwControlMap[JOY_MAX_AXES];
PDWORD	pdwRawValue[JOY_MAX_AXES];

// none of these cvars are saved over a session
// this means that advanced controller configuration needs to be executed
// each time.  this avoids any problems with getting back to a default usage
// or when changing from one controller to another.  this way at least something
// works.
cvar_t	in_joystick = {"joystick","0",CVAR_ARCHIVE};
cvar_t	joy_name = {"joyname", "joystick"};
cvar_t	joy_advanced = {"joyadvanced", "0"};
cvar_t	joy_advaxisx = {"joyadvaxisx", "0"};
cvar_t	joy_advaxisy = {"joyadvaxisy", "0"};
cvar_t	joy_advaxisz = {"joyadvaxisz", "0"};
cvar_t	joy_advaxisr = {"joyadvaxisr", "0"};
cvar_t	joy_advaxisu = {"joyadvaxisu", "0"};
cvar_t	joy_advaxisv = {"joyadvaxisv", "0"};
cvar_t	joy_forwardthreshold = {"joyforwardthreshold", "0.15"};
cvar_t	joy_sidethreshold = {"joysidethreshold", "0.15"};
cvar_t	joy_pitchthreshold = {"joypitchthreshold", "0.15"};
cvar_t	joy_yawthreshold = {"joyyawthreshold", "0.15"};
cvar_t	joy_forwardsensitivity = {"joyforwardsensitivity", "-1.0"};
cvar_t	joy_sidesensitivity = {"joysidesensitivity", "-1.0"};
cvar_t	joy_pitchsensitivity = {"joypitchsensitivity", "1.0"};
cvar_t	joy_yawsensitivity = {"joyyawsensitivity", "-1.0"};
cvar_t	joy_wwhack1 = {"joywwhack1", "0.0"};
cvar_t	joy_wwhack2 = {"joywwhack2", "0.0"};

static qbool	joy_avail = false, joy_advancedinit = false, joy_haspov = false;
static DWORD	joy_oldbuttonstate, joy_oldpovstate;

static int		joy_id;
static DWORD	joy_flags;
static DWORD	joy_numbuttons;

static LPDIRECTINPUT		g_pdi;
static LPDIRECTINPUTDEVICE	g_pMouse;

static JOYINFOEX	ji;

static HINSTANCE hInstDI;

// Some drivers send DIMOFS_Z, some send WM_MOUSEWHEEL, and some send both.
// To get the mouse wheel to work in any case but avoid duplicate events,
// we will only use one event source, wherever we receive the first event.
int		in_mwheeltype = MWHEEL_UNKNOWN;

typedef struct MYDATA {
	LONG  lX;                   // X axis goes here
	LONG  lY;                   // Y axis goes here
	LONG  lZ;                   // Z axis goes here
	BYTE  bButtonA;             // One button goes here
	BYTE  bButtonB;             // Another button goes here
	BYTE  bButtonC;             // Another button goes here
	BYTE  bButtonD;             // Another button goes here
	BYTE  bButtonE;             // Another button goes here
	BYTE  bButtonF;             // Another button goes here
	BYTE  bButtonG;             // Another button goes here
	BYTE  bButtonH;             // Another button goes here
} MYDATA;


// This structure corresponds to c_dfDIMouse2 in dinput8.lib
// 0x80000000 is something undocumented but must be there, otherwise
// IDirectInputDevice_SetDataFormat may fail.
static DIOBJECTDATAFORMAT rgodf[] = {
  { &GUID_XAxis,    FIELD_OFFSET(MYDATA, lX),       DIDFT_AXIS | DIDFT_ANYINSTANCE,   0,},
  { &GUID_YAxis,    FIELD_OFFSET(MYDATA, lY),       DIDFT_AXIS | DIDFT_ANYINSTANCE,   0,},
  { &GUID_ZAxis,    FIELD_OFFSET(MYDATA, lZ),       0x80000000 | DIDFT_AXIS | DIDFT_ANYINSTANCE,   0,},
  { NULL,           FIELD_OFFSET(MYDATA, bButtonA), DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { NULL,           FIELD_OFFSET(MYDATA, bButtonB), DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { NULL,           FIELD_OFFSET(MYDATA, bButtonC), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { NULL,           FIELD_OFFSET(MYDATA, bButtonD), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { NULL,           FIELD_OFFSET(MYDATA, bButtonE), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { NULL,           FIELD_OFFSET(MYDATA, bButtonF), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { NULL,           FIELD_OFFSET(MYDATA, bButtonG), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { NULL,           FIELD_OFFSET(MYDATA, bButtonH), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
};

#define NUM_OBJECTS (sizeof(rgodf) / sizeof(rgodf[0]))

static DIDATAFORMAT	df = {
	sizeof(DIDATAFORMAT),       // this structure
	sizeof(DIOBJECTDATAFORMAT), // size of object data format
	DIDF_RELAXIS,               // absolute axis coordinates
	sizeof(MYDATA),             // device data size
	NUM_OBJECTS,                // number of objects
	rgodf,                      // and here they are
};

// forward-referenced functions
static void IN_StartupJoystick (void);
static void Joy_AdvancedUpdate_f (void);
static void IN_JoyMove (usercmd_t *cmd);

static void IN_LoadKeys_f (void);


/*
===========
Force_CenterView_f
===========
*/
static void Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
}


/*
===========
IN_ShowMouse
===========
*/
void IN_ShowMouse (void)
{
	if (!mouseshowtoggle)
	{
		ShowCursor (TRUE);
		mouseshowtoggle = 1;
	}
}


/*
===========
IN_HideMouse
===========
*/
void IN_HideMouse (void)
{
	if (mouseshowtoggle)
	{
		ShowCursor (FALSE);
		mouseshowtoggle = 0;
	}
}


/*
===========
IN_ActivateMouse
===========
*/
void IN_ActivateMouse (void)
{
	mouseactivatetoggle = true;

	if (mouseinitialized)
	{
		if (dinput)
		{
			if (g_pMouse)
			{
				if (!dinput_acquired)
				{
					IDirectInputDevice_Acquire(g_pMouse);
					dinput_acquired = true;
				}
			}
			else
			{
				return;
			}
		}
		else
		{
			RECT window_rect;
			window_rect.left = window_x;
			window_rect.top = window_y;
			window_rect.right = window_x + window_width;
			window_rect.bottom = window_y + window_height;

			if (mouseparmsvalid)
				restore_spi = SystemParametersInfo (SPI_SETMOUSE, 0, newmouseparms, 0);

			SetCursorPos (window_center_x, window_center_y);
			SetCapture (mainwindow);
			ClipCursor (&window_rect);
		}

		in_mouseactive = true;
	}
}


/*
===========
IN_SetQuakeMouseState
===========
*/
void IN_SetQuakeMouseState (void)
{
	if (mouseactivatetoggle)
		IN_ActivateMouse ();
}


/*
===========
IN_DeactivateMouse
===========
*/
void IN_DeactivateMouse (void)
{

	mouseactivatetoggle = false;

	if (mouseinitialized)
	{
		if (dinput)
		{
			if (g_pMouse)
			{
				if (dinput_acquired)
				{
					IDirectInputDevice_Unacquire(g_pMouse);
					dinput_acquired = false;
				}
			}
		}
		else
		{
		if (restore_spi)
			SystemParametersInfo (SPI_SETMOUSE, 0, originalmouseparms, 0);

		ClipCursor (NULL);
		ReleaseCapture ();
	}

		in_mouseactive = false;
	}
}


/*
===========
IN_RestoreOriginalMouseState
===========
*/
void IN_RestoreOriginalMouseState (void)
{
	if (mouseactivatetoggle)
	{
		IN_DeactivateMouse ();
		mouseactivatetoggle = true;
	}

// try to redraw the cursor so it gets reinitialized, because sometimes it
// has garbage after the mode switch
	ShowCursor (TRUE);
	ShowCursor (FALSE);
}


/*
===========
IN_InitDInput
===========
*/
static qbool IN_InitDInput (void)
{
    HRESULT		hr;
	DIPROPDWORD	dipdw = {
		{
			sizeof(DIPROPDWORD),        // diph.dwSize
			sizeof(DIPROPHEADER),       // diph.dwHeaderSize
			0,                          // diph.dwObj
			DIPH_DEVICE,                // diph.dwHow
		},
		DINPUT_BUFFERSIZE,              // dwData
	};

	if (!hInstDI)
	{
		hInstDI = LoadLibrary("dinput.dll");

		if (hInstDI == NULL)
		{
			Com_Printf ("Couldn't load dinput.dll\n");
			return false;
		}
	}

	if (!pDirectInputCreate)
	{
		pDirectInputCreate = (HRESULT (WINAPI *)(HINSTANCE, DWORD, LPDIRECTINPUT *, LPUNKNOWN))GetProcAddress(hInstDI,"DirectInputCreateA");

		if (!pDirectInputCreate)
		{
			Com_Printf ("Couldn't get DI proc addr\n");
			return false;
		}
	}

// register with DirectInput and get an IDirectInput to play with.
	hr = iDirectInputCreate(global_hInstance, DIRECTINPUT_VERSION, &g_pdi, NULL);

	if (FAILED(hr))
	{
		Com_Printf ("DirectInputCreate failed\n");
		return false;
	}

// obtain an interface to the system mouse device.
	hr = IDirectInput_CreateDevice(g_pdi, &GUID_SysMouse, &g_pMouse, NULL);

	if (FAILED(hr))
	{
		Com_Printf ("Couldn't open DI mouse device\n");
		return false;
	}

// set the data format to "mouse format".
	hr = IDirectInputDevice_SetDataFormat(g_pMouse, &df);

	if (FAILED(hr))
	{
		// Tonik: haven't tested, but I suppose SetDataFormat(foo, c_dfDIMouse2)
		// may fail if DirectX 7 or higher is not installed
		// So morph c_dfDIMouse2  into c_dfDIMouse and see is that works
		df.dwDataSize = 16;
		df.dwNumObjs = 7;

		hr = IDirectInputDevice_SetDataFormat(g_pMouse, &df);

		if (FAILED(hr))
		{
			Com_Printf ("Couldn't set DI mouse format\n");
			return false;
		}

		// here we could set mouse_buttons to 4 if we cared :)
	}

// set the cooperativity level.
	hr = IDirectInputDevice_SetCooperativeLevel(g_pMouse, mainwindow,
			DISCL_EXCLUSIVE | DISCL_FOREGROUND);

	if (FAILED(hr))
	{
		Com_Printf ("Couldn't set DI coop level\n");
		return false;
	}


// set the buffer size to DINPUT_BUFFERSIZE elements.
// the buffer size is a DWORD property associated with the device
	hr = IDirectInputDevice_SetProperty(g_pMouse, DIPROP_BUFFERSIZE, &dipdw.diph);

	if (FAILED(hr))
	{
		Com_Printf ("Couldn't set DI buffersize\n");
		return false;
	}

	return true;
}


/*
===========
IN_StartupMouse
===========
*/
static void IN_StartupMouse (void)
{
	if ( COM_CheckParm ("-nomouse") )
		return;

	mouseinitialized = true;

	mouse_buttons = 8;

	in_mwheeltype = MWHEEL_UNKNOWN;

	if (in_dinput.value || COM_CheckParm ("-dinput"))
	{
		dinput = IN_InitDInput ();

		if (dinput)
		{
			Com_Printf ("DirectInput initialized\n");
		}
		else
		{
			Com_Printf ("DirectInput not initialized\n");
		}
	}

	if (!dinput)
	{
		mouseparmsvalid = SystemParametersInfo (SPI_GETMOUSE, 0, originalmouseparms, 0);

		if (mouseparmsvalid)
		{
			if ( COM_CheckParm ("-noforcemspd") )
				newmouseparms[2] = originalmouseparms[2];

			if ( COM_CheckParm ("-noforcemaccel") )
			{
				newmouseparms[0] = originalmouseparms[0];
				newmouseparms[1] = originalmouseparms[1];
			}

			if ( COM_CheckParm ("-noforcemparms") )
			{
				newmouseparms[0] = originalmouseparms[0];
				newmouseparms[1] = originalmouseparms[1];
				newmouseparms[2] = originalmouseparms[2];
			}
		}
	}

// if a fullscreen video mode was set before the mouse was initialized,
// set the mouse state appropriately
	if (mouseactivatetoggle)
		IN_ActivateMouse ();
}


/*
===========
IN_Init
===========
*/
void IN_Init (void)
{
	// mouse variables
	Cvar_Register (&m_filter);
	Cvar_Register (&in_dinput);

	// joystick variables
	Cvar_Register (&in_joystick);

	// keyboard variables
	Cvar_Register (&cl_keypad);

	Cmd_AddCommand ("force_centerview", Force_CenterView_f);
	Cmd_AddCommand ("loadkeys", IN_LoadKeys_f);

	uiWheelMessage = RegisterWindowMessage ( "MSWHEEL_ROLLMSG" );

	IN_StartupMouse ();
	IN_StartupJoystick ();
}

/*
===========
IN_Shutdown
===========
*/
void IN_Shutdown (void)
{
	IN_DeactivateMouse ();
	IN_ShowMouse ();

    if (g_pMouse)
	{
		IDirectInputDevice_Release(g_pMouse);
		g_pMouse = NULL;
	}

    if (g_pdi)
	{
		IDirectInput_Release(g_pdi);
		g_pdi = NULL;
	}
}


/*
===========
IN_MouseEvent
===========
*/
void IN_MouseEvent (int mstate)
{
	int		i;

	if (in_mouseactive && !dinput)
	{
	// perform button actions
		for (i=0 ; i<mouse_buttons ; i++)
		{
			if ( (mstate & (1<<i)) &&
				!(mouse_oldbuttonstate & (1<<i)) )
			{
				Key_Event (K_MOUSE1 + i, true);
			}

			if ( !(mstate & (1<<i)) &&
				(mouse_oldbuttonstate & (1<<i)) )
			{
					Key_Event (K_MOUSE1 + i, false);
			}
		}

		mouse_oldbuttonstate = mstate;
	}
}


/*
===========
IN_MouseMove
===========
*/
static void IN_MouseMove (usercmd_t *cmd)
{
	int		mx, my;
	int					i;
	DIDEVICEOBJECTDATA	od;
	DWORD				dwElements;
	HRESULT				hr;

	if (!in_mouseactive)
		return;

	if (dinput)
	{
		mx = 0;
		my = 0;

		for (;;)
		{
			dwElements = 1;

			hr = IDirectInputDevice_GetDeviceData(g_pMouse,
					sizeof(DIDEVICEOBJECTDATA), &od, &dwElements, 0);

			if ((hr == DIERR_INPUTLOST) || (hr == DIERR_NOTACQUIRED))
			{
				dinput_acquired = true;
				IDirectInputDevice_Acquire(g_pMouse);
				break;
			}

			/* Unable to read data or no data available */
			if (FAILED(hr) || dwElements == 0)
			{
				break;
			}

			/* Look at the element to see what happened */

			switch (od.dwOfs)
			{
				case DIMOFS_X:
					mx += od.dwData;
					break;

				case DIMOFS_Y:
					my += od.dwData;
					break;

				case DIMOFS_Z:
					if (in_mwheeltype != MWHEEL_WINDOWMSG)
					{
						in_mwheeltype = MWHEEL_DINPUT;

						if ((int)od.dwData > 0) {
							Key_Event(K_MWHEELUP, true);
							Key_Event(K_MWHEELUP, false);
						} else {
							Key_Event(K_MWHEELDOWN, true);
							Key_Event(K_MWHEELDOWN, false);
						}
					}
					break;

				case DIMOFS_BUTTON0:
				case DIMOFS_BUTTON1:
				case DIMOFS_BUTTON2:
				case DIMOFS_BUTTON3:
				case DIMOFS_BUTTON0 + 4: // DIMOFS_BUTTON4/5/6/7 are only in
				case DIMOFS_BUTTON0 + 5: // in DirectX 7 and higher
				case DIMOFS_BUTTON0 + 6: //
				case DIMOFS_BUTTON0 + 7: //
					if (od.dwData & 0x80)
						mstate_di |= 1 << (od.dwOfs - DIMOFS_BUTTON0);
					else
						mstate_di &= ~(1 << (od.dwOfs - DIMOFS_BUTTON0));
					break;
			}
		}

	// perform button actions
		for (i=0 ; i<mouse_buttons ; i++)
		{
			if ( (mstate_di & (1<<i)) &&
				!(mouse_oldbuttonstate & (1<<i)) )
			{
				Key_Event (K_MOUSE1 + i, true);
			}

			if ( !(mstate_di & (1<<i)) &&
				(mouse_oldbuttonstate & (1<<i)) )
			{
				Key_Event (K_MOUSE1 + i, false);
			}
		}

		mouse_oldbuttonstate = mstate_di;
	}
	else
	{
		GetCursorPos (&current_pos);
		mx = current_pos.x - window_center_x;
		my = current_pos.y - window_center_y;

		// if the mouse has moved, force it to the center, so there's room to move
		if (mx || my)
			SetCursorPos (window_center_x, window_center_y);
	}

	if (m_filter.value)
	{
		mouse_x = (mx + old_mouse_x) * 0.5;
		mouse_y = (my + old_mouse_y) * 0.5;
	}
	else
	{
		mouse_x = mx;
		mouse_y = my;
	}

	old_mouse_x = mx;
	old_mouse_y = my;

	mouse_x *= sensitivity.value;
	mouse_y *= sensitivity.value;

// add mouse X/Y movement to cmd
	if ( (in_strafe.state & 1) || (lookstrafe.value && mlook_active))
		cmd->sidemove += m_side.value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw.value * mouse_x;

	if (mlook_active)
		V_StopPitchDrift ();

	if (mlook_active && !(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch.value * mouse_y;
		if (cl.viewangles[PITCH] > cl.maxpitch)
			cl.viewangles[PITCH] = cl.maxpitch;
		if (cl.viewangles[PITCH] < cl.minpitch)
			cl.viewangles[PITCH] = cl.minpitch;
	}
	else
	{
		cmd->forwardmove -= m_forward.value * mouse_y;
	}
}


/*
===========
IN_Move
===========
*/
void IN_Move (usercmd_t *cmd)
{
	if (vid_activewindow && !vid_hidden)
	{
		IN_MouseMove (cmd);
		IN_JoyMove (cmd);
	}
}


/*
===================
IN_ClearStates
===================
*/
void IN_ClearStates (void)
{
	mouse_oldbuttonstate = 0;
}


/*
===============
IN_StartupJoystick
===============
*/
static void IN_StartupJoystick (void)
{
	int			numdevs;
	JOYCAPS		jc;
	MMRESULT	mmr = JOYERR_NOERROR;

 	// assume no joystick
	joy_avail = false;

	// only initialize if the user wants it
	if (!COM_CheckParm ("-joystick"))
		return;

	Cvar_Register (&joy_name);
	Cvar_Register (&joy_advanced);
	Cvar_Register (&joy_advaxisx);
	Cvar_Register (&joy_advaxisy);
	Cvar_Register (&joy_advaxisz);
	Cvar_Register (&joy_advaxisr);
	Cvar_Register (&joy_advaxisu);
	Cvar_Register (&joy_advaxisv);
	Cvar_Register (&joy_forwardthreshold);
	Cvar_Register (&joy_sidethreshold);
	Cvar_Register (&joy_pitchthreshold);
	Cvar_Register (&joy_yawthreshold);
	Cvar_Register (&joy_forwardsensitivity);
	Cvar_Register (&joy_sidesensitivity);
	Cvar_Register (&joy_pitchsensitivity);
	Cvar_Register (&joy_yawsensitivity);
	Cvar_Register (&joy_wwhack1);
	Cvar_Register (&joy_wwhack2);

	Cmd_AddCommand ("joyadvancedupdate", Joy_AdvancedUpdate_f);

	// verify joystick driver is present
	if ((numdevs = joyGetNumDevs ()) == 0)
	{
		Com_Printf ("\njoystick not found -- driver not present\n\n");
		return;
	}

	// cycle through the joystick ids for the first valid one
	for (joy_id=0 ; joy_id<numdevs ; joy_id++)
	{
		memset (&ji, 0, sizeof(ji));
		ji.dwSize = sizeof(ji);
		ji.dwFlags = JOY_RETURNCENTERED;

		if ((mmr = joyGetPosEx (joy_id, &ji)) == JOYERR_NOERROR)
			break;
	}

	// abort startup if we didn't find a valid joystick
	if (mmr != JOYERR_NOERROR)
	{
		Com_DPrintf ("\njoystick not found -- no valid joysticks (%x)\n\n", mmr);
		return;
	}

	// get the capabilities of the selected joystick
	// abort startup if command fails
	memset (&jc, 0, sizeof(jc));
	if ((mmr = joyGetDevCaps (joy_id, &jc, sizeof(jc))) != JOYERR_NOERROR)
	{
		Com_Printf ("\njoystick not found -- invalid joystick capabilities (%x)\n\n", mmr);
		return;
	}

	// save the joystick's number of buttons and POV status
	joy_numbuttons = jc.wNumButtons;
	joy_haspov = jc.wCaps & JOYCAPS_HASPOV;

	// old button and POV states default to no buttons pressed
	joy_oldbuttonstate = joy_oldpovstate = 0;

	// mark the joystick as available and advanced initialization not completed
	// this is needed as cvars are not available during initialization

	joy_avail = true;
	joy_advancedinit = false;

	Com_Printf ("\njoystick detected\n\n");
}


/*
===========
RawValuePointer
===========
*/
static PDWORD RawValuePointer (int axis)
{
	switch (axis)
	{
	case JOY_AXIS_X:
		return &ji.dwXpos;
	case JOY_AXIS_Y:
		return &ji.dwYpos;
	case JOY_AXIS_Z:
		return &ji.dwZpos;
	case JOY_AXIS_R:
		return &ji.dwRpos;
	case JOY_AXIS_U:
		return &ji.dwUpos;
	case JOY_AXIS_V:
		return &ji.dwVpos;
	}
	return NULL;	// shut up compiler
}


/*
===========
Joy_AdvancedUpdate_f
===========
*/
static void Joy_AdvancedUpdate_f (void)
{

	// called once by IN_ReadJoystick and by user whenever an update is needed
	// cvars are now available
	int	i;
	DWORD dwTemp;

	// initialize all the maps
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		dwAxisMap[i] = AxisNada;
		dwControlMap[i] = JOY_ABSOLUTE_AXIS;
		pdwRawValue[i] = RawValuePointer(i);
	}

	if( joy_advanced.value == 0.0)
	{
		// default joystick initialization
		// 2 axes only with joystick control
		dwAxisMap[JOY_AXIS_X] = AxisTurn;
		// dwControlMap[JOY_AXIS_X] = JOY_ABSOLUTE_AXIS;
		dwAxisMap[JOY_AXIS_Y] = AxisForward;
		// dwControlMap[JOY_AXIS_Y] = JOY_ABSOLUTE_AXIS;
	}
	else
	{
		if (strcmp (joy_name.string, "joystick") != 0)
		{
			// notify user of advanced controller
			Com_Printf ("\n%s configured\n\n", joy_name.string);
		}

		// advanced initialization here
		// data supplied by user via joy_axisn cvars
		dwTemp = (DWORD) joy_advaxisx.value;
		dwAxisMap[JOY_AXIS_X] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_X] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisy.value;
		dwAxisMap[JOY_AXIS_Y] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Y] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisz.value;
		dwAxisMap[JOY_AXIS_Z] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Z] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisr.value;
		dwAxisMap[JOY_AXIS_R] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_R] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisu.value;
		dwAxisMap[JOY_AXIS_U] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_U] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisv.value;
		dwAxisMap[JOY_AXIS_V] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_V] = dwTemp & JOY_RELATIVE_AXIS;
	}

	// compute the axes to collect from DirectInput
	joy_flags = JOY_RETURNCENTERED | JOY_RETURNBUTTONS | JOY_RETURNPOV;
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		if (dwAxisMap[i] != AxisNada)
		{
			joy_flags |= dwAxisFlags[i];
		}
	}
}


/*
===========
IN_Commands
===========
*/
void IN_Commands (void)
{
	int		i, key_index;
	DWORD	buttonstate, povstate;

	if (!joy_avail)
		return;

	// loop through the joystick buttons
	// key a joystick event or auxillary event for higher number buttons for each state change
	buttonstate = ji.dwButtons;
	for (i=0 ; i < joy_numbuttons ; i++)
	{
		if ( (buttonstate & (1<<i)) && !(joy_oldbuttonstate & (1<<i)) )
		{
			key_index = (i < 4) ? K_JOY1 : K_AUX1;
			Key_Event (key_index + i, true);
		}

		if ( !(buttonstate & (1<<i)) && (joy_oldbuttonstate & (1<<i)) )
		{
			key_index = (i < 4) ? K_JOY1 : K_AUX1;
			Key_Event (key_index + i, false);
		}
	}
	joy_oldbuttonstate = buttonstate;

	if (joy_haspov)
	{
		// convert POV information into 4 bits of state information
		// this avoids any potential problems related to moving from one
		// direction to another without going through the center position
		povstate = 0;
		if(ji.dwPOV != JOY_POVCENTERED)
		{
			if (ji.dwPOV == JOY_POVFORWARD)
				povstate |= 0x01;
			if (ji.dwPOV == JOY_POVRIGHT)
				povstate |= 0x02;
			if (ji.dwPOV == JOY_POVBACKWARD)
				povstate |= 0x04;
			if (ji.dwPOV == JOY_POVLEFT)
				povstate |= 0x08;
		}
		// determine which bits have changed and key an auxillary event for each change
		for (i=0 ; i < 4 ; i++)
		{
			if ( (povstate & (1<<i)) && !(joy_oldpovstate & (1<<i)) )
			{
				Key_Event (K_AUX29 + i, true);
			}

			if ( !(povstate & (1<<i)) && (joy_oldpovstate & (1<<i)) )
			{
				Key_Event (K_AUX29 + i, false);
			}
		}
		joy_oldpovstate = povstate;
	}
}


/*
===============
IN_ReadJoystick
===============
*/
static qbool IN_ReadJoystick (void)
{
	memset (&ji, 0, sizeof(ji));
	ji.dwSize = sizeof(ji);
	ji.dwFlags = joy_flags;

	if (joyGetPosEx (joy_id, &ji) == JOYERR_NOERROR)
	{
		// this is a hack -- there is a bug in the Logitech WingMan Warrior DirectInput Driver
		// rather than having 32768 be the zero point, they have the zero point at 32668
		// go figure -- anyway, now we get the full resolution out of the device
		if (joy_wwhack1.value != 0.0)
		{
			ji.dwUpos += 100;
		}
		return true;
	}
	else
	{
		// read error occurred
		// turning off the joystick seems too harsh for 1 read error,\
		// but what should be done?
		// Com_Printf ("IN_ReadJoystick: no response\n");
		// joy_avail = false;
		return false;
	}
}


/*
===========
IN_JoyMove
===========
*/
static void IN_JoyMove (usercmd_t *cmd)
{
	float	speed, aspeed;
	float	fAxisValue, fTemp;
	int		i;

	// complete initialization if first time in
	// this is needed as cvars are not available at initialization time
	if( joy_advancedinit != true )
	{
		Joy_AdvancedUpdate_f();
		joy_advancedinit = true;
	}

	// verify joystick is available and that the user wants to use it
	if (!joy_avail || !in_joystick.value)
	{
		return;
	}

	// collect the joystick data, if possible
	if (IN_ReadJoystick () != true)
	{
		return;
	}

	if (in_speed.state & 1)
		speed = cl_movespeedkey.value;
	else
		speed = 1;
	aspeed = speed * cls.frametime;

	// loop through the axes
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		// get the floating point zero-centered, potentially-inverted data for the current axis
		fAxisValue = (float) *pdwRawValue[i];
		// move centerpoint to zero
		fAxisValue -= 32768.0;

		if (joy_wwhack2.value != 0.0)
		{
			if (dwAxisMap[i] == AxisTurn)
			{
				// this is a special formula for the Logitech WingMan Warrior
				// y=ax^b; where a = 300 and b = 1.3
				// also x values are in increments of 800 (so this is factored out)
				// then bounds check result to level out excessively high spin rates
				fTemp = 300.0 * pow(abs(fAxisValue) / 800.0, 1.3);
				if (fTemp > 14000.0)
					fTemp = 14000.0;
				// restore direction information
				fAxisValue = (fAxisValue > 0.0) ? fTemp : -fTemp;
			}
		}

		// convert range from -32768..32767 to -1..1
		fAxisValue /= 32768.0;

		switch (dwAxisMap[i])
		{
		case AxisForward:
			if ((joy_advanced.value == 0.0) && mlook_active)
			{
				// user wants forward control to become look control
				if (fabs(fAxisValue) > joy_pitchthreshold.value)
				{
					// if mouse invert is on, invert the joystick pitch value
					// only absolute control support here (joy_advanced is false)
					if (m_pitch.value < 0.0)
					{
						cl.viewangles[PITCH] -= (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					}
					else
					{
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					}
					V_StopPitchDrift();
				}
				else
				{
					// no pitch movement
					// disable pitch return-to-center unless requested by user
					// *** this code can be removed when the lookspring bug is fixed
					// *** the bug always has the lookspring feature on
					if(lookspring.value == 0.0)
						V_StopPitchDrift();
				}
			}
			else
			{
				// user wants forward control to be forward control
				if (fabs(fAxisValue) > joy_forwardthreshold.value)
				{
					cmd->forwardmove += (fAxisValue * joy_forwardsensitivity.value) * speed * cl_forwardspeed.value;
				}
			}
			break;

		case AxisSide:
			if (fabs(fAxisValue) > joy_sidethreshold.value)
			{
				cmd->sidemove += (fAxisValue * joy_sidesensitivity.value) * speed * cl_sidespeed.value;
			}
			break;

		case AxisTurn:
			if ((in_strafe.state & 1) || (lookstrafe.value && mlook_active))
			{
				// user wants turn control to become side control
				if (fabs(fAxisValue) > joy_sidethreshold.value)
				{
					cmd->sidemove -= (fAxisValue * joy_sidesensitivity.value) * speed * cl_sidespeed.value;
				}
			}
			else
			{
				// user wants turn control to be turn control
				if (fabs(fAxisValue) > joy_yawthreshold.value)
				{
					if(dwControlMap[i] == JOY_ABSOLUTE_AXIS)
					{
						cl.viewangles[YAW] += (fAxisValue * joy_yawsensitivity.value) * aspeed * cl_yawspeed.value;
					}
					else
					{
						cl.viewangles[YAW] += (fAxisValue * joy_yawsensitivity.value) * speed * 180.0;
					}

				}
			}
			break;

		case AxisLook:
			if (mlook_active)
			{
				if (fabs(fAxisValue) > joy_pitchthreshold.value)
				{
					// pitch movement detected and pitch movement desired by user
					if(dwControlMap[i] == JOY_ABSOLUTE_AXIS)
					{
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					}
					else
					{
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity.value) * speed * 180.0;
					}
					V_StopPitchDrift();
				}
				else
				{
					// no pitch movement
					// disable pitch return-to-center unless requested by user
					// *** this code can be removed when the lookspring bug is fixed
					// *** the bug always has the lookspring feature on
					if(lookspring.value == 0.0)
						V_StopPitchDrift();
				}
			}
			break;

		default:
			break;
		}
	}

	// bounds check pitch
	if (cl.viewangles[PITCH] > cl.maxpitch)
		cl.viewangles[PITCH] = cl.maxpitch;
	if (cl.viewangles[PITCH] < cl.minpitch)
		cl.viewangles[PITCH] = cl.minpitch;
}

//==========================================================================

// builtin keymap
static byte scantokey[128] =
{
//  0       1        2       3       4       5       6       7
//  8       9        A       B       C       D       E       F
	0  ,   K_ESCAPE,'1',    '2',    '3',    '4',    '5',    '6',
	'7',    '8',    '9',    '0',    '-',    '=',    K_BACKSPACE, 9,   // 0
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
	'o',    'p',    '[',    ']',    K_ENTER,K_LCTRL,'a',    's',      // 1
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';',
	'\'',   '`',    K_LSHIFT,'\\',   'z',    'x',    'c',    'v',      // 2
	'b',    'n',    'm',    ',',    '.',    '/',    K_RSHIFT,KP_STAR,
	K_LALT, ' ',  K_CAPSLOCK,K_F1,  K_F2,   K_F3,   K_F4,   K_F5,     // 3
	K_F6,   K_F7,   K_F8,   K_F9,   K_F10,  K_PAUSE,K_SCROLLLOCK,K_HOME,
	K_UPARROW,K_PGUP,KP_MINUS,K_LEFTARROW,KP_5,K_RIGHTARROW,KP_PLUS,K_END, // 4
	K_DOWNARROW,K_PGDN,K_INS,K_DEL, 0,      0,      0,      K_F11,
	K_F12,  0,      0,      0,      0,      0,      0,      0,        // 5
	0,      0,      0,      0,      0,      0,      0,      0,
	0,      0,      0,      0,      0,      0,      0,      0,
	0,      0,      0,      0,      0,      0,      0,      0,
	0,      0,      0,      0,      0,      0,      0,      0
};

// user-defined keymap
byte	keymap[256];	// 128-255 are extended scancodes
byte	shiftkeymap[256];	// generated when shift is pressed
byte	altgrkeymap[256];	// generated when Alt-GR is pressed
qbool	keymap_active = false;

extern void Key_EventEx (int key, int shiftkey, qbool down);

/*
=======
IN_TranslateKeyEvent

Map from windows to quake keynums and generate Key_EventEx
=======
*/
void IN_TranslateKeyEvent (int lKeyData, qbool down)
{
	int		extended, scancode, key, shiftkey;

	extended = (lKeyData >> 24) & 1;

	scancode = (lKeyData>>16)&255;
	if (scancode > 127)
		return;

	if (keymap_active)
	{
		key = keymap[scancode + (extended ? 128 : 0)];
		if (keydown[K_ALTGR])
			shiftkey = altgrkeymap[scancode + (extended ? 128 : 0)];
		else if (keydown[K_SHIFT])
			shiftkey = shiftkeymap[scancode + (extended ? 128 : 0)];
		else
			shiftkey = key;
		Key_EventEx (key, shiftkey, down);
		return;
	}

	key = scantokey[scancode];

	if (cl_keypad.value) {
		if (extended) {
			switch (key) {
				case K_ENTER:		key = KP_ENTER; break;
				case '/':			key = KP_SLASH; break;
				case K_PAUSE:		key = KP_NUMLOCK; break;
				case K_LALT:		key = K_RALT; break;
				case K_LCTRL:		key = K_RCTRL; break;
			}
		} else {
			switch (key) {
				case K_HOME:		key = KP_HOME; break;
				case K_UPARROW:		key = KP_UPARROW; break;
				case K_PGUP:		key = KP_PGUP; break;
				case K_LEFTARROW:	key = KP_LEFTARROW; break;
				case K_RIGHTARROW:	key = KP_RIGHTARROW; break;
				case K_END:			key = KP_END; break;
				case K_DOWNARROW:	key = KP_DOWNARROW; break;
				case K_PGDN:		key = KP_PGDN; break;
				case K_INS:			key = KP_INS; break;
				case K_DEL:			key = KP_DEL; break;
			}
		}
	} else {
		// cl_keypad 0, compatibility mode
		switch (key) {
			case KP_STAR:	key = '*'; break;
			case KP_MINUS:	key = '-'; break;
			case KP_5:		key = '5'; break;
			case KP_PLUS:	key = '+'; break;
		}
	}

	Key_Event (key, down);
}

/*
===========
IN_LoadKeys_f

Load a custom keymap from file
The format is like this:
keymap_name "Default"
keycode     27 ] }
keycode     28 ENTER
keycode ext 28 KP_ENTER
keycode     40 ' #34
===========
*/
static void IN_LoadKeys_f (void)
{
	int n, keynum, count, cmd_shift;
	qbool ext;
	char *data;
	char filename[MAX_QPATH];
	char layout[64] = "custom";

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("loadkeys <filename> : load keymap from file\n"
					"loadkeys -d : restore default keymap\n");
		return;
	}

	if (!strcmp(Cmd_Argv(1), "-d"))
	{
		keymap_active = false;
		return;
	}

	strlcpy (filename, Cmd_Argv(1), sizeof(filename) - 5);
	COM_DefaultExtension (filename, ".kmap");

	// check if the given file can be found in subdirectory "keymaps":
	data = FS_LoadTempFile (va("keymaps/%s", filename));

	// if not found, recheck in main directory:
	if (!data)
	  data = FS_LoadTempFile (filename);
	if (!data)
	{
		Com_Printf ("Couldn't load %s\n", filename);
		return;
	}

	keymap_active = false;

	count = 0;
	data = strtok (data, "\r\n");
	for ( ; data; data = strtok (NULL, "\r\n"))
	{
		if (strlen(data) >= 256)	// sanity check
			return;

		Cmd_TokenizeString (data);

		if (!Cmd_Argc())
			continue;

		ext = false;
		cmd_shift = 0;

		if ((!Q_stricmp(Cmd_Argv(0), "keymap_name") || !Q_stricmp(Cmd_Argv(0), "layout")) && Cmd_Argc() > 1)
		{
			strlcpy (layout, Cmd_Argv(1), sizeof(layout));
			continue;
		}

#if 0
		if (!Q_stricmp(Cmd_Argv(0), "keymap_version")
		{
			// do something here
			continue;
		}
#endif

		if (!Q_stricmp(Cmd_Argv(0), "ext")) {
			ext = true;
			cmd_shift++;
		}

		if (Q_stricmp(Cmd_Argv(cmd_shift), "keycode"))
			continue;	// any unrecognized keywords are silently ignored (FIXME?)

		cmd_shift++;

		if (!Q_stricmp(Cmd_Argv(cmd_shift), "ext")) {
			ext = true;
			cmd_shift++;
		}

		n = Q_atoi(Cmd_Argv(cmd_shift));

		if (n <= 0 || n > 128)
		{
			Com_Printf ("\"%s\" is not a valid scan code\n", Cmd_Argv(cmd_shift));
			continue;
		}

		keynum = Key_StringToKeynum(Cmd_Argv(cmd_shift + 1));
		if (keynum > 0) {
			keymap[ext ? n + 128 : n] = keynum;
			count++;

			if (Cmd_Argc() > cmd_shift + 2) {
				// user defined shifted key
				keynum = Key_StringToKeynum(Cmd_Argv(cmd_shift + 2));
				if (keynum > 0)
				{
					shiftkeymap[ext ? n + 128 : n] = keynum;
					count++;
				}
				else
					Com_Printf ("\"%s\" is not a valid key\n", Cmd_Argv(cmd_shift + 2));
			}
			else {
				if ( islower(keynum) )
					keynum += 'A' - 'a';	// convert to upper case
				shiftkeymap[ext ? n + 128 : n] = keynum;
			}

			// Massa - third level of keys:
			if (Cmd_Argc() > cmd_shift + 3) {
					// user defined altgr key
					keynum = Key_StringToKeynum(Cmd_Argv(cmd_shift + 3));
					if (keynum > 0) {
							altgrkeymap[ext ? n + 128 : n] = keynum;
							count++;
					}
					else
						Com_Printf ("\"%s\" is not a valid key\n", Cmd_Argv(cmd_shift + 3));
			}
			else {
				// if not given, use the same value as for the shifted key
				altgrkeymap[ext ? n + 128 : n] = keymap[ext ? n + 128 : n];
			}
		}
		else
			Com_Printf ("\"%s\" is not a valid key\n", Cmd_Argv(cmd_shift + 1));
	}

	if (!count) {
		Com_Printf ("No keycodes loaded -- not a valid .kmap file?\n");
		return;
	}

	// some keys are hard-wired
	keymap[1] = K_ESCAPE;
	keymap[28] = K_ENTER;
	keymap[72 + 128] = K_UPARROW;
	keymap[80 + 128] = K_DOWNARROW;
	keymap[75 + 128] = K_LEFTARROW;
	keymap[77 + 128] = K_RIGHTARROW;

	keymap_active = true;

	if (cl_warncmd.value || developer.value)
		Com_Printf ("%s keymap loaded\n", layout);
}
