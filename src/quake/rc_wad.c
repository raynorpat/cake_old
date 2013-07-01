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
// rc_wad.c - .wad file loading

#include "quakedef.h"
#include "rc_wad.h"
#include "crc.h"

typedef struct
{
	char		identification[4];		// should be WAD2 or 2DAW
	int			numlumps;
	int			infotableofs;
} wadinfo_t;

static int			wad_numlumps;
static lumpinfo_t	*wad_lumps;
byte				*wad_base = NULL;
static int			wad_filesize;

void SwapPic (qpic_t *pic);

/*
==================
W_CleanupName

Lowercases name and pads with spaces and a terminating 0 to the length of
lumpinfo_t->name.
Used so lumpname lookups can proceed rapidly by comparing 4 chars at a time
Space padding is so names can be printed nicely in tables.
Can safely be performed in place.
==================
*/
static void W_CleanupName (char *in, char *out)
{
	int		i;
	int		c;
	
	for (i=0 ; i<16 ; i++ )
	{
		c = (int)(unsigned char)in[i];
		if (!c)
			break;
			
		if ( isupper(c) )
			c += ('a' - 'A');
		out[i] = c;
	}
	
	for ( ; i< 16 ; i++ )
		out[i] = 0;
}



/*
====================
W_LoadWadFile
====================
*/
void W_FreeWadFile (void)
{
	Q_free (wad_base);
	wad_base = NULL;
	wad_lumps = NULL;
	wad_numlumps = 0;
	wad_filesize = 0;
}


/*
====================
W_LoadWadFile
====================
*/
void W_LoadWadFile (char *filename)
{
	lumpinfo_t		*lump_p;
	wadinfo_t		*header;
	unsigned		i;
	int				infotableofs;
	fs_offset_t		filesize;

	// only one .wad can be loaded at a time
	W_FreeWadFile ();

	wad_base = FS_LoadFile (filename, false, &filesize);
	if (!wad_base)
		Sys_Error ("W_LoadWadFile: couldn't load %s", filename);

	wad_filesize = filesize;

	header = (wadinfo_t *)wad_base;
	
	if (header->identification[0] != 'W'
	|| header->identification[1] != 'A'
	|| header->identification[2] != 'D'
	|| header->identification[3] != '2')
		Sys_Error ("Wad file %s doesn't have WAD2 id",filename);
		
	wad_numlumps = LittleLong(header->numlumps);
	infotableofs = LittleLong(header->infotableofs);
	wad_lumps = (lumpinfo_t *)(wad_base + infotableofs);

	if (infotableofs + wad_numlumps * sizeof(lump_t) > wad_filesize)
		Sys_Error ("Wad lump table exceeds file size");
	
	for (i=0, lump_p = wad_lumps ; i<wad_numlumps ; i++,lump_p++)
	{
		lump_p->filepos = LittleLong(lump_p->filepos);
		lump_p->size = LittleLong(lump_p->size);
		lump_p->disksize = LittleLong(lump_p->disksize);
		W_CleanupName (lump_p->name, lump_p->name);

		if (lump_p->filepos < sizeof(wadinfo_t) ||
				lump_p->filepos + lump_p->disksize > wad_filesize)
			Sys_Error ("Wad lump %s exceeds file size", lump_p->name);

		if (lump_p->type == TYP_QPIC)
			SwapPic ( (qpic_t *)(wad_base + lump_p->filepos));
	}
}


/*
=============
W_GetLumpinfo
=============
*/
lumpinfo_t *W_GetLumpinfo (char *name, qbool crash)
{
	int		i;
	lumpinfo_t	*lump_p;
	char	clean[16];
	
	W_CleanupName (name, clean);
	
	for (lump_p=wad_lumps, i=0 ; i<wad_numlumps ; i++,lump_p++)
	{
		if (!strcmp(clean, lump_p->name))
			return lump_p;
	}
	
	if(crash)
		Sys_Error ("W_GetLumpinfo: %s not found", name);
	return NULL;
}

void *W_GetLumpName (char *name, qbool crash)
{
	lumpinfo_t	*lump;
	
	lump = W_GetLumpinfo (name, crash);
	if (!lump)
		return NULL;

	if ( lump->type == TYP_QPIC &&
			((qpic_t *)(wad_base + lump->filepos))->width *
			((qpic_t *)(wad_base + lump->filepos))->height > lump->disksize )
		Sys_Error ("Wad lump %s has incorrect size", lump->name);

	if (!strcmp(name, "conchars")) {
		if (lump->disksize < 16384)
			Sys_Error ("W_GetLumpName: conchars lump is < 16384 bytes");
	}

	return (void *)(wad_base + lump->filepos);
}


/*
=============================================================================

automatic byte swapping

=============================================================================
*/

void SwapPic (qpic_t *pic)
{
	pic->width = LittleLong(pic->width);
	pic->height = LittleLong(pic->height);	
}
