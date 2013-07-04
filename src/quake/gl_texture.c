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
// gl_texture.c - GL texture management

#include "gl_local.h"
#include "rc_image.h"
#include "crc.h"
#include "version.h"

cvar_t	gl_picmip = {"gl_picmip", "0"};

#define MAX_STACK_PIXELS (256 * 256)

gltexture_t	*active_gltextures, *free_gltextures;
int			numgltextures;

gltexture_t *notexture, *nulltexture, *particletexture;

unsigned int d_8to24table[256];
unsigned int d_8to24table_rgba[256];
unsigned int d_8to24table_fbright[256];
unsigned int d_8to24table_nobright[256];

/*
================================================================================

	COMMANDS

================================================================================
*/

typedef struct
{
	int	magfilter;
	int minfilter;
	char *name;
} glmode_t;
glmode_t modes[] = {
	{GL_NEAREST, GL_NEAREST,				"GL_NEAREST"},
	{GL_NEAREST, GL_NEAREST_MIPMAP_NEAREST,	"GL_NEAREST_MIPMAP_NEAREST"},
	{GL_NEAREST, GL_NEAREST_MIPMAP_LINEAR,	"GL_NEAREST_MIPMAP_LINEAR"},
	{GL_LINEAR,  GL_LINEAR,					"GL_LINEAR"},
	{GL_LINEAR,  GL_LINEAR_MIPMAP_NEAREST,	"GL_LINEAR_MIPMAP_NEAREST"},
	{GL_LINEAR,  GL_LINEAR_MIPMAP_LINEAR,	"GL_LINEAR_MIPMAP_LINEAR"},
};
#define NUM_GLMODES 6
int gl_texturemode = 2;

void GL_PixelStore (int rowalign, int rowlen)
{
	static int oldrowalign = -1;
	static int oldrowlen = -1;

	if (oldrowalign != rowalign)
	{
		qglPixelStorei (GL_UNPACK_ALIGNMENT, rowalign);
		oldrowalign = rowalign;
	}

	if (oldrowlen != rowlen)
	{
		qglPixelStorei (GL_UNPACK_ROW_LENGTH, rowlen);
		oldrowlen = rowlen;
	}
}


/*
===============
TexMgr_SetFilterModes
===============
*/
static void TexMgr_SetFilterModes (gltexture_t *glt)
{
	GL_BindTexture (GL_TEXTURE0_ARB, glt);

	if (glt->flags & TEXPREF_NEAREST)
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
	else if (glt->flags & TEXPREF_LINEAR)
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	else if (glt->flags & TEXPREF_MIPMAP)
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, modes[gl_texturemode].magfilter);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, modes[gl_texturemode].minfilter);
	}
	else
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, modes[gl_texturemode].magfilter);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, modes[gl_texturemode].magfilter);
	}
}

/*
===============
TexMgr_TextureMode_f
===============
*/
void TexMgr_TextureMode_f (void)
{
	gltexture_t	*glt;
	char *arg;
	int i;

	switch (Cmd_Argc())
	{
	case 1:
		Com_Printf ("\"gl_texturemode\" is \"%s\"\n", modes[gl_texturemode].name);
		break;

	case 2:
		arg = Cmd_Argv(1);
		if (arg[0] == 'G' || arg[0] == 'g')
		{
			for (i=0; i<NUM_GLMODES; i++)
			{
				if (!stricmp (modes[i].name, arg))
				{
					gl_texturemode = i;
					goto stuff;
				}
			}
			Com_Printf ("\"%s\" is not a valid texturemode\n", arg);
			return;
		}
		else if (arg[0] >= '0' && arg[0] <= '9')
		{
			i = atoi(arg);
			if (i > NUM_GLMODES || i < 1)
			{
				Com_Printf ("\"%s\" is not a valid texturemode\n", arg);
				return;
			}
			gl_texturemode = i - 1;
		}
		else
			Com_Printf ("\"%s\" is not a valid texturemode\n", arg);

stuff:
		for (glt=active_gltextures; glt; glt=glt->next)
			TexMgr_SetFilterModes (glt);

		break;
	default:
		Com_Printf ("usage: gl_texturemode <mode>\n");
		break;
	}
}

/*
===============
TexMgr_Imagelist_f

report loaded textures
===============
*/
void TexMgr_Imagelist_f (void)
{
	float mb;
	int texels = 0;
	gltexture_t	*glt;

	for (glt=active_gltextures; glt; glt=glt->next)
	{
		Com_Printf ("   %4i x%4i %s\n", glt->width, glt->height, glt->name);
		texels += glt->width * glt->height;
	}

	mb = (float)texels * 4 / 0x100000;
	Com_Printf ("%i textures %i pixels %1.1f megabytes\n", numgltextures, texels, mb);
}


/*
================================================================================

	TEXTURE MANAGER

================================================================================
*/

/*
================
TexMgr_FindTexture
================
*/
gltexture_t *TexMgr_FindTexture (model_t *owner, char *name)
{
	gltexture_t	*glt;

	if (name)
	{
		for (glt=active_gltextures; glt; glt=glt->next)
			if (glt->owner == owner && !strcmp (glt->name, name))
				return glt;
	}

	return NULL;
}

/*
================
TexMgr_NewTexture
================
*/
gltexture_t *TexMgr_NewTexture (void)
{
	gltexture_t *glt;

	if (!free_gltextures)
	{
		free_gltextures = (gltexture_t *) malloc (sizeof (gltexture_t));
		memset (free_gltextures, 0, sizeof (gltexture_t));
		free_gltextures->next = NULL;
	}

	glt = free_gltextures;
	free_gltextures = glt->next;
	glt->next = active_gltextures;
	active_gltextures = glt;

	qglGenTextures (1, (GLuint *) &glt->texnum);
	numgltextures++;
	return glt;
}

/*
================
TexMgr_FreeTexture
================
*/
void TexMgr_FreeTexture (gltexture_t *kill)
{
	gltexture_t *glt;

	if (kill == NULL)
		return;

	if (active_gltextures == kill)
	{
		active_gltextures = kill->next;
		kill->next = free_gltextures;
		free_gltextures = kill;

		qglDeleteTextures (1, (const GLuint *) &kill->texnum);
		numgltextures--;
		return;
	}

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		if (glt->next == kill)
		{
			glt->next = kill->next;
			kill->next = free_gltextures;
			free_gltextures = kill;

			qglDeleteTextures (1, (const GLuint *) &kill->texnum);
			numgltextures--;
			return;
		}
	}
}

/*
================
TexMgr_FreeTextures

compares each bit in "flags" to the one in glt->flags only if that bit is active in "mask"
================
*/
void TexMgr_FreeTextures (int flags, int mask)
{
	gltexture_t *glt, *next;

	for (glt = active_gltextures; glt; glt = next)
	{
		next = glt->next;

		if ((glt->flags & mask) == (flags & mask))
			TexMgr_FreeTexture (glt);
	}
}

/*
================
TexMgr_FreeTexturesForOwner
================
*/
void TexMgr_FreeTexturesForOwner (model_t *owner)
{
	gltexture_t *glt, *next;

	for (glt = active_gltextures; glt; glt = next)
	{
		next = glt->next;

		if (glt && glt->owner == owner)
			TexMgr_FreeTexture (glt);
	}
}


/*
================================================================================

	IMAGE LOADING

================================================================================
*/

static inline unsigned npot32(unsigned k)
{
    if (k == 0)
        return 1;

    k--;
    k = k | (k >> 1);
    k = k | (k >> 2);
    k = k | (k >> 4);
    k = k | (k >> 8);
    k = k | (k >> 16);

    return k + 1;
}

static qbool makePowerOfTwo(int *width, int *height)
{
    if (!(*width & (*width - 1)) && !(*height & (*height - 1)))
        return true;   // already power of two

    if (gl_support_texture_npot && (qglGenerateMipmap != NULL))
        return false;  // assume full NPOT texture support

    *width = npot32(*width);
    *height = npot32(*height);
    return false;
}

/*
================
mipMap

Operates in place, quartering the size of the texture
================
*/
static void mipMap(byte *out, byte *in, int width, int height)
{
    int     i, j;

    width <<= 2;
    height >>= 1;
    for (i = 0; i < height; i++, in += width)
	{
        for (j = 0; j < width; j += 8, out += 4, in += 8)
		{
            out[0] = (in[0] + in[4] + in[width + 0] + in[width + 4]) >> 2;
            out[1] = (in[1] + in[5] + in[width + 1] + in[width + 5]) >> 2;
            out[2] = (in[2] + in[6] + in[width + 2] + in[width + 6]) >> 2;
            out[3] = (in[3] + in[7] + in[width + 3] + in[width + 7]) >> 2;
        }
    }
}


/*
================
resampleTexture 
================
*/
static void resampleTexture (const byte *in, int inwidth, int inheight, byte *out, int outwidth, int outheight)
{
	int i, j;
    const byte  *inrow1, *inrow2;
    unsigned    frac, fracstep;
    unsigned    p1[MAX_TEXTURE_SIZE], p2[MAX_TEXTURE_SIZE];
    const byte  *pix1, *pix2, *pix3, *pix4;
    float       heightScale;

    if (outwidth > MAX_TEXTURE_SIZE)
        Sys_Error("resampleTexture: outwidth > %d", MAX_TEXTURE_SIZE);

    fracstep = inwidth * 0x10000 / outwidth;

    frac = fracstep >> 2;
    for (i = 0; i < outwidth; i++)
	{
        p1[i] = 4 * (frac >> 16);
        frac += fracstep;
    }
    frac = 3 * (fracstep >> 2);
    for (i = 0; i < outwidth; i++)
	{
        p2[i] = 4 * (frac >> 16);
        frac += fracstep;
    }

    heightScale = (float)inheight / outheight;
    inwidth <<= 2;
    for (i = 0; i < outheight; i++)
	{
        inrow1 = in + inwidth * (int)((i + 0.25f) * heightScale);
        inrow2 = in + inwidth * (int)((i + 0.75f) * heightScale);
        for (j = 0; j < outwidth; j++)
		{
            pix1 = inrow1 + p1[j];
            pix2 = inrow1 + p2[j];
            pix3 = inrow2 + p1[j];
            pix4 = inrow2 + p2[j];
            out[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
            out[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
            out[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
            out[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
            out += 4;
        }
    }
}


/*
================
TexMgr_LoadImage32 

handles 32bit source data
================
*/
void TexMgr_LoadImage32 (gltexture_t *glt, byte *data)
{
	byte        *scaled;
    int         scaled_width, scaled_height;
    qbool		power_of_two;

    scaled_width = glt->width;
    scaled_height = glt->height;
    power_of_two = makePowerOfTwo(&scaled_width, &scaled_height);

	if ((glt->flags & TEXPREF_MIPMAP) && !(glt->flags & TEXPREF_NOPICMIP))
	{
        // let people sample down the textures for speed
        scaled_width >>= (int)gl_picmip.value;
        scaled_height >>= (int)gl_picmip.value;
    }

    // don't ever bother with > 256 textures
    while (scaled_width > gl_maxtexturesize || scaled_height > gl_maxtexturesize)
	{
        scaled_width >>= 1;
        scaled_height >>= 1;
    }

    if (scaled_width < 1)
        scaled_width = 1;
    if (scaled_height < 1)
        scaled_height = 1;

	if (scaled_width == glt->width && scaled_height == glt->height)
	{
        // optimized case, do nothing
        scaled = data;
	}
	else if (power_of_two)
	{
        // optimized case, use faster mipmap operation
        scaled = data;
        while (glt->width > scaled_width || glt->height > scaled_height)
		{
            mipMap (scaled, scaled, glt->width, glt->height);
            glt->width >>= 1;
            glt->height >>= 1;
        }
	}
	else
	{
        scaled = malloc (scaled_width * scaled_height * 4);
        resampleTexture (data, glt->width, glt->height, scaled, scaled_width, scaled_height);
    }

	// upload
	GL_BindTexture (GL_TEXTURE0_ARB, glt);
	qglEnable (GL_TEXTURE_2D);	// some ATI drivers might need this... http://www.opengl.org/wiki/Common_Mistakes#Automatic_mipmap_generation
	GL_PixelStore (4, 0);

	qglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, scaled_width, scaled_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, scaled);

	// upload mipmaps
	if (glt->flags & TEXPREF_MIPMAP)
	{
		if (qglGenerateMipmap != NULL) {
			qglGenerateMipmap (GL_TEXTURE_2D);
		} else {
			int miplevel = 0;

			while (scaled_width > 1 || scaled_height > 1)
			{
				mipMap (scaled, scaled, scaled_width, scaled_height);

				scaled_width = max (scaled_width >> 1, 1);
				scaled_height = max (scaled_height >> 1, 1);

				miplevel++;
				qglTexImage2D (GL_TEXTURE_2D, miplevel, GL_RGBA, scaled_width, scaled_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, scaled);
			}
		}
	}

	// set filter
	TexMgr_SetFilterModes (glt);
	
	if (scaled != data)
        free (scaled);
}

/*
================
TexMgr_LoadImage8 

handles 8bit source data, then passes it to LoadImage32
================
*/
void TexMgr_LoadImage8 (gltexture_t *glt, byte *data)
{
	byte	padbyte;
	unsigned int *usepal;
	byte    stackbuf[MAX_STACK_PIXELS * 4];
    byte    *buffer, *dest;
    int     i, s, p;

	// HACK HACK HACK -- taken from tomazquake
	if (strstr(glt->name, "shot1sid") && glt->width==32 && glt->height==32 && CRC_Block(data, 1024) == 65393)
	{
		// This texture in b_shell1.bsp has some of the first 32 pixels painted white.
		// They are invisible in software, but look really ugly in GL. So we just copy
		// 32 pixels from the bottom to make it look nice.
		memcpy (data, data + 32*31, 32);
	}

	// choose palette and padbyte
	if (glt->flags & TEXPREF_FULLBRIGHT)
	{
		usepal = d_8to24table_fbright;
		padbyte = 0;
	}
	else if (glt->flags & TEXPREF_NOBRIGHT && gl_fullbrights.value)
	{
		usepal = d_8to24table_nobright;
		padbyte = 0;
	}
	else
	{
		usepal = d_8to24table;
		padbyte = 255;
	}

    s = glt->width * glt->height;
    if (s > MAX_STACK_PIXELS)
        buffer = malloc(s * 4);
    else
        buffer = stackbuf;

    dest = buffer;
    for (i = 0; i < s; i++)
	{
        p = data[i];
        *(unsigned long int *)dest = usepal[p];

        if (p == padbyte)
		{
            // transparent, so scan around for another color
            // to avoid alpha fringes
            // FIXME: do a full flood fill so mips work...
            if (i > glt->width && data[i - glt->width] != padbyte)
                p = data[i - glt->width];
            else if (i < s - glt->width && data[i + glt->width] != padbyte)
                p = data[i + glt->width];
            else if (i > 0 && data[i - 1] != padbyte)
                p = data[i - 1];
            else if (i < s - 1 && data[i + 1] != padbyte)
                p = data[i + 1];
            else
                p = 0;

            // copy rgb components
            dest[0] = ((byte *)&d_8to24table[p])[0];
            dest[1] = ((byte *)&d_8to24table[p])[1];
            dest[2] = ((byte *)&d_8to24table[p])[2];
        }

        dest += 4;
    }

	// upload it
	TexMgr_LoadImage32 (glt, buffer);

	if (s > MAX_STACK_PIXELS)
        free(buffer);
}

/*
================
TexMgr_LoadLightmap

handles lightmap data
================
*/
void TexMgr_LoadLightmap (gltexture_t *glt, byte *data)
{
	extern GLenum gl_Lightmap_Type;
	extern GLenum gl_Lightmap_Format;

	// upload it
	GL_BindTexture (GL_TEXTURE0_ARB, glt);

	// internal format can't be bgra but format can be
	GL_PixelStore (4, 0);

	qglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, glt->width, glt->height, 0, gl_Lightmap_Format, gl_Lightmap_Type, data);

	// set filter modes
	TexMgr_SetFilterModes (glt);
}

/*
================
TexMgr_LoadImage

the one entry point for loading all textures
================
*/
gltexture_t *TexMgr_LoadImage (model_t *owner, char *name, int width, int height, enum srcformat format,
							   byte *data, char *source_file, unsigned source_offset, unsigned flags)
{
	unsigned short crc = 0;
	gltexture_t *glt = NULL;
	int mark = 0;

	// cache check
	switch (format)
	{
		case SRC_INDEXED:
			crc = CRC_Block(data, width * height);
			break;
		case SRC_LIGHTMAP:
		case SRC_RGBA:
			crc = CRC_Block(data, width * height * 4);
			break;
	}

	if ((flags & TEXPREF_OVERWRITE) && (glt = TexMgr_FindTexture (owner, name)))
	{
		if (glt->source_crc == crc)
			return glt;
	}
	else
	{
		// setup new texture
		glt = TexMgr_NewTexture ();
	}

	// copy data
	glt->owner = owner;
	strncpy (glt->name, name, sizeof(glt->name));
	glt->width = width;
	glt->height = height;
	glt->flags = flags;
	strncpy (glt->source_file, source_file, sizeof(glt->source_file));
	glt->source_offset = source_offset;
	glt->source_format = format;
	glt->source_width = width;
	glt->source_height = height;
	glt->source_crc = crc;
	glt->shirt = -1;
	glt->pants = -1;

	// upload it
	mark = Hunk_LowMark();

	switch (glt->source_format)
	{
		case SRC_INDEXED:
			TexMgr_LoadImage8 (glt, data);
			break;
		case SRC_LIGHTMAP:
			TexMgr_LoadLightmap (glt, data);
			break;
		case SRC_RGBA:
			TexMgr_LoadImage32 (glt, data);
			break;
	}

	Hunk_FreeToLowMark(mark);

	return glt;
}

/*
================================================================================

	COLORMAPPING AND TEXTURE RELOADING

================================================================================
*/

/*
================
TexMgr_ReloadImage

reloads a texture, and colormaps it if needed
================
*/
void TexMgr_ReloadImage (gltexture_t *glt, int shirt, int pants)
{
	byte	translation[256];
	byte	*src, *dst, *data = NULL, *translated;
	int		mark, size, i;

	// get source data
	mark = Hunk_LowMark ();

	if (glt->source_file[0] && glt->source_offset)
	{
		// lump inside file
		data = FS_LoadFile (glt->source_file, true, NULL);

		if (!data)
			goto invalid;

		data += glt->source_offset;
	}
	else if (glt->source_file[0] && !glt->source_offset)
		data = Image_LoadImage (glt->source_file, (int *) &glt->source_width, (int *) &glt->source_height); // simple file
	else if (!glt->source_file[0] && glt->source_offset)
		data = (byte *) glt->source_offset; // image in memory

	if (!data)
	{
invalid:
		Com_Printf ("TexMgr_ReloadImage: invalid source for %s\n", glt->name);
		Hunk_FreeToLowMark (mark);
		return;
	}

	glt->width = glt->source_width;
	glt->height = glt->source_height;

	// apply shirt and pants colors
	// if shirt and pants are -1,-1, use existing shirt and pants colors
	// if existing shirt and pants colors are -1,-1, don't bother colormapping
	if (shirt > -1 && pants > -1)
	{
		if (glt->source_format == SRC_INDEXED)
		{
			glt->shirt = shirt;
			glt->pants = pants;
		}
		else
		{
			Com_DPrintf ("TexMgr_ReloadImage: can't colormap a non SRC_INDEXED texture: %s\n", glt->name);
		}
	}

	if (glt->shirt > -1 && glt->pants > -1)
	{
		// create new translation table
		for (i = 0; i < 256; i++)
			translation[i] = i;

		shirt = glt->shirt * 16;

		if (shirt < 128)
			for (i = 0; i < 16; i++)
				translation[TOP_RANGE+i] = shirt + i;
		else
			for (i = 0; i < 16; i++)
				translation[TOP_RANGE+i] = shirt + 15 - i;

		pants = glt->pants * 16;

		if (pants < 128)
			for (i = 0; i < 16; i++)
				translation[BOTTOM_RANGE+i] = pants + i;
		else
			for (i = 0; i < 16; i++)
				translation[BOTTOM_RANGE+i] = pants + 15 - i;

		// translate texture
		size = glt->width * glt->height;
		dst = translated = (byte *) Hunk_Alloc (size);
		src = data;

		for (i = 0; i < size; i++)
			*dst++ = translation[*src++];

		data = translated;
	}

	// upload it
	switch (glt->source_format)
	{
	case SRC_INDEXED:
		TexMgr_LoadImage8 (glt, data);
		break;
	case SRC_LIGHTMAP:
		TexMgr_LoadLightmap (glt, data);
		break;
	case SRC_RGBA:
		TexMgr_LoadImage32 (glt, data);
		break;
	}

	Hunk_FreeToLowMark (mark);
}



/*
================================================================================

INIT/SHUTDOWN

================================================================================
*/

/*
=================
TexMgr_LoadPalette
=================
*/
static void TexMgr_LoadPalette (void)
{
	byte *pal, *src, *dst;
	int i, mark;
	qfile_t *f;

	f = FS_Open ("gfx/palette.lmp", "rb", false, false);
	if (!f)
		Sys_Error ("Couldn't load gfx/palette.lmp");

	mark = Hunk_LowMark ();
	pal = Hunk_Alloc (768);
	FS_Read (f, pal, 768);
	FS_Close(f);

	// standard palette, 255 is transparent
	dst = (byte *) d_8to24table;
	src = pal;

	for (i = 0; i < 256; i++)
	{
		dst[2] = *src++;
		dst[1] = *src++;
		dst[0] = *src++;
		dst[3] = 255;
		dst += 4;
	}

	// prevent pink fringes by setting alpha to black
	d_8to24table[255] = 0;

	// rgba table for particles and anything else that needs to index the palette explicitly
	dst = (byte *) d_8to24table_rgba;
	src = pal;

	for (i = 0; i < 256; i++)
	{
		dst[0] = *src++;
		dst[1] = *src++;
		dst[2] = *src++;
		dst[3] = 255;
		dst += 4;
	}

	// prevent pink fringes by setting alpha to black
	d_8to24table[255] = 0;

	// fullbright palette, 0-223 are black (for additive blending)
	src = pal + 224 * 3;
	dst = (byte *) (d_8to24table_fbright) + 224 * 4;

	for (i = 224; i < 256; i++)
	{
		dst[2] = *src++;
		dst[1] = *src++;
		dst[0] = *src++;
		dst[3] = 255;
		dst += 4;
	}

	for (i = 0; i < 224; i++)
	{
		// comply with strict aliasing
		byte *rgba = (byte *) &d_8to24table_fbright[i];

		rgba[0] = 0;
		rgba[1] = 0;
		rgba[2] = 0;
		rgba[3] = 255;
	}

	// nobright palette, 224-255 are black (for additive blending)
	dst = (byte *) d_8to24table_nobright;
	src = pal;

	for (i = 0; i < 256; i++)
	{
		dst[2] = *src++;
		dst[1] = *src++;
		dst[0] = *src++;
		dst[3] = 255;
		dst += 4;
	}

	for (i = 224; i < 256; i++)
	{
		// comply with strict aliasing
		byte *rgba = (byte *) &d_8to24table_nobright[i];

		rgba[0] = 0;
		rgba[1] = 0;
		rgba[2] = 0;
		rgba[3] = 255;
	}

	// correct alpha colour
	d_8to24table_nobright[255] = 0;
	d_8to24table_fbright[255] = 0;

	Hunk_FreeToLowMark (mark);
}

static void TexMgr_InitParticleTexture (void)
{
	int			x,y;
	static byte	data[2*2*4];
	byte		*dst;

	dst = data;
	for (x=0 ; x<2 ; x++)
	{
		for (y=0 ; y<2 ; y++)
		{
			*dst++ = 255;
			*dst++ = 255;
			*dst++ = 255;
			*dst++ = x || y ? 0 : 255;
		}
	}

	particletexture = TexMgr_LoadImage (NULL, "particle", 2, 2, SRC_RGBA, data, "", (unsigned)data, TEXPREF_NEAREST);
}

/*
================================================================================

	TEXTURE BINDING / TEXTURE UNIT SWITCHING

================================================================================
*/

int	r_currenttexture[3] = {-1, -1, -1}; // to avoid unnecessary texture sets

// default tmu if qglActiveTexture has never been called
GLenum r_currenttmu = GL_TEXTURE0_ARB;
static int r_lastmode[3] = {GL_NONE, GL_NONE, GL_NONE};
static GLenum r_lasttype[3] = {GL_TEXTURE_2D, GL_TEXTURE_2D, GL_TEXTURE_2D};
static int r_lastscale[3] = {-1, -1, -1};

void GL_ActiveTexture (GLenum tmu)
{
	if (r_currenttmu == tmu)
		return;

	r_currenttmu = tmu;
	qglActiveTexture (tmu);
}


void GL_TextureType (GLenum tmu, GLenum type)
{
	int tmunum = tmu - GL_TEXTURE0_ARB;

	if (type != r_lasttype[tmunum])
	{
		GL_ActiveTexture (tmu);

		// only disable if there is something to enable
		if (r_lasttype[tmunum] != GL_NONE)
			qglDisable (r_lasttype[tmunum]);

		qglEnable (type);

		// update type
		r_lasttype[tmunum] = type;

		// force a texenv update irrespective of whether or not it's changed
		r_lastmode[tmunum] = GL_INVALID_VALUE;
		r_lastscale[tmunum] = -1;

		// likewise a texture update
		r_currenttexture[tmunum] = -1;
	}
}


void GL_TexEnv (GLenum tmu, GLenum type, GLenum mode)
{
	int scale = 1;
	int tmunum = (int) tmu - (int) GL_TEXTURE0_ARB;

	// activate the texture unit
	GL_ActiveTexture (tmu);
	GL_TextureType (tmu, type);

	if (mode != r_lastmode[tmunum])
	{
		// enable texturing if it was previously disabled
		if (r_lastmode[tmunum] == GL_NONE)
			qglEnable (GL_TEXTURE_2D);

		if (mode == GL_NONE)
		{
			// call recursively to revert tmu to default state
			GL_TexEnv (tmu, type, GL_REPLACE);
			qglDisable (GL_TEXTURE_2D);
			r_lasttype[tmunum] = GL_NONE;	// force a ren-enable of whatever the type was by setting this to an invalid type
			scale = 1;
		}
		else if (mode == GL_RGB_SCALE_ARB)
		{
			qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			qglTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
			qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
			scale = 2;
		}
		else if (mode == GL_INTERPOLATE_ARB)
		{
			qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			qglTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_INTERPOLATE_ARB);
			qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PRIMARY_COLOR_ARB);
			qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_CONSTANT_ARB);
			scale = 1;
		}
		else
		{
			qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode);
			scale = 1;
		}

		r_lastmode[tmunum] = mode;
	}

	if (scale != r_lastscale[tmunum])
	{
		qglTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, (float) scale);
		r_lastscale[tmunum] = scale;
	}
}


/*
================
GL_BindTexture

handles switching textures between different tmus more logically
================
*/
void GL_BindTexture (GLenum tmu, gltexture_t *texture)
{
	int tmunum = 0;

	tmunum = tmu - GL_TEXTURE0_ARB;
	GL_TextureType (tmu, GL_TEXTURE_2D);

	if (!texture)
	{
		r_currenttexture[tmunum] = -1;
	}
	else if (texture->texnum != r_currenttexture[tmunum])
	{
		// activate the correct tmu
		GL_ActiveTexture (tmu);

		r_currenttexture[tmunum] = texture->texnum;
		qglBindTexture (GL_TEXTURE_2D, texture->texnum);
		texture->visframe = r_framecount;
	}
}


// convenience; just unbinds from current textures so that the next GL_BindTexture calls will force an update
// use whenever we want to call glBindTexture ourselves
void GL_UnbindTextures (void)
{
	// unbind all textures
	r_currenttexture[0] = -1;
	r_currenttexture[1] = -1;
	r_currenttexture[2] = -1;

	// unbind cached environments
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_NONE);
	GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_NONE);
	GL_TexEnv (GL_TEXTURE2_ARB, GL_TEXTURE_2D, GL_NONE);

	// back to TMU0
	GL_ActiveTexture (GL_TEXTURE0_ARB);
}


static void r_textures_start(void)
{
	static byte notexture_data[16] = {159,91,83,255,0,0,0,255,0,0,0,255,159,91,83,255}; // black and pink checker
	static byte nulltexture_data[16] = {127, 191, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255, 127, 191, 255, 255}; // black and blue checker
	extern texture_t *r_notexture_mip, *r_notexture_mip2;

	// load palette
	TexMgr_LoadPalette ();

	// load notexture images
	notexture = TexMgr_LoadImage (NULL, "notexture", 2, 2, SRC_RGBA, notexture_data, "", (unsigned)notexture_data, TEXPREF_NEAREST | TEXPREF_NOPICMIP);
	nulltexture = TexMgr_LoadImage (NULL, "nulltexture", 2, 2, SRC_RGBA, nulltexture_data, "", (unsigned)nulltexture_data, TEXPREF_NEAREST | TEXPREF_NOPICMIP);

	// have to assign these here becuase Mod_Init is called before TexMgr_Init
	r_notexture_mip->gltexture = r_notexture_mip2->gltexture = notexture;

	// generate particle images
	TexMgr_InitParticleTexture ();
}

static void r_textures_shutdown(void)
{
	gltexture_t *kill;

	// clear out the texture list
	while (active_gltextures)
	{
		kill = active_gltextures;

		numgltextures--;
		qglDeleteTextures(1, &kill->texnum);

		active_gltextures = active_gltextures->next;
		kill->next = free_gltextures;
		free_gltextures = kill;
	}
}

static void r_textures_newmap(void)
{
}

/*
================
TexMgr_Init

must be called before any texture loading
================
*/
void TexMgr_Init (void)
{
	Cvar_Register (&gl_picmip);

	Cmd_AddCommand ("gl_texturemode", &TexMgr_TextureMode_f);
	Cmd_AddCommand ("imagelist", &TexMgr_Imagelist_f);

	// init texture list
	free_gltextures = NULL;
	active_gltextures = NULL;
	numgltextures = 0;

	R_RegisterModule("R_Textures", r_textures_start, r_textures_shutdown, r_textures_newmap);
}
