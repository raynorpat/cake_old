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
// r_misc.c

#include "gl_local.h"
#include "rc_image.h"

#include <time.h>

unsigned int playertextures; // up to 32 color translated skins
unsigned int playerfbtextures[MAX_CLIENTS];

#define ALPHATYPE_ALIAS		1
#define ALPHATYPE_BRUSH		2
#define ALPHATYPE_SPRITE	3
#define ALPHATYPE_LIQUID	4
#define ALPHATYPE_PARTICLE	5
#define ALPHATYPE_FENCE		6
#define ALPHATYPE_SURFACE	7
#define ALPHATYPE_CORONA	8

typedef struct alphalist_s
{
	union
	{
		entity_t *ent;
		msurface_t *surf;
		particle_type_t *particles;
		dlight_t *light;
	};

	xcommand_t setupfunc;
	xcommand_t takedownfunc;

	glmatrix *matrix;

	int entnum;
	int type;
	float dist;
} alphalist_t;


#define MAX_ALPHALIST 65536

alphalist_t *r_alphalist[MAX_ALPHALIST] = {NULL};
int r_numalphalist = 0;

float R_GetDist (float *origin)
{
	// no need to sqrt these as all we're concerned about is relative distances
	// (if x < y then sqrt (x) is also < sqrt (y))
	return
	(
		(origin[0] - r_origin[0]) * (origin[0] - r_origin[0]) +
		(origin[1] - r_origin[1]) * (origin[1] - r_origin[1]) +
		(origin[2] - r_origin[2]) * (origin[2] - r_origin[2])
	);
}


void R_AlphaListBeginMap (void)
{
	int i;

	// if we ever move this from Hunk memory we'll need to free it too...
	for (i = 0; i < MAX_ALPHALIST; i++)
		r_alphalist[i] = NULL;

	r_numalphalist = 0;
}


void R_AlphaListBeginFrame (void)
{
	r_numalphalist = 0;
}


alphalist_t *R_GrabAlphaSlot (void)
{
	if (r_numalphalist >= MAX_ALPHALIST)
		return NULL;

	if (!r_alphalist[r_numalphalist])
		r_alphalist[r_numalphalist] = (alphalist_t *) Hunk_Alloc (sizeof (alphalist_t));

	if (!r_alphalist[r_numalphalist])
		return NULL;

	r_alphalist[r_numalphalist]->setupfunc = NULL;
	r_alphalist[r_numalphalist]->takedownfunc = NULL;

	r_numalphalist++;
	return r_alphalist[r_numalphalist - 1];
}


void R_CoronasBegin (void);
void R_CoronasEnd (void);
void R_RenderCorona (dlight_t *light);

void R_AddCoronaToAlphaList (dlight_t *l)
{
	alphalist_t *al;

	if (!(al = R_GrabAlphaSlot ())) return;

	al->setupfunc = R_CoronasBegin;
	al->takedownfunc = R_CoronasEnd;
	al->type = ALPHATYPE_CORONA;
	al->dist = R_GetDist (l->origin);
	al->light = l;
}


void R_ParticlesBegin (void);
void R_ParticlesEnd (void);

void R_AddParticlesToAlphaList (particle_type_t *pt)
{
	alphalist_t *al;

	if (!(al = R_GrabAlphaSlot ()))
		return;

	al->setupfunc = R_ParticlesBegin;
	al->takedownfunc = R_ParticlesEnd;
	al->type = ALPHATYPE_PARTICLE;
	al->dist = R_GetDist (pt->spawnorg);
	al->particles = pt;
}


void R_LiquidBegin (void);
void R_LiquidEnd (void);
void R_SpriteBegin (void);
void R_SpriteEnd (void);

float *R_TransformMidPoint (float *midpoint, glmatrix *m)
{
	static float transformed[3] = {0, 0, 0};

	if (m)
	{
		transformed[0] = midpoint[0] * m->m16[0] + midpoint[1] * m->m16[4] + midpoint[2] * m->m16[8] + m->m16[12];
		transformed[1] = midpoint[0] * m->m16[1] + midpoint[1] * m->m16[5] + midpoint[2] * m->m16[9] + m->m16[13];
		transformed[2] = midpoint[0] * m->m16[2] + midpoint[1] * m->m16[6] + midpoint[2] * m->m16[10] + m->m16[14];
	}
	else
	{
		transformed[0] = midpoint[0];
		transformed[1] = midpoint[1];
		transformed[2] = midpoint[2];
	}

	return transformed;
}


void R_AddLiquidToAlphaList (r_modelsurf_t *ms)
{
	alphalist_t *al;

	if (!(al = R_GrabAlphaSlot ()))
		return;

	al->setupfunc = R_LiquidBegin;
	al->takedownfunc = R_LiquidEnd;

	// to do - translate midpoint by matrix
	al->type = ALPHATYPE_LIQUID;
	al->dist = R_GetDist (R_TransformMidPoint (ms->surface->midpoint, ms->matrix));
	al->surf = ms->surface;
	al->matrix = ms->matrix;
	al->entnum = ms->entnum;
}


void R_ExtraSurfsBegin (void);
void R_ExtraSurfsEnd (void);

void R_AddSurfaceToAlphaList (r_modelsurf_t *ms)
{
	alphalist_t *al;

	if (!(al = R_GrabAlphaSlot ()))
		return;

	al->setupfunc = R_ExtraSurfsBegin;
	al->takedownfunc = R_ExtraSurfsEnd;

	// to do - translate midpoint by matrix
	al->type = ALPHATYPE_SURFACE;
	al->dist = R_GetDist (R_TransformMidPoint (ms->surface->midpoint, ms->matrix));
	al->surf = ms->surface;
	al->matrix = ms->matrix;
	al->entnum = ms->entnum;
}


void R_AddFenceToAlphaList (r_modelsurf_t *ms)
{
	alphalist_t *al;

	if (!(al = R_GrabAlphaSlot ()))
		return;

	al->setupfunc = R_ExtraSurfsBegin;
	al->takedownfunc = R_ExtraSurfsEnd;

	// to do - translate midpoint by matrix
	al->type = ALPHATYPE_FENCE;
	al->dist = R_GetDist (R_TransformMidPoint (ms->surface->midpoint, ms->matrix));
	al->surf = ms->surface;
	al->matrix = ms->matrix;
	al->entnum = ms->entnum;
}


void R_AddEntityToAlphaList (entity_t *ent)
{
	alphalist_t *al;

	if (!(al = R_GrabAlphaSlot ()))
		return;

	switch (ent->model->type)
	{
	case mod_alias:
		al->type = ALPHATYPE_ALIAS;
		break;

	case mod_brush:
		al->type = ALPHATYPE_BRUSH;
		break;

	case mod_sprite:
		al->type = ALPHATYPE_SPRITE;
		al->setupfunc = R_SpriteBegin;
		al->takedownfunc = R_SpriteEnd;
		break;

	default:
		Host_Error ("R_AddEntityToAlphaList: unknown model type\n");
		return;
	}

	al->dist = R_GetDist (ent->origin);
	al->ent = ent;
}


int R_AlphaSortFunc (const void *a1, const void *a2)
{
	alphalist_t *al1 = *(alphalist_t **) a1;
	alphalist_t *al2 = *(alphalist_t **) a2;

	// back to front ordering
	return (int) (al2->dist - al1->dist);
}


void R_InvalidateSprite (void);
void R_RenderFenceTexture (msurface_t *surf, glmatrix *matrix, int entnum);
void R_DrawAliasBatches (entity_t **ents, int numents, void *meshbuffer);
void R_SetupAliasModel (entity_t *e);
void R_DrawAlphaBModel (entity_t *ent);
void R_InvalidateLiquid (void);
void R_InvalidateExtraSurf (void);
void R_RenderAlphaSurface (msurface_t *surf, glmatrix *m, int entnum);
void R_RenderLiquidSurface (msurface_t *surf, glmatrix *m);


void R_DrawAlphaList (void)
{
	int i;
	alphalist_t *prev;
	alphalist_t *curr;

	if (!r_numalphalist)
		return;

	if (r_numalphalist > 1)
	{
		qsort
		(
			r_alphalist,
			r_numalphalist,
			sizeof (alphalist_t *),
			R_AlphaSortFunc
		);
	}

	qglEnable (GL_BLEND);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDepthMask (GL_FALSE);

	qglEnable (GL_ALPHA_TEST);
	qglAlphaFunc (GL_GREATER, 0.005);

	prev = NULL;

	// invalidate all cached states
	R_InvalidateSprite ();
	R_InvalidateLiquid ();
	R_InvalidateExtraSurf ();

	for (i = 0; i < r_numalphalist; i++)
	{
		qbool newtype = false;

		curr = r_alphalist[i];

		if (!prev)
			newtype = true;
		else if (prev->type != curr->type)
		{
			// invalidate all cached states
			R_InvalidateSprite ();
			R_InvalidateLiquid ();
			R_InvalidateExtraSurf ();

			// callbacks for state takedown (if needed)
			if (prev->takedownfunc)
				prev->takedownfunc ();

			// bring up new (also needed if !prev)
			newtype = true;
		}
		else
			newtype = false;

		if (newtype && curr->setupfunc)
			curr->setupfunc ();

		// drawing function
		switch (curr->type)
		{
		case ALPHATYPE_ALIAS:
			R_SetupAliasModel (curr->ent);

			if (curr->ent->visframe == r_framecount)
				R_DrawAliasBatches (&curr->ent, 1, scratchbuf);

			break;

		case ALPHATYPE_BRUSH:
			R_DrawAlphaBModel (curr->ent);
			break;

		case ALPHATYPE_SPRITE:
			R_DrawSpriteModel (curr->ent);
			break;

		case ALPHATYPE_PARTICLE:
			R_DrawParticlesForType (curr->particles);
			break;

		case ALPHATYPE_FENCE:
			R_RenderFenceTexture (curr->surf, curr->matrix, curr->entnum);
			break;

		case ALPHATYPE_LIQUID:
			R_RenderLiquidSurface (curr->surf, curr->matrix);
			break;

		case ALPHATYPE_SURFACE:
			R_RenderAlphaSurface (curr->surf, curr->matrix, curr->entnum);
			break;

		case ALPHATYPE_CORONA:
			R_RenderCorona (curr->light);
			break;

		default:
			break;
		}

		prev = curr;
	}

	if (prev->takedownfunc)
		prev->takedownfunc ();

	qglColor4f (1, 1, 1, 1);	// current color is undefined after vertex arrays so this is always needed
	qglAlphaFunc (GL_GREATER, 0.666);
	qglDisable (GL_ALPHA_TEST);

	qglDisable (GL_BLEND);
	qglDepthMask (GL_TRUE);

	// not strictly speaking necessary but it does the job well enough
	r_numalphalist = 0;
}


/*
=============================================================

  TRANSLATED PLAYER SKINS

=============================================================
*/

typedef struct r_translation_s {
	int topcolor;
	int bottomcolor;
	char skinname[32];
} r_translation_t;

static r_translation_t r_translations[MAX_CLIENTS];
static struct skin_s *r_baseskin;
static char cur_baseskin[32];

// called on startup and every time gamedir changes
void R_FlushTranslations (void)
{
	int i;

	Skin_Flush ();
	for (i = 0; i < MAX_CLIENTS; i++) {
		r_translations[i].topcolor = -1; // this will force a rebuild
		r_translations[i].skinname[0] = 0;
	}
	r_baseskin = NULL;
	cur_baseskin[0] = 0;
}

void R_TranslatePlayerSkin (int playernum, byte *original)
{
	int		top, bottom;
	byte	translate[256];
	unsigned	translate32[256];
	int		i, j;
	unsigned	pixels[512*256], *out;
	int			inwidth, inheight;
	int			tinwidth, tinheight;
	byte		*inrow;
	unsigned	frac, fracstep;
	extern	byte		player_8bit_texels[320*200];

	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_NONE);

	top = r_translations[playernum].topcolor;
	bottom = r_translations[playernum].bottomcolor;
	top = (top < 0) ? 0 : ((top > 13) ? 13 : top);
	bottom = (bottom < 0) ? 0 : ((bottom > 13) ? 13 : bottom);
	top *= 16;
	bottom *= 16;

	for (i = 0; i < 256; i++)
			translate[i] = i;

	for (i = 0; i < 16; i++)
	{
		if (top < 128)	// the artists made some backwards ranges.  sigh.
			translate[TOP_RANGE+i] = top+i;
		else
			translate[TOP_RANGE+i] = top+15-i;

		if (bottom < 128)
			translate[BOTTOM_RANGE+i] = bottom+i;
		else
			translate[BOTTOM_RANGE+i] = bottom+15-i;
	}

	for (i=0 ; i<256 ; i++)
		translate32[i] = d_8to24table_rgba[translate[i]];


	//
	// locate the original skin pixels
	//
	// real model width
	tinwidth = 296;
	tinheight = 194;

	if (original) {
		//skin data width
		inwidth = 320;
		inheight = 200;
	} else {
		original = player_8bit_texels;
		inwidth = 296;
		inheight = 194;
	}

	// because this happens during gameplay, do it fast
	// instead of sending it through gl_upload 8
	GL_ActiveTexture (GL_TEXTURE0_ARB);
	qglBindTexture (GL_TEXTURE_2D, playertextures + playernum);

	// scale and upload the texture
	out = pixels;
	memset(pixels, 0, sizeof(pixels));
	fracstep = tinwidth*0x10000/inwidth;
	for (i=0 ; i<inheight ; i++, out += inwidth)
	{
		inrow = original + inwidth*(i*tinheight/inheight);
		frac = fracstep >> 1;
		for (j=0 ; j<inwidth ; j+=4)
		{
			out[j] = translate32[inrow[frac>>16]];
			frac += fracstep;
			out[j+1] = translate32[inrow[frac>>16]];
			frac += fracstep;
			out[j+2] = translate32[inrow[frac>>16]];
			frac += fracstep;
			out[j+3] = translate32[inrow[frac>>16]];
			frac += fracstep;
		}
	}

	qglTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, inwidth, inheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	GL_TexEnv(GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_MODULATE);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	playerfbtextures[playernum] = 0;
	/*
	if (Mod_CheckFullbrights ((byte *)original, inwidth*inheight))
	{
		playerfbtextures[playernum] = playertextures + playernum + MAX_CLIENTS;
		GL_Bind (playerfbtextures[playernum]);

		out = pixels;
		memset(pixels, 0, sizeof(pixels));
		fracstep = tinwidth*0x10000/inwidth;

		// make all non-fullbright colors transparent
		for (i=0 ; i<inheight ; i++, out += inwidth)
		{
			inrow = original + inwidth*(i*tinheight/inheight);
			frac = fracstep >> 1;
			for (j=0 ; j<inwidth ; j+=4)
			{
				if (inrow[frac>>16] < 224)
					out[j] = translate32[inrow[frac>>16]] & LittleLong(0x00FFFFFF); // transparent
				else
					out[j] = translate32[inrow[frac>>16]]; // fullbright
				frac += fracstep;
				if (inrow[frac>>16] < 224)
					out[j+1] = translate32[inrow[frac>>16]] & LittleLong(0x00FFFFFF);
				else
					out[j+1] = translate32[inrow[frac>>16]];
				frac += fracstep;
				if (inrow[frac>>16] < 224)
					out[j+2] = translate32[inrow[frac>>16]] & LittleLong(0x00FFFFFF);
				else
					out[j+2] = translate32[inrow[frac>>16]];
				frac += fracstep;
				if (inrow[frac>>16] < 224)
					out[j+3] = translate32[inrow[frac>>16]] & LittleLong(0x00FFFFFF);
				else
					out[j+3] = translate32[inrow[frac>>16]];
				frac += fracstep;
			}
		}

		qglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, inwidth, inheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	*/
}

void R_GetTranslatedPlayerSkin (int colormap, unsigned int *texture, unsigned int *fb_texture)
{
	struct skin_s *skin;
	byte *data;
	r_translation_t *cur;
	translation_info_t *new;

	assert (colormap >= 0 && colormap <= MAX_CLIENTS);
	if (!colormap)
		return;

	cur = r_translations + (colormap - 1);
	new = r_refdef2.translations + (colormap - 1);

	// rebuild if necessary
	if (new->topcolor != cur->topcolor
		|| new->bottomcolor != cur->bottomcolor
		|| strcmp(new->skinname, cur->skinname))
	{
		cur->topcolor = new->topcolor;
		cur->bottomcolor = new->bottomcolor;
		strlcpy (cur->skinname, new->skinname, sizeof(cur->skinname));

		if (!cur->skinname[0]) {
			data = NULL;
			goto inlineskin;
		}

		Skin_Find (r_translations[colormap - 1].skinname, &skin);
		data = Skin_Cache (skin);
		if (!data) {
			if (r_refdef2.baseskin[0] && strcmp(r_refdef2.baseskin, cur->skinname)) {
				if (strcmp(cur_baseskin, r_refdef2.baseskin))
				{
					strlcpy (cur_baseskin, r_refdef2.baseskin, sizeof(cur_baseskin));
					if (!cur_baseskin[0]) {
						data = NULL;
						goto inlineskin;
					}
					Skin_Find (r_refdef2.baseskin, &r_baseskin);
				}
				data = Skin_Cache (r_baseskin);
			}
		}

inlineskin:
		R_TranslatePlayerSkin (colormap - 1, data);
	}

	*texture = playertextures + (colormap - 1);
	*fb_texture = playerfbtextures[colormap - 1];
}

/*
===============
R_NewMap
===============
*/
void CL_WipeParticles (void);

void R_NewMap (struct model_s *worldmodel)
{
	int		i;

	r_worldmodel = worldmodel;

	memset (&r_worldentity, 0, sizeof(r_worldentity));
	r_worldentity.model = r_worldmodel;

	CL_WipeParticles ();

	R_Modules_NewMap();

	// clear down stuff from the previous map
	R_ModelSurfsBeginMap ();
	R_AlphaListBeginMap ();

	// create any static vertex and index buffers we need for this map
	R_CreateMeshIndexBuffer ();

// clear out efrags in case the level hasn't been reloaded
// FIXME: is this one short?
	for (i = 0; i < r_worldmodel->numleafs; i++)
		r_worldmodel->leafs[i].efrags = NULL;
		 	
	r_viewleaf = NULL;

	GL_BuildLightmaps ();

	r_framecount = 0;
	r_visframecount = 0;

	Sky_NewMap (); // skybox in worldspawn
	Fog_NewMap (); // global fog in worldspawn

	R_FlushTranslations ();

	R_InitVertexBuffers ();
}


/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/ 

typedef struct _TargaHeader {
	unsigned char   id_length, colormap_type, image_type;
	unsigned short  colormap_index, colormap_length;
	unsigned char   colormap_size;
	unsigned short  x_origin, y_origin, width, height;
	unsigned char   pixel_size, attributes;
} TargaHeader;


/* 
================== 
R_ScreenShot_f
================== 
*/  
void R_ScreenShot_f (void)
{
	byte	*buffer;
	char	pcxname[MAX_OSPATH]; 
	char	checkname[MAX_OSPATH];
	int		i, c, temp;
	FILE	*f;

	if (Cmd_Argc() == 2)
	{
		strlcpy (pcxname, Cmd_Argv(1), sizeof(pcxname));
		COM_ForceExtension (pcxname, ".tga");
	}
	else
	{
		// 
		// find a file name to save it to 
		// 
		strcpy(pcxname,"quake00.tga");
		
		for (i = 0; i <= 99; i++) 
		{ 
			pcxname[5] = i/10 + '0'; 
			pcxname[6] = i%10 + '0'; 
			sprintf (checkname, "%s/%s", cls.gamedir, pcxname);
			f = fopen (checkname, "rb");
			if (!f)
				break;  // file doesn't exist
			fclose (f);
		} 
		if (i==100) 
		{
			Com_Printf ("R_ScreenShot_f: Couldn't create a TGA file\n"); 
			return;
		}
	}		

	buffer = Q_malloc (vid.width * vid.height * 3 + 18);
	memset (buffer, 0, 18);
	buffer[2] = 2;          // uncompressed type
	buffer[12] = vid.width&255;
	buffer[13] = vid.width>>8;
	buffer[14] = vid.height&255;
	buffer[15] = vid.height>>8;
	buffer[16] = 24;        // pixel size

	qglReadPixels (0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, buffer+18 ); 

	// swap rgb to bgr
	c = 18 + vid.width * vid.height * 3;
	for (i=18 ; i<c ; i+=3)
	{
		temp = buffer[i];
		buffer[i] = buffer[i+2];
		buffer[i+2] = temp;
	}
	COM_WriteFile (va("%s/%s", cls.gamedirfile, pcxname), buffer, vid.width*vid.height*3 + 18 );

	Q_free (buffer);
	Com_Printf ("Wrote %s\n", pcxname);
} 
 


/*
Find closest color in the palette for named color
*/
int MipColor(int r, int g, int b)
{
	int i;
	float dist;
	int best = 0;
	float bestdist;
	int r1, g1, b1;
	byte *pal = (byte *)d_8to24table;
	static int lr = -1, lg = -1, lb = -1;
	static int lastbest;

	if (r == lr && g == lg && b == lb)
		return lastbest;

	bestdist = 256*256*3;

	for (i = 0; i < 256; i++) {
		r1 = pal[i*3] - r;
		g1 = pal[i*3+1] - g;
		b1 = pal[i*3+2] - b;
		dist = r1*r1 + g1*g1 + b1*b1;
		if (dist < bestdist) {
			bestdist = dist;
			best = i;
		}
	}
	lr = r; lg = g; lb = b;
	lastbest = best;
	return best;
}

void R_DrawCharToSnap (int num, byte *dest, int width)
{
	int		row, col;
	byte	*source;
	int		drawline;
	int		x;
	extern byte *draw_chars;

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	drawline = 8;

	while (drawline--)
	{
		for (x=0 ; x<8 ; x++)
			if (source[x])
				dest[x] = source[x];
			else
				dest[x] = 98;
		source += 128;
		dest -= width;
	}
}

void R_DrawStringToSnap (const char *s, byte *buf, int x, int y, int width)
{
	byte *dest;
	const unsigned char *p;

	dest = buf + ((y * width) + x);

	p = (const unsigned char *)s;
	while (*p) {
		R_DrawCharToSnap(*p++, dest, width);
		dest += 8;
	}
}


/* 
================== 
R_RSShot

Memory pointed to by pcxdata is allocated using Hunk_TempAlloc
Never store this pointer for later use!

On failure (not enough memory), *pcxdata will be set to NULL
================== 
*/
#define RSSHOT_WIDTH 320
#define RSSHOT_HEIGHT 200

void R_RSShot (byte **pcxdata, int *pcxsize)
{ 
	int     x, y;
	unsigned char	*src, *dest;
	unsigned char	*newbuf;
	int w, h;
	int dx, dy, dex, dey, nx;
	int r, b, g;
	int count;
	float fracw, frach;
	char st[80];
	time_t now;
	byte *pal = (byte *)d_8to24table;
	extern cvar_t name;

// 
// save the pcx file 
// 
	newbuf = Q_malloc (vid.height * vid.width * 3);

	qglReadPixels (0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, newbuf);

	w = min (vid.width, RSSHOT_WIDTH);
	h = min (vid.height, RSSHOT_HEIGHT);

	fracw = (float)vid.width / (float)w;
	frach = (float)vid.height / (float)h;

	for (y = 0; y < h; y++) {
		dest = newbuf + (w*3 * y);

		for (x = 0; x < w; x++) {
			r = g = b = 0;

			dx = x * fracw;
			dex = (x + 1) * fracw;
			if (dex == dx) dex++; // at least one
			dy = y * frach;
			dey = (y + 1) * frach;
			if (dey == dy) dey++; // at least one

			count = 0;
			for (/* */; dy < dey; dy++) {
				src = newbuf + (vid.width * 3 * dy) + dx * 3;
				for (nx = dx; nx < dex; nx++) {
					r += *src++;
					g += *src++;
					b += *src++;
					count++;
				}
			}
			r /= count;
			g /= count;
			b /= count;
			*dest++ = r;
			*dest++ = g;
			*dest++ = b;
		}
	}

	// convert to eight bit
	for (y = 0; y < h; y++) {
		src = newbuf + (w * 3 * y);
		dest = newbuf + (w * y);

		for (x = 0; x < w; x++) {
			*dest++ = MipColor(src[0], src[1], src[2]);
			src += 3;
		}
	}

	time(&now);
	strcpy(st, ctime(&now));
	st[strlen(st) - 1] = 0;
	R_DrawStringToSnap (st, newbuf, w - strlen(st)*8, h - 1, w);

	strlcpy (st, cls.servername, sizeof(st));
	R_DrawStringToSnap (st, newbuf, w - strlen(st)*8, h - 11, w);

	strlcpy (st, name.string, sizeof(st));
	R_DrawStringToSnap (st, newbuf, w - strlen(st)*8, h - 21, w);

	// +w*(h-1) and -w are because we have the data upside down in newbuf
	WritePCX (newbuf + w*(h-1), w, h, -w, pal, pcxdata, pcxsize);

	Q_free (newbuf);

	// return with pcxdata and pcxsize
} 
