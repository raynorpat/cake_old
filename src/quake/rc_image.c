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

#include <limits.h>
#include "quakedef.h"
#include "rc_image.h"

char loadfilename[MAX_OSPATH]; // file scope so that error messages can use it

byte *Image_LoadTGA (byte *tgabuf, int *width, int *height);

/*
============
Image_LoadImage

returns a pointer to hunk allocated RGBA data

TODO: search order: tga pcx lmp
============
*/
byte *Image_LoadImage (char *name, int *width, int *height)
{
	qfile_t	*f;

	sprintf (loadfilename, "%s.tga", name);
	f = FS_Open (loadfilename, "rb", false, false);

	if (f)
	{
		// NB - it is expected that the caller will do a Hunk_FreeToLowMark
		byte *tgaload = NULL;
		byte *tgabuf = (byte *) Hunk_Alloc (FS_FileSize(f));

		FS_Read (f, tgabuf, FS_FileSize(f));
		FS_Close (f);

		tgaload = Image_LoadTGA (tgabuf, width, height);

		return tgaload;
	}

	return NULL;
}


/*
=========================================================

TARGA LOADING

=========================================================
*/

byte *Image_LoadTGA (byte *tgabuf, int *width, int *height)
{
	int				columns, rows, numPixels;
	byte			*pixbuf;
	int				row, column;
	byte			*targa_rgba;
	int				realrow; // johnfitz -- fix for upside-down targas
	qbool			upside_down; // johnfitz -- fix for upside-down targas
	targaheader_t	*tgaheader;

	tgaheader = (targaheader_t *) tgabuf;
	tgabuf += sizeof (targaheader_t);

	tgaheader->colormap_index = LittleShort (tgaheader->colormap_index);
	tgaheader->colormap_length = LittleShort (tgaheader->colormap_length);
	tgaheader->x_origin = LittleShort (tgaheader->x_origin);
	tgaheader->y_origin = LittleShort (tgaheader->y_origin);
	tgaheader->width = LittleShort (tgaheader->width);
	tgaheader->height = LittleShort (tgaheader->height);

	if (tgaheader->image_type != 2 && tgaheader->image_type != 10)
		Sys_Error ("Image_LoadTGA: %s is not a type 2 or type 10 targa\n", loadfilename);

	if (tgaheader->colormap_type != 0 || (tgaheader->pixel_size != 32 && tgaheader->pixel_size != 24))
		Sys_Error ("Image_LoadTGA: %s is not a 24bit or 32bit targa\n", loadfilename);

	columns = tgaheader->width;
	rows = tgaheader->height;
	numPixels = columns * rows;
	upside_down = !(tgaheader->attributes & 0x20); // johnfitz -- fix for upside-down targas

	targa_rgba = (byte *) Hunk_Alloc (numPixels * 4);

	if (tgaheader->id_length != 0)
		tgabuf += tgaheader->id_length;

	if (tgaheader->image_type == 2) // Uncompressed, RGB images
	{
		for (row = rows - 1; row >= 0; row--)
		{
			// johnfitz -- fix for upside-down targas
			realrow = upside_down ? row : rows - 1 - row;
			pixbuf = targa_rgba + realrow * columns * 4;

			// johnfitz
			for (column = 0; column < columns; column++)
			{
				unsigned char red, green, blue, alphabyte;

				switch (tgaheader->pixel_size)
				{
				case 24:
					blue = *tgabuf++;
					green = *tgabuf++;
					red = *tgabuf++;
					*pixbuf++ = blue;
					*pixbuf++ = green;
					*pixbuf++ = red;
					*pixbuf++ = 255;
					break;
				case 32:
					blue = *tgabuf++;
					green = *tgabuf++;
					red = *tgabuf++;
					alphabyte = *tgabuf++;
					*pixbuf++ = blue;
					*pixbuf++ = green;
					*pixbuf++ = red;
					*pixbuf++ = alphabyte;
					break;
				}
			}
		}
	}
	else if (tgaheader->image_type == 10) // Runlength encoded RGB images
	{
		unsigned char red = 0, green = 0, blue = 0, alphabyte = 0, packetHeader = 0, packetSize = 0, j = 0;

		for (row = rows - 1; row >= 0; row--)
		{
			// johnfitz -- fix for upside-down targas
			realrow = upside_down ? row : rows - 1 - row;
			pixbuf = targa_rgba + realrow * columns * 4;

			// johnfitz
			for (column = 0; column < columns;)
			{
				packetHeader = *tgabuf++;
				packetSize = 1 + (packetHeader & 0x7f);

				if (packetHeader & 0x80) // run-length packet
				{
					switch (tgaheader->pixel_size)
					{
					case 24:
						blue = *tgabuf++;
						green = *tgabuf++;
						red = *tgabuf++;
						alphabyte = 255;
						break;
					case 32:
						blue = *tgabuf++;
						green = *tgabuf++;
						red = *tgabuf++;
						alphabyte = *tgabuf++;
						break;
					}

					for (j = 0; j < packetSize; j++)
					{
						*pixbuf++ = blue;
						*pixbuf++ = green;
						*pixbuf++ = red;
						*pixbuf++ = alphabyte;
						column++;

						if (column == columns) // run spans across rows
						{
							column = 0;

							if (row > 0)
								row--;
							else
								goto breakOut;

							// johnfitz -- fix for upside-down targas
							realrow = upside_down ? row : rows - 1 - row;
							pixbuf = targa_rgba + realrow * columns * 4;
							// johnfitz
						}
					}
				}
				else // non run-length packet
				{
					for (j = 0; j < packetSize; j++)
					{
						switch (tgaheader->pixel_size)
						{
						case 24:
							blue = *tgabuf++;
							green = *tgabuf++;
							red = *tgabuf++;
							*pixbuf++ = blue;
							*pixbuf++ = green;
							*pixbuf++ = red;
							*pixbuf++ = 255;
							break;
						case 32:
							blue = *tgabuf++;
							green = *tgabuf++;
							red = *tgabuf++;
							alphabyte = *tgabuf++;
							*pixbuf++ = blue;
							*pixbuf++ = green;
							*pixbuf++ = red;
							*pixbuf++ = alphabyte;
							break;
						}

						column++;

						if (column == columns) // pixel packet run spans across rows
						{
							column = 0;

							if (row > 0)
								row--;
							else
								goto breakOut;

							// johnfitz -- fix for upside-down targas
							realrow = upside_down ? row : rows - 1 - row;
							pixbuf = targa_rgba + realrow * columns * 4;
							// johnfitz
						}
					}
				}
			}

breakOut:;
		}
	}

	*width = (int) (tgaheader->width);
	*height = (int) (tgaheader->height);
	return targa_rgba;
}
