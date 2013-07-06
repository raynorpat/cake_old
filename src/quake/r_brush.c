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
// r_brush.c: brush model rendering.

#include "gl_local.h"

float R_PlaneDist (mplane_t *plane, entity_t *ent);

extern float r_globalwateralpha;
extern GLuint r_surfacevbo;

GLenum gl_Lightmap_Format = GL_RGBA;
GLenum gl_Lightmap_Type = GL_UNSIGNED_BYTE;

void R_BuildLightMap (msurface_t *surf, byte *dest, int stride);
void R_CheckSubdivide (msurface_t *surf, model_t *mod);

void R_AllocModelSurf (msurface_t *surf, texture_t *tex, glmatrix *matrix, int entnum);


// using tall but narrow lightmaps means that we map need to touch more lightmaps to do updates, but we
// also need to touch considerably less of each lightmap
// these can never go so that a lightmap is > 1MB because it needs to use the scratchbuf for startup testing
#define	LM_BLOCK_WIDTH	256
#define	LM_BLOCK_HEIGHT	256

gltexture_t	lightmap_textures[MAX_LIGHTMAPS];

unsigned	blocklights[LM_BLOCK_WIDTH * LM_BLOCK_HEIGHT * 3];

typedef struct gl_lightmap_s
{
	glRect_t dirtyrect;
	qbool modified;
	int *allocated;
	byte *data;
	GLuint texnum;
} gl_lightmap_t;

gl_lightmap_t gl_lightmaps[MAX_LIGHTMAPS];
int lm_used = 0;

cvar_t r_instancedlight = {"r_instancedlight", "1", CVAR_ARCHIVE};

// so that I don't need to update it in a million places if I change the conditions
qbool R_UseInstancedLight (model_t *mod)
{
	if (mod->firstmodelsurface == 0 && r_instancedlight.value)// && !mod->lightdata)
		return true;
	else return false;
}


/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation (texture_t *base, int frame)
{
	texture_t *saved = base;
	int		relative;
	int		count;

	if (frame)
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}
	
	if (!base->anim_total)
		return base;

	relative = (int)(r_refdef2.time*10) % base->anim_total;
	count = 0;	

	while (base->anim_min > relative || base->anim_max <= relative)
	{
		base = base->anim_next;

		if (!base)
			return saved;
		if (++count > 100)
			return saved;
	}

	return base;
}


/*
=============================================================

	BRUSH MODELS

=============================================================
*/

/*
=================
R_DrawBrushModel
=================
*/
float m_identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1};

void R_MarkNodes (mnode_t *node)
{
	if (node->contents < 0) return;

	node->visframe = r_visframecount;

	R_MarkNodes (node->children[0]);
	R_MarkNodes (node->children[1]);
}

void R_BBoxForEnt (entity_t *e);

void R_DrawBrushModel (entity_t *e)
{
	int			i;
	msurface_t	*surf;
	float		dot;
	model_t		*clmodel;
	int			r_numdrawsurfs;
	glmatrix	*entmatrix;

	if (!r_drawentities.value) return;

	// to do - an offscreen model should probably cache anyway...
	R_BBoxForEnt(e);
	if (R_CullBox (e->mins, e->maxs)) return;

	clmodel = e->model;
	VectorSubtract (r_refdef2.vieworg, e->origin, e->modelorg);

	// hide the entity until we know that we need to draw it (based on r_numdrawsurfs)
	e->visframe = -1;
	r_numdrawsurfs = 0;

	// assume no transforms yet
	entmatrix = NULL;

	// always set identity so that we're prepared if we ever need it
	GL_IdentityMatrix (&e->matrix);

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		// fucking thing works again...
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (e->modelorg, temp);
		AngleVectors (e->angles, forward, right, up);

		e->modelorg[0] = DotProduct (temp, forward);
		e->modelorg[1] = -DotProduct (temp, right);
		e->modelorg[2] = DotProduct (temp, up);

		entmatrix = &e->matrix;
	}

	if (e->origin[0] || e->origin[1] || e->origin[2])
	{
		entmatrix = &e->matrix;
		GL_TranslateMatrix (&e->matrix, e->origin[0], e->origin[1], e->origin[2]);
	}

	// don't need to negate angles[0] as it's not going through the extra negation in R_RotateForEntity
	if (e->angles[1]) GL_RotateMatrix (&e->matrix, e->angles[1], 0, 0, 1);
	if (e->angles[0]) GL_RotateMatrix (&e->matrix, e->angles[0], 0, 1, 0);
	if (e->angles[2]) GL_RotateMatrix (&e->matrix, e->angles[2], 1, 0, 0);

	// draw it
	for (i = 0, surf = &clmodel->surfaces[clmodel->firstmodelsurface]; i < clmodel->nummodelsurfaces; i++, surf++)
	{
		// fucking thing works again...
		dot = R_PlaneDist (surf->plane, e);

		if (((surf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) || (!(surf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			// now mark as visible in this frame
			surf->visframe = r_framecount;

			if (surf->flags & SURF_DRAWTURB)
			{
				// set the correct wateralpha
				if (e->alpha < 255)
					surf->wateralpha = (float) e->alpha / 255.0f;
				else if (r_globalwateralpha > 0)
					surf->wateralpha = r_globalwateralpha;
				else surf->wateralpha = r_wateralpha.value;

				// turb surfaces don't animate and aren't backface culled
				// alpha surfs get added to the alpha list later on
				R_AllocModelSurf (surf, surf->texinfo->texture, entmatrix, e->entnum);
				R_CheckSubdivide (surf, clmodel);
			}
			else if (surf->flags & SURF_DRAWSKY)
			{
				// always alloc; no animation or lightmap
				R_AllocModelSurf (surf, surf->texinfo->texture, entmatrix, e->entnum);
			}
			else
			{
				// override
				surf->wateralpha = (float) e->alpha / 255.0f;

				// set the correct lightmap to use
				if (R_UseInstancedLight (clmodel))
					surf->lightmaptexturenum = 0;
				else surf->lightmaptexturenum = surf->truelightmaptexturenum;

				// always alloc
				R_AllocModelSurf (surf, R_TextureAnimation (surf->texinfo->texture, e->frame), entmatrix, e->entnum);
				r_numdrawsurfs++;
			}

			// correct the model (all brush surfs are now merged into the world, this is
			// only used for rebuilding and translating the verts)
			surf->model = e->model;

			rs_brushpolys++;
		}
	}

	// calculate dynamic lighting for bmodel if it's not an
	// instanced model
	if (clmodel->firstmodelsurface != 0)
	{
		// this is a hack to set visframe on nodes in this bmodel so that we can test visibility in R_MarkLights
		R_MarkNodes (clmodel->nodes + clmodel->firstnode);

		// and send it through the same as the world
		R_PushDlights (clmodel->nodes + clmodel->firstnode);
	}
	else if (R_UseInstancedLight (clmodel))
	{
		vec3_t lightcolor;
		int row = e->entnum >> 8;
		int col = e->entnum & 255;
		byte *base;

		R_LightPoint (e, lightcolor);

		// scale back to the same range as the world (accounting for overbrighting)
		if (gl_overbright.value)
		{
			lightcolor[0] *= (1.0f / 256.0f);
			lightcolor[1] *= (1.0f / 256.0f);
			lightcolor[2] *= (1.0f / 256.0f);
		}
		else
		{
			lightcolor[0] *= (1.0f / 128.0f);
			lightcolor[1] *= (1.0f / 128.0f);
			lightcolor[2] *= (1.0f / 128.0f);
		}

		base = gl_lightmaps[0].data + (row * LM_BLOCK_WIDTH + col) * 4;

		if (gl_Lightmap_Format == GL_BGRA)
		{
			base[0] = BYTE_CLAMP (lightcolor[2]);
			base[1] = BYTE_CLAMP (lightcolor[1]);
			base[2] = BYTE_CLAMP (lightcolor[0]);
			base[3] = 255;
		}
		else
		{
			base[0] = BYTE_CLAMP (lightcolor[0]);
			base[1] = BYTE_CLAMP (lightcolor[1]);
			base[2] = BYTE_CLAMP (lightcolor[2]);
			base[3] = 255;
		}

		gl_lightmaps[0].modified = true;

		if (col < gl_lightmaps[0].dirtyrect.left) gl_lightmaps[0].dirtyrect.left = col;
		if (col > gl_lightmaps[0].dirtyrect.right) gl_lightmaps[0].dirtyrect.right = col;
		if (row < gl_lightmaps[0].dirtyrect.top) gl_lightmaps[0].dirtyrect.top = row;
		if (row > gl_lightmaps[0].dirtyrect.bottom) gl_lightmaps[0].dirtyrect.bottom = row;
	}

	// now mark that we got regular surfaces for this entity
	// (to do - if the entity was merged to the world we don't need to draw it separately)
	if (r_numdrawsurfs) e->visframe = r_framecount;
}


/*
=============================================================

	LIGHTMAPS

=============================================================
*/

/*
================
R_RenderDynamicLightmaps
================
*/
void R_RenderDynamicLightmaps (msurface_t *surf)
{
	int			maps;
	glRect_t *dirtyrect;

	// lightmap 0 is the entity lightmap
	if (!surf->lightmaptexturenum) return;

	if (!r_worldmodel->lightdata) return;
	if (surf->flags & SURF_DRAWSKY) return;
	if (surf->flags & SURF_DRAWTURB) return;

	// dirty all lightmaps which were released
	if (!gl_lightmaps[surf->lightmaptexturenum].texnum) goto really_dynamic;

	// track if overbright needs to change (better than previous way)
	if (surf->overbright != (int) gl_overbright.value)
	{
		surf->overbright = (int) gl_overbright.value;
		goto really_dynamic;
	}

	// check for lightmap modification
	for (maps = 0; maps < MAX_SURFACE_STYLES && surf->styles[maps] != 255; maps++)
		if (d_lightstylevalue[surf->styles[maps]] != surf->cached_light[maps])
			goto dynamic;

	// dynamic this frame || dynamic previous frame
	if (surf->dlightframe == r_framecount || surf->cached_dlight)
	{
dynamic:;
		if (r_dynamic.value)
		{
			// mh - goto label for when gl_overbright changes
really_dynamic:;
			gl_lightmaps[surf->lightmaptexturenum].modified = true;
			dirtyrect = &gl_lightmaps[surf->lightmaptexturenum].dirtyrect;

			// mh - fixed dirtyrect to work right and be eminently more sensible
			if (surf->lightrect.left < dirtyrect->left) dirtyrect->left = surf->lightrect.left;
			if (surf->lightrect.right > dirtyrect->right) dirtyrect->right = surf->lightrect.right;
			if (surf->lightrect.top < dirtyrect->top) dirtyrect->top = surf->lightrect.top;
			if (surf->lightrect.bottom > dirtyrect->bottom) dirtyrect->bottom = surf->lightrect.bottom;

			R_BuildLightMap (surf, surf->lightbase, LM_BLOCK_WIDTH * 4);
		}
	}
}


void R_UpdateLightmap (gl_lightmap_t *lm, gltexture_t *tex)
{
	if (!r_worldmodel->lightdata) return;
	if (!lm->data) return;

	if (!lm->texnum)
	{
		qglGenTextures (1, &lm->texnum);
		tex->texnum = lm->texnum;
		GL_BindTexture (GL_TEXTURE1_ARB, tex);

		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		qglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, LM_BLOCK_WIDTH, LM_BLOCK_HEIGHT, 0, gl_Lightmap_Format, gl_Lightmap_Type, lm->data);
	}

	if (lm->modified)
	{
		glRect_t *dirtyrect = &lm->dirtyrect;

		// let's not update any lightmaps that have invalid dirtyrects
		// (although we should still reset the dirtyrect even if so)
		if (dirtyrect->left < dirtyrect->right && dirtyrect->top < dirtyrect->bottom)
		{
			void *dataptr = lm->data + (dirtyrect->top * LM_BLOCK_WIDTH + dirtyrect->left) * 4;

			GL_BindTexture (GL_TEXTURE1_ARB, tex);

			// mh - fixed dirtyrect to work right and be eminently more sensible
			GL_PixelStore (4, LM_BLOCK_WIDTH);

			qglTexSubImage2D
			(
				GL_TEXTURE_2D,
				0,
				dirtyrect->left,
				dirtyrect->top,
				(dirtyrect->right - dirtyrect->left),
				(dirtyrect->bottom - dirtyrect->top),
				gl_Lightmap_Format,
				gl_Lightmap_Type,
				dataptr
			);

			rs_dynamiclightmaps++;
		}

		lm->modified = false;
		dirtyrect->left = LM_BLOCK_WIDTH;
		dirtyrect->top = LM_BLOCK_HEIGHT;
		dirtyrect->right = 0;
		dirtyrect->bottom = 0;
	}
}


void R_UpdateLightmaps (void)
{
	int i;

	// testing
	// Image_WriteTGA ("lightmap0.tga", gl_lightmaps[0].data, LM_BLOCK_WIDTH, LM_BLOCK_HEIGHT, 32, false);

	if (gl_lightmaps[0].modified)
	{
		// expand the rect so that it'll be valid for updating
		if (gl_lightmaps[0].dirtyrect.left > 1) gl_lightmaps[0].dirtyrect.left--;
		if (gl_lightmaps[0].dirtyrect.top > 1) gl_lightmaps[0].dirtyrect.top--;
		if (gl_lightmaps[0].dirtyrect.right < LM_BLOCK_WIDTH) gl_lightmaps[0].dirtyrect.right++;
		if (gl_lightmaps[0].dirtyrect.bottom < LM_BLOCK_HEIGHT) gl_lightmaps[0].dirtyrect.bottom++;
	}

	for (i = 0; i < MAX_LIGHTMAPS; i++)
	{
#if 1
		R_UpdateLightmap (&gl_lightmaps[i], &lightmap_textures[i]);
#else
		if (gl_lightmaps[i].modified)
		{
			glRect_t *dirtyrect = &gl_lightmaps[i].dirtyrect;

			// let's not update any lightmaps that have invalid dirtyrects
			// (although we should still reset the dirtyrect even if so)
			if (dirtyrect->left < dirtyrect->right && dirtyrect->top < dirtyrect->bottom)
			{
				GL_BindTexture (GL_TEXTURE0, &lightmap_textures[i]);

				// mh - fixed dirtyrect to work right and be eminently more sensible
				GL_PixelStore (4, LM_BLOCK_WIDTH);

				qglTexSubImage2D
				(
					GL_TEXTURE_2D,
					0,
					dirtyrect->left,
					dirtyrect->top,
					(dirtyrect->right - dirtyrect->left),
					(dirtyrect->bottom - dirtyrect->top),
					gl_Lightmap_Format,
					gl_Lightmap_Type,
					gl_lightmaps[i].data + dirtyrect->top * LM_BLOCK_WIDTH * 4 + dirtyrect->left * 4
				);

				rs_dynamiclightmaps++;
			}

			gl_lightmaps[i].modified = false;
			dirtyrect->left = LM_BLOCK_WIDTH;
			dirtyrect->top = LM_BLOCK_HEIGHT;
			dirtyrect->right = 0;
			dirtyrect->bottom = 0;
		}
#endif
	}
}


/*
=====================
R_LMFindOptimalTSIMode

glTexSubImage2D can be incredibly slow on some systems.  To work around this we measure the time spent in
a call for updating a full lightmap texture for various formats and data types, then select the fastest of
the ones that worked.  If any takes longer than 666ms to update we error out - this is just too slow.

Alternatively we could just use mode 5 everywhere and be done with it.
=====================
*/

typedef struct tsitest_s
{
	char formatstr[64];
	char typestr[64];
	GLenum format;
	GLenum type;
} tsitest_t;

tsitest_t tsimodes[] =
{
	// prefer BGRA over RGBA and prefer 32-bit block transfers over 8-bit ones (some drivers don't recognize they're
	// using a 32-bit format anyway and optimize accordingly).  GL_UNSIGNED_INT_8_8_8_8 is a weird type that's only
	// fast on ATI and is one of the slowest on everything else so don't even bother looking for it.
	{"GL_BGRA", "GL_UNSIGNED_INT_8_8_8_8_REV", GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV},
	{"GL_BGRA", "GL_UNSIGNED_BYTE", GL_BGRA, GL_UNSIGNED_BYTE},
	{"GL_RGBA", "GL_UNSIGNED_BYTE", GL_RGBA, GL_UNSIGNED_BYTE}
};


void R_LMFindOptimalTSIMode (void)
{
	int i;
	int numtsimodes = sizeof (tsimodes) / sizeof (tsitest_t);
	unsigned int lm_texnum = 0;

	for (i = 0; i < numtsimodes; i++)
	{
		// clear last the error (if any)
		qglGetError ();

		// create a new texture object
		qglGenTextures (1, &lm_texnum);
		qglBindTexture (GL_TEXTURE_2D, lm_texnum);
		GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);
		qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		qglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, LM_BLOCK_WIDTH, LM_BLOCK_HEIGHT, 0, tsimodes[i].format, tsimodes[i].type, NULL);

		if (qglGetError () == GL_NO_ERROR)
		{
			// verify that we can do a successful upload to it
			qglTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, LM_BLOCK_WIDTH, LM_BLOCK_HEIGHT, tsimodes[i].format, tsimodes[i].type, scratchbuf);

			if (qglGetError () == GL_NO_ERROR)
			{
				// yeah, it worked
				qglDeleteTextures (1, &lm_texnum);
				gl_Lightmap_Type = tsimodes[i].type;
				gl_Lightmap_Format = tsimodes[i].format;
				Com_DPrintf ("lightmaps use GL_RGBA/%s/%s\n", tsimodes[i].formatstr, tsimodes[i].typestr);
				return;
			}
		}

		// boo!
		qglDeleteTextures (1, &lm_texnum);
	}

	// nothing worked so we've got a shitty driver here folks
	Sys_Error ("Failed to find a working lightmap mode!");
}


/*
========================
AllocBlock

returns a texture number and the position inside it
========================
*/
int AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	// texnum 0 is the entity lightmap
	// for (texnum = 1; texnum < MAX_LIGHTMAPS; texnum++)
	// Only scan over the last four textures. Only negligible effects on the
	// packing efficiency, but much faster for maps with a lot of lightmaps.
	// (updated - the new lightmap size is 4x the old one, so we only use 1 texture here)
	for (texnum = lm_used < 2 ? 1 : lm_used - 1; texnum < MAX_LIGHTMAPS; texnum++)
	{
		if (texnum > lm_used)
			lm_used = texnum;

		if (!gl_lightmaps[texnum].allocated)
		{
			// Hunk_Alloc will memset 0 for us
			gl_lightmaps[texnum].allocated = (int *) Hunk_Alloc (LM_BLOCK_WIDTH * sizeof (int));
		}

		best = LM_BLOCK_HEIGHT;

		for (i = 0; i < LM_BLOCK_WIDTH - w; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (gl_lightmaps[texnum].allocated[i+j] >= best)
					break;

				if (gl_lightmaps[texnum].allocated[i+j] > best2)
					best2 = gl_lightmaps[texnum].allocated[i+j];
			}

			if (j == w)
			{
				// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > LM_BLOCK_HEIGHT)
			continue;

		for (i=0 ; i<w ; i++)
			gl_lightmaps[texnum].allocated[*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("AllocBlock: full");
	return 0;
}


/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap (msurface_t *surf)
{
	if (surf->flags & SURF_DRAWSKY) return;
	if (surf->flags & SURF_DRAWTURB) return;

	surf->smax = (surf->extents[0] >> 4) + 1;
	surf->tmax = (surf->extents[1] >> 4) + 1;

	surf->lightmaptexturenum = AllocBlock (surf->smax, surf->tmax, &surf->lightrect.left, &surf->lightrect.top);

	// cache out the lightmap that was actually created so that we can switch it back if we need to
	surf->truelightmaptexturenum = surf->lightmaptexturenum;

	// complete the light rectangle for the surf
	surf->lightrect.right = surf->lightrect.left + surf->smax;
	surf->lightrect.bottom = surf->lightrect.top + surf->tmax;

	// cache initial cvars
	surf->overbright = (int) gl_overbright.value;

	// we need 4 component so we can map to the gpu format
	// this will be either bgra (preferred) or rgba (yuck) with alpha of 255 and is decided at upload time
	if (!gl_lightmaps[surf->lightmaptexturenum].data)
		gl_lightmaps[surf->lightmaptexturenum].data = (byte *) Hunk_Alloc (LM_BLOCK_WIDTH * LM_BLOCK_HEIGHT * 4);

	surf->lightbase = gl_lightmaps[surf->lightmaptexturenum].data;
	surf->lightbase += (surf->lightrect.top * LM_BLOCK_WIDTH + surf->lightrect.left) * 4;

	R_BuildLightMap (surf, surf->lightbase, LM_BLOCK_WIDTH * 4);
}


void GL_RegenerateVertexes (model_t *mod, msurface_t *surf, glmatrix *matrix)
{
	int i, j;
	glvertex_t *glv = surf->glvertexes;
	medge_t *r_pedge;
	float *vec;

	for (i = 0, j = surf->numglvertexes; i < surf->numglvertexes; i++, j--)
	{
		int lindex = mod->surfedges[surf->firstedge + i];
		int stripdst = i ? (i * 2 - 1) : 0;

		if (stripdst >= surf->numglvertexes) stripdst = j * 2;
		if (surf->flags & SURF_DRAWTURB) stripdst = i;

		glv = &surf->glvertexes[stripdst];

		if (lindex > 0)
		{
			r_pedge = &mod->edges[lindex];
			vec = mod->vertexes[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &mod->edges[-lindex];
			vec = mod->vertexes[r_pedge->v[1]].position;
		}

		glv->v[0] = vec[0] * matrix->m16[0] + vec[1] * matrix->m16[4] + vec[2] * matrix->m16[8] + matrix->m16[12];
		glv->v[1] = vec[0] * matrix->m16[1] + vec[1] * matrix->m16[5] + vec[2] * matrix->m16[9] + matrix->m16[13];
		glv->v[2] = vec[0] * matrix->m16[2] + vec[1] * matrix->m16[6] + vec[2] * matrix->m16[10] + matrix->m16[14];
	}
}


/*
================
GL_BuildPolygonForSurface

called at level load time
================
*/
void GL_BuildPolygonForSurface (msurface_t *surf, model_t *mod)
{
	int			i, j;
	medge_t		*r_pedge;
	glvertex_t	*verts;
	float		texscale = (1.0f / 32.0f);
	unsigned short *ndx;

	// done separately
	if ((surf->flags & SURF_DRAWTURB) && !gl_support_shader_objects) return;

	// detect sky
	if (!strncasecmp (surf->texinfo->texture->name, "sky", 3)) // sky surface
	{
		surf->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
		texscale = (1.0 / 128.0); // warp animation repeats every 128
	}
	else if (surf->flags & SURF_DRAWTURB)
	{
		texscale = 1.0f;
	}
	else if (surf->texinfo->flags & TEX_MISSING)
	{
		if (surf->samples) // lightmapped
		{
			surf->flags |= SURF_NOTEXTURE;
		}
		else // not lightmapped
		{
			surf->flags |= (SURF_NOTEXTURE | SURF_DRAWTILED);
			texscale = (1.0 / 32.0); //to match r_notexture_mip
		}
	}

	// rebuild the verts in generation order for better cache locality
	surf->glvertexes = &mod->glvertexes[mod->numglvertexes];
	surf->firstglvertex = mod->numglvertexes;
	mod->numglvertexes += surf->numglvertexes;

	surf->glindexes = &mod->glindexes[mod->numglindexes];
	surf->firstglindex = mod->numglindexes;
	mod->numglindexes += surf->numglindexes;

	// for glDrawArrays caching
	surf->model = mod;

	// generate the indexes here
	if (surf->flags & SURF_DRAWTURB)
	{
		for (i = 2, ndx = surf->glindexes; i < surf->numglvertexes; i++, ndx += 3)
		{
			ndx[0] = 0;
			ndx[1] = i - 1;
			ndx[2] = i;
		}
	}
	else
	{
		for (i = 2, ndx = surf->glindexes; i < surf->numglvertexes; i++, ndx += 3)
		{
			ndx[0] = i - 2;
			ndx[1] = (i & 1) ? i : (i - 1);
			ndx[2] = (i & 1) ? (i - 1) : i;
		}
	}

	// reconstruct the polygon
	surf->midpoint[0] = surf->midpoint[1] = surf->midpoint[2] = 0;

	for (i = 0, j = surf->numglvertexes; i < surf->numglvertexes; i++, j--)
	{
		int lindex = mod->surfedges[surf->firstedge + i];
		int stripdst = i ? (i * 2 - 1) : 0;

		if (stripdst >= surf->numglvertexes) stripdst = j * 2;
		if (surf->flags & SURF_DRAWTURB) stripdst = i;

		verts = &surf->glvertexes[stripdst];

		if (lindex > 0)
		{
			r_pedge = &mod->edges[lindex];
			VectorCopy (mod->vertexes[r_pedge->v[0]].position, verts->v);
		}
		else
		{
			r_pedge = &mod->edges[-lindex];
			VectorCopy (mod->vertexes[r_pedge->v[1]].position, verts->v);
		}

		VectorAdd (surf->midpoint, verts->v, surf->midpoint);

		if (surf->flags & SURF_DRAWTILED)
		{
			// tiled texture with texcoords derived from the verts
			verts->st1[0] = DotProduct (verts->v, surf->texinfo->vecs[0]) * texscale;
			verts->st1[1] = DotProduct (verts->v, surf->texinfo->vecs[1]) * texscale;

			// cache the unscaled s/t in st2 for GLSL
			verts->st2[0] = DotProduct (verts->v, surf->texinfo->vecs[0]);
			verts->st2[1] = DotProduct (verts->v, surf->texinfo->vecs[1]);
		}
		else
		{
			verts->st1[0] = (DotProduct (verts->v, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3]) / surf->texinfo->texture->width;
			verts->st1[1] = (DotProduct (verts->v, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3]) / surf->texinfo->texture->height;

			// lightmap texture coordinates
			verts->st2[0] = DotProduct (verts->v, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3];
			verts->st2[0] -= surf->texturemins[0];
			verts->st2[0] += surf->lightrect.left * 16;
			verts->st2[0] += 8;
			verts->st2[0] /= LM_BLOCK_WIDTH * 16; //surf->texinfo->texture->width;

			verts->st2[1] = DotProduct (verts->v, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];
			verts->st2[1] -= surf->texturemins[1];
			verts->st2[1] += surf->lightrect.top * 16;
			verts->st2[1] += 8;
			verts->st2[1] /= LM_BLOCK_HEIGHT * 16; //surf->texinfo->texture->height;
		}
	}

	VectorScale (surf->midpoint, 1.0f / surf->numglvertexes, surf->midpoint);
}

/*
==================
GL_BuildLightmaps

Called at level load time
Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void GL_DeleteAllLightmaps (void)
{
	int i;

	for (i = 0; i < MAX_LIGHTMAPS; i++)
	{
		if (gl_lightmaps[i].texnum)
		{
			qglDeleteTextures (1, &gl_lightmaps[i].texnum);
			gl_lightmaps[i].texnum = 0;
		}

		memset (&lightmap_textures[i], 0, sizeof (gltexture_t));
	}

	lm_used = 0;
}


int Surf_TextureSort (msurface_t *s1, msurface_t *s2)
{
	return s1->texinfo->texture - s2->texinfo->texture;
}


int Surf_NumberSort (msurface_t *s1, msurface_t *s2)
{
	return s1->surfnum - s2->surfnum;
}

void Mod_SetCorrectBBox (model_t *mod);

void GL_BuildLightmaps (void)
{
	int		i, j, numlm;
	model_t	*m;

	r_framecount = 1; // no dlightcache
	lm_used = 0;

	for (i=0; i < MAX_LIGHTMAPS; i++)
	{
		// these were changed to dynamic allocation to relieve memory pressure
		// putting them on the hunk means we just need to NULL the pointers
		gl_lightmaps[i].data = NULL;
		gl_lightmaps[i].allocated = NULL;
	}

	for (j=1 ; j<MAX_MODELS ; j++)
	{
		msurface_t *surf = NULL;

		if (!(m = cl.model_precache[j])) break;
		if (m->type != mod_brush) continue;

		Mod_SetCorrectBBox (m);

		if (m->name[0] == '*') continue;

		// order surfs by texture
		qsort (m->surfaces, m->numsurfaces, sizeof (msurface_t), (sortfunc_t) Surf_TextureSort);

		// restart vertexes
		m->numglvertexes = 0;
		m->numglindexes = 0;

		for (i = 0, surf = m->surfaces; i < m->numsurfaces; i++, surf++)
		{
			GL_CreateSurfaceLightmap (surf);
			GL_BuildPolygonForSurface (surf, m);
		}

		// restore original order
		qsort (m->surfaces, m->numsurfaces, sizeof (msurface_t), (sortfunc_t) Surf_NumberSort);
	}

	// create lightmap 0 as the entity lightmap
	gl_lightmaps[0].allocated = (int *) Hunk_Alloc (LM_BLOCK_WIDTH * sizeof (int));
	gl_lightmaps[0].data = (byte *) Hunk_Alloc (LM_BLOCK_WIDTH * LM_BLOCK_HEIGHT * 4);

	// give it a fake allocated so that it will create properly
	gl_lightmaps[0].allocated[0] = 1;

	// upload all lightmaps that were filled
	// lightmap 0 is the entity lightmap
	for (i = 0, numlm = 0; i < MAX_LIGHTMAPS; i++)
	{
		if (!gl_lightmaps[i].allocated || !gl_lightmaps[i].allocated[0])
		{
			if (gl_lightmaps[i].texnum)
			{
				qglDeleteTextures (1, &gl_lightmaps[i].texnum);
				gl_lightmaps[i].texnum = 0;
			}

			continue;
		}

		// clear anything left over
		memset (&lightmap_textures[i], 0, sizeof (gltexture_t));

		// faster lightmap loading through reuse of texture objects
		if (!gl_lightmaps[i].texnum)
		{
			qglGenTextures (1, &gl_lightmaps[i].texnum);
			lightmap_textures[i].texnum = gl_lightmaps[i].texnum;
			GL_BindTexture (GL_TEXTURE0_ARB, &lightmap_textures[i]);

			if (i == 0)
			{
				// the entity lightmap needs to read a single texel so it uses point sampling
				qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			}
			else
			{
				qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}

			qglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, LM_BLOCK_WIDTH, LM_BLOCK_HEIGHT, 0, gl_Lightmap_Format, gl_Lightmap_Type, gl_lightmaps[i].data);
		}
		else
		{
			lightmap_textures[i].texnum = gl_lightmaps[i].texnum;
			GL_BindTexture (GL_TEXTURE0_ARB, &lightmap_textures[i]);

			qglTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, LM_BLOCK_WIDTH, LM_BLOCK_HEIGHT, gl_Lightmap_Format, gl_Lightmap_Type, gl_lightmaps[i].data);
		}

		gl_lightmaps[i].modified = false;

		gl_lightmaps[i].dirtyrect.left = LM_BLOCK_WIDTH;
		gl_lightmaps[i].dirtyrect.top = LM_BLOCK_HEIGHT;
		gl_lightmaps[i].dirtyrect.right = 0;
		gl_lightmaps[i].dirtyrect.bottom = 0;

		numlm++;
	}
}

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights (msurface_t *surf)
{
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		impact, local;
	int			s, t;
	int			i;
	mtexinfo_t	*tex;
	float		brightness;
	unsigned	*bl;

	tex = surf->texinfo;

	for (lnum = 0; lnum < r_refdef2.numDlights; lnum++)
	{
		// not hit by this light
		if (!(surf->dlightbits[lnum >> 5] & (1 << (lnum & 31)))) continue;

		rad = r_refdef2.dlights[lnum].radius;
		dist = DotProduct (r_refdef2.dlights[lnum].origin, surf->plane->normal) - surf->plane->dist;
		rad -= fabs(dist);
		minlight = r_refdef2.dlights[lnum].minlight;

		if (rad < minlight)
			continue;

		minlight = rad - minlight;

		for (i=0 ; i<3 ; i++)
			impact[i] = r_refdef2.dlights[lnum].origin[i] -	surf->plane->normal[i]*dist;

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];

		bl = blocklights;

		for (t = 0; t < surf->tmax; t++)
		{
			td = local[1] - t*16;

			if (td < 0) td = -td;

			for (s = 0; s < surf->smax; s++)
			{
				sd = local[0] - s*16;

				if (sd < 0) sd = -sd;

				if (sd > td)
					dist = sd + (td>>1);
				else dist = td + (sd >> 1);

				if (dist < minlight)
				{
					brightness = (rad - dist);
					bl[0] += (int) (brightness * r_refdef2.dlights[lnum].rgb[0]);
					bl[1] += (int) (brightness * r_refdef2.dlights[lnum].rgb[1]);
					bl[2] += (int) (brightness * r_refdef2.dlights[lnum].rgb[2]);
				}

				bl += 3;
			}
		}
	}
}


/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void R_BuildLightMap (msurface_t *surf, byte *dest, int stride)
{
	int			t;
	int			i, j, size;
	byte		*lightmap;
	unsigned	scale;
	int			maps;
	unsigned	*bl;
	int			shift = gl_overbright.value ? 8 : 7;

	surf->cached_dlight = (surf->dlightframe == r_framecount);

	size = surf->smax * surf->tmax;
	lightmap = surf->samples;

	if (r_worldmodel->lightdata)
	{
		// clear to no light
		memset (&blocklights[0], 0, size * 3 * sizeof (unsigned int));

		// add all the lightmaps
		if (lightmap)
		{
			for (maps = 0; maps < MAX_SURFACE_STYLES && surf->styles[maps] != 255; maps++)
			{
				surf->cached_light[maps] = scale = d_lightstylevalue[surf->styles[maps]];

				// lit support via lordhavoc
				bl = blocklights;

				for (i = 0; i < size; i++, bl += 3, lightmap += 3)
				{
					bl[0] += lightmap[0] * scale;
					bl[1] += lightmap[1] * scale;
					bl[2] += lightmap[2] * scale;
				}
			}
		}

		// add all the dynamic lights
		if (surf->dlightframe == r_framecount)
			R_AddDynamicLights (surf);
	}
	else
	{
		// set to full bright if no light data
		memset (&blocklights[0], 255, size * 3 * sizeof (unsigned int));
	}

	// bound, invert, and shift
	stride -= surf->smax * 4;
	bl = blocklights;

	// this is faster than *dest++ and *bl++
	if (gl_Lightmap_Format == GL_BGRA)
	{
		for (i = 0; i < surf->tmax; i++, dest += stride)
		{
			for (j = 0; j < surf->smax; j++, dest += 4, bl += 3)
			{
				t = bl[2] >> shift; dest[0] = t > 255 ? 255 : t;
				t = bl[1] >> shift; dest[1] = t > 255 ? 255 : t;
				t = bl[0] >> shift; dest[2] = t > 255 ? 255 : t;
				dest[3] = 255;
			}
		}
	}
	else
	{
		// RGBA
		for (i = 0; i < surf->tmax; i++, dest += stride)
		{
			for (j = 0; j < surf->smax; j++, dest += 4, bl += 3)
			{
				t = bl[0] >> shift; dest[0] = t > 255 ? 255 : t;
				t = bl[1] >> shift; dest[1] = t > 255 ? 255 : t;
				t = bl[2] >> shift; dest[2] = t > 255 ? 255 : t;
				dest[3] = 255;
			}
		}
	}
}


void R_RecursiveVBOSetup (mnode_t *node, int *numvboverts)
{
	if (node->contents == CONTENTS_SOLID) return;
	if (node->contents < 0) return;

	R_RecursiveVBOSetup (node->children[0], numvboverts);

	if (node->numsurfaces)
	{
		int c;
		msurface_t *surf = r_worldmodel->surfaces + node->firstsurface;

		for (c = node->numsurfaces; c; c--, surf++)
		{
			if (surf->flags & SURF_DRAWTURB) continue;

			surf->vboffset = numvboverts[0];
			numvboverts[0] += surf->numglvertexes;
		}
	}

	R_RecursiveVBOSetup (node->children[1], numvboverts);
}


void R_BuildBrushBuffers (void)
{
	int i, j;
	model_t *m;
	msurface_t *surf = NULL;
	int numvboverts = 0;

	if (!gl_support_arb_vertex_buffer_object) return;

	for (j = 1; j < MAX_MODELS; j++)
	{
		if (!(m = cl.model_precache[j])) break;
		if (m->type != mod_brush) continue;

		// allocate offsets in bsp tree order so that we're sequentially hopping around
		// in the final vbo as much as possible
		R_RecursiveVBOSetup (m->nodes + m->firstnode, &numvboverts);
	}

	qglDeleteBuffersARB (1, &r_surfacevbo);
	qglGenBuffersARB (1, &r_surfacevbo);
	GL_BindBuffer (GL_ARRAY_BUFFER_ARB, r_surfacevbo);
	qglBufferDataARB (GL_ARRAY_BUFFER_ARB, numvboverts * sizeof (glvertex_t), NULL, GL_STATIC_DRAW_ARB);

	for (j = 1; j < MAX_MODELS; j++)
	{
		if (!(m = cl.model_precache[j])) break;
		if (m->type != mod_brush) continue;
		if (m->name[0] == '*') continue;

		for (i = 0, surf = m->surfaces; i < m->numsurfaces; i++, surf++)
		{
			if (surf->vboffset == -1) continue;
			if (surf->flags & SURF_DRAWTURB) continue;

			qglBufferSubDataARB (GL_ARRAY_BUFFER_ARB, surf->vboffset * sizeof (glvertex_t), surf->numglvertexes * sizeof (glvertex_t), surf->glvertexes);
		}
	}

	GL_BindBuffer (GL_ARRAY_BUFFER_ARB, 0);
}

