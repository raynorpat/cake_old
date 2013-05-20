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
#include "resource.h"
#include "sound.h"
#include "winquake.h"

#define DIRECTINPUT_VERSION 0x0300
#include <dinput.h>

#define DINPUT_BUFFERSIZE           16
#define iDirectInputCreate(a,b,c,d)	pDirectInputCreate(a,b,c,d)

HRESULT (WINAPI *pDirectInputCreate)(HINSTANCE hinst, DWORD dwVersion,
	LPDIRECTINPUT * lplpDirectInput, LPUNKNOWN punkOuter);

static int (WINAPI *qwglChoosePixelFormat)(HDC, CONST PIXELFORMATDESCRIPTOR *);
static int (WINAPI *qwglDescribePixelFormat)(HDC, int, UINT, LPPIXELFORMATDESCRIPTOR);
//static int (WINAPI *qwglGetPixelFormat)(HDC);
static BOOL (WINAPI *qwglSetPixelFormat)(HDC, int, CONST PIXELFORMATDESCRIPTOR *);
static BOOL (WINAPI *qwglSwapBuffers)(HDC);
static HGLRC (WINAPI *qwglCreateContext)(HDC);
static BOOL (WINAPI *qwglDeleteContext)(HGLRC);
static HGLRC (WINAPI *qwglGetCurrentContext)(VOID);
static HDC (WINAPI *qwglGetCurrentDC)(VOID);
static PROC (WINAPI *qwglGetProcAddress)(LPCSTR);
static BOOL (WINAPI *qwglMakeCurrent)(HDC, HGLRC);
static BOOL (WINAPI *qwglSwapIntervalEXT)(int interval);
static const char *(WINAPI *qwglGetExtensionsStringARB)(HDC hdc);

static dllfunction_t wglfuncs[] =
{
	{"wglChoosePixelFormat", (void **) &qwglChoosePixelFormat},
	{"wglDescribePixelFormat", (void **) &qwglDescribePixelFormat},
//	{"wglGetPixelFormat", (void **) &qwglGetPixelFormat},
	{"wglSetPixelFormat", (void **) &qwglSetPixelFormat},
	{"wglSwapBuffers", (void **) &qwglSwapBuffers},
	{"wglCreateContext", (void **) &qwglCreateContext},
	{"wglDeleteContext", (void **) &qwglDeleteContext},
	{"wglGetProcAddress", (void **) &qwglGetProcAddress},
	{"wglMakeCurrent", (void **) &qwglMakeCurrent},
	{"wglGetCurrentContext", (void **) &qwglGetCurrentContext},
	{"wglGetCurrentDC", (void **) &qwglGetCurrentDC},
	{NULL, NULL}
};

static dllfunction_t wglswapintervalfuncs[] =
{
	{"wglSwapIntervalEXT", (void **) &qwglSwapIntervalEXT},
	{NULL, NULL}
};

static DEVMODE	gdevmode;
static qbool	vid_initialized = false;
static qbool	vid_wassuspended = false;
static qbool 	vid_usingmouse = false;
static qbool 	vid_usingvsync = false;
static qbool 	vid_usemouse = false;
static qbool	vid_usevsync = false;
static HICON	hIcon;

// used by cd_win.c and snd_win.c
HWND			mainwindow;

static HDC		baseDC;
static HGLRC	baseRC;

static int vid_isfullscreen;

//====================================

int window_x, window_y, window_width, window_height;
int window_center_x, window_center_y;
unsigned int uiWheelMessage = (unsigned int)0;

static void IN_Activate (qbool grab);

qbool			mouseactive;
qbool			mouseinitialized;
static qbool	dinput;

static qbool	dinput_acquired = false;
static int		mouse_buttons = 0;
static int		mouse_oldbuttonstate = 0;
static POINT	current_pos;
static int		mouse_x = 0, mouse_y = 0, old_mouse_x = 0, old_mouse_y = 0;

static qbool	restore_spi = false;
static int		originalmouseparms[3], newmouseparms[3] = {0, 0, 0};
static qbool	mouseparmsvalid = false;
static unsigned int		mstate_di = (unsigned int)0;

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

cvar_t	m_filter = {"m_filter", "0"};
cvar_t	in_dinput = {"in_dinput", "1", CVAR_ARCHIVE};
cvar_t	in_joystick = {"joystick","0", CVAR_ARCHIVE};

// compatibility with old Quake -- setting to 0 disables KP_* codes
cvar_t	cl_keypad = {"cl_keypad","1"};

// none of these cvars are saved over a session
// this means that advanced controller configuration needs to be executed
// each time.  this avoids any problems with getting back to a default usage
// or when changing from one controller to another.  this way at least something
// works.
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
static void Joy_AdvancedUpdate_f (void);
static void IN_JoyMove (usercmd_t *cmd);
static void IN_StartupMouse (void);
static void IN_StartupJoystick (void);
static void IN_LoadKeys_f (void);

//====================================

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

	if (mouseinitialized && mouseactive && !dinput)
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

void VID_Finish (void)
{
	static int old_vsync = -1;

	if (old_vsync != vid_vsync.value) {
		int interval = 0;
		old_vsync = bound(-1, vid_vsync.value, 2);
		Cvar_SetValue(&vid_vsync, old_vsync);

		if ( old_vsync == 1 )
			interval = ( gl_videosynccontroltearavailable ) ? -1 : 1;
		else if ( old_vsync == 2 )
			interval = 1;

		if( gl_videosyncavailable && qwglSwapIntervalEXT )
			qwglSwapIntervalEXT ( interval );
	}

	// handle the mouse state when windowed if that's changed
	vid_usemouse = false;
	if (vid_mouse.value && key_dest == key_game)
		vid_usemouse = true;
	if (vid_isfullscreen)
		vid_usemouse = true;
	if (!vid_activewindow)
		vid_usemouse = false;
	IN_Activate(vid_usemouse);

	if (!vid_hidden)
	{
		if (gl_finish.value)
			RB_Finish();
		SwapBuffers(baseDC);
	}
}


int VID_SetGamma(unsigned short *ramps)
{
	HDC hdc = GetDC (NULL);
	int i = SetDeviceGammaRamp(hdc, ramps);
	ReleaseDC (NULL, hdc);
	return i; // return success or failure
}

int VID_GetGamma(unsigned short *ramps)
{
	HDC hdc = GetDC (NULL);
	int i = GetDeviceGammaRamp(hdc, ramps);
	ReleaseDC (NULL, hdc);
	return i; // return success or failure
}

static HINSTANCE gldll;

int GL_OpenLibrary(const char *name)
{
	Com_Printf("Loading GL driver %s\n", name);
	GL_CloseLibrary();
	if (!(gldll = LoadLibrary(name)))
	{
		Com_Printf("Unable to LoadLibrary %s\n", name);
		return false;
	}
	strcpy(gl_driver, name);
	return true;
}

void GL_CloseLibrary(void)
{
	if (gldll)
	{
		FreeLibrary(gldll);
		gldll = 0;
		gl_driver[0] = 0;
		qwglGetProcAddress = NULL;
		gl_extensions = "";
		gl_platform = "";
		gl_platformextensions = "";
	}
}

void *GL_GetProcAddress(const char *name)
{
	if (gldll)
	{
		void *p = NULL;
		if (qwglGetProcAddress != NULL)
			p = (void *) qwglGetProcAddress(name);
		if (p == NULL)
			p = (void *) GetProcAddress(gldll, name);
		return p;
	}
	else
		return NULL;
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
	extern qbool keydown[256];
	int		i;

// send an up event for each key, to make sure the server clears them all
	for (i=0 ; i<256 ; i++)
	{
		if (keydown[i])
			Key_Event (i, false);
	}

	Key_ClearStates ();
	if (vid_usingmouse)
		mouse_oldbuttonstate = 0;
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
		vid_allowhwgamma = true;
		if (vid_isfullscreen)
		{
			if (vid_wassuspended)
			{
				vid_wassuspended = false;
				ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN);
				ShowWindow (mainwindow, SW_SHOWNORMAL);
			}

			// Fix for alt-tab bug in NVidia drivers
			MoveWindow (mainwindow, 0, 0, gdevmode.dmPelsWidth, gdevmode.dmPelsHeight, false);
		}
	}

	if (!fActive)
	{
		vid_allowhwgamma = false;
		IN_Activate (false);
		if (vid_isfullscreen)
		{
			ChangeDisplaySettings (NULL, 0);
			vid_wassuspended = true;
		}
		VID_RestoreSystemGamma();
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

		case WM_SYSCOMMAND:
			// prevent screensaver from occuring while the active window
			// note: password-locked screensavers on Vista+ still work
			if (vid_activewindow && ((wParam & 0xFFF0) == SC_SCREENSAVE || (wParam & 0xFFF0) == SC_MONITORPOWER))
				lRet = 0;
			else
				lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
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
			
			if (vid_usingmouse && !dinput_acquired)
			{
				// perform button actions
				int i;
				for (i=0 ; i<mouse_buttons ; i++)
					if ((temp ^ mouse_oldbuttonstate) & (1<<i))
						Key_Event (K_MOUSE1 + i, (temp & (1<<i)) != 0);
				mouse_oldbuttonstate = temp;
			}

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

//	    case WM_DESTROY:
//          PostQuitMessage (0);
//	        break;

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

/*
===================
VID_Init
===================
*/
static void IN_Init(void);
void VID_Init (void)
{
	WNDCLASS wc;

	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_APPICON));

	// Register the frame class
	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC)MainWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = global_hInstance;
	wc.hIcon         = hIcon;
	wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName  = 0;
	wc.lpszClassName = "QuakeWindowClass";

	if (!RegisterClass (&wc))
		Sys_Error("Couldn't register window class\n");

	IN_Init();
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
	int CenterX, CenterY;
	const char *gldrivername;

	if (vid_initialized)
		Sys_Error("VID_InitMode called when video is already initialised\n");

	gldrivername = "opengl32.dll";
	i = COM_CheckParm("-gl_driver");
	if (i && i < com_argc - 1)
		gldrivername = com_argv[i + 1];
	if (!GL_OpenLibrary(gldrivername))
	{
		Com_Printf("Unable to load GL driver %s\n", gldrivername);
		return false;
	}

	memset(&gdevmode, 0, sizeof(gdevmode));

	if (hwnd_dialog)
	{
		DestroyWindow (hwnd_dialog);
		hwnd_dialog = NULL;
	}

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
		ExWindowStyle = WS_EX_TOPMOST;
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
		Com_Printf("CreateWindowEx(%d, %s, %s, %d, %d, %d, %d, %d, %p, %p, %d, %p) failed\n", ExWindowStyle, "QuakeWindowClass", "WinQuake", WindowStyle, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, global_hInstance, NULL);
		VID_Shutdown();
		return false;
	}

	ShowWindow (mainwindow, SW_SHOWDEFAULT);
	UpdateWindow (mainwindow);

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
	
	baseDC = GetDC(mainwindow);

	if ((pixelformat = ChoosePixelFormat(baseDC, &pfd)) == 0)
	{
		VID_Shutdown();
		Com_Printf("ChoosePixelFormat(%d, %p) failed\n", baseDC, &pfd);
		return false;
	}

	if (SetPixelFormat(baseDC, pixelformat, &pfd) == false)
	{
		VID_Shutdown();
		Com_Printf("SetPixelFormat(%d, %d, %p) failed\n", baseDC, pixelformat, &pfd);
		return false;
	}

	if (!GL_CheckExtension("wgl", wglfuncs, NULL, false))
	{
		VID_Shutdown();
		Con_Print("wgl functions not found\n");
		return false;
	}
	
	baseRC = qwglCreateContext(baseDC);
	if (!baseRC)
	{
		VID_Shutdown();
		Com_Printf("Could not initialize GL (wglCreateContext failed).\n\nMake sure you are in 65536 color mode, and try running -window.\n");
		return false;
	}
	if (!qwglMakeCurrent(baseDC, baseRC))
	{
		VID_Shutdown();
		Com_Printf("wglMakeCurrent(%d, %d) failed\n", baseDC, baseRC);
		return false;
	}

	if ((qglGetString = (const GLubyte* (GLAPIENTRY *)(GLenum name))GL_GetProcAddress("glGetString")) == NULL)
	{
		VID_Shutdown();
		Com_Printf("glGetString not found\n");
		return false;
	}
	if ((qwglGetExtensionsStringARB = (const char *(WINAPI *)(HDC hdc))GL_GetProcAddress("wglGetExtensionsStringARB")) == NULL)
		Com_Printf("wglGetExtensionsStringARB not found\n");

	gl_renderer = (const char *)qglGetString(GL_RENDERER);
	gl_vendor = (const char *)qglGetString(GL_VENDOR);
	gl_version = (const char *)qglGetString(GL_VERSION);
	gl_extensions = (const char *)qglGetString(GL_EXTENSIONS);
	gl_platform = "WGL";
	gl_platformextensions = "";

	if (qwglGetExtensionsStringARB)
		gl_platformextensions = (const char *)qwglGetExtensionsStringARB(hdc);

	if (!gl_extensions)
		gl_extensions = "";
	if (!gl_platformextensions)
		gl_platformextensions = "";

	gl_videosyncavailable = GL_CheckExtension("WGL_EXT_swap_control", wglswapintervalfuncs, NULL, false);
	gl_videosynccontroltearavailable = GL_CheckExtension("WGL_EXT_swap_control_tear", NULL, NULL, false);

	VID_CheckExtensions();

	vid_usingmouse = false;
	vid_usingvsync = false;
	vid_hidden = false;
	vid_initialized = true;

	vid.colormap = host_colormap;

	IN_StartupMouse ();
	IN_StartupJoystick ();

	return true;
}

static void IN_Shutdown(void);
void VID_Shutdown (void)
{
	if(!vid_initialized)
		return;

	VID_RestoreSystemGamma();

	vid_initialized = false;
	IN_Shutdown();
	if (qwglMakeCurrent)
		qwglMakeCurrent(NULL, NULL);

	if (baseRC && qwglDeleteContext)
		qwglDeleteContext(baseRC);

	// close the library before we get rid of the window
	GL_CloseLibrary();

	if (baseDC && mainwindow)
		ReleaseDC(mainwindow, baseDC);

	AppActivate(false, false);

	if (mainwindow)
		DestroyWindow(mainwindow);
	mainwindow = 0;

	if (vid_isfullscreen)
		ChangeDisplaySettings (NULL, 0);
	vid_isfullscreen = false;
}

static void IN_Activate (qbool grab)
{
	if (!mouseinitialized)
		return;

	if (grab)
	{
		if (!vid_usingmouse)
		{
			vid_usingmouse = true;
			if (dinput && g_pMouse)
			{
				IDirectInputDevice_Acquire(g_pMouse);
				dinput_acquired = true;
			}
			else
			{
				RECT window_rect;
				window_rect.left = window_x;
				window_rect.top = window_y;
				window_rect.right = window_x + vid.width;
				window_rect.bottom = window_y + vid.height;
				if (mouseparmsvalid)
					restore_spi = SystemParametersInfo (SPI_SETMOUSE, 0, newmouseparms, 0);
				SetCursorPos ((window_x + vid.width / 2), (window_y + vid.height / 2));
				SetCapture (mainwindow);
				ClipCursor (&window_rect);
			}
			ShowCursor (false);
		}
	}
	else
	{
		if (vid_usingmouse)
		{
			vid_usingmouse = false;
			if (dinput_acquired)
			{
				IDirectInputDevice_Unacquire(g_pMouse);
				dinput_acquired = false;
			}
			else
			{
				if (restore_spi)
					SystemParametersInfo (SPI_SETMOUSE, 0, originalmouseparms, 0);
				ClipCursor (NULL);
				ReleaseCapture ();
			}
			ShowCursor (true);
		}
	}
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

	in_mwheeltype = MWHEEL_UNKNOWN;

	if (in_dinput.value || COM_CheckParm ("-dinput"))
	{
		dinput = IN_InitDInput ();

		if (dinput)
			Com_Printf ("DirectInput initialized\n");
		else
			Com_Printf ("DirectInput not initialized\n");
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

	mouse_buttons = 8;
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

	if (!vid_usingmouse)
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

static void IN_Init(void)
{
	uiWheelMessage = RegisterWindowMessage ( "MSWHEEL_ROLLMSG" );

	Cvar_Register (&m_filter);
	Cvar_Register (&in_dinput);
	Cvar_Register (&cl_keypad);

	// joystick variables
	Cvar_Register (&in_joystick);
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
	Cmd_AddCommand ("loadkeys", IN_LoadKeys_f);
}

static void IN_Shutdown(void)
{
	IN_Activate (false);

	if (g_pMouse)
		IDirectInputDevice_Release(g_pMouse);
	g_pMouse = NULL;

	if (g_pdi)
		IDirectInput_Release(g_pdi);
	g_pdi = NULL;
}
