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

/*
============
Image_LoadImage

returns a pointer to hunk allocated RGBA data

TODO: search order: tga pcx lmp
============
*/
byte *Image_LoadImage (char *name, int *width, int *height)
{
	FILE	*f;

	sprintf (loadfilename, "%s.tga", name);
	FS_FOpenFile (loadfilename, &f);
	if (f)
		return Image_LoadTGA (f, width, height);

	sprintf (loadfilename, "%s.pcx", name);
	FS_FOpenFile (loadfilename, &f);
	if (f)
		return Image_LoadPCX (f, width, height);

	return NULL;
}


/*
=========================================================

TARGA LOADING

=========================================================
*/

typedef struct targaheader_s {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} targaheader_t;

targaheader_t targa_header;

int fgetLittleShort (FILE *f)
{
	byte	b1, b2;

	b1 = fgetc(f);
	b2 = fgetc(f);

	return (short)(b1 + b2*256);
}

int fgetLittleLong (FILE *f)
{
	byte	b1, b2, b3, b4;

	b1 = fgetc(f);
	b2 = fgetc(f);
	b3 = fgetc(f);
	b4 = fgetc(f);

	return b1 + (b2<<8) + (b3<<16) + (b4<<24);
}

/*
=============
Image_LoadTGA
=============
*/
byte *Image_LoadTGA (FILE *fin, int *width, int *height)
{
	int				columns, rows, numPixels;
	byte			*pixbuf;
	int				row, column;
	byte			*targa_rgba;

	targa_header.id_length = fgetc(fin);
	targa_header.colormap_type = fgetc(fin);
	targa_header.image_type = fgetc(fin);

	targa_header.colormap_index = fgetLittleShort(fin);
	targa_header.colormap_length = fgetLittleShort(fin);
	targa_header.colormap_size = fgetc(fin);
	targa_header.x_origin = fgetLittleShort(fin);
	targa_header.y_origin = fgetLittleShort(fin);
	targa_header.width = fgetLittleShort(fin);
	targa_header.height = fgetLittleShort(fin);
	targa_header.pixel_size = fgetc(fin);
	targa_header.attributes = fgetc(fin);

	if (targa_header.image_type!=2 && targa_header.image_type!=10)
		Sys_Error ("Image_LoadTGA: %s is not a type 2 or type 10 targa\n", loadfilename);

	if (targa_header.colormap_type !=0 || (targa_header.pixel_size!=32 && targa_header.pixel_size!=24))
		Sys_Error ("Image_LoadTGA: %s is not a 24bit or 32bit targa\n", loadfilename);

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	targa_rgba = Hunk_Alloc (numPixels*4);

	if (targa_header.id_length != 0)
		fseek(fin, targa_header.id_length, SEEK_CUR);  // skip TARGA image comment

	if (targa_header.image_type==2) // Uncompressed, RGB images
	{
		for(row=rows-1; row>=0; row--)
		{
			pixbuf = targa_rgba + row*columns*4;
			for(column=0; column<columns; column++)
			{
				unsigned char red,green,blue,alphabyte;
				switch (targa_header.pixel_size)
				{
				case 24:
						blue = getc(fin);
						green = getc(fin);
						red = getc(fin);
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = 255;
						break;
				case 32:
						blue = getc(fin);
						green = getc(fin);
						red = getc(fin);
						alphabyte = getc(fin);
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						break;
				}
			}
		}
	}
	else if (targa_header.image_type==10) // Runlength encoded RGB images
	{
		unsigned char red,green,blue,alphabyte,packetHeader,packetSize,j;
		for(row=rows-1; row>=0; row--)
		{
			pixbuf = targa_rgba + row*columns*4;
			for(column=0; column<columns; )
			{
				packetHeader=getc(fin);
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) // run-length packet
				{
					switch (targa_header.pixel_size)
					{
					case 24:
							blue = getc(fin);
							green = getc(fin);
							red = getc(fin);
							alphabyte = 255;
							break;
					case 32:
							blue = getc(fin);
							green = getc(fin);
							red = getc(fin);
							alphabyte = getc(fin);
							break;
					}

					for(j=0;j<packetSize;j++)
					{
						*pixbuf++=red;
						*pixbuf++=green;
						*pixbuf++=blue;
						*pixbuf++=alphabyte;
						column++;
						if (column==columns) // run spans across rows
						{
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row*columns*4;
						}
					}
				}
				else // non run-length packet
				{
					for(j=0;j<packetSize;j++)
					{
						switch (targa_header.pixel_size)
						{
						case 24:
								blue = getc(fin);
								green = getc(fin);
								red = getc(fin);
								*pixbuf++ = red;
								*pixbuf++ = green;
								*pixbuf++ = blue;
								*pixbuf++ = 255;
								break;
						case 32:
								blue = getc(fin);
								green = getc(fin);
								red = getc(fin);
								alphabyte = getc(fin);
								*pixbuf++ = red;
								*pixbuf++ = green;
								*pixbuf++ = blue;
								*pixbuf++ = alphabyte;
								break;
						}
						column++;
						if (column==columns) // pixel packet run spans across rows
						{
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row*columns*4;
						}
					}
				}
			}
			breakOut:;
		}
	}

	fclose(fin);

	*width = (int)(targa_header.width);
	*height = (int)(targa_header.height);
	return targa_rgba;
}


/*
=================================================================

  PCX Loading

=================================================================
*/

typedef struct
{
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char	filler[58];
    unsigned 	data;			// unbounded
} pcxheader_t;

/*
============
Image_LoadPCX
============
*/
byte *Image_LoadPCX (FILE *f, int *width, int *height)
{
	pcxheader_t	*pcx, pcxbuf;
	byte	palette[768];
	byte	*pix;
	int		x, y;
	int		dataByte, runLength;
	int		count;
	byte	*pcx_rgb;

	fread (&pcxbuf, 1, sizeof(pcxbuf), f);

	pcx = &pcxbuf;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| pcx->xmax >= 320
		|| pcx->ymax >= 256)
		Sys_Error ("Image_LoadPCX: bad pcx file: %s\n", loadfilename);

	// seek to palette
	fseek (f, -768, SEEK_END);
	fread (palette, 1, 768, f);

	fseek (f, sizeof(pcxbuf) - 4, SEEK_SET);

	count = (pcx->xmax+1) * (pcx->ymax+1);

	pcx_rgb = Hunk_Alloc ( count * 4);

	for (y=0 ; y<=pcx->ymax ; y++)
	{
		pix = pcx_rgb + 4*y*(pcx->xmax+1);
		for (x=0 ; x<=pcx->ymax ; ) //FIXME -- should this be xmax?
		{
			dataByte = fgetc(f);

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = fgetc(f);
			}
			else
				runLength = 1;

			while(runLength-- > 0)
			{
				pix[0] = palette[dataByte*3];
				pix[1] = palette[dataByte*3+1];
				pix[2] = palette[dataByte*3+2];
				pix[3] = 255;
				pix += 4;
				x++;
			}
		}
	}

	*width = (int)(pcx->xmax) + 1;
	*height = (int)(pcx->ymax) + 1;
	return pcx_rgb;
}

void WritePCX (byte *data, int width, int height, int rowbytes, byte *palette,	// [in]
				byte **pcxdata, int *pcxsize)									// [out]
{
	int		i, j;
	pcxheader_t	*pcx;
	byte	*pack;

	assert (pcxdata != NULL);
	assert (pcxsize != NULL);

	pcx = Hunk_TempAlloc (width*height*2+1000);
	if (!pcx) {
		Com_Printf ("WritePCX: not enough memory\n");
		*pcxdata = NULL;
		*pcxsize = 0;
		return;
	} 

	pcx->manufacturer = 0x0a;	// PCX id
	pcx->version = 5;			// 256 color
	pcx->encoding = 1;			// uncompressed
	pcx->bits_per_pixel = 8;	// 256 color
	pcx->xmin = 0;
	pcx->ymin = 0;
	pcx->xmax = LittleShort((short)(width-1));
	pcx->ymax = LittleShort((short)(height-1));
	pcx->hres = LittleShort((short)width);
	pcx->vres = LittleShort((short)height);
	memset (pcx->palette, 0, sizeof(pcx->palette));
	pcx->color_planes = 1;				// chunky image
	pcx->bytes_per_line = LittleShort((short)width);
	pcx->palette_type = LittleShort(2);	// not a grey scale
	memset (pcx->filler, 0, sizeof(pcx->filler));

	// pack the image
	pack = (byte *)&pcx->data;

	for (i=0 ; i<height ; i++)
	{
		for (j=0 ; j<width ; j++)
		{
			if ( (*data & 0xc0) != 0xc0)
				*pack++ = *data++;
			else
			{
				*pack++ = 0xc1;
				*pack++ = *data++;
			}
		}
		data += rowbytes - width;
	}

	// write the palette
	*pack++ = 0x0c; // palette ID byte
	for (i=0 ; i<768 ; i++)
		*pack++ = *palette++;

	// fill results
	*pcxdata = (byte *) pcx;
	*pcxsize = pack - (byte *)pcx;
}


/*
=========================================================

HQ2x filter
authors: byuu and blargg
license: public domain

note: this is a clean reimplementation of the original HQ2x filter, which was
written by Maxim Stepin (MaxSt). it is not 100% identical, but very similar.

=========================================================
*/

#define MASK_G      0x0000ff00
#define MASK_RB     0x00ff00ff
#define MASK_RGB    0x00ffffff
#define MASK_ALPHA  0xff000000

static const byte hqTable[256] = {
    4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 15, 12, 5,  3, 17, 13,
    4, 4, 6, 18, 4, 4, 6, 18, 5,  3, 12, 12, 5,  3,  1, 12,
    4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 17, 13, 5,  3, 16, 14,
    4, 4, 6, 18, 4, 4, 6, 18, 5,  3, 16, 12, 5,  3,  1, 14,
    4, 4, 6,  2, 4, 4, 6,  2, 5, 19, 12, 12, 5, 19, 16, 12,
    4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 16, 12, 5,  3, 16, 12,
    4, 4, 6,  2, 4, 4, 6,  2, 5, 19,  1, 12, 5, 19,  1, 14,
    4, 4, 6,  2, 4, 4, 6, 18, 5,  3, 16, 12, 5, 19,  1, 14,
    4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 15, 12, 5,  3, 17, 13,
    4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 16, 12, 5,  3, 16, 12,
    4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 17, 13, 5,  3, 16, 14,
    4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 16, 13, 5,  3,  1, 14,
    4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 16, 12, 5,  3, 16, 13,
    4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 16, 12, 5,  3,  1, 12,
    4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 16, 12, 5,  3,  1, 14,
    4, 4, 6,  2, 4, 4, 6,  2, 5,  3,  1, 12, 5,  3,  1, 14,
};

static byte rotTable[256];
static byte equBitmap[65536 / CHAR_BIT];

static int same(int A, int B)
{
    return Q_IsBitSet(equBitmap, (A << 8) + B);
}

static int diff(int A, int B)
{
    return !same(A, B);
}

static unsigned long int generic(int A, int B, int C, int w1, int w2, int w3, int s)
{
    unsigned long int a = d_8to24table[A];
    unsigned long int b = d_8to24table[B];
    unsigned long int c = d_8to24table[C];
    unsigned long int g, rb, alpha;

    if ((A & B & C) == 255)
        return 0;

    // if transparent, scan around for another color to avoid alpha fringes
    if (A == 255) {
        if (B == 255)
            a = c & MASK_RGB;
        else
            a = b & MASK_RGB;
    }
    if (B == 255)
        b = a & MASK_RGB;
    if (C == 255)
        c = a & MASK_RGB;

    g      = ((a & MASK_G)  * w1 + (b & MASK_G)  * w2 + (c & MASK_G)  * w3) >> s;
    rb     = ((a & MASK_RB) * w1 + (b & MASK_RB) * w2 + (c & MASK_RB) * w3) >> s;
    alpha  = ((a >> 24)     * w1 + (b >> 24)     * w2 + (c >> 24)     * w3) << (24 - s);

    return (g & MASK_G) | (rb & MASK_RB) | (alpha & MASK_ALPHA);
}

static unsigned long int blend0(int A)
{
    if (A == 255)
        return 0;

    return d_8to24table[A];
}

static unsigned long int blend1(int A, int B)
{
    return generic(A, B, 0, 3, 1, 0, 2);
}

static unsigned long int blend2(int A, int B, int C)
{
    return generic(A, B, C, 2, 1, 1, 2);
}

static unsigned long int blend3(int A, int B, int C)
{
    return generic(A, B, C, 5, 2, 1, 3);
}

static unsigned long int blend4(int A, int B, int C)
{
    return generic(A, B, C, 6, 1, 1, 3);
}

static unsigned long int blend5(int A, int B, int C)
{
    return generic(A, B, C, 2, 3, 3, 3);
}

static unsigned long int blend6(int A, int B, int C)
{
    return generic(A, B, C, 14, 1, 1, 4);
}

static unsigned long int blend(int rule, int E, int A, int B, int D, int F, int H)
{
    switch (rule) {
        default:
        case  0: return blend0(E);
        case  1: return blend1(E, A);
        case  2: return blend1(E, D);
        case  3: return blend1(E, B);
        case  4: return blend2(E, D, B);
        case  5: return blend2(E, A, B);
        case  6: return blend2(E, A, D);
        case  7: return blend3(E, B, D);
        case  8: return blend3(E, D, B);
        case  9: return blend4(E, D, B);
        case 10: return blend5(E, D, B);
        case 11: return blend6(E, D, B);
        case 12: return same(B, D) ? blend2(E, D, B) : blend0(E);
        case 13: return same(B, D) ? blend5(E, D, B) : blend0(E);
        case 14: return same(B, D) ? blend6(E, D, B) : blend0(E);
        case 15: return same(B, D) ? blend2(E, D, B) : blend1(E, A);
        case 16: return same(B, D) ? blend4(E, D, B) : blend1(E, A);
        case 17: return same(B, D) ? blend5(E, D, B) : blend1(E, A);
        case 18: return same(B, F) ? blend3(E, B, D) : blend1(E, D);
        case 19: return same(D, H) ? blend3(E, D, B) : blend1(E, B);
    }
}

void HQ2x_Render(unsigned long int *output, const byte *input, int width, int height)
{
    int x, y;

    for (y = 0; y < height; y++) {
        const byte *in = input + y * width;
        unsigned long int *out0 = output + y * width * 4;
        unsigned long int *out1 = output + y * width * 4 + width * 2;

        int prevline = (y == 0 ? 0 : width);
        int nextline = (y == height - 1 ? 0 : width);

        for (x = 0; x < width; x++) {
            int prev = (x == 0 ? 0 : 1);
            int next = (x == width - 1 ? 0 : 1);

            int A = *(in - prevline - prev);
            int B = *(in - prevline);
            int C = *(in - prevline + next);
            int D = *(in - prev);
            int E = *(in);
            int F = *(in + next);
            int G = *(in + nextline - prev);
            int H = *(in + nextline);
            int I = *(in + nextline + next);

            int pattern;
            pattern  = diff(E, A) << 0;
            pattern |= diff(E, B) << 1;
            pattern |= diff(E, C) << 2;
            pattern |= diff(E, D) << 3;
            pattern |= diff(E, F) << 4;
            pattern |= diff(E, G) << 5;
            pattern |= diff(E, H) << 6;
            pattern |= diff(E, I) << 7;

            *(out0 + 0) = blend(hqTable[pattern], E, A, B, D, F, H); pattern = rotTable[pattern];
            *(out0 + 1) = blend(hqTable[pattern], E, C, F, B, H, D); pattern = rotTable[pattern];
            *(out1 + 1) = blend(hqTable[pattern], E, I, H, F, D, B); pattern = rotTable[pattern];
            *(out1 + 0) = blend(hqTable[pattern], E, G, D, H, B, F);

            in++;
            out0 += 2;
            out1 += 2;
        }
    }
}

static void pix2ycc(float c[3], int C)
{
    int r = d_8to24table[C] & 255;
    int g = (d_8to24table[C] >> 8) & 255;
    int b = (d_8to24table[C] >> 16) & 255;

    c[0] = r *  0.299f + g *  0.587f + b *  0.114f;
    c[1] = r * -0.169f + g * -0.331f + b *  0.499f + 128.0f;
    c[2] = r *  0.499f + g * -0.418f + b * -0.081f + 128.0f;
}

void HQ2x_Init(void)
{
    int n;

    for (n = 0; n < 256; n++) {
        rotTable[n] = ((n >> 2) & 0x11) | ((n << 2) & 0x88)
                    | ((n & 0x01) << 5) | ((n & 0x08) << 3)
                    | ((n & 0x10) >> 3) | ((n & 0x80) >> 5);
    }

    memset(equBitmap, 0, sizeof(equBitmap));

    for (n = 0; n < 65536; n++) {
        int A = n >> 8;
        int B = n & 255;
        float a[3];
        float b[3];

        if (A == 255 && B == 255) {
            Q_SetBit(equBitmap, n);
            continue;
        }

        if (A == 255 || B == 255)
            continue;

        pix2ycc(a, A);
        pix2ycc(b, B);

        if (fabs(a[0] - b[0]) > 0x30)
            continue;
        if (fabs(a[1] - b[1]) > 0x07)
            continue;
        if (fabs(a[2] - b[2]) > 0x06)
            continue;

        Q_SetBit(equBitmap, n);
    }
}
