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
#include "crc.h"
#include "version.h"

cvar_t	gl_picmip = {"gl_picmip", "0"};

int		gl_hardware_maxsize;

gltexture_t	*active_gltextures, *free_gltextures;
int		numgltextures;

gltexture_t *particletexture;

unsigned	d_8to24table[256];

//====================================================================

int	currenttexture = -1;

#ifdef _WIN32
lpMTexFUNC qglMultiTexCoord2f = NULL;
lpSelTexFUNC qglActiveTexture = NULL;
#endif

void GL_Bind (int texnum)
{
	if (currenttexture == texnum)
		return;

	currenttexture = texnum;
	glBindTexture (GL_TEXTURE_2D, texnum);
}

void GL_SelectTexture (GLenum target)
{
	static GLenum currenttarget;
	static int ct0, ct1;

	if (target == currenttarget)
		return;

	qglActiveTexture (target);

	if (target == GL_TEXTURE0_ARB)
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
int gl_texturemode = 5; // bilinear

/*
===============
TexMgr_SetFilterModes
===============
*/
static void TexMgr_SetFilterModes (gltexture_t *glt)
{
	GL_Bind (glt->texnum);

	if (glt->flags & TEXPREF_NEAREST)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
	else if (glt->flags & TEXPREF_LINEAR)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	else if (glt->flags & TEXPREF_MIPMAP)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, modes[gl_texturemode].magfilter);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, modes[gl_texturemode].minfilter);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, modes[gl_texturemode].magfilter);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, modes[gl_texturemode].magfilter);
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

	glGenTextures (1, (GLuint *) &glt->texnum);
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

		glDeleteTextures (1, (const GLuint *) &kill->texnum);
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

			glDeleteTextures (1, (const GLuint *) &kill->texnum);
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

/*
================
TexMgr_Pad

return smallest power of two greater than or equal to x (but always at least 2)
================
*/
int TexMgr_Pad(int x)
{
	int i;
	for (i = 2; i < x; i<<=1);
	return i;
}

/*
===============
WrapCoords 

wrap lookup coords for AlphaEdgeFix
TODO: speed this up
===============
*/
static int WrapCoords(int x, int y, int width, int height)
{
	if (x < 0)
		x += width;
	else if (x > width-1)
		x -= width;

	if (y < 0)
		y += height;
	else if (y > height-1)
		y -= height;

	return y * width + x;
}

/*
===============
TexMgr_SafeTextureSize 

return a size with hardware and user prefs in mind
===============
*/
int TexMgr_SafeTextureSize (int s)
{
	s = TexMgr_Pad(s);
	s >>= (int)gl_picmip.value;
	if ((int)gl_hardware_maxsize > 0)
		s = min((int)gl_hardware_maxsize, s);
	return s;
}

/*
================
TexMgr_MipMap

Operates in place, quartering the size of the texture
================
*/
void TexMgr_MipMap (byte *in, int width, int height)
{
	int		i, j;
	byte	*out;

	width <<=2;
	height >>= 1;
	out = in;

	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=8, out+=4, in+=8)
		{
			out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
			out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
			out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
			out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
		}
	}
}

/*
================
TexMgr_ResampleTexture 

bilinear resample
================
*/
unsigned *TexMgr_ResampleTexture (unsigned *in, int inwidth, int inheight, qbool alpha)
{
	byte *nwpx, *nepx, *swpx, *sepx, *dest;
	unsigned xfrac, yfrac, x, y, modx, mody, imodx, imody, injump, outjump;
	unsigned *out;
	int i, j, outwidth, outheight;

	if (inwidth == TexMgr_Pad(inwidth) && inheight == TexMgr_Pad(inheight))
		return in;

	outwidth = TexMgr_Pad(inwidth);
	outheight = TexMgr_Pad(inheight);
	out = Hunk_Alloc(outwidth*outheight*4);

	xfrac = (inwidth << 8) / outwidth;
	yfrac = (inheight << 8) / outheight;
	y = outjump = 0;

	for (i=0; i<outheight; i++)
	{
		mody = y & 0xFF;
		imody = 256 - mody;
		injump = (y>>8) * inwidth;
		x = 0;

		for (j=0; j<outwidth; j++)
		{
			modx = x & 0xFF;
			imodx = 256 - modx;

			nwpx = (byte *)(in + (x>>8) + injump);
			nepx = nwpx + 4; 
			swpx = nwpx + inwidth*4;
			sepx = swpx + 4;

			dest = (byte *)(out + outjump + j);

			dest[0] = (nwpx[0]*imodx*imody + nepx[0]*modx*imody + swpx[0]*imodx*mody + sepx[0]*modx*mody)>>16;
			dest[1] = (nwpx[1]*imodx*imody + nepx[1]*modx*imody + swpx[1]*imodx*mody + sepx[1]*modx*mody)>>16;
			dest[2] = (nwpx[2]*imodx*imody + nepx[2]*modx*imody + swpx[2]*imodx*mody + sepx[2]*modx*mody)>>16;
			if (alpha)
				dest[3] = (nwpx[3]*imodx*imody + nepx[3]*modx*imody + swpx[3]*imodx*mody + sepx[3]*modx*mody)>>16;
			else
				dest[3] = 255;
			
			x += xfrac;
		}
		outjump += outwidth;
		y += yfrac;
	}

	return out;
}

/*
================
TexMgr_8to32
================
*/
unsigned *TexMgr_8to32 (byte *in, int pixels)
{
	int i;
	unsigned *out;

	out = Hunk_Alloc(pixels*4);

	if (pixels & 3)
	{
		for (i=0 ; i<pixels ; i++)
			out[i] = d_8to24table[in[i]];
	}
	else
	{
		for (i=0 ; i<pixels ; i+=4)
		{
			out[i] = d_8to24table[in[i]];
			out[i+1] = d_8to24table[in[i+1]];
			out[i+2] = d_8to24table[in[i+2]];
			out[i+3] = d_8to24table[in[i+3]];
		}
	}

	return out;
}

/*
================
TexMgr_PadImage

return image padded up to power-of-two dimentions
================
*/
byte *TexMgr_PadImage(byte *in, int width, int height)
{
	int i,j,w,h;
	byte *out;

	if (width == TexMgr_Pad(width) && height == TexMgr_Pad(height))
		return in;

	w = TexMgr_Pad(width);
	h = TexMgr_Pad(height);
	out = Hunk_Alloc(w*h);

	for (i = 0; i < h; i++) // each row
	{
		for (j = 0; j < w; j++) // each pixel in that row
		{
			if (i < height && j < width)
				out[i*w+j] = in[i*width+j];
			else
				out[i*w+j] = 255;
		}
	}

	return out;
}

/*
===============
TexMgr_AlphaEdgeFix

eliminate pink edges on sprites
operates in place on 32bit data
===============
*/
void TexMgr_AlphaEdgeFix(byte *data, int width, int height)
{
	int i,j,k,ii,jj,p,c,n,b;

	for (i=0; i<height; i++) // for each row
	{
		for (j=0; j<width; j++) // for each pixel
		{
			p = (i*width+j)<<2; // current pixel position in data
			if (data[p+3] == 0) // if pixel is transparent
			{
				for (k=0; k<3; k++) // for each color byte
				{
					n = 9; // number of non-transparent neighbors (include self)
					c = 0; // running total
					for (ii=-1; ii<2; ii++) // for each row of neighbors
					{
						for (jj=-1; jj<2; jj++) // for each pixel in this row of neighbors
						{
							b = WrapCoords(j+jj,i+ii,width,height) * 4;
							data[b+3] ? c += data[b+k] : n-- ;
						}
					}
					data[p+k] = n ? (byte)(c/n) : 0; // average of all non-transparent neighbors
				}
			}
		}
	}
}

/*
================
TexMgr_LoadImage32 

handles 32bit source data
================
*/
void TexMgr_LoadImage32 (gltexture_t *glt, unsigned *data)
{
	// resample up
	data = TexMgr_ResampleTexture (data, glt->width, glt->height, (glt->flags & TEXPREF_ALPHA));
	glt->width = TexMgr_Pad(glt->width);
	glt->height = TexMgr_Pad(glt->height);

	// mipmap down
	while (glt->width > TexMgr_SafeTextureSize (glt->width) || glt->height > TexMgr_SafeTextureSize (glt->height))
	{
		TexMgr_MipMap ((byte *)data, glt->width, glt->height);
		glt->width >>= 1;
		glt->height >>= 1;
	}

	// upload
	GL_Bind (glt->texnum);
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, glt->width, glt->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	// upload mipmaps
	if (glt->flags & TEXPREF_MIPMAP)
	{
		int	miplevel, mipwidth, mipheight;

		mipwidth = glt->width;
		mipheight = glt->height;

		for (miplevel=1; mipwidth > 1 || mipheight > 1; miplevel++)
		{
			TexMgr_MipMap ((byte *)data, mipwidth, mipheight);

			mipwidth = max (mipwidth>>1, 1);
			mipheight = max (mipheight>>1, 1);

			glTexImage2D (GL_TEXTURE_2D, miplevel, GL_RGBA, mipwidth, mipheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		}
	}
	
	// set filter modes
	TexMgr_SetFilterModes (glt);
}

/*
================
TexMgr_LoadImage8 

handles 8bit source data, then passes it to LoadImage32
================
*/
void TexMgr_LoadImage8 (gltexture_t *glt, byte *data)
{
	int i;

	// HACK HACK HACK -- taken from tomazquake
	if (strstr(glt->name, "shot1sid") && glt->width==32 && glt->height==32 && CRC_Block(data, 1024) == 65393)
	{
		// This texture in b_shell1.bsp has some of the first 32 pixels painted white.
		// They are invisible in software, but look really ugly in GL. So we just copy
		// 32 pixels from the bottom to make it look nice.
		memcpy (data, data + 32*31, 32);
	}

	// detect false alpha cases
	if (glt->flags & TEXPREF_ALPHA && !(glt->flags & TEXPREF_CONCHARS))
	{
		for (i = 0; i < glt->width * glt->height; i++)
			if (data[i] == 255)
				break;
		if (i == glt->width*glt->height)
			glt->flags -= TEXPREF_ALPHA;
	}

	// pad it
	if (glt->flags & TEXPREF_PAD)
	{
		data = TexMgr_PadImage (data, glt->width, glt->height);
		glt->width = TexMgr_Pad(glt->width);
		glt->height = TexMgr_Pad(glt->height);
	}
		
	// convert to 32bit
 	data = (byte *)TexMgr_8to32(data, glt->width * glt->height);

	// fix edges
	if (glt->flags & TEXPREF_ALPHA)
		TexMgr_AlphaEdgeFix (data, glt->width, glt->height);

	// upload it
	TexMgr_LoadImage32 (glt, (unsigned *)data);
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
	strncpy (glt->source_file, source_file, sizeof(glt->source_file));
	glt->source_offset = source_offset;
	glt->source_format = format;
	glt->source_width = width;
	glt->source_height = height;
	glt->source_crc = crc;

	//upload it
	mark = Hunk_LowMark();

	switch (glt->source_format)
	{
		case SRC_INDEXED:
			TexMgr_LoadImage8 (glt, data);
			break;
		case SRC_RGBA:
			TexMgr_LoadImage32 (glt, (unsigned *)data);
			break;
	}

	Hunk_FreeToLowMark(mark);

	return glt;
}


/*
================
TexMgr_ReloadImage

reloads a texture
================
*/
void TexMgr_ReloadImage (gltexture_t *glt)
{
	byte	*data = NULL;
	int		mark;

	// get source data
	mark = Hunk_LowMark ();

	if (glt->source_file[0] && glt->source_offset)
		data = FS_LoadHunkFile (glt->source_file) + glt->source_offset; // lump inside file
//	else if (glt->source_file[0] && !glt->source_offset)
//		data = Image_LoadImage (glt->source_file, &glt->source_width, &glt->source_height); // simple file
	else if (!glt->source_file[0] && glt->source_offset)
		data = (byte *) glt->source_offset; // image in memory

	if (!data)
	{
		Com_Printf ("TexMgr_ReloadImage: invalid source for %s\n", glt->name);
		Hunk_FreeToLowMark(mark);
		return;
	}

	// upload it
	switch (glt->source_format)
	{
		case SRC_INDEXED:
			TexMgr_LoadImage8 (glt, data);
			break;
		case SRC_RGBA:
			TexMgr_LoadImage32 (glt, (unsigned *)data);
			break;
	}

	Hunk_FreeToLowMark(mark);
}

/*
================
TexMgr_ReloadImages

reloads all texture images. called only by vid_restart
================
*/
void TexMgr_ReloadImages (void)
{
	gltexture_t *glt;

	for (glt=active_gltextures; glt; glt=glt->next)
	{
		glGenTextures (1, (GLuint *) &glt->texnum);
		TexMgr_ReloadImage (glt);
	}
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

	particletexture = TexMgr_LoadImage (NULL, "particle", 2, 2, SRC_RGBA, data, "", (unsigned)data, TEXPREF_PERSIST | TEXPREF_ALPHA | TEXPREF_NEAREST);
}

static void r_textures_start(void)
{
	static byte notexture_data[16] = {159,91,83,255,0,0,0,255,0,0,0,255,159,91,83,255}; // black and pink checker

	// poll max size from hardware
	glGetIntegerv (GL_MAX_TEXTURE_SIZE, &gl_hardware_maxsize);

	// load palette
	TexMgr_LoadPalette ();

	// load notexture image
	notexture = TexMgr_LoadImage (NULL, "notexture", 2, 2, SRC_RGBA, notexture_data, "", (unsigned)notexture_data, TEXPREF_NEAREST | TEXPREF_PERSIST);

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
		glDeleteTextures(1, &kill->texnum);

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
TexMgr_Flush
================
*/
void TexMgr_Flush (void)
{
	TexMgr_FreeTextures (0, TEXPREF_PERSIST); //deletes all textures where TEXPREF_PERSIST is unset
	TexMgr_LoadPalette ();
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

	// init texture list
	free_gltextures = (gltexture_t *) Hunk_AllocName (MAX_GLTEXTURES * sizeof(gltexture_t), "gltextures");
	active_gltextures = NULL;
	for (i=0; i<MAX_GLTEXTURES; i++)
		free_gltextures[i].next = &free_gltextures[i+1];
	free_gltextures[i].next = NULL;
	numgltextures = 0;

	R_RegisterModule("R_Textures", r_textures_start, r_textures_shutdown, r_textures_newmap);
}
