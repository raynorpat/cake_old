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

gltexture_t	*char_texture;

qpic_t		*pic_ovr, *pic_ins; // new cursor handling
qpic_t		*pic_nul; // for missing gfx, don't crash

byte pic_ovr_data[8][8] =
{
	{255,255,255,255,255,255,255,255},
	{255, 15, 15, 15, 15, 15, 15,255},
	{255, 15, 15, 15, 15, 15, 15,  2},
	{255, 15, 15, 15, 15, 15, 15,  2},
	{255, 15, 15, 15, 15, 15, 15,  2},
	{255, 15, 15, 15, 15, 15, 15,  2},
	{255, 15, 15, 15, 15, 15, 15,  2},
	{255,255,  2,  2,  2,  2,  2,  2},
};

byte pic_ins_data[9][8] =
{
	{ 15, 15,255,255,255,255,255,255},
	{ 15, 15,  2,255,255,255,255,255},
	{ 15, 15,  2,255,255,255,255,255},
	{ 15, 15,  2,255,255,255,255,255},
	{ 15, 15,  2,255,255,255,255,255},
	{ 15, 15,  2,255,255,255,255,255},
	{ 15, 15,  2,255,255,255,255,255},
	{ 15, 15,  2,255,255,255,255,255},
	{255,  2,  2,255,255,255,255,255},
};

byte pic_nul_data[8][8] =
{
	{252,252,252,252,  0,  0,  0,  0},
	{252,252,252,252,  0,  0,  0,  0},
	{252,252,252,252,  0,  0,  0,  0},
	{252,252,252,252,  0,  0,  0,  0},
	{  0,  0,  0,  0,252,252,252,252},
	{  0,  0,  0,  0,252,252,252,252},
	{  0,  0,  0,  0,252,252,252,252},
	{  0,  0,  0,  0,252,252,252,252},
};

byte pic_stipple_data[8][8] =
{
	{255,  0,  0,  0,255,  0,  0,  0},
	{  0,  0,255,  0,  0,  0,255,  0},
	{255,  0,  0,  0,255,  0,  0,  0},
	{  0,  0,255,  0,  0,  0,255,  0},
	{255,  0,  0,  0,255,  0,  0,  0},
	{  0,  0,255,  0,  0,  0,255,  0},
	{255,  0,  0,  0,255,  0,  0,  0},
	{  0,  0,255,  0,  0,  0,255,  0},
};

byte pic_crosshair_data[8][8] =
{
	{255,255,255,255,255,255,255,255},
	{255,255,255,  8,  9,255,255,255},
	{255,255,255,  6,  8,  2,255,255},
	{255,  6,  8,  8,  6,  8,  8,255},
	{255,255,  2,  8,  8,  2,  2,  2},
	{255,255,255,  7,  8,  2,255,255},
	{255,255,255,255,  2,  2,255,255},
	{255,255,255,255,255,255,255,255},
};

byte pic_newcrosshair_data[16][16] =
{
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	{0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xff},
	{0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xff},
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
};
 
gltexture_t *new_crosshair;

typedef struct
{
	gltexture_t *gltexture;
	float		sl, tl, sh, th;
} glpic_t;

canvastype currentcanvas = CANVAS_NONE; // for RB_SetCanvas

//==============================================================================
//
//  PIC CACHING
//
//==============================================================================

typedef struct cachepic_s
{
	char		name[MAX_QPATH];
	qpic_t		pic;
	byte		padding[32];	// for appended glpic
} cachepic_t;

#define	MAX_CACHED_PICS		128
cachepic_t	menu_cachepics[MAX_CACHED_PICS];
int			menu_numcachepics;

extern byte	*wad_base;

/*
================
R_CacheWadPic
================
*/
qpic_t *R_CacheWadPic (char *name)
{
	qpic_t	*p;
	glpic_t	*gl;
	char texturename[64];
	unsigned offset;

	p = W_GetLumpName (name, false);
	if (!p)
		return pic_nul;
	gl = (glpic_t *)p->data;

	sprintf (texturename, "gfx:%s", name);

	offset = (unsigned)p - (unsigned)wad_base + sizeof(int)*2;

	gl->gltexture = TexMgr_LoadImage (NULL, texturename, p->width, p->height, SRC_INDEXED, p->data, "gfx", offset, TEXPREF_NOPICMIP);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return p;
}

void Draw_InvalidateState (void);

/*
================
R_CachePic
================
*/
qpic_t *R_CachePic (char *path)
{
	cachepic_t	*pic;
	int			i;
	qpic_t		*dat;
	glpic_t		*gl;

	Draw_InvalidateState ();

	for (pic = menu_cachepics, i = 0; i < menu_numcachepics; pic++, i++)
	{
		if (!strcmp (path, pic->name))
			return &pic->pic;
	}

	if (menu_numcachepics == MAX_CACHED_PICS)
		Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");

	menu_numcachepics++;
	strcpy (pic->name, path);

	// load the pic from disk
	dat = (qpic_t *)FS_LoadTempFile (path);
	if (!dat)
		Sys_Error ("R_CachePic: failed to load %s", path);
	SwapPic (dat);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	gl = (glpic_t *)pic->pic.data;
	gl->gltexture = TexMgr_LoadImage (NULL, path, dat->width, dat->height, SRC_INDEXED, dat->data, path, sizeof(int)*2, TEXPREF_NOPICMIP);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return &pic->pic;
}


/*
================
Draw_MakePic
================
*/
qpic_t *Draw_MakePic (char *name, int width, int height, byte *data)
{
	qpic_t		*pic;
	glpic_t		*gl;

	pic = Hunk_Alloc (sizeof(qpic_t) - 4 + sizeof (glpic_t));
	pic->width = width;
	pic->height = height;

	gl = (glpic_t *)pic->data;
	gl->gltexture = TexMgr_LoadImage (NULL, name, width, height, SRC_INDEXED, data, "", (unsigned) data, TEXPREF_NEAREST | TEXPREF_NOPICMIP);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return pic;
}

gltexture_t *Draw_MakeTexture (char *name, int width, int height, byte *data)
{
	int flags = TEXPREF_NEAREST | TEXPREF_NOPICMIP;
	return TexMgr_LoadImage (NULL, name, width, height, SRC_INDEXED, data, "", (unsigned) data, flags);
}

//==============================================================================
//
//  INIT
//
//==============================================================================

static void gl_draw_start(void)
{
	byte		*data;
	unsigned	offset;
	cachepic_t	*pic;
	int			i;

	// empty lmp cache
	for (pic = menu_cachepics, i = 0; i < menu_numcachepics; pic++, i++)
		pic->name[0] = 0;
	menu_numcachepics = 0;

	// load wad file
	W_LoadWadFile ("gfx.wad");

	// load conchars from wad file
	data = W_GetLumpName ("conchars", false);
	if (!data)
		Sys_Error ("Draw_LoadPics: couldn't load conchars");
	offset = (unsigned)data - (unsigned)wad_base;
	char_texture = TexMgr_LoadImage (NULL, "gfx:conchars", 128, 128, SRC_INDEXED, data,	"gfx", offset, TEXPREF_NEAREST | TEXPREF_NOPICMIP | TEXPREF_CONCHARS);

	// create internal pics
	pic_ins = Draw_MakePic ("ins", 8, 9, &pic_ins_data[0][0]);
	pic_ovr = Draw_MakePic ("ovr", 8, 8, &pic_ovr_data[0][0]);
	pic_nul = Draw_MakePic ("nul", 8, 8, &pic_nul_data[0][0]);

	// and the crosshair
	new_crosshair = Draw_MakeTexture ("crosshair", 16, 16, &pic_newcrosshair_data[0][0]);

	// precache all of our menu pics so that they're valid when we come to use them, otherwise we get weirdness
	// when they're cached on-demand with the new renderer code.
	R_CachePic ("gfx/bigbox.lmp");
	R_CachePic ("gfx/box_bl.lmp");
	R_CachePic ("gfx/box_bm.lmp");
	R_CachePic ("gfx/box_br.lmp");
	R_CachePic ("gfx/box_ml.lmp");
	R_CachePic ("gfx/box_mm.lmp");
	R_CachePic ("gfx/box_mm2.lmp");
	R_CachePic ("gfx/box_mr.lmp");
	R_CachePic ("gfx/box_tl.lmp");
	R_CachePic ("gfx/box_tm.lmp");
	R_CachePic ("gfx/box_tr.lmp");
	R_CachePic ("gfx/complete.lmp");
	R_CachePic ("gfx/conback.lmp");
	R_CachePic ("gfx/dim_drct.lmp");
	R_CachePic ("gfx/dim_ipx.lmp");
	R_CachePic ("gfx/dim_modm.lmp");
	R_CachePic ("gfx/dim_mult.lmp");
	R_CachePic ("gfx/dim_tcp.lmp");
	R_CachePic ("gfx/finale.lmp");
	R_CachePic ("gfx/help0.lmp");
	R_CachePic ("gfx/help1.lmp");
	R_CachePic ("gfx/help2.lmp");
	R_CachePic ("gfx/help3.lmp");
	R_CachePic ("gfx/help4.lmp");
	R_CachePic ("gfx/help5.lmp");
	R_CachePic ("gfx/inter.lmp");
	R_CachePic ("gfx/loading.lmp");
	R_CachePic ("gfx/mainmenu.lmp");
	R_CachePic ("gfx/menudot1.lmp");
	R_CachePic ("gfx/menudot2.lmp");
	R_CachePic ("gfx/menudot3.lmp");
	R_CachePic ("gfx/menudot4.lmp");
	R_CachePic ("gfx/menudot5.lmp");
	R_CachePic ("gfx/menudot6.lmp");
	R_CachePic ("gfx/menuplyr.lmp");
	R_CachePic ("gfx/mp_menu.lmp");
	R_CachePic ("gfx/netmen1.lmp");
	R_CachePic ("gfx/netmen2.lmp");
	R_CachePic ("gfx/netmen3.lmp");
	R_CachePic ("gfx/netmen4.lmp");
	R_CachePic ("gfx/netmen5.lmp");
	R_CachePic ("gfx/pause.lmp");
	R_CachePic ("gfx/p_load.lmp");
	R_CachePic ("gfx/p_multi.lmp");
	R_CachePic ("gfx/p_option.lmp");
	R_CachePic ("gfx/p_save.lmp");
	R_CachePic ("gfx/qplaque.lmp");
	R_CachePic ("gfx/ranking.lmp");
	R_CachePic ("gfx/sp_menu.lmp");
	R_CachePic ("gfx/ttl_cstm.lmp");
	R_CachePic ("gfx/ttl_main.lmp");
	R_CachePic ("gfx/ttl_sgl.lmp");
	R_CachePic ("gfx/vidmodes.lmp");
}

static void gl_draw_shutdown(void)
{
	cachepic_t	*pic;
	int			i;

	// empty lmp cache
	for (pic = menu_cachepics, i = 0; i < menu_numcachepics; pic++, i++)
		pic->name[0] = 0;
	menu_numcachepics = 0;
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


//==============================================================================
//
//  2D DRAWING
//
//==============================================================================


float xofs_2d = 0;
float yofs_2d = 0;
float xscale_2d = 1;
float yscale_2d = 1;

// note - this just needs to be set to an invalid pointer here; it can't be NULL because NULL is a valid test
gltexture_t *draw_lasttexture = (gltexture_t *) 0xffffffff;
r_defaultquad_t *r_draw_quads = NULL;
extern unsigned int r_quadindexbuffer;

void Draw_EndBatching (void)
{
	if (r_num_quads)
	{
		GL_SetIndices (0, r_quad_indexes);
		GL_DrawIndexedPrimitive (GL_TRIANGLES, r_num_quads * 6, r_num_quads * 4);
	}

	r_draw_quads = r_default_quads;
	r_num_quads = 0;
}


void Draw_Vertex (r_defaultquad_t *rdq, float x, float y, unsigned int *color, float s, float t)
{
	rdq->xyz[0] = x;
	rdq->xyz[1] = y;
	rdq->xyz[2] = 0;

	rdq->rgba = color[0];

	rdq->st[0] = s;
	rdq->st[1] = t;
}


void Draw_Textured2DQuad (float x, float y, float w, float h, float sl, float tl, float sh, float th, byte *color)
{
	r_defaultquad_t *dst = NULL;	// this might change below so we can't set it here
	byte defaultcolor[4] = {255, 255, 255, 255};
	byte *realcolor = color ? color : defaultcolor;

	x *= xscale_2d;
	y *= yscale_2d;
	x += xofs_2d;
	y += yofs_2d;
	w *= xscale_2d;
	h *= yscale_2d;

	if (r_num_quads + 4 >= r_max_quads)
		Draw_EndBatching ();

	// now we can safely set the dst pointer
	dst = (r_defaultquad_t *) r_draw_quads;
	r_draw_quads += 4;
	r_num_quads++;

	Draw_Vertex (&dst[0], x, y, (unsigned int *) realcolor, sl, tl);
	Draw_Vertex (&dst[1], x + w, y, (unsigned int *) realcolor, sh, tl);
	Draw_Vertex (&dst[2], x + w, y + h, (unsigned int *) realcolor, sh, th);
	Draw_Vertex (&dst[3], x, y + h, (unsigned int *) realcolor, sl, th);
}


void Draw_Colored2DQuad (float x, float y, float w, float h, byte *color)
{
	Draw_Textured2DQuad (x, y, w, h, 0, 0, 0, 0, color);
}


void Draw_InvalidateState (void)
{
	Draw_EndBatching ();

	// note again - not NULL because NULL is a valid state
	draw_lasttexture = (gltexture_t *) 0xffffffff;
}


void Draw_DisableTexture (void)
{
	Draw_EndBatching ();
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_NONE);
}


void Draw_EnableTexture (void)
{
	Draw_EndBatching ();
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_MODULATE);
}


void Draw_TestState (gltexture_t *texture)
{
	if (texture != draw_lasttexture)
	{
		Draw_EndBatching ();

		GL_SetStreamSource (0, GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (r_defaultquad_t), r_default_quads->xyz);
		GL_SetStreamSource (0, GLSTREAM_COLOR, 4, GL_UNSIGNED_BYTE, sizeof (r_defaultquad_t), r_default_quads->color);
		GL_SetStreamSource (0, GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (0, GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);

		if (texture)
		{
			GL_SetStreamSource (0, GLSTREAM_TEXCOORD0, 2, GL_FLOAT, sizeof (r_defaultquad_t), r_default_quads->st);
			GL_BindTexture (GL_TEXTURE0_ARB, texture);
		}
		else GL_SetStreamSource (0, GLSTREAM_TEXCOORD0, 0, GL_NONE, 0, NULL);

		draw_lasttexture = texture;
	}
}


/*
================
Draw_CharacterQuad
================
*/
void Draw_CharacterQuad (int x, int y, char num, byte *color)
{
	int				row, col;
	float			frow, fcol, size;

	row = num >> 4;
	col = num & 15;

	frow = row * 0.0625;
	fcol = col * 0.0625;
	size = 0.0625;

	Draw_Textured2DQuad (x, y, 8, 8, fcol, frow, fcol + size, frow + size, color);
}


/*
================
R_DrawChar
================
*/
void R_DrawChar (int x, int y, int num)
{
	unsigned int rgba = 0xffffffff;

	if (y <= -8)
		return;			// totally off screen

	num &= 255;

	if (num == 32)
		return; //don't waste verts on spaces

	Draw_TestState (char_texture);
	Draw_CharacterQuad (x, y, (char) num, (byte *) &rgba);
}


/*
================
R_DrawColoredChar
================
*/
void R_DrawColoredChar (int x, int y, int num, unsigned int color)
{
	if (y <= -8)
		return;			// totally off screen

	num &= 255;

	if (num == 32)
		return; //don't waste verts on spaces

	Draw_TestState (char_texture);
	Draw_CharacterQuad (x, y, (char) num, (byte *) &color);
}


/*
================
Draw_String
================
*/
void R_DrawString (int x, int y, const char *str)
{
	unsigned int rgba = 0xffffffff;

	if (y <= -8)
		return;			// totally off screen

	Draw_TestState (char_texture);
	
	while (*str)
	{
		if (*str != 32) // don't waste verts on spaces
			Draw_CharacterQuad (x, y, *str, (byte *) &rgba);

		str++;
		x += 8;
	}
}



void Draw_Texture (int x, int y, int w, int h, void *tex, byte *color)
{
	unsigned int defaultcolor = 0xffffffff;

	// provide a default color if none is specified
	if (!color) color = (byte *) &defaultcolor;

	Draw_TestState ((gltexture_t *) tex);
	Draw_Textured2DQuad (x, y, w, h, 0, 0, 1, 1, color);
}


void Draw_ColoredPic (int x, int y, qpic_t *pic, byte *color)
{
	glpic_t			*gl;

	gl = (glpic_t *) pic->data;

	Draw_TestState (gl->gltexture);
	Draw_Textured2DQuad (x, y, pic->width, pic->height, gl->sl, gl->tl, gl->sh, gl->th, color);
}


void R_DrawPic (int x, int y, qpic_t *pic)
{
	Draw_ColoredPic (x, y, pic, NULL);
}


/*
=============
R_DrawTransPicTranslate

Only used for the player color selection menu
=============
*/
void R_DrawTransPicTranslate (int x, int y, qpic_t *pic, int top, int bottom)
{
	static int oldtop = -2;
	static int oldbottom = -2;
	gltexture_t *glt;
	glpic_t *glp;

	if (top != oldtop || bottom != oldbottom)
	{
		oldtop = top;
		oldbottom = bottom;
		glp = (glpic_t *) pic->data;
		glt = glp->gltexture;
		TexMgr_ReloadImage (glt, top, bottom);
	}

	Draw_EndBatching ();
	R_DrawPic (x, y, pic);
}


/*
================
R_DrawConsoleBackground
================
*/
void R_DrawConsoleBackground (float alpha)
{
	qpic_t *pic;

	pic = R_CachePic ("gfx/conback.lmp");
	pic->width = vid_conwidth.value;
	pic->height = vid_conheight.value;

	if (alpha > 0.0)
	{
		byte color[4] = {255, 255, 255, 255};
		color[3] = BYTE_CLAMPF (alpha);

		Draw_ColoredPic (0, 0, pic, color);
	}
}


/*
=============
R_DrawFilledRect

Fills a box of pixels with a single indexed color
=============
*/
void R_DrawFilledRect (int x, int y, int w, int h, int c, float alpha)
{
	byte *fillcolor = (byte *) &d_8to24table_rgba[c];
	fillcolor[3] = BYTE_CLAMPF (alpha);

	Draw_TestState (NULL);
	Draw_DisableTexture ();
	Draw_Colored2DQuad (x, y, w, h, fillcolor);
	Draw_EnableTexture ();
}


/*
================
R_DrawCrosshair
================
*/
void R_DrawCrosshair (int crossx, int crossy, int color)
{
	extern cvar_t crosshair;
	extern cvar_t scr_crosshairscale;
	extern cvar_t scr_sbarscale;
	float s = 1.0f; //clamp (1.0, scr_crosshairscale.value, 10.0);

	if (!crosshair.value)
		return;

	if (crosshair.value > 1)
	{
		// was 4, 8 for new crosshair
		crossx = ((vid.width / 2) - (8.0f * s)) / s;
		crossy = (((vid.height) / 2) - (8.0f * s)) / s;
	}
	else
	{
		// was 4 for old crosshair
		crossx = ((vid.width / 2) - (4.0f * s)) / s;
		crossy = (((vid.height) / 2) - (4.0f * s)) / s;
	}

	RB_SetCanvas (CANVAS_DEFAULT);

	if (s != 1)
	{
		qglPushMatrix ();
		qglScalef (s, s, 1);
	}

	if (crosshair.value > 1)
		Draw_Texture (crossx, crossy, 16, 16, new_crosshair, (byte *) &d_8to24table_rgba[color & 255]);
	else R_DrawChar (crossx, crossy, '+');

	if (s != 1)
		qglPopMatrix ();
}

//=============================================================================

void R_LoadingScreen (void)
{
	float x, y;
	qpic_t *pic;

	// don't do anything if not initialized yet
	if (vid_hidden)
		return;

	RB_SetCanvas (CANVAS_MENU);

	// draw the loading plaque
	pic = R_CachePic("gfx/loading.lmp");
	x = (320 - pic->width) / 2;
	y = (200 - pic->height) / 2;
	R_DrawPic (x, y, pic);

	// refresh
	VID_Finish();
}

void R_FadeScreen (void)
{
	byte fadecolor[4] = {0, 0, 0, 160};

	RB_SetCanvas (CANVAS_DEFAULT);

	Draw_TestState (NULL);
	Draw_DisableTexture ();
	Draw_Colored2DQuad (0, 0, vid.width, vid.height, fadecolor);
	Draw_EnableTexture ();
}

//====================================================================

void RB_SetCanvas (canvastype newcanvas)
{
	extern cvar_t scr_menuscale;
	extern cvar_t scr_sbarscale;
	extern cvar_t scr_crosshairscale;
	float s;
	int lines;

	if (newcanvas == currentcanvas)
		return;

	currentcanvas = newcanvas;

	// enforce defaults
	xofs_2d = 0;
	yofs_2d = 0;
	xscale_2d = 1;
	yscale_2d = 1;

	switch(newcanvas)
	{
	case CANVAS_CONSOLE:
		lines = vid.height - (scr_con_current * vid.height / vid_conheight.value);

		xofs_2d = 0;
		yofs_2d = scr_con_current - vid.height;

		xscale_2d = (float) vid.width / (float) vid_conwidth.value;
		yscale_2d = (float) vid.height / (float) vid_conheight.value;
		break;

	case CANVAS_MENU:
		s = min ((float) vid.width / 320.0, (float) vid.height / 200.0);
		s = clamp (1.0, scr_menuscale.value, s);

		xofs_2d = (vid.width - (320 * s)) / 2;
		yofs_2d = (vid.height - (200 * s)) / 2;

		xscale_2d = s;
		yscale_2d = s;
		break;

	case CANVAS_SBAR:
		s = clamp (1.0, scr_sbarscale.value, (float)vid.width / 320.0);

		xofs_2d = (vid.width - (320.0f * s)) / 2;
		yofs_2d = vid.height - 48 * scr_sbarscale.value;

		xscale_2d = s;
		yscale_2d = s;
		break;

	case CANVAS_BOTTOMLEFT: // used by devstats
		s = (float) vid.width / (float) vid_conwidth.value; // use console scale

		xofs_2d = 0;
		yofs_2d = vid.height - (200.0f * s);

		xscale_2d = s;
		yscale_2d = s;
		break;

	case CANVAS_BOTTOMRIGHT: // used by fps
		s = (float) vid.width / (float) vid_conwidth.value; // use console scale

		xofs_2d = vid.width - (320.0f * s);
		yofs_2d = vid.height - (200.0f * s);

		xscale_2d = s;
		yscale_2d = s;
		break;

	case CANVAS_DEFAULT:
	default:
		// just use default values from above
		break;
	}
}


/*
================
GL_Set2D
================
*/
// underwater warp!!!
void R_DrawUnderwaterWarp (void);

void GL_Set2D (void)
{
	currentcanvas = CANVAS_INVALID;

	Draw_InvalidateState ();

	// unbind everything and go back to TMU0
	GL_UnbindBuffers ();

	// unbind all textures
	GL_BindTexture (GL_TEXTURE0_ARB, NULL);
	GL_BindTexture (GL_TEXTURE1_ARB, NULL);
	GL_BindTexture (GL_TEXTURE2_ARB, NULL);

	// unbined cached environments
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_NONE);
	GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_NONE);
	GL_TexEnv (GL_TEXTURE2_ARB, GL_TEXTURE_2D, GL_NONE);

	// back to TMU0
	GL_ActiveTexture (GL_TEXTURE0_ARB);

	RB_SetCanvas (CANVAS_DEFAULT);

	// partition the depth range for 3D/2D stuff
	qglDepthRange (QGL_DEPTH_2D_BEGIN, QGL_DEPTH_2D_END);

	qglViewport (0, 0, vid.width, vid.height);

	qglMatrixMode (GL_PROJECTION);
    qglLoadIdentity ();
	qglOrtho (0, vid.width, vid.height, 0, -99999, 99999);

	qglMatrixMode (GL_MODELVIEW);
    qglLoadIdentity ();

	qglDisable (GL_DEPTH_TEST);
	qglDepthMask (GL_FALSE);
	qglDisable (GL_CULL_FACE);

	// the underwater warp needs to be drawn before enabling alpha test or it won't blend properly
//	R_DrawUnderwaterWarp ();
	
	qglEnable (GL_BLEND);
	qglDisable (GL_ALPHA_TEST);
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_MODULATE);
}

//====================================================================

/*
=============
R_SetPalette
=============
*/
unsigned r_rawpalette[256];

void R_SetPalette ( unsigned char *palette )
{
	int		i;

	byte *rp = ( byte * ) r_rawpalette;

	if ( palette )
	{
		for ( i = 0; i < 256; i++ )
		{
			rp[i*4+0] = palette[i*3+0];
			rp[i*4+1] = palette[i*3+1];
			rp[i*4+2] = palette[i*3+2];
			rp[i*4+3] = 0xff;
		}
	}
	else
	{
		for ( i = 0; i < 256; i++ )
		{
			rp[i*4+0] = d_8to24table_rgba[i] & 0xff;
			rp[i*4+1] = ( d_8to24table_rgba[i] >> 8 ) & 0xff;
			rp[i*4+2] = ( d_8to24table_rgba[i] >> 16 ) & 0xff;
			rp[i*4+3] = 0xff;
		}
	}

	qglClearColor (0,0,0,0);
	qglClear (GL_COLOR_BUFFER_BIT);
	qglClearColor (1,0, 0.5, 0.5);
}

/*
=============
R_DrawStretchRaw
=============
*/
void R_DrawStretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data)
{
	unsigned	image32[256*256];
	int			i, j, trows;
	byte		*source;
	int			frac, fracstep;
	float		hscale;
	int			row;
	float		t;
	unsigned	*dest;

	qglViewport (0, 0, vid.width, vid.height);

	qglMatrixMode (GL_PROJECTION);
    qglLoadIdentity ();
	qglOrtho (0, vid.width, vid.height, 0, -99999, 99999);

	qglMatrixMode (GL_MODELVIEW);
    qglLoadIdentity ();

	GL_BindTexture (GL_TEXTURE0_ARB, NULL);

	if (rows<=256)
	{
		hscale = 1;
		trows = rows;
	}
	else
	{
		hscale = rows/256.0;
		trows = 256;
	}
	t = rows * hscale / 256;

	for (i=0 ; i<trows ; i++)
	{
		row = (int)(i*hscale);
		if (row > rows)
			break;
		source = data + cols*row;
		dest = &image32[i*256];
		fracstep = cols*0x10000/256;
		frac = fracstep >> 1;
		for (j=0 ; j<256 ; j++)
		{
			dest[j] = r_rawpalette[source[frac>>16]];
			frac += fracstep;
		}
	}

	qglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, image32);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	qglBegin (GL_QUADS);
	qglTexCoord2f (0, 0);
	qglVertex2f (x, y);
	qglTexCoord2f (1, 0);
	qglVertex2f (x+w, y);
	qglTexCoord2f (1, t);
	qglVertex2f (x+w, y+h);
	qglTexCoord2f (0, t);
	qglVertex2f (x, y+h);
	qglEnd ();
}

//====================================================================

int	netgraphtexture; // netgraph texture

#define NET_GRAPHHEIGHT 32

static byte ngraph_texels[NET_GRAPHHEIGHT][NET_TIMINGS];

static void R_LineGraph (int x, int h)
{
	int		i;
	int		s;
	int		color;

	s = NET_GRAPHHEIGHT;

	if (h == 10000)
		color = 0x6f;	// yellow
	else if (h == 9999)
		color = 0x4f;	// red
	else if (h == 9998)
		color = 0xd0;	// blue
	else
		color = 0xfe;	// white

	if (h>s)
		h = s;
	
	for (i=0 ; i<h ; i++)
		if (i & 1)
			ngraph_texels[NET_GRAPHHEIGHT - i - 1][x] = 0xff;
		else
			ngraph_texels[NET_GRAPHHEIGHT - i - 1][x] = (byte)color;

	for ( ; i<s ; i++)
		ngraph_texels[NET_GRAPHHEIGHT - i - 1][x] = (byte)0xff;
}

/*
==============
R_DrawNetGraph
==============
*/
void R_DrawNetGraph (void)
{
/*
	int		a, x, i, y;
	int lost;
	char st[80];
	unsigned	ngraph_pixels[NET_GRAPHHEIGHT][NET_TIMINGS];

	x = 0;
	lost = CL_CalcNet();
	for (a=0 ; a<NET_TIMINGS ; a++)
	{
		i = (cls.netchan.outgoing_sequence-a) & NET_TIMINGSMASK;
		R_LineGraph (NET_TIMINGS-1-a, packet_latency[i]);
	}

	// now load the netgraph texture into gl and draw it
	for (y = 0; y < NET_GRAPHHEIGHT; y++)
		for (x = 0; x < NET_TIMINGS; x++)
			ngraph_pixels[y][x] = d_8to24table[ngraph_texels[y][x]];

	x =	0;
	y = 320 - 24 - NET_GRAPHHEIGHT - 1;

	if (r_netgraph.value != 2 && r_netgraph.value != 3)
		Draw_TextBox (x, y, NET_TIMINGS/8, NET_GRAPHHEIGHT/8 + 1);

	if (r_netgraph.value != 3) {
		sprintf (st, "%3i%% packet loss", lost);
		R_DrawString (8, y + 8, st);
	}

	x = 8;
	y += 16;
	
    GL_Bind(netgraphtexture);

	qglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 
		NET_TIMINGS, NET_GRAPHHEIGHT, 0, GL_RGBA, 
		GL_UNSIGNED_BYTE, ngraph_pixels);

	qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	qglBegin (GL_QUADS);
	qglTexCoord2f (0, 0);
	qglVertex2f (x, y);
	qglTexCoord2f (1, 0);
	qglVertex2f (x+NET_TIMINGS, y);
	qglTexCoord2f (1, 1);
	qglVertex2f (x+NET_TIMINGS, y+NET_GRAPHHEIGHT);
	qglTexCoord2f (0, 1);
	qglVertex2f (x, y+NET_GRAPHHEIGHT);
	qglEnd ();
*/
}
