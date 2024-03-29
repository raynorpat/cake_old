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

gltexture_t *playertextures[MAX_CLIENTS]; // up to 32 color translated skins
gltexture_t *playerfbtextures[MAX_CLIENTS];

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
		qsort (r_alphalist, r_numalphalist, sizeof (alphalist_t *), R_AlphaSortFunc);

	qglEnable (GL_BLEND);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDepthMask (GL_FALSE);

	qglEnable (GL_ALPHA_TEST);
	qglAlphaFunc (GL_GREATER, 0.005);

	prev = NULL;

	// invalidate all cached states
	R_InvalidateSprite ();
	R_InvalidateLiquid ();

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
	char		name[64];
	int			top, bottom;
	int			inwidth, inheight;
	aliashdr_t	*paliashdr;
	int			skinnum;
	entity_t	*e;
	extern byte	player_8bit_texels[320*200];

	// get correct texture pixels
	e = &cl_visedicts[playernum];
	if (!e->model || e->model->type != mod_alias)
		return;

	paliashdr = (aliashdr_t *) Mod_Extradata (e->model);

	skinnum = e->skinnum;
	if (skinnum < 0 || skinnum >= paliashdr->numskins)
	{
		Com_DPrintf ("(%d): Invalid player skin #%d\n", playernum, skinnum);
		skinnum = 0;
	}
	
	top = r_translations[playernum].topcolor;
	bottom = r_translations[playernum].bottomcolor;

	if (original) {
		//skin data width
		inwidth = 320;
		inheight = 200;
	} else {
		original = player_8bit_texels;
		inwidth = 296;
		inheight = 194;
	}

	// upload new image
	sprintf (name, "player_%i", playernum);
	playertextures[playernum] = TexMgr_LoadImage (e->model, name, inwidth, inheight, SRC_INDEXED, original,
		paliashdr->gltextures[skinnum][0]->source_file, paliashdr->gltextures[skinnum][0]->source_offset, TEXPREF_OVERWRITE);

	// now recolor it
	if (!gl_nocolors.value)
		if (playertextures[playernum])
			TexMgr_ReloadImage (playertextures[playernum], top, bottom);
}

void R_GetTranslatedPlayerSkin (int colormap, gltexture_t **texture, gltexture_t **fb_texture)
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
	if (new->topcolor != cur->topcolor || new->bottomcolor != cur->bottomcolor || strcmp(new->skinname, cur->skinname))
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
				if (strcmp(cur_baseskin, r_refdef2.baseskin)) {
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

	*texture = playertextures[colormap - 1];
	*fb_texture = playerfbtextures[colormap - 1];
}

/*
===============
R_NewMap
===============
*/
void CL_WipeParticles (void);
extern qbool r_recachesurfaces;
extern char	mod_worldname[256];

void R_NewMap (struct model_s *worldmodel)
{
	int		i;

	r_worldmodel = worldmodel;

	memset (&r_worldentity, 0, sizeof(r_worldentity));
	r_worldentity.model = r_worldmodel;

	CL_WipeParticles ();

	R_Modules_NewMap();

	R_SetDefaultLightStyles ();

	// clear down stuff from the previous map
	R_ModelSurfsBeginMap ();
	R_AlphaListBeginMap ();

	// create any static vertex and index buffers we need for this map
	R_CreateMeshIndexBuffer ();

	// clear playertextures
	for (i = 0; i < MAX_CLIENTS; i++) {
		playertextures[i] = NULL;
		playerfbtextures[i] = NULL;
	}

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

	// switch off all view blending
	v_blend[0] = v_blend[1] = v_blend[2] = v_blend[3] = 0;

	// force all surfs to recache the first time they're seen
	r_recachesurfaces = true;

	// for the next map
	mod_worldname[0] = 0;

	R_BuildBrushBuffers ();
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
	qfile_t	*f;

	// find a file name to save it to 
	strcpy(pcxname,"quake00.tga");
		
	for (i = 0; i <= 99; i++) 
	{ 
		pcxname[5] = i/10 + '0'; 
		pcxname[6] = i%10 + '0'; 
		sprintf (checkname, "%s/%s", cls.gamedir, pcxname);
		f = FS_Open (checkname, "rb", false, false);
		if (!f)
			break;  // file doesn't exist
		FS_Close (f);
	} 
	if (i==100) 
	{
		Com_Printf ("R_ScreenShot_f: Couldn't create a TGA file\n"); 
		return;
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
	FS_WriteFile (pcxname, buffer, vid.width*vid.height*3 + 18);

	Q_free (buffer);
	Com_Printf ("Wrote %s\n", pcxname);
} 


/* 
============================================================================== 
 
						VIDEO
 
============================================================================== 
*/ 

/* 
================== 
R_TakeVideoFrame
================== 
*/
void R_TakeVideoFrame(int width, int height, byte * captureBuffer, byte * encodeBuffer, qbool motionJpeg)
{
	int             i, frameSize;

	qglReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, captureBuffer);

	/*
	if(motionJpeg)
	{
		frameSize = SaveJPGToBuffer(encodeBuffer, 90, width, height, captureBuffer);
		CL_WriteAVIVideoFrame(encodeBuffer, frameSize);
	}
	else
	*/
	{
		frameSize = width * height;

		for(i = 0; i < frameSize; i++)  // Pack to 24bpp and swap R and B
		{
			encodeBuffer[i * 3] = captureBuffer[i * 4 + 2];
			encodeBuffer[i * 3 + 1] = captureBuffer[i * 4 + 1];
			encodeBuffer[i * 3 + 2] = captureBuffer[i * 4];
		}

		CL_WriteAVIVideoFrame(encodeBuffer, frameSize * 3);
	}
}