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

	gl->gltexture = TexMgr_LoadImage (NULL, texturename, p->width, p->height, SRC_INDEXED, p->data, "gfx", offset, TEXPREF_NOPICMIP | TEXPREF_HQ2X);
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
qpic_t *R_CachePic (char *path)
{
	cachepic_t	*pic;
	int			i;
	qpic_t		*dat;
	glpic_t		*gl;

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
		Sys_Error ("Draw_CachePic: failed to load %s", path);
	SwapPic (dat);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	gl = (glpic_t *)pic->pic.data;
	gl->gltexture = TexMgr_LoadImage (NULL, path, dat->width, dat->height, SRC_INDEXED, dat->data, path, sizeof(int)*2, TEXPREF_NOPICMIP | TEXPREF_HQ2X);
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
	qglBegin (GL_QUADS);
	qglTexCoord2f (s1, t1);
	qglVertex2f (x, y);
	qglTexCoord2f (s2, t1);
	qglVertex2f (x + w, y);
	qglTexCoord2f (s2, t2);
	qglVertex2f (x + w, y + h);
	qglTexCoord2f (s1, t2);
	qglVertex2f (x, y + h);
	qglEnd ();
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

void R_DrawCrosshair (int num, int crossx, int crossy)
{
	RB_SetCanvas (CANVAS_CROSSHAIR);

	R_DrawChar (-4 + crossx, -4 + crossy, '+');
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
	byte *pal = (byte *)d_8to24table;

	qglDisable (GL_TEXTURE_2D);
	qglEnable (GL_BLEND);
	qglDisable (GL_ALPHA_TEST);
	qglColor4f (pal[c*4]/255.0, pal[c*4+1]/255.0, pal[c*4+2]/255.0, alpha);

	qglBegin (GL_QUADS);
	qglVertex2f (x,y);
	qglVertex2f (x+w, y);
	qglVertex2f (x+w, y+h);
	qglVertex2f (x, y+h);
	qglEnd ();

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

	RB_SetCanvas (CANVAS_MENU);

	// draw the loading plaque
	pic = R_CachePic("gfx/loading.lmp");
	x = (320 - pic->width) / 2;
	y = (200 - pic->height) / 2;
	R_DrawPic ( x, y, pic );

	// refresh
	VID_Finish();
}

void R_FadeScreen (void)
{
	RB_SetCanvas (CANVAS_DEFAULT);

	qglEnable (GL_BLEND);
	qglDisable (GL_ALPHA_TEST);
	qglDisable (GL_TEXTURE_2D);
	qglColor4f (0, 0, 0, 0.5);

	qglBegin (GL_QUADS);
	qglVertex2f (0,0);
	qglVertex2f (vid.width, 0);
	qglVertex2f (vid.width, vid.height);
	qglVertex2f (0, vid.height);
	qglEnd ();

	qglColor4f (1,1,1,1);
	qglEnable (GL_TEXTURE_2D);
	qglEnable (GL_ALPHA_TEST);
	qglDisable (GL_BLEND);
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
			rp[i*4+0] = d_8to24table[i] & 0xff;
			rp[i*4+1] = ( d_8to24table[i] >> 8 ) & 0xff;
			rp[i*4+2] = ( d_8to24table[i] >> 16 ) & 0xff;
			rp[i*4+3] = 0xff;
		}
	}

	qglClearColor (0,0,0,0);
	qglClear (GL_COLOR_BUFFER_BIT);
	qglClearColor (1,0, 0.5 , 0.5);
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

	RB_SetCanvas (CANVAS_DEFAULT);

	GL_Bind (0);

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
}
