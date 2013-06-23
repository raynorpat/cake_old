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
// q_shared.h -- functions shared by all subsystems
#ifndef _Q_SHARED_H_
#define _Q_SHARED_H_

#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#ifdef _MSC_VER
#pragma warning( disable : 4201 4244 4127 4201 4214 4514 4305 4115 4018 4996 )
#define strcasecmp _stricmp 
#define strncasecmp _strnicmp
#endif

#ifdef _MSC_VER
#define inline __inline
#define HAVE_INLINE
#endif

#include "sys.h"
#include "mathlib.h"

#undef gamma	// math.h defines this

#define	QUAKE_GAME			// as opposed to utilities

#define PROGRAM "Cake"

typedef unsigned char 		byte;
#define _DEF_BYTE_

typedef enum {false, true} qbool;

#ifndef NULL
#define NULL ((void *)0)
#endif

#define PAD(x,y) (((x)+(y)-1) & ~((y)-1))

#ifdef _WIN32
#define IS_SLASH(c) ((c) == '/' || (c) == '\\')
#else
#define IS_SLASH(c) ((c) == '/')
#endif


#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#define clamp(min, x, max) ((x) < (min) ? (min) : (x) > (max) ? (max) : (x))

//#define bound(a,b,c) (max((a), min((b), (c))))
#define bound(a,b,c) ((a) >= (c) ? (a) : \
					(b) < (a) ? (a) : (b) > (c) ? (c) : (b))

//============================================================================

#define	MAX_QPATH			64			// max length of a quake game pathname
#define	MAX_OSPATH			128			// max length of a filesystem pathname

#define	ON_EPSILON			0.1			// point on plane side epsilon
#define BACKFACE_EPSILON	0.01

extern byte scratchbuf[];
#define SCRATCHBUF_SIZE 1048576

//============================================================================

typedef struct sizebuf_s
{
	qbool	allowoverflow;	// if false, do a Sys_Error
	qbool	overflowed;		// set to true if the buffer size failed
	byte	*data;
	int		maxsize;
	int		cursize;
} sizebuf_t;

void SZ_Init (sizebuf_t *buf, byte *data, int length);
void SZ_Clear (sizebuf_t *buf);
void *SZ_GetSpace (sizebuf_t *buf, int length);
void SZ_Write (sizebuf_t *buf, const void *data, int length);
void SZ_Print (sizebuf_t *buf, const char *data);	// strcats onto the sizebuf

//============================================================================

short	ShortSwap (short l);
int		LongSwap (int l);
float	FloatSwap (float f);

#ifdef BIGENDIAN
#define BigShort(x) (x)
#define BigLong(x) (x)
#define BigFloat(x) (x)
#define LittleShort(x) ShortSwap (x)
#define LittleLong(x) LongSwap(x)
#define LittleFloat(x) FloatSwap(x)
#else
#define BigShort(x) ShortSwap (x)
#define BigLong(x) LongSwap(x)
#define BigFloat(x) FloatSwap(x)
#define LittleShort(x) (x)
#define LittleLong(x) (x)
#define LittleFloat(x) (x)
#endif

//============================================================================

#ifdef _WIN32
#define Q_stricmp(s1, s2) _stricmp((s1), (s2))
#define Q_strnicmp(s1, s2, n) _strnicmp((s1), (s2), (n))
#else
#define Q_stricmp(s1, s2) strcasecmp((s1), (s2))
#define Q_strnicmp(s1, s2, n) strncasecmp((s1), (s2), (n))
#endif

#define Q_strncatz(dest, src, sizeofdest)	\
	do {	\
		strncat(dest, src, sizeofdest - strlen(dest) - 1);	\
		dest[sizeofdest - 1] = 0;	\
	} while (0)

void Q_strncpyz(char *d, const char *s, int n);

int	Q_atoi (char *str);
float Q_atof (char *str);
char *Q_ftos (float value);		// removes trailing zero chars

size_t strlcpy (char *dst, const char *src, size_t size);
size_t strlcat (char *dst, const char *src, size_t size);
int snprintf(char *buffer, size_t count, char const *format, ...);
#define Q_snprintfz snprintf 	// nuke this one day

qbool Q_glob_match (const char *pattern, const char *text);

int Com_HashKey (char *name);

//============================================================================

void *Q_malloc (size_t size);
void *Q_calloc (size_t count, size_t size);
char *Q_strdup (const char *src);
// might be turned into a function that makes sure all Q_*alloc calls are matched with Q_free
#define Q_free(ptr) free(ptr)

//============================================================================

// these are here for net.h's sake
#define	MAX_MSGLEN		1450		// max length of a reliable message
#define	MAX_DATAGRAM	1450		// max length of unreliable message
#define	MAX_BIG_MSGLEN	8000		// max length of a demo or loop message, >= MAX_MSGLEN + header

#define	MAX_NQMSGLEN	8000		// max length of a reliable message
#define MAX_OVERALLMSGLEN	MAX_NQMSGLEN
#define	MAX_NQDATAGRAM	1024		// max length of unreliable message

//============================================================================

#define MVDPLAY
#define WITH_NQPROGS

#endif /* _Q_SHARED_H_ */

