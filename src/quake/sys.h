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
// sys.h -- non-portable functions
#ifndef _SYS_H_
#define _SYS_H_

//
// DLL management
//

// Win32 specific
#ifdef _WIN32
#include <windows.h>
typedef HMODULE dllhandle_t;
#else
// Other platforms
typedef void* dllhandle_t;
#endif

typedef struct
{
	const char *name;
	void **funcvariable;
}
dllfunction_t;

dllhandle_t Sys_LoadLibrary (const char* name);
void Sys_UnloadLibrary (dllhandle_t handle);
void* Sys_GetProcAddress (dllhandle_t handle, const char* name);

//
// file IO
//

void Sys_mkdir (char *path);
int Sys_remove (char *path);

void Sys_Error (char *error, ...);
// an error will cause the entire program to exit

void Sys_Printf (char *fmt, ...);
// send text to the console

double Sys_DoubleTime (void);

char *Sys_ConsoleInput (void);

// if successful, returns malloc'ed string (make sure to free it afterwards)
// returns NULL if the operation failed for some reason
char *Sys_GetClipboardText (void);

void Sys_SendKeyEvents (void);
// Perform Key_Event () callbacks until the input que is empty

void Sys_Init (void);
void Sys_Quit (void);

#endif /* _SYS_H_ */

