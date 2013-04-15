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

#include "gl_local.h"
#include "rc_wad.h"

byte		*draw_chars;				// 8*8 graphic characters

gltexture_t	*translate_texture, *char_texture;
gltexture_t *crosshairtextures[3];

static byte crosshairdata[3][64] = {
	{
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	},

	{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	},

	{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	}
};

static void	R_LoadCharset (void);

//=============================================================================

typedef struct cachepic_s
{
	mpic_t		pic;
	char		name[MAX_QPATH];
	gltexture_t *gltexture;
	float		sl, tl, sh, th;
} cachepic_t;

#define	MAX_CACHED_PICS		256
static cachepic_t	cachepics[MAX_CACHED_PICS];
static int			numcachepics;

byte		menuplyr_pixels[4096];		// the menu needs them

static mpic_t *R_CachePic_impl (char *path, qbool wad, qbool crash)
{
	qpic_t	*p;
	cachepic_t	*pic;
	int		i;
	unsigned offset;
	extern byte	*wad_base;
	char texturename[64];

	for (pic = cachepics, i = 0; i < numcachepics; pic++, i++)
		if (!strcmp (path, pic->name))
			return &pic->pic;

	if (wad) {
		p = W_GetLumpName (path, crash);
		if (!p)
			return NULL;
	}
	else {
		// load the pic from disk
		p = (qpic_t *)FS_LoadTempFile (path);	
		if (!p) {
			if (crash)
				Sys_Error ("R_CachePic: failed to load %s", path);
			else
				return NULL;
		}
		SwapPic (p);

		// HACK HACK HACK --- we need to keep the bytes for
		// the translatable player picture just for the menu
		// configuration dialog
		if (!strcmp (path, "gfx/menuplyr.lmp")) {
			if ((unsigned)(p->width*p->height) > sizeof(menuplyr_pixels))
				Sys_Error ("gfx/menuplyr.lmp has invalid dimensions");
			memcpy (menuplyr_pixels, p->data, p->width*p->height);
		}
	}

	if (numcachepics == MAX_CACHED_PICS)
		Sys_Error ("numcachepics == MAX_CACHED_PICS");

	numcachepics++;

	strlcpy (pic->name, path, sizeof(pic->name));
	pic->pic.width = p->width;
	pic->pic.height = p->height;

	sprintf (texturename, "pic:%s", path);
	offset = (unsigned)p - (unsigned)wad_base + sizeof(int)*2;

	pic->gltexture = TexMgr_LoadImage (NULL, texturename, p->width, p->height, SRC_INDEXED_UPSCALE, p->data, "", offset, TEXPREF_UPSCALE);

	pic->sl = 0;
	pic->sh = 1;
	pic->tl = 0;
	pic->th = 1;

	return &pic->pic;
}

mpic_t *R_CachePic (char *path)
{
	return R_CachePic_impl (path, false, true);
}

mpic_t *R_CacheWadPic (char *name)
{
	return R_CachePic_impl (name, true, false);
}


static void R_LoadCharset (void)
{
	int		i;
	byte	buf[128*256];
	byte	*src, *dest;
	unsigned offset;
	extern byte	*wad_base;

	draw_chars = W_GetLumpName ("conchars", true);
	offset = (unsigned)draw_chars - (unsigned)wad_base;
	for (i=0 ; i<256*64 ; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;	// proper transparent color

	// Convert the 128*128 conchars texture to 128*256 leaving
	// empty space between rows so that chars don't stumble on
	// each other because of texture smoothing.
	// This hack costs us 64K of GL texture memory
	memset (buf, 255, sizeof(buf));
	src = draw_chars;
	dest = buf;
	for (i=0 ; i<16 ; i++) {
		memcpy (dest, src, 128*8);
		src += 128*8;
		dest += 128*8*2;
	}

	char_texture = TexMgr_LoadImage (NULL, "pic:conchars", 128, 256, SRC_INDEXED, buf, "", offset, TEXPREF_NEAREST);
}

static void gl_draw_start(void)
{
	int	i;

	numcachepics = 0;
	draw_chars = NULL;

	// load the crosshair pics
	for (i=0 ; i<3 ; i++)
		crosshairtextures[i] = TexMgr_LoadImage (NULL, va("pic:crosshair_%d", i), 8, 8, SRC_INDEXED, crosshairdata[i], "", (unsigned)crosshairdata[i], TEXPREF_NEAREST | TEXPREF_PERSIST);

	// load wad file
	W_LoadWadFile ("gfx.wad");

	// load the charset by hand
	R_LoadCharset ();
}

static void gl_draw_shutdown(void)
{
	numcachepics = 0;
	draw_chars = NULL;
}

static void gl_draw_newmap(void)
{
}


/*
===============
R_Draw_Init
===============
*/
void R_Draw_Init (void)
{
	R_RegisterModule("GL_Draw", gl_draw_start, gl_draw_shutdown, gl_draw_newmap);
}

void R_FlushPics (void)
{
	int i;

	if (!draw_chars)
		return;		// not initialized yet (FIXME?)

	numcachepics = 0;
	draw_chars = NULL;

	// load the crosshair pics
	for (i=0 ; i<3 ; i++)
		crosshairtextures[i] = TexMgr_LoadImage (NULL, va("pic:crosshair_%d", i), 8, 8, SRC_INDEXED, crosshairdata[i], "", (unsigned)crosshairdata[i], TEXPREF_NEAREST | TEXPREF_PERSIST);

	// load wad file
	W_LoadWadFile ("gfx.wad");

	// load the charset by hand
	R_LoadCharset ();
}

static void Draw_StretchPic ( int x, int y, int w, int h, float s1, float t1, float s2, float t2 )
{
	float v[3], tc[2];

	R_PushElems ( quad_elems, 6 );

	VectorSet (v, x, y, 0);
	tc[0] = s1; tc[1] = t1;
	R_PushCoord (tc);
	R_PushVertex (v);

	VectorSet (v, x+w, y, 0);
	tc[0] = s2; tc[1] = t1;
	R_PushCoord (tc);
	R_PushVertex (v);

	VectorSet (v, x+w, y+h, 0);
	tc[0] = s2; tc[1] = t2;
	R_PushCoord (tc);
	R_PushVertex (v);

	VectorSet (v, x, y+h, 0);
	tc[0] = s1; tc[1] = t2;
	R_PushCoord (tc);
	R_PushVertex (v);

	R_VertexTCBase ( 0, false );
	R_LockArrays ();
	R_FlushArrays ();
	R_UnlockArrays ();
	R_ClearArrays ();
}


/*
================
R_DrawChar

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void R_DrawChar (int x, int y, int num)
{
	float frow, fcol;

	if (y <= -8)
		return;			// totally off screen

	if (num == 32)
		return;		// space

	num &= 255;

	frow = (num>>4)*0.0625;
	fcol = (num&15)*0.0625;

	GL_Bind (char_texture->texnum);

	Draw_StretchPic( x, y, 8, 8, fcol, frow, fcol + 0.0625, frow + 0.03125 );
}

void R_DrawString (int x, int y, const char *str)
{
	float frow, fcol;
	int num;

	if (y <= -8)
		return;			// totally off screen
	if (!*str)
		return;

	GL_Bind (char_texture->texnum);
	
	while (*str) // stop rendering when out of characters
	{
		if ((num = *str++) != 32) // skip spaces
		{
			frow = (float) (num >> 4)*0.0625;
			fcol = (float) (num & 15)*0.0625;

			Draw_StretchPic( x, y, 8, 8, fcol, frow, fcol + 0.0625, frow + 0.03125 );
		}

		x += 8;
	}
}


void R_DrawCrosshair (int num, byte color, int crossx, int crossy)
{
	int		x, y;
	extern vrect_t scr_vrect;

	x = scr_vrect.x + scr_vrect.width/2 + crossx; 
	y = scr_vrect.y + scr_vrect.height/2 + crossy;

	if (num == 2 || num == 3 || num == 4) {
		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglColor3ubv ((byte *) &d_8to24table[(byte) color]);

		GL_Bind (crosshairtextures[num - 2]->texnum);

		Draw_StretchPic( x, y, 16, 16, 0, 0, 1, 1 );

		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		qglColor4f (1, 1, 1, 1);
	}
	else {
		R_DrawChar (x - 4, y - 4, '+');
	}
}

void R_DrawPic (int x, int y, mpic_t *pic)
{
	cachepic_t *cpic = (cachepic_t *) pic;

	if (!pic)
		return;

	GL_Bind (cpic->gltexture->texnum);

	Draw_StretchPic( x, y, pic->width, pic->height, 0, 0, 1, 1 );
}

void R_DrawSubPic (int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height)
{
	float newsl, newtl, newsh, newth;
	float oldglwidth, oldglheight;
	cachepic_t *cpic = (cachepic_t *) pic;

	if (!pic)
		return;
	
	oldglwidth = cpic->sh - cpic->sl;
	oldglheight = cpic->th - cpic->tl;

	newsl = cpic->sl + (srcx*oldglwidth)/pic->width;
	newsh = newsl + (width*oldglwidth)/pic->width;

	newtl = cpic->tl + (srcy*oldglheight)/pic->height;
	newth = newtl + (height*oldglheight)/pic->height;
	
	GL_Bind (cpic->gltexture->texnum);

	qglBegin (GL_QUADS);
	qglTexCoord2f (newsl, newtl);
	qglVertex2f (x, y);
	qglTexCoord2f (newsh, newtl);
	qglVertex2f (x + width, y);
	qglTexCoord2f (newsh, newth);
	qglVertex2f (x + width, y + height);
	qglTexCoord2f (newsl, newth);
	qglVertex2f (x, y + height);
	qglEnd ();
}

/*
=============
R_DrawTransPicTranslate

Only used for the player color selection menu
=============
*/
void R_DrawTransPicTranslate (int x, int y, mpic_t *pic, byte *translation)
{
/*
	int				v, u, c;
	unsigned		trans[64*64], *dest;
	byte			*src;
	int				p;

	if (!pic)
		return;

	c = pic->width * pic->height;

	dest = trans;
	for (v=0 ; v<64 ; v++, dest += 64)
	{
		src = &menuplyr_pixels[ ((v*pic->height)>>6) *pic->width];
		for (u=0 ; u<64 ; u++)
		{
			p = src[(u*pic->width)>>6];
			if (p == 255)
				dest[u] = 0;
			else
				dest[u] =  d_8to24table[translation[p]];
		}
	}

	translate_texture = TexMgr_LoadImage32 ("translate_texture", 64, 64, trans, TEXPREF_NONE);
	GL_Bind (translate_texture->texnum);

	qglBegin (GL_QUADS);
	qglTexCoord2f (0, 0);
	qglVertex2f (x, y);
	qglTexCoord2f (1, 0);
	qglVertex2f (x+pic->width, y);
	qglTexCoord2f (1, 1);
	qglVertex2f (x+pic->width, y+pic->height);
	qglTexCoord2f (0, 1);
	qglVertex2f (x, y+pic->height);
	qglEnd ();
	*/
}


void R_DrawStretchPic (int x, int y, int width, int height, mpic_t *pic, float alpha)
{
	cachepic_t *cpic = (cachepic_t *) pic;

	if (!pic || !alpha)
		return;

	qglDisable(GL_ALPHA_TEST);
	qglEnable (GL_BLEND);
//	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglCullFace(GL_FRONT);

	qglColor4f (1, 1, 1, alpha);

	GL_Bind (cpic->gltexture->texnum);

	Draw_StretchPic ( x, y, width, height, 0, 0, 1, 1 );

	qglColor4f (1, 1, 1, 1);

	qglEnable(GL_ALPHA_TEST);
	qglDisable (GL_BLEND);
}


/*
=============
R_DrawFilledRect

Fills a box of pixels with a single indexed color
=============
*/
void R_DrawFilledRect (int x, int y, int w, int h, int c)
{
	float v[3];
	byte *pal = (byte *)d_8to24table;

	qglDisable (GL_TEXTURE_2D);
	qglColor4f (pal[c*4]/255.0, pal[c*4+1]/255.0, pal[c*4+2]/255.0, 1.0);

	VectorSet (v, x, y, 0);
	R_PushVertex (v);

	VectorSet (v, x+w, y, 0);
	R_PushVertex (v);

	VectorSet (v, x+w, y+h, 0);
	R_PushVertex (v);

	VectorSet (v, x, y+h, 0);
	R_PushVertex (v);

	R_VertexTCBase ( 0, false );
	R_LockArrays ();
	R_FlushArrays ();
	R_UnlockArrays ();
	R_ClearArrays ();

	qglColor4f (1, 1, 1, 1);
	qglEnable (GL_TEXTURE_2D);
}

//=============================================================================

void R_LoadingScreen (void)
{
	float x, y;
	mpic_t *pic;

	// don't do anything if not initialized yet
	if (vid_hidden)
		return;

	VID_GetWindowSize(&vid.realx, &vid.realy, &vid.realwidth, &vid.realheight);

	// draw the loading plaque
	pic = R_CachePic("gfx/loading.lmp");
	x = (vid.width - pic->width) / 2;
	y = (vid.height - pic->height) / 2;
	R_DrawPic ( x, y, pic );

	// refresh
	VID_Finish();
}

void R_FadeScreen (void)
{
	float v[3];

	qglEnable (GL_BLEND);
	qglDisable (GL_TEXTURE_2D);
	
	R_PushElems ( quad_elems, 6 );

	qglColor4f ( 0, 0, 0, 0.8 );

	VectorSet (v, 0, 0, 0);
	R_PushVertex (v);

	VectorSet (v, vid.width, 0, 0);
	R_PushVertex (v);

	VectorSet (v, vid.width, vid.height, 0);
	R_PushVertex (v);

	VectorSet (v, 0, vid.height, 0);
	R_PushVertex (v);

	R_VertexTCBase ( 0, false );
	R_LockArrays ();
	R_FlushArrays ();
	R_UnlockArrays ();
	R_ClearArrays ();
	
	qglColor4f (1,1,1,1);

	qglEnable (GL_TEXTURE_2D);
	qglDisable (GL_BLEND);
}

//=============================================================================

/*
=============
Draw_StretchRaw

Used for cinematics
=============
*/
void Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data)
{
#if 0
	extern unsigned	r_rawpalette[256];
	unsigned	image32[256*256];
	unsigned	*dest = image32;
	int			i, j, trows;
	byte		*source;
	int			frac, fracstep;
	float		hscale;
	int			row;
	float		t, tc[2], v[3];

	GL_Bind (0);

	if (rows <= 256)
	{
		hscale = 1;
		trows = rows;
	}
	else
	{
		hscale = rows/256.0;
		trows = 256;
	}

	t = rows*hscale / 256;
	fracstep = cols*0x10000/256;

	memset ( image32, 0, sizeof(unsigned)*256*256 );
	
	for (i=0 ; i<trows ; i++, dest+=256)
	{
		row = (int)(i*hscale);
		if (row > rows)
			break;
		source = data + cols*row;
		frac = fracstep >> 1;
		for (j=0 ; j<256 ; j+=4)
		{
			dest[j] = r_rawpalette[source[frac>>16]];
			frac += fracstep;
			dest[j+1] = r_rawpalette[source[frac>>16]];
			frac += fracstep;
			dest[j+2] = r_rawpalette[source[frac>>16]];
			frac += fracstep;
			dest[j+3] = r_rawpalette[source[frac>>16]];
			frac += fracstep;
		}
	}
	
	qglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, image32);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	R_PushElems ( quad_elems, 6 );

	VectorSet (v, x, y, 0);
	tc[0] = 1.0/512.0; tc[1] = 1.0/512.0;
	R_PushCoord (tc);
	R_PushVertex (v);

	VectorSet (v, x+w, y, 0);
	tc[0] = 511.0/512.0; tc[1] = 1.0/512.0;
	R_PushCoord (tc);
	R_PushVertex (v);

	VectorSet (v, x+w, y+h, 0);
	tc[0] = 511.0/512.0; tc[1] = t;
	R_PushCoord (tc);
	R_PushVertex (v);

	VectorSet (v, x, y+h, 0);
	tc[0] = 1.0/512.0; tc[1] = t;
	R_PushCoord (tc);
	R_PushVertex (v);

	R_VertexTCBase ( 0, false );
	R_LockArrays ();
	R_FlushArrays ();
	R_UnlockArrays ();
	R_ClearArrays ();
#endif
}