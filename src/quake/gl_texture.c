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
int		numgltextures;

gltexture_t *particletexture;

unsigned	d_8to24table[256];

//====================================================================

int	currenttexture = -1;

void GL_SelectTexture ( GLenum target )
{
	static GLenum currenttarget;
	static int ct0, ct1;

	if ( target == currenttarget )
		return;

	if(qglActiveTexture)
		qglActiveTexture (target);

	if ( target == GL_TEXTURE0_ARB )
	{
		ct1 = currenttexture;
		currenttexture = ct0;
	}
	else
	{
		ct0 = currenttexture;
		currenttexture = ct1;
	}

	currenttarget = target;
}

void GL_Bind ( int texnum )
{
	if ( currenttexture == texnum )
		return;

	currenttexture = texnum;
	qglBindTexture (GL_TEXTURE_2D, texnum);
}

void GL_MBind( GLenum target, int texnum )
{
	GL_SelectTexture( target );
	GL_Bind( texnum );
}

qbool mtexenabled = false;

/*
================
GL_DisableMultitexture -- selects texture unit 0
================
*/
void GL_DisableMultitexture(void)
{
	if (mtexenabled)
	{
		qglDisable(GL_TEXTURE_2D);
		GL_SelectTexture(GL_TEXTURE0_ARB);
		mtexenabled = false;
	}
}

/*
================
GL_EnableMultitexture -- selects texture unit 1
================
*/
void GL_EnableMultitexture(void)
{
	GL_SelectTexture(GL_TEXTURE1_ARB);
	qglEnable(GL_TEXTURE_2D);
	mtexenabled = true;
}

//====================================================================

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

/*
===============
TexMgr_SetFilterModes
===============
*/
static void TexMgr_SetFilterModes (gltexture_t *glt)
{
	if (glt->flags & TEXPREF_MIPMAP)
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, modes[gl_texturemode].magfilter);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, modes[gl_texturemode].minfilter);
	}
	else
	{
		if (glt->flags & TEXPREF_UPSCALE)
		{
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else if (glt->flags & TEXPREF_NEAREST)
		{
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		}
		else if (glt->flags & TEXPREF_LINEAR)
		{
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		}
		else
		{
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, modes[gl_texturemode].magfilter);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, modes[gl_texturemode].magfilter);
		}
	}

	if (glt->flags & TEXPREF_MIPMAP)
	{
        qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
	else
	{
		if (gl_support_clamptoedge)
		{
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		else
		{
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		}
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
		{
			GL_Bind(glt->texnum);
			TexMgr_SetFilterModes (glt);
		}

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

MANAGEMENT

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

	if (numgltextures == MAX_GLTEXTURES)
		Sys_Error("numgltextures == MAX_GLTEXTURES\n");

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
	GL_Bind (glt->texnum);
	qglTexImage2D (GL_TEXTURE_2D, glt->baselevel, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);

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
				qglTexImage2D (GL_TEXTURE_2D, miplevel, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
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

    s = glt->width * glt->height;
    if (s > MAX_STACK_PIXELS)
        buffer = malloc(s * 4);
    else
        buffer = stackbuf;

    dest = buffer;
    for (i = 0; i < s; i++)
	{
        p = data[i];
        *(unsigned long int *)dest = d_8to24table[p];

        if (p == 255)
		{
            // transparent, so scan around for another color
            // to avoid alpha fringes
            // FIXME: do a full flood fill so mips work...
            if (i > glt->width && data[i - glt->width] != 255)
                p = data[i - glt->width];
            else if (i < s - glt->width && data[i + glt->width] != 255)
                p = data[i + glt->width];
            else if (i > 0 && data[i - 1] != 255)
                p = data[i - 1];
            else if (i < s - 1 && data[i + 1] != 255)
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
TexMgr_LoadUpscaledImage8 

handles 8bit source data, then passes it to LoadImage32
================
*/
void TexMgr_LoadUpscaledImage8 (gltexture_t *glt, byte *data)
{
	byte *buffer;

	buffer = malloc(glt->width * glt->height * 16);

	// send it through hq2x
	HQ2x_Render((unsigned long int *)buffer, data, glt->width, glt->height);

	// upload scaled image
	glt->width *= 2;
	glt->height *= 2;
	TexMgr_LoadImage32 (glt, buffer);
    free(buffer);

	// upload original as mipmap level 1
	glt->baselevel = 1;
	glt->width /= 2;
	glt->height /= 2;
	TexMgr_LoadImage8(glt, data);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
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
	unsigned short crc;
	gltexture_t *glt = NULL;
	int mark;

	// cache check
	switch (format)
	{
		case SRC_INDEXED_UPSCALE:
		case SRC_INDEXED:
			crc = CRC_Block(data, width * height);
			break;
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
	glt->baselevel = 0;
	strncpy (glt->source_file, source_file, sizeof(glt->source_file));
	glt->source_offset = source_offset;
	glt->source_format = format;
	glt->source_width = width;
	glt->source_height = height;
	glt->source_crc = crc;

	// upload it
	mark = Hunk_LowMark();

	switch (glt->source_format)
	{
		case SRC_INDEXED:
			TexMgr_LoadImage8 (glt, data);
			break;
		case SRC_INDEXED_UPSCALE:
			TexMgr_LoadUpscaledImage8 (glt, data);
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
	unsigned v;
	unsigned *table;
	byte r,g,b;
	byte *pal;
	int i, mark;
	FILE *f;

	FS_FOpenFile ("gfx/palette.lmp", &f);
	if (!f)
		Sys_Error ("Couldn't load gfx/palette.lmp");

	mark = Hunk_LowMark ();

	pal = Hunk_Alloc (768);
	fread (pal, 1, 768, f);
	fclose(f);

	table = d_8to24table;
	for (i=0 ; i<256 ; i++, pal+=3)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		v = (r<<0) + (g<<8) + (b<<16) + (255<<24);
		*table++ = v;
	}
	d_8to24table[255] &= 0xffffff;	// 255 is transparent

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

static void r_textures_start(void)
{
	static byte notexture_data[16] = {159,91,83,255,0,0,0,255,0,0,0,255,159,91,83,255}; // black and pink checker

	// load palette
	TexMgr_LoadPalette ();

	// load notexture image
	notexture = TexMgr_LoadImage (NULL, "notexture", 2, 2, SRC_RGBA, notexture_data, "", (unsigned)notexture_data, TEXPREF_NEAREST);

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
	int i;

	Cvar_Register (&gl_picmip);

	Cmd_AddCommand ("gl_texturemode", &TexMgr_TextureMode_f);
	Cmd_AddCommand ("imagelist", &TexMgr_Imagelist_f);

	HQ2x_Init();

	// init texture list
	free_gltextures = (gltexture_t *) Hunk_AllocName (MAX_GLTEXTURES * sizeof(gltexture_t), "gltextures");
	active_gltextures = NULL;
	for (i=0; i<MAX_GLTEXTURES; i++)
		free_gltextures[i].next = &free_gltextures[i+1];
	free_gltextures[i].next = NULL;
	numgltextures = 0;

	R_RegisterModule("R_Textures", r_textures_start, r_textures_shutdown, r_textures_newmap);
}
