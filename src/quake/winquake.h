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
// winquake.h: Win32-specific Quake header file
#ifndef _WINQUAKE_H_
#define _WINQUAKE_H_

#ifdef _WIN32

#include <windows.h>

#ifndef SERVERONLY

//
// uncategorized Win32 stuff
//
extern HINSTANCE	global_hInstance;
extern HWND			hwnd_dialog;	// startup screen handle



//
// sound
//
#include <dsound.h>

extern LPDIRECTSOUND		pDS;
extern LPDIRECTSOUNDBUFFER	pDSBuf;
extern DWORD				gSndBufSize;

void S_BlockSound (void);
void S_UnblockSound (void);


//
// video
//
extern HWND			mainwindow;

//
// input
//
enum { MWHEEL_UNKNOWN, MWHEEL_DINPUT, MWHEEL_WINDOWMSG };

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL	0x020A
#endif

#ifndef MK_XBUTTON1
#define MK_XBUTTON1		0x20
#define MK_XBUTTON2		0x40
#endif

#ifndef WM_XBUTTONDOWN
#define WM_XBUTTONDOWN	0x020B
#define WM_XBUTTONUP	0x020C
#endif

#endif // !SERVERONLY

#endif // _WIN32

#endif /* _WINQUAKE_H_ */

