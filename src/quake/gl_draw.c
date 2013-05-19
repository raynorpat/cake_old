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

typedef struct
{
	gltexture_t *gltexture;
	float		sl, tl, sh, th;
} glpic_t;

canvastype currentcanvas = CANVAS_NONE; // for GL_SetCanvas

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

byte		menuplyr_pixels[4096];

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

/*
================
R_CachePic
================
*/
qpic_t	*R_CachePic (char *path)
{
	cachepic_t	*pic;
	int			i;
	qpic_t		*dat;
	glpic_t		*gl;

	for (pic=menu_cachepics, i=0 ; i<menu_numcachepics ; pic++, i++)
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
		Sys_Error ("Draw_CachePic: failed to load %s", path);
	SwapPic (dat);

	// HACK HACK HACK --- we need to keep the bytes for
	// the translatable player picture just for the menu
	// configuration dialog
	if (!strcmp (path, "gfx/menuplyr.lmp"))
		memcpy (menuplyr_pixels, dat->data, dat->width*dat->height);

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
	gl->gltexture = TexMgr_LoadImage (NULL, name, width, height, SRC_INDEXED, data, "", (unsigned)data, TEXPREF_NEAREST | TEXPREF_NOPICMIP);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return pic;
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

	// empty lmp cache
	for (pic = menu_cachepics, i = 0; i < menu_numcachepics; pic++, i++)
		pic->name[0] = 0;
	menu_numcachepics = 0;
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

	Draw_StretchPic( x, y, 8, 8, fcol, frow, fcol + 0.0625, frow + 0.0625 );
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

			Draw_StretchPic( x, y, 8, 8, fcol, frow, fcol + 0.0625, frow + 0.0625 );
		}

		x += 8;
	}
}

void R_DrawCrosshair (int num, byte color, int crossx, int crossy)
{
	GL_SetCanvas (CANVAS_CROSSHAIR);
	R_DrawChar (-4, -4, '+');
}

void R_DrawPic (int x, int y, qpic_t *pic)
{
	glpic_t *glpic = (glpic_t *) pic->data;

	if (!pic)
		return;

	GL_Bind (glpic->gltexture->texnum);

	Draw_StretchPic( x, y, pic->width, pic->height, 0, 0, 1, 1 );
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

	if (top != oldtop || bottom != oldbottom)
	{
		oldtop = top;
		oldbottom = bottom;
		glt = ((glpic_t *)pic->data)->gltexture;
		TexMgr_ReloadImage (glt, top, bottom);
	}

	R_DrawPic (x, y, pic);
}


void R_DrawStretchPic (int x, int y, int width, int height, qpic_t *pic, float alpha)
{
	glpic_t *glpic = (glpic_t *) pic->data;

	if (!pic)
		return;

	if (alpha > 0.0)
	{
		if (alpha < 1.0)
		{
			qglEnable (GL_BLEND);
			qglColor4f (1,1,1,alpha);
			qglDisable (GL_ALPHA_TEST);
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}

		GL_Bind (glpic->gltexture->texnum);
		Draw_StretchPic (x, y, width, height, 0, 0, 1, 1);

		if (alpha < 1.0)
		{
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			qglEnable (GL_ALPHA_TEST);
			qglDisable (GL_BLEND);
			qglColor4f (1,1,1,1);
		}
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
	float v[3];
	byte *pal = (byte *)d_8to24table;

	qglDisable (GL_TEXTURE_2D);
	qglEnable (GL_BLEND);
	qglDisable (GL_ALPHA_TEST);
	qglColor4f (pal[c*4]/255.0, pal[c*4+1]/255.0, pal[c*4+2]/255.0, alpha);

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
	qglDisable (GL_BLEND);
	qglEnable (GL_ALPHA_TEST);
	qglEnable (GL_TEXTURE_2D);
}

//=============================================================================

void R_LoadingScreen (void)
{
	float x, y;
	qpic_t *pic;

	// don't do anything if not initialized yet
	if (vid_hidden)
		return;

	VID_GetWindowSize(&vid.realx, &vid.realy, &vid.realwidth, &vid.realheight);

	GL_SetCanvas (CANVAS_MENU);

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
	GL_SetCanvas (CANVAS_DEFAULT);

	qglEnable (GL_BLEND);
	qglDisable (GL_ALPHA_TEST);
	qglDisable (GL_TEXTURE_2D);
	qglColor4f (0, 0, 0, 0.5);

	qglBegin (GL_QUADS);
	qglVertex2f (0,0);
	qglVertex2f (vid.realwidth, 0);
	qglVertex2f (vid.realwidth, vid.realheight);
	qglVertex2f (0, vid.realheight);
	qglEnd ();

	qglColor4f (1,1,1,1);
	qglEnable (GL_TEXTURE_2D);
	qglEnable (GL_ALPHA_TEST);
	qglDisable (GL_BLEND);

	Sbar_Changed();
}

/*
================
GL_SetCanvas
================
*/
void GL_SetCanvas (canvastype newcanvas)
{
	extern vrect_t scr_vrect;
	extern cvar_t scr_menuscale;
	extern cvar_t scr_sbarscale;
	extern cvar_t scr_crosshairscale;
	float s;
	int lines;

	if (newcanvas == currentcanvas)
		return;

	currentcanvas = newcanvas;

	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();

	switch(newcanvas)
	{
	case CANVAS_DEFAULT:
		qglOrtho (0, vid.realwidth, vid.realheight, 0, -99999, 99999);
		qglViewport (0, 0, vid.realwidth, vid.realheight);
		break;
	case CANVAS_CONSOLE:
		lines = vid.height - (scr_con_current * vid.height / vid.realheight);
		qglOrtho (0, vid.width, vid.height + lines, lines, -99999, 99999);
		qglViewport (0, 0, vid.realwidth, vid.realheight);
		break;
	case CANVAS_MENU:
		s = min ((float)vid.realwidth / 320.0, (float)vid.realheight / 200.0);
		s = clamp (1.0, scr_menuscale.value, s);
		qglOrtho (0, 320, 200, 0, -99999, 99999);
		qglViewport ((vid.realwidth - 320*s) / 2, (vid.realheight - 200*s) / 2, 320*s, 200*s);
		break;
	case CANVAS_SBAR:
		s = clamp (1.0, scr_sbarscale.value, (float)vid.realwidth / 320.0);
		qglOrtho (0, 320, 48, 0, -99999, 99999);
		qglViewport ((vid.realwidth - 320*s) / 2, 0, 320*s, 48*s);
		break;
	case CANVAS_WARPIMAGE:
		qglOrtho (0, 128, 0, 128, -99999, 99999);
		qglViewport (0, vid.realheight-gl_warpimagesize, gl_warpimagesize, gl_warpimagesize);
		break;
	case CANVAS_CROSSHAIR: //0,0 is center of viewport
		s = clamp (1.0, scr_crosshairscale.value, 10.0);
		qglOrtho (scr_vrect.width/-2/s, scr_vrect.width/2/s, scr_vrect.height/2/s, scr_vrect.height/-2/s, -99999, 99999);
		qglViewport (scr_vrect.x, vid.realheight - scr_vrect.y - scr_vrect.height, scr_vrect.width & ~1, scr_vrect.height & ~1);
		break;
	case CANVAS_BOTTOMLEFT: //used by devstats
		s = (float)vid.realwidth/vid.width;
		qglOrtho (0, 320, 200, 0, -99999, 99999);
		qglViewport (0, 0, 320*s, 200*s);
		break;
	case CANVAS_BOTTOMRIGHT: //used by fps
		s = (float)vid.realwidth/vid.width;
		qglOrtho (0, 320, 200, 0, -99999, 99999);
		qglViewport (vid.realwidth-320*s, 0, 320*s, 200*s);
		break;
	default:
		Sys_Error ("GL_SetCanvas: bad canvas type");
	}

	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();
}
