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

/*
=============================================================================

  scrap allocation

  Allocate all the little status bar objects into a single texture
  to crutch up stupid hardware / drivers

=============================================================================
*/

// some cards have low quality of alpha pics, so load the pics
// without transparent pixels into a different scrap block.
// scrap 0 is solid pics, 1 is transparent
#define	MAX_SCRAPS		2
#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

static int	scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
/* static */ byte	scrap_texels[MAX_SCRAPS][BLOCK_WIDTH*BLOCK_HEIGHT];
static qbool scrap_dirty = false;
static gltexture_t *scrap_textures[MAX_SCRAPS];

/*
================
Scrap_AllocBlock

returns an index into scrap_texnums[] and the position inside it
================
*/ 
static int Scrap_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	for (texnum=0 ; texnum<MAX_SCRAPS ; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i=0 ; i<BLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (scrap_allocated[texnum][i+j] >= best)
					break;
				if (scrap_allocated[texnum][i+j] > best2)
					best2 = scrap_allocated[texnum][i+j];
			}
			if (j == w)
			{
				// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i=0 ; i<w ; i++)
			scrap_allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("Scrap_AllocBlock: full");
	return 0;
}

static void Scrap_Upload (void)
{
	char name[8];
	int	i;

	for (i=0; i<MAX_SCRAPS; i++)
	{
		sprintf (name, "scrap%i", i);
		scrap_textures[i] = TexMgr_LoadImage (NULL, name, BLOCK_WIDTH, BLOCK_HEIGHT, SRC_INDEXED, scrap_texels[i], "", (unsigned)scrap_texels[i], TEXPREF_ALPHA | TEXPREF_OVERWRITE);
	}

	scrap_dirty = false;
}

//=============================================================================
/* Support Routines */

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

	// load little ones into the scrap
	if (wad && p->width < 64 && p->height < 64)
	{
		int		x, y;
		int		i, j, k;
		int		texnum;

		texnum = Scrap_AllocBlock (p->width, p->height, &x, &y);
		scrap_dirty = true;
		k = 0;
		for (i=0 ; i<p->height ; i++)
			for (j=0 ; j<p->width ; j++, k++)
				scrap_texels[texnum][(y+i)*BLOCK_WIDTH + x + j] = p->data[k];
		pic->gltexture = scrap_textures[texnum];
		pic->sl = x/(float)BLOCK_WIDTH;
		pic->sh = (x+p->width)/(float)BLOCK_WIDTH;
		pic->tl = y/(float)BLOCK_WIDTH;
		pic->th = (y+p->height)/(float)BLOCK_WIDTH;
	}
	else
	{
		extern byte	*wad_base;
		char texturename[64];
		sprintf (texturename, "gfx:%s", path);

		offset = (unsigned)p - (unsigned)wad_base + sizeof(int)*2;

		pic->gltexture = TexMgr_LoadImage (NULL, texturename, p->width, p->height, SRC_INDEXED, p->data, "gfx",
										  offset, TEXPREF_ALPHA | TEXPREF_PAD);

		pic->sl = 0;
		pic->sh = (float)p->width/(float)TexMgr_Pad(p->width);
		pic->tl = 0;
		pic->th = (float)p->height/(float)TexMgr_Pad(p->height);
	}

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

	char_texture = TexMgr_LoadImage (NULL, "gfx:conchars", 128, 256, SRC_INDEXED, buf, "gfx", offset, TEXPREF_ALPHA | TEXPREF_NEAREST | TEXPREF_CONCHARS);
}

static void gl_draw_start(void)
{
	int	i;
	int flags = TEXPREF_NEAREST | TEXPREF_ALPHA | TEXPREF_PERSIST;

	numcachepics = 0;

	memset (scrap_allocated, 0, sizeof(scrap_allocated));
	memset (scrap_texels, 0, sizeof(scrap_texels));
	Scrap_Upload (); //creates 2 empty gltextures

	draw_chars = NULL;

	// load the crosshair pics
	for (i=0 ; i<3 ; i++)
		crosshairtextures[i] = TexMgr_LoadImage (NULL, va("pic:crosshair_%d", i), 8, 8, SRC_INDEXED, crosshairdata[i], "", (unsigned)crosshairdata[i], flags);

	W_LoadWadFile ("gfx.wad");

	// load the charset by hand
	R_LoadCharset ();
}

static void gl_draw_shutdown(void)
{
	numcachepics = 0;

	memset (scrap_allocated, 0, sizeof(scrap_allocated));
	memset (scrap_texels, 0, sizeof(scrap_texels));
	Scrap_Upload (); //creates 2 empty gltextures

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
	numcachepics = 0;

	R_RegisterModule("GL_Draw", gl_draw_start, gl_draw_shutdown, gl_draw_newmap);
}

void R_FlushPics (void)
{
	if (!draw_chars)
		return;		// not initialized yet (FIXME?)

	gl_draw_shutdown();

	W_LoadWadFile ("gfx.wad");

	// load the charset by hand
	R_LoadCharset ();
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
	int				row, col;
	float			frow, fcol;

	if (y <= -8)
		return;			// totally off screen

	if (num == 32)
		return;		// space

	num &= 255;

	row = num>>4;
	col = num&15;

	frow = row*0.0625;
	fcol = col*0.0625;

	GL_Bind (char_texture->texnum);

	qglBegin (GL_QUADS);
	qglTexCoord2f (fcol, frow);
	qglVertex2f (x, y);
	qglTexCoord2f (fcol + 0.0625, frow);
	qglVertex2f (x+8, y);
	qglTexCoord2f (fcol + 0.0625, frow + 0.03125);
	qglVertex2f (x+8, y+8);
	qglTexCoord2f (fcol, frow + 0.03125);
	qglVertex2f (x, y+8);
	qglEnd ();
}

void R_DrawString (int x, int y, const char *str)
{
	float			frow, fcol;
	int num;

	if (y <= -8)
		return;			// totally off screen
	if (!*str)
		return;

	GL_Bind (char_texture->texnum);

	qglBegin (GL_QUADS);

	while (*str) // stop rendering when out of characters
	{
		if ((num = *str++) != 32) // skip spaces
		{
			frow = (float) (num >> 4)*0.0625;
			fcol = (float) (num & 15)*0.0625;
			qglTexCoord2f (fcol, frow);
			qglVertex2f (x, y);
			qglTexCoord2f (fcol + 0.0625, frow);
			qglVertex2f (x+8, y);
			qglTexCoord2f (fcol + 0.0625, frow + 0.03125);
			qglVertex2f (x+8, y+8);
			qglTexCoord2f (fcol, frow + 0.03125);
			qglVertex2f (x, y+8);
		}

		x += 8;
	}

	qglEnd ();
}


void R_DrawCrosshair (int num, byte color, int crossx, int crossy)
{
	int		x, y;
	int		ofs1, ofs2;
	extern vrect_t scr_vrect;

	x = scr_vrect.x + scr_vrect.width/2 + crossx; 
	y = scr_vrect.y + scr_vrect.height/2 + crossy;

	if (num == 2 || num == 3 || num == 4) {
		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglColor3ubv ((byte *) &d_8to24table[(byte) color]);
		GL_Bind (crosshairtextures[num - 2]->texnum);

		if (vid.width == 320) {
			ofs1 = 3;//3.5;
			ofs2 = 5;//4.5;
		} else {
			ofs1 = 7;
			ofs2 = 9;
		}
		qglBegin (GL_QUADS);
		qglTexCoord2f (0, 0);
		qglVertex2f (x - ofs1, y - ofs1);
		qglTexCoord2f (1, 0);
		qglVertex2f (x + ofs2, y - ofs1);
		qglTexCoord2f (1, 1);
		qglVertex2f (x + ofs2, y + ofs2);
		qglTexCoord2f (0, 1);
		qglVertex2f (x - ofs1, y + ofs2);
		qglEnd ();
		
		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		qglColor3f (1, 1, 1);
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

	if (scrap_dirty)
		Scrap_Upload ();

	GL_Bind (cpic->gltexture->texnum);
	qglBegin (GL_QUADS);
	qglTexCoord2f (cpic->sl, cpic->tl);
	qglVertex2f (x, y);
	qglTexCoord2f (cpic->sh, cpic->tl);
	qglVertex2f (x + pic->width, y);
	qglTexCoord2f (cpic->sh, cpic->th);
	qglVertex2f (x + pic->width, y + pic->height);
	qglTexCoord2f (cpic->sl, cpic->th);
	qglVertex2f (x, y + pic->height);
	qglEnd ();
}

void R_DrawSubPic (int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height)
{
	float newsl, newtl, newsh, newth;
	float oldglwidth, oldglheight;
	cachepic_t *cpic = (cachepic_t *) pic;

	if (!pic)
		return;

	if (scrap_dirty)
		Scrap_Upload ();
	
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

	translate_texture = TexMgr_LoadImage32 ("translate_texture", 64, 64, trans, TEXPREF_ALPHA);
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

	if (scrap_dirty)
		Scrap_Upload ();

	qglDisable(GL_ALPHA_TEST);
	qglEnable (GL_BLEND);
//	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglCullFace(GL_FRONT);
	qglColor4f (1, 1, 1, alpha);
	GL_Bind (cpic->gltexture->texnum);
	qglBegin (GL_QUADS);
	qglTexCoord2f (cpic->sl, cpic->tl);
	qglVertex2f (x, y);
	qglTexCoord2f (cpic->sh, cpic->tl);
	qglVertex2f (x + width, y);
	qglTexCoord2f (cpic->sh, cpic->th);
	qglVertex2f (x + width, y + height);
	qglTexCoord2f (cpic->sl, cpic->th);
	qglVertex2f (x, y + height);
	qglEnd ();
	qglColor3f (1, 1, 1);
	qglEnable(GL_ALPHA_TEST);
	qglDisable (GL_BLEND);
}


/*
=============
R_DrawTile

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void R_DrawTile (int x, int y, int w, int h, mpic_t *pic)
{
	GL_Bind (((cachepic_t *)pic)->gltexture->texnum);
	qglBegin (GL_QUADS);
	qglTexCoord2f (x/64.0, y/64.0);
	qglVertex2f (x, y);
	qglTexCoord2f ( (x+w)/64.0, y/64.0);
	qglVertex2f (x+w, y);
	qglTexCoord2f ( (x+w)/64.0, (y+h)/64.0);
	qglVertex2f (x+w, y+h);
	qglTexCoord2f ( x/64.0, (y+h)/64.0 );
	qglVertex2f (x, y+h);
	qglEnd ();
}


/*
=============
R_DrawFilledRect

Fills a box of pixels with a single color
=============
*/
void R_DrawFilledRect (int x, int y, int w, int h, int c)
{
	byte *pal = (byte *)d_8to24table;

	qglDisable (GL_TEXTURE_2D);
	qglColor3f (pal[c*4]/255.0, pal[c*4+1]/255.0, pal[c*4+2]/255.0);

	qglBegin (GL_QUADS);
	qglVertex2f (x,y);
	qglVertex2f (x+w, y);
	qglVertex2f (x+w, y+h);
	qglVertex2f (x, y+h);
	qglEnd ();

	qglColor3f (1, 1, 1);
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
	x = (vid.width - pic->width)/2;
	y = (vid.height - pic->height)/2;
	R_DrawPic (x, y, pic );

	// refresh
	VID_Finish();
}

void R_FadeScreen (void)
{
	qglEnable (GL_BLEND);
	qglDisable (GL_TEXTURE_2D);
	qglColor4f (0, 0, 0, 0.7);
	qglBegin (GL_QUADS);

	qglVertex2f (0,0);
	qglVertex2f (vid.width, 0);
	qglVertex2f (vid.width, vid.height);
	qglVertex2f (0, vid.height);

	qglEnd ();
	qglColor4f (1,1,1,1);
	qglEnable (GL_TEXTURE_2D);
	qglDisable (GL_BLEND);
}

//=============================================================================

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void GL_Set2D (void)
{
	qglViewport (0, 0, vid.realwidth, vid.realheight);

	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	qglOrtho  (0, vid.width, vid.height, 0, -99999, 99999);

	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();

	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);
	qglDisable (GL_BLEND);
	qglEnable (GL_ALPHA_TEST);
//	qglDisable (GL_ALPHA_TEST);

	qglColor3f (1, 1, 1);
}
