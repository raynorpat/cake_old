/*
	version.c

	Build number and version strings

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
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA
*/

#include "common.h"
#include "version.h"

#define STRINGIFY2(arg) #arg
#define STRINGIFY(arg) STRINGIFY2(arg)

extern const char *buildstring;
const char *buildstring = __TIME__ " " __DATE__
#ifdef BUILDTYPE
" " STRINGIFY(BUILDTYPE)
#endif
;

/*
=======================
CL_Version_f
======================
*/
void CL_Version_f (void)
{
	Com_Printf (PROGRAM " version: %s\n", VersionString());
	Com_Printf ("built %s", buildstring);
}

/*
=======================
VersionString
======================
*/
char *VersionString (void)
{
	static char str[32];

#ifdef RELEASE_VERSION
	sprintf (str, "%s", PROGRAM_VERSION);
#else
	sprintf (str, "%s (build %i)", PROGRAM_VERSION, build_number());
#endif

	return str;
}
