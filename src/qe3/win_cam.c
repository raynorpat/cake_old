// win_cam.c -- windows specific camera view code

#include "qe3.h"

/*
============
CameraWndProc
============
*/
LONG WINAPI WCam_WndProc (
    HWND    hWnd,
    UINT    uMsg,
    WPARAM  wParam,
    LPARAM  lParam)
{
	int		fwKeys, xPos, yPos;
    RECT	rect;

    GetClientRect(hWnd, &rect);

    switch (uMsg)
    {
	case WM_CREATE:
		{
			HFONT	hfont;
			
            g_qeglobals.d_hdcBase = GetDC(hWnd);
			QEW_SetupPixelFormat(g_qeglobals.d_hdcBase, true);

            if ( ( g_qeglobals.d_hglrcBase = wglCreateContext( g_qeglobals.d_hdcBase ) ) == 0 )
				Error ("wglCreateContext failed");
            if (!wglMakeCurrent( g_qeglobals.d_hdcBase, g_qeglobals.d_hglrcBase ))
				Error ("wglMakeCurrent failed");

			Texture_SetMode(g_qeglobals.d_savedinfo.iTexMenu);

			//
			// create GL font
			//
			hfont = CreateFont(
				12,	// logical height of font (default was 10)
				6,	// logical average character width (default was 7)
				0,	// angle of escapement 
				0,	// base-line orientation angle 
				0,	// font weight 
				0,	// italic attribute flag 
				0,	// underline attribute flag 
				0,	// strikeout attribute flag 
				0,	// character set identifier 
				0,	// output precision 
				0,	// clipping precision 
				0,	// output quality 
				0,	// pitch and family 
				"system font" 	// pointer to typeface name string (default was 0)
				);

			if ( !hfont )
				Error( "couldn't create font" );

			SelectObject (g_qeglobals.d_hdcBase, hfont);

			if ( ( g_qeglobals.d_font_list = glGenLists (256) ) == 0 )
				Error( "couldn't create font dlists" );
			
			// create the bitmap display lists
			// we're making images of glyphs 0 thru 255
			if ( !wglUseFontBitmaps (g_qeglobals.d_hdcBase, 1, 255, g_qeglobals.d_font_list) )
				Error( "wglUseFontBitmaps faileD" );
			
			// indicate start of glyph display lists
			glListBase (g_qeglobals.d_font_list);

			// report OpenGL information
			Sys_Printf ("GL_VENDOR: %s\n", glGetString (GL_VENDOR));
			Sys_Printf ("GL_RENDERER: %s\n", glGetString (GL_RENDERER));
			Sys_Printf ("GL_VERSION: %s\n", glGetString (GL_VERSION));
			Sys_Printf ("GL_EXTENSIONS: %s\n", glGetString (GL_EXTENSIONS));
		}
		return 0;
	case WM_PAINT:
        { 
			PAINTSTRUCT	ps;
			
            if (!wglMakeCurrent( g_qeglobals.d_hdcBase, g_qeglobals.d_hglrcBase ))
				Error ("wglMakeCurrent failed");

			if ( BeginPaint(hWnd, &ps) )
			{
				QE_CheckOpenGLForErrors();
				Cam_Draw ();
				QE_CheckOpenGLForErrors();

				EndPaint(hWnd, &ps);
				SwapBuffers(g_qeglobals.d_hdcBase);
			}
        }
		return 0;
		
	case WM_USER+267:	// benchmark
        { 
			PAINTSTRUCT	ps;
			WINDOWPLACEMENT wp;
			double	start, end;
			int		i;
			
			memset( &wp, 0, sizeof( wp ) );
			wp.length = sizeof( wp );
			GetWindowPlacement( g_qeglobals.d_hwndCamera, &wp );
			
			MoveWindow( g_qeglobals.d_hwndCamera, 30, 30, 400, 400, TRUE );
			
			BeginPaint(hWnd, &ps);
            if (!wglMakeCurrent( g_qeglobals.d_hdcBase, g_qeglobals.d_hglrcBase))
				Error ("wglMakeCurrent failed");
			glDrawBuffer (GL_FRONT);
			
			start = Sys_DoubleTime ();
			for (i=0 ; i<100 ; i++)
			{
				g_qeglobals.d_camera.angles[YAW] = i*4;
				Cam_Draw ();
			}
			SwapBuffers(g_qeglobals.d_hdcBase);
			glDrawBuffer (GL_BACK);
			end = Sys_DoubleTime ();
			EndPaint(hWnd, &ps);
			Sys_Printf ("Benchmark: %5.2f seconds\n", end-start);

			SetWindowPlacement( g_qeglobals.d_hwndCamera, &wp );
        }
		break;
		
	case WM_KEYDOWN:
		if ( QE_KeyDown (wParam) )
			return 0;
		else 
			return DefWindowProc( hWnd, uMsg, wParam, lParam );
		
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_LBUTTONDOWN:
		if (GetTopWindow(g_qeglobals.d_hwndMain) != hWnd)
			BringWindowToTop(hWnd);
		
		SetFocus (g_qeglobals.d_hwndCamera);
		SetCapture (g_qeglobals.d_hwndCamera);
		fwKeys = wParam;        // key flags 
		xPos = (short)LOWORD(lParam);  // horizontal position of cursor 
		yPos = (short)HIWORD(lParam);  // vertical position of cursor 
		yPos = (int)rect.bottom - 1 - yPos;
		Cam_MouseDown (xPos, yPos, fwKeys);
		return 0;
		
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
	case WM_LBUTTONUP:
		fwKeys = wParam;        // key flags 
		xPos = (short)LOWORD(lParam);  // horizontal position of cursor 
		yPos = (short)HIWORD(lParam);  // vertical position of cursor 
		yPos = (int)rect.bottom - 1 - yPos;
		Cam_MouseUp (xPos, yPos, fwKeys);
		if (! (fwKeys & (MK_LBUTTON|MK_RBUTTON|MK_MBUTTON)))
			ReleaseCapture ();
		return 0;
		
	case WM_MOUSEMOVE:
		fwKeys = wParam;        // key flags 
		xPos = (short)LOWORD(lParam);  // horizontal position of cursor 
		yPos = (short)HIWORD(lParam);  // vertical position of cursor 
		yPos = (int)rect.bottom - 1 - yPos;
		Cam_MouseMoved (xPos, yPos, fwKeys);
		return 0;
		
	case WM_SIZE:
		g_qeglobals.d_camera.width = rect.right;
		g_qeglobals.d_camera.height = rect.bottom;
		InvalidateRect(g_qeglobals.d_hwndCamera, NULL, false);
		return 0;

	case WM_KILLFOCUS:
	case WM_SETFOCUS:
		SendMessage( hWnd, WM_NCACTIVATE, uMsg == WM_SETFOCUS, 0 );
		return 0;

	case WM_NCCALCSIZE:// don't let windows copy pixels
		DefWindowProc (hWnd, uMsg, wParam, lParam);
		return WVR_REDRAW;

	case WM_CLOSE:
		DestroyWindow (hWnd);
		return 0;
		
	case WM_DESTROY:
		QEW_StopGL( hWnd, g_qeglobals.d_hglrcBase, g_qeglobals.d_hdcBase );
		return 0;
    }

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}


/*
==============
WCam_Create
==============
*/
void WCam_Create (HINSTANCE hInstance)
{
    WNDCLASS   wc;

    /* Register the camera class */
	memset (&wc, 0, sizeof(wc));

    wc.style         = 0;
    wc.lpfnWndProc   = (WNDPROC)WCam_WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = 0;
    wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = CAMERA_WINDOW_CLASS;

    if (!RegisterClass (&wc) )
        Error ("WCam_Register: failed");
	
	g_qeglobals.d_hwndCamera = CreateWindow (CAMERA_WINDOW_CLASS ,
		"Camera",
		QE3_STYLE,
		ZWIN_WIDTH,
		20,
		(int)(screen_width*CWIN_SIZE),
		(int)(screen_height*CWIN_SIZE),	// size
		g_qeglobals.d_hwndMain,	// parent window
		0,		// no menu
		hInstance,
		0);

	if (!g_qeglobals.d_hwndCamera)
		Error ("Couldn't create Camera Window");

	LoadWindowState(g_qeglobals.d_hwndCamera, "camerawindow");
    ShowWindow (g_qeglobals.d_hwndCamera, SW_SHOWDEFAULT);
}
