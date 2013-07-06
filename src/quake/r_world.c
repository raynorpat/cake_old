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
// r_world.c: world model rendering

#include "gl_local.h"

extern qbool r_recachesurfaces;
int r_firstentitysurface = 0;

static int r_world_tmus = 0;
GLuint r_surfacevbo = 0;

extern float r_globalwateralpha;

void R_RenderDynamicLightmaps (msurface_t *surf);
void R_RenderModelSurfsWater (void);
void R_UpdateLightmap (int lnum);
void R_CheckSubdivide (msurface_t *surf, model_t *mod);

extern gltexture_t lightmap_textures[MAX_LIGHTMAPS];

byte *CM_FatPVS (vec3_t org);
extern byte mod_novis[MAX_MAP_LEAFS/8];
int vis_changed; //if true, force pvs to be refreshed

void R_RenderModelSurfsSky (r_modelsurf_t **mslist, int numms);

void R_AddSurfaceToAlphaList (r_modelsurf_t *ms);
void R_AddLiquidToAlphaList (r_modelsurf_t *ms);
void R_AddFenceToAlphaList (r_modelsurf_t *ms);
void R_UpdateLightmaps (void);


// all surfaces from all models are collected into a modelsurfs list which may then be sorted, etc as we see fit
// this allows models to be batched together as efficiently as possible

// ideally this would be a dynamically expanding pool but it ain't.
// oh well; we'll alloc the modelsurfs off the hunk anyway
// (65536 would be enough)
#define R_MAX_MODELSURFS	0x40000
r_modelsurf_t *r_modelsurfs[R_MAX_MODELSURFS] = {NULL};
r_modelsurf_t *lightmap_modelsurfs[MAX_LIGHTMAPS] = {NULL};
int r_num_modelsurfs = 0;
qbool r_skysurfaces = false;
entity_t r_worldentity;
extern mplane_t frustum[];

// this is called on a new map load (see r_newmap) to init the lists
// as the hunk will have been cleared from the previous map
void R_ModelSurfsBeginMap (void)
{
	int i;

	if (!r_world_tmus) qglGetIntegerv (GL_MAX_TEXTURE_UNITS_ARB, &r_world_tmus);

	for (i = 0; i < R_MAX_MODELSURFS; i++)
	{
		if (r_modelsurfs[i])
		{
			free (r_modelsurfs[i]);
			r_modelsurfs[i] = NULL;
		}
	}

	r_num_modelsurfs = 0;
}


// this is called at the start of each frame to clear down all used modelsurfs
// it may also be called as required during a frame, but be warned that it will clear down anything which has already accumulated!!!
// (so make sure that you've drawn what you want to draw before calling it!)
void R_ModelSurfsBeginFrame (void)
{
	gltexture_t *glt;
	extern gltexture_t *active_gltextures;

	// clear all modelsurfs from all textures that used them
	for (glt = active_gltextures; glt; glt = glt->next)
		glt->surfchain = NULL;

	// no modelsurfs in use yet
	r_skysurfaces = false;
}


// this adds a new modelsurf to the list; called for each surf that needs rendering in a frame
// matrix can be null
void R_AllocModelSurf (msurface_t *surf, texture_t *tex, glmatrix *matrix, int entnum)
{
	// oh shit
	if (r_num_modelsurfs >= R_MAX_MODELSURFS)
	{
		Com_Printf ("R_MAX_MODELSURFS - overflow (this won't happen after fullvis)\n");
		return;
	}

	// set necessary flags
	if (surf->flags & SURF_DRAWSKY) r_skysurfaces = true;

	// alloc if needed; we could pre-alloc a few thousand to be on the safe side
	// but in practice (e.g. with DirectQ which uses the same system) it doesn't really matter.
	// Hunk_Alloc is, however, slow...
	if (!r_modelsurfs[r_num_modelsurfs])
		r_modelsurfs[r_num_modelsurfs] = (r_modelsurf_t *) malloc (sizeof (r_modelsurf_t));

	// fill it in
	r_modelsurfs[r_num_modelsurfs]->surface = surf;
	r_modelsurfs[r_num_modelsurfs]->texture = tex;
	r_modelsurfs[r_num_modelsurfs]->matrix = matrix;

	// no chains yet
	r_modelsurfs[r_num_modelsurfs]->surfchain = NULL;
	r_modelsurfs[r_num_modelsurfs]->lightchain = NULL;

	// the entnum is used for correcting lightmap coords for instanced bmodels
	r_modelsurfs[r_num_modelsurfs]->entnum = entnum;

	// go to the next modelsurf
	r_num_modelsurfs++;
}


extern cvar_t r_lavaalpha;
extern cvar_t r_telealpha;
extern cvar_t r_slimealpha;
extern cvar_t r_lockalpha;

void R_SortModelSurfs (void)
{
	int i;
	r_modelsurf_t *ms;

	// clear the lightmap surfs
	for (i = 0; i < MAX_LIGHTMAPS; i++)
		lightmap_modelsurfs[i] = NULL;

	// reverse the modelsurfs into texture chains so that the final sort order will be front to back
	for (i = r_num_modelsurfs - 1; i >= 0; i--)
	{
		ms = r_modelsurfs[i];
		rs_brushpolys++; // count wpolys here

		// defer dynamic lights to here so that we can restrict R_PushDLights to surfs actually in the pvs
		R_RenderDynamicLightmaps (ms->surface);

		// override water alpha for certain surface types
		if (!r_lockalpha.value)
		{
			if (ms->surface->flags & SURF_DRAWLAVA) ms->surface->wateralpha = r_lavaalpha.value;
			if (ms->surface->flags & SURF_DRAWTELE) ms->surface->wateralpha = r_telealpha.value;
			if (ms->surface->flags & SURF_DRAWSLIME) ms->surface->wateralpha = r_slimealpha.value;
		}

		// draw sky as it passes; everything else is chained in modelsurfs
		if (ms->surface->flags & SURF_DRAWSKY)
			; // do nothing; handled separately
		else if (ms->surface->flags & SURF_DRAWFENCE)
			R_AddFenceToAlphaList (ms);
		else if (ms->surface->wateralpha < 1)
		{
			if (ms->surface->flags & SURF_DRAWTURB)
				R_AddLiquidToAlphaList (ms);
			else R_AddSurfaceToAlphaList (ms);
		}
		else
		{
			// chain in the list by texture
			ms->surfchain = ms->texture->gltexture->surfchain;
			ms->texture->gltexture->surfchain = ms;
		}
	}
}


float R_PlaneDist (mplane_t *plane, entity_t *ent)
{
	// for axial planes it's quicker to just eval the dist and there's no need to cache;
	// for non-axial we check the cache and use if it current, otherwise calc it and cache it
	if (plane->type < 3)
		return ent->modelorg[plane->type] - plane->dist;
	else if (plane->cacheframe == r_framecount && plane->cacheent == ent)
	{
		// Com_Printf ("Cached planedist\n");
		return plane->cachedist;
	}
	else plane->cachedist = DotProduct (ent->modelorg, plane->normal) - plane->dist;

	// cache the result for this plane
	plane->cacheframe = r_framecount;
	plane->cacheent = ent;

	// and return what we got
	return plane->cachedist;
}


void R_RecursiveWorldNode (mnode_t *node, int clipflags)
{
	float dot;
	int c, side;

	if (node->contents == CONTENTS_SOLID) return;
	if (node->visframe != r_visframecount) return;

	if (clipflags)
	{
		for (c = 0, side = 0; c < 4; c++)
		{
			// clipflags is a 4 bit mask, for each bit 0 = auto accept on this side, 1 = need to test on this side
			// BOPS returns 0 (intersect), 1 (inside), or 2 (outside).  if a node is inside on any side then all
			// of it's child nodes are also guaranteed to be inside on the same side
			if (!(clipflags & (1 << c))) continue;
			if ((side = BOX_ON_PLANE_SIDE (node->minmaxs, node->minmaxs + 3, &frustum[c])) == 2) return;
			if (side == 1) clipflags &= ~(1 << c);
		}
	}

	if (node->contents < 0)
	{
		// node is a leaf so add stuff for drawing
		msurface_t **mark = ((mleaf_t *) node)->firstmarksurface;
		int c = ((mleaf_t *) node)->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark)->visframe = r_framecount;
				(*mark)->clipflags = clipflags;
				mark++;
			} while (--c);
			}

		R_StoreEfrags (&((mleaf_t *) node)->efrags);
		return;
	}

	// figure which side of the node we're on
	dot = R_PlaneDist (node->plane, &r_worldentity);
	side = (dot >= 0 ? 0 : 1);

	// recurse down the children, front side first
	R_RecursiveWorldNode (node->children[side], clipflags);

	if ((c = node->numsurfaces) != 0)
	{
		msurface_t *surf = r_worldmodel->surfaces + node->firstsurface;
		texture_t *tex = NULL;

		if (dot < 0 - BACKFACE_EPSILON)
			side = SURF_PLANEBACK;
		else if (dot > BACKFACE_EPSILON)
			side = 0;

		for (; c; c--, surf++)
		{
			if (surf->visframe != r_framecount) continue;
			if ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK)) continue;		// wrong side

			// this can be optimized by only checking for culling if the leaf or node containing the surf intersects the frustum
			if (clipflags && surf->clipflags)
				if (R_CullBox (surf->mins, surf->maxs)) continue;

			// get the correct animation here as modelsurfs aren't aware of the entity frame;
			// the world never does alternate anims
			tex = R_TextureAnimation (surf->texinfo->texture, 0);

			// add to the list; the world is untransformed and has no matrix
			R_AllocModelSurf (surf, tex, NULL, -1);

			// take r_wateralpha from the world
			// (to do - add a worldspawn flag for overriding this...)
			if (surf->flags & SURF_DRAWTURB)
			{
				if (r_globalwateralpha > 0)
					surf->wateralpha = r_globalwateralpha;
				else surf->wateralpha = r_wateralpha.value;

				R_CheckSubdivide (surf, r_worldmodel);
			}
			else surf->wateralpha = 1;
		}
	}

	// recurse down the back side (the compiler should optimize this tail recursion)
	R_RecursiveWorldNode (node->children[!side], clipflags);
}


void R_SetVertexDecl353 (void)
{
	if (gl_support_arb_vertex_buffer_object)
	{
		GL_BindBuffer (GL_ARRAY_BUFFER_ARB, r_surfacevbo);

		GL_SetStreamSource (GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (glvertex_t), (void *) (0));
		GL_SetStreamSource (GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (GLSTREAM_TEXCOORD0, 2, GL_FLOAT, sizeof (glvertex_t), (void *) (3 * sizeof (float)));
		GL_SetStreamSource (GLSTREAM_TEXCOORD1, 2, GL_FLOAT, sizeof (glvertex_t), (void *) (5 * sizeof (float)));
		GL_SetStreamSource (GLSTREAM_TEXCOORD2, 2, GL_FLOAT, sizeof (glvertex_t), (void *) (3 * sizeof (float)));
	}
	else
	{
		glvertex_t *verts = (glvertex_t *) scratchbuf;

		GL_SetStreamSource (GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (glvertex_t), verts->v);
		GL_SetStreamSource (GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (GLSTREAM_TEXCOORD0, 2, GL_FLOAT, sizeof (glvertex_t), &verts->verts[3]);
		GL_SetStreamSource (GLSTREAM_TEXCOORD1, 2, GL_FLOAT, sizeof (glvertex_t), &verts->verts[5]);
		GL_SetStreamSource (GLSTREAM_TEXCOORD2, 2, GL_FLOAT, sizeof (glvertex_t), &verts->verts[3]);
	}
}


void R_SetVertexDecl350 (void)
{
	if (gl_support_arb_vertex_buffer_object)
	{
		GL_BindBuffer (GL_ARRAY_BUFFER_ARB, r_surfacevbo);

		GL_SetStreamSource (GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (glvertex_t), (void *) (0));
		GL_SetStreamSource (GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (GLSTREAM_TEXCOORD0, 2, GL_FLOAT, sizeof (glvertex_t), (void *) (3 * sizeof (float)));
		GL_SetStreamSource (GLSTREAM_TEXCOORD1, 2, GL_FLOAT, sizeof (glvertex_t), (void *) (5 * sizeof (float)));
		GL_SetStreamSource (GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);
	}
	else
	{
		glvertex_t *verts = (glvertex_t *) scratchbuf;

		GL_SetStreamSource (GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (glvertex_t), verts->v);
		GL_SetStreamSource (GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (GLSTREAM_TEXCOORD0, 2, GL_FLOAT, sizeof (glvertex_t), &verts->verts[3]);
		GL_SetStreamSource (GLSTREAM_TEXCOORD1, 2, GL_FLOAT, sizeof (glvertex_t), &verts->verts[5]);
		GL_SetStreamSource (GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);
	}
}


void R_SetVertexDecl300 (void)
{
	if (gl_support_arb_vertex_buffer_object)
	{
		GL_BindBuffer (GL_ARRAY_BUFFER_ARB, r_surfacevbo);

		GL_SetStreamSource (GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (glvertex_t), (void *) (0));
		GL_SetStreamSource (GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (GLSTREAM_TEXCOORD0, 2, GL_FLOAT, sizeof (glvertex_t), (void *) (3 * sizeof (float)));
		GL_SetStreamSource (GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);
	}
	else
	{
		glvertex_t *verts = (glvertex_t *) scratchbuf;

		GL_SetStreamSource (GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (glvertex_t), verts->v);
		GL_SetStreamSource (GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (GLSTREAM_TEXCOORD0, 2, GL_FLOAT, sizeof (glvertex_t), &verts->verts[3]);
		GL_SetStreamSource (GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);
	}
}


void R_SetVertexDecl500 (void)
{
	if (gl_support_arb_vertex_buffer_object)
	{
		GL_BindBuffer (GL_ARRAY_BUFFER_ARB, r_surfacevbo);

		GL_SetStreamSource (GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (glvertex_t), (void *) (0));
		GL_SetStreamSource (GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (GLSTREAM_TEXCOORD0, 2, GL_FLOAT, sizeof (glvertex_t), (void *) (5 * sizeof (float)));
		GL_SetStreamSource (GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);
	}
	else
	{
		glvertex_t *verts = (glvertex_t *) scratchbuf;

		GL_SetStreamSource (GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (glvertex_t), verts->v);
		GL_SetStreamSource (GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (GLSTREAM_TEXCOORD0, 2, GL_FLOAT, sizeof (glvertex_t), &verts->verts[5]);
		GL_SetStreamSource (GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);
	}
}


void R_SetVertexDecl000 (void)
{
	if (gl_support_arb_vertex_buffer_object)
	{
		GL_BindBuffer (GL_ARRAY_BUFFER_ARB, r_surfacevbo);

		GL_SetStreamSource (GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (glvertex_t), (void *) (0));
		GL_SetStreamSource (GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (GLSTREAM_TEXCOORD0, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);
	}
	else
	{
		glvertex_t *verts = (glvertex_t *) scratchbuf;

		GL_SetStreamSource (GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (glvertex_t), verts->v);
		GL_SetStreamSource (GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (GLSTREAM_TEXCOORD0, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);
	}
}


#define R_MAX_DRAWSURFACES	4096
msurface_t *r_drawsurfaces[R_MAX_DRAWSURFACES];
int r_numdrawsurfaces;
int r_numdrawverts;
glmatrix *r_lastmatrix;
texture_t *lasttexture = NULL;
gltexture_t *lastlightmap = NULL;
float last_surfalpha = -1;
qbool r_restart_surface = false;


void R_BeginSurfaces (void)
{
	r_numdrawsurfaces = 0;
	r_numdrawverts = 0;

	lasttexture = NULL;
	lastlightmap = NULL;
	last_surfalpha = -1;

	r_lastmatrix = NULL;
	r_restart_surface = false;
}


void R_FlushSurfaces (void)
{
	if (r_numdrawsurfaces > 1)
	{
		int i;
		glvertex_t *verts = (glvertex_t *) scratchbuf;
		unsigned short *ndx = (unsigned short *) (verts + r_numdrawverts);
		int numvertexes = 0;
		int numindexes = 0;

		GL_SetIndices (ndx);

		for (i = 0; i < r_numdrawsurfaces; i++)
		{
			msurface_t *surf = r_drawsurfaces[i];
			unsigned short *srcindexes = surf->glindexes;
			int n = (surf->numglindexes + 7) >> 3;

			// we can't just memcpy the indexes as we need to add an offset to each so instead we'll Duff the bastards
			switch (surf->numglindexes % 8)
			{
			case 0: do {*ndx++ = numvertexes + *srcindexes++;
			case 7: *ndx++ = numvertexes + *srcindexes++;
			case 6: *ndx++ = numvertexes + *srcindexes++;
			case 5: *ndx++ = numvertexes + *srcindexes++;
			case 4: *ndx++ = numvertexes + *srcindexes++;
			case 3: *ndx++ = numvertexes + *srcindexes++;
			case 2: *ndx++ = numvertexes + *srcindexes++;
			case 1: *ndx++ = numvertexes + *srcindexes++;
			} while (--n > 0);
			}

			memcpy (verts, surf->glvertexes, surf->numglvertexes * sizeof (glvertex_t));
			verts += surf->numglvertexes;
			numvertexes += surf->numglvertexes;
			numindexes += surf->numglindexes;
		}

		GL_DrawIndexedPrimitive (GL_TRIANGLES, numindexes, numvertexes);
	}
	else if (r_numdrawsurfaces)
	{
		int i;
		glvertex_t *verts = (glvertex_t *) scratchbuf;
		int numvertexes = 0;

		for (i = 0; i < r_numdrawsurfaces; i++)
		{
			msurface_t *surf = r_drawsurfaces[i];

			memcpy (verts, surf->glvertexes, surf->numglvertexes * sizeof (glvertex_t));
			verts += surf->numglvertexes;

			qglDrawArrays (GL_TRIANGLE_STRIP, numvertexes, surf->numglvertexes);
			numvertexes += surf->numglvertexes;
		}
	}

	r_numdrawsurfaces = 0;
	r_numdrawverts = 0;
}


qbool R_UseInstancedLight (model_t *mod);

void R_BatchSurface (msurface_t *surf, glmatrix *matrix, int entnum)
{
	if (matrix != r_lastmatrix)
	{
		R_FlushSurfaces ();

		// always load the world matrix as a baseline
		qglLoadMatrixf (r_world_matrix.m16);

		if (matrix)
		{
			// we need to do it this way as water transforms will go to shit otherwise
			// we'll put it back when we rewrite water surface handling...
			qglMultMatrixf (matrix->m16);
		}

		r_lastmatrix = matrix;
	}
	else if (r_numdrawsurfaces >= R_MAX_DRAWSURFACES)
		R_FlushSurfaces ();
	else if (r_numdrawverts >= 16384)
		R_FlushSurfaces ();

	if (entnum > -1 && R_UseInstancedLight (surf->model))
	{
		int i;
		glvertex_t *verts = (glvertex_t *) scratchbuf;
		float st20 = ((float) (entnum & 255) / 255.0f) + 0.0005f;
		float st21 = ((float) (entnum >> 8) / 255.0f) + 0.0005f;

		// flush everything so that we can safely use our scratchbuffer here
		R_FlushSurfaces ();

		// generate the vertexes with the correct lightmap texcoords for instanced lighting
		for (i = 0; i < surf->numglvertexes; i++, verts++)
		{
			verts->v[0] = surf->glvertexes[i].v[0];
			verts->v[1] = surf->glvertexes[i].v[1];
			verts->v[2] = surf->glvertexes[i].v[2];

			verts->st1[0] = surf->glvertexes[i].st1[0];
			verts->st1[1] = surf->glvertexes[i].st1[1];

			verts->st2[0] = st20;
			verts->st2[1] = st21;
		}

		qglDrawArrays (GL_TRIANGLE_STRIP, 0, surf->numglvertexes);

		return;
	}

	if (gl_support_arb_vertex_buffer_object)
	{
		if (surf->vboffset < 0) return;

		qglDrawArrays (GL_TRIANGLE_STRIP, surf->vboffset, surf->numglvertexes);
	}
	else
	{
		r_numdrawverts += surf->numglvertexes;
		r_drawsurfaces[r_numdrawsurfaces] = surf;
		r_numdrawsurfaces++;
	}
}


void R_EndSurfaces (void)
{
	// flush anything left over
	R_FlushSurfaces ();
	GL_BindBuffer (GL_ARRAY_BUFFER_ARB, 0);

	if (r_lastmatrix)
	{
		qglLoadMatrixf (r_world_matrix.m16);
		r_lastmatrix = NULL;
	}
}


void R_Setup_Multitexture (void)
{
	qglColor4f (1, 1, 1, 1);
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);

	if (gl_overbright.value)
		GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_RGB_SCALE_ARB);
	else GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_MODULATE);

	GL_TexEnv (GL_TEXTURE2_ARB, GL_TEXTURE_2D, GL_NONE);
}


void R_Takedown_Multitexture (void)
{
	GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_NONE);
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);
	qglColor4f (1, 1, 1, 1);
}


void R_Setup_Multitexture2 (void)
{
	qglColor4f (1, 1, 1, 1);
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);

	if (gl_overbright.value)
		GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_RGB_SCALE_ARB);
	else GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_MODULATE);

	GL_TexEnv (GL_TEXTURE2_ARB, GL_TEXTURE_2D, GL_ADD);
}


void R_Takedown_Multitexture2 (void)
{
	// need the test because this is called from alpha and fence surfs
	if (r_world_tmus > 2)
		GL_TexEnv (GL_TEXTURE2_ARB, GL_TEXTURE_2D, GL_NONE);

	GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_NONE);
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);
	qglColor4f (1, 1, 1, 1);
}


void R_Setup_Fullbright (void)
{
	qglDepthMask (GL_FALSE);
	qglEnable (GL_BLEND);
	qglBlendFunc (GL_ONE, GL_ONE);
	Fog_StartAdditive ();
}


void R_Takedown_Fullbright (void)
{
	Fog_StopAdditive ();
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDisable (GL_BLEND);
	qglDepthMask (GL_TRUE);
}


void R_StartExtraSurfs (void)
{
	if (lasttexture->fullbright && r_world_tmus > 2)
	{
		GL_TexEnv (GL_TEXTURE2_ARB, GL_TEXTURE_2D, GL_ADD);
		GL_BindTexture (GL_TEXTURE2_ARB, lasttexture->fullbright);
		R_SetVertexDecl353 ();
	}
	else
	{
		GL_TexEnv (GL_TEXTURE2_ARB, GL_TEXTURE_2D, GL_NONE);
		R_SetVertexDecl350 ();
	}
}


void R_ExtraSurfsBegin (void)
{
	R_Setup_Multitexture ();
	R_BeginSurfaces ();
}


void R_ExtraSurfsEnd (void)
{
	R_EndSurfaces ();

	// need to call this because we may have had 3 tmus up
	R_Takedown_Multitexture2 ();

	// because fences may have disabled it
	qglEnable (GL_BLEND);
}


void R_RenderFenceTexture (msurface_t *surf, glmatrix *matrix, int entnum)
{
	if (surf->texinfo->texture != lasttexture) r_restart_surface = true;
	if (&lightmap_textures[surf->lightmaptexturenum] != lastlightmap) r_restart_surface = true;
	if (surf->wateralpha != last_surfalpha) r_restart_surface = true;

	if (r_restart_surface)
	{
		R_FlushSurfaces ();

		if (surf->wateralpha > 0.99f)
			qglDisable (GL_BLEND);

		GL_BindTexture (GL_TEXTURE0_ARB, surf->texinfo->texture->gltexture);
		GL_BindTexture (GL_TEXTURE1_ARB, &lightmap_textures[surf->lightmaptexturenum]);

		lasttexture = surf->texinfo->texture;
		lastlightmap = &lightmap_textures[surf->lightmaptexturenum];
		last_surfalpha = surf->wateralpha;

		R_StartExtraSurfs ();

		if (surf->wateralpha < 1)
		{
			// this needs to be done after we enable the vertex arrays as they will switch off color
			qglColor4f (1, 1, 1, surf->wateralpha);
			GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_MODULATE);
		}

		r_restart_surface = false;
	}

	R_BatchSurface (surf, matrix, entnum);
}


void R_RenderAlphaSurface (msurface_t *surf, glmatrix *m, int entnum)
{
	if (surf->texinfo->texture != lasttexture) r_restart_surface = true;
	if (&lightmap_textures[surf->lightmaptexturenum] != lastlightmap) r_restart_surface = true;
	if (surf->wateralpha != last_surfalpha) r_restart_surface = true;

	if (r_restart_surface)
	{
		R_FlushSurfaces ();

		GL_BindTexture (GL_TEXTURE0_ARB, surf->texinfo->texture->gltexture);
		GL_BindTexture (GL_TEXTURE1_ARB, &lightmap_textures[surf->lightmaptexturenum]);

		lasttexture = surf->texinfo->texture;
		lastlightmap = &lightmap_textures[surf->lightmaptexturenum];
		last_surfalpha = surf->wateralpha;

		R_StartExtraSurfs ();

		// this needs to be done after we enable the vertex arrays as they will switch off color
		qglColor4f (1, 1, 1, surf->wateralpha);
		GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_MODULATE);

		r_restart_surface = false;
	}

	R_BatchSurface (surf, m, entnum);
}


void R_RenderModelSurfs_Multitexture (qbool fbpass)
{
	gltexture_t *glt;
	r_modelsurf_t *ms;
	extern gltexture_t *active_gltextures;
	qbool stateset = false;
	int i, lm;

	R_BeginSurfaces ();

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		gltexture_t *fullbright = NULL;
		int r_numdrawsurfs = 0;

		if (!(ms = glt->surfchain)) continue;

		// do water later
		if (ms->surface->flags & SURF_DRAWTURB) continue;

		if (r_world_tmus > 2 && gl_fullbrights.value)
		{
			// selectively skip fullbrights
			if (fbpass && !ms->texture->fullbright) continue;
			if (!fbpass && ms->texture->fullbright) continue;
		}

		fullbright = ms->texture->fullbright;

		// clear the lightmap chains
		for (i = 0; i < MAX_LIGHTMAPS; i++)
			lightmap_modelsurfs[i] = NULL;

		// sort the modelsurfs by lightmap (reduces state and texture changes, increases batch size)
		for (; ms; ms = ms->surfchain)
		{
			ms->lightchain = lightmap_modelsurfs[ms->surface->lightmaptexturenum];
			lightmap_modelsurfs[ms->surface->lightmaptexturenum] = ms;
			r_numdrawsurfs++;
		}

		// nothing to draw
		if (!r_numdrawsurfs) continue;

		// deferred until we know that we've got something
		if (!stateset)
		{
			if (fbpass)
			{
				R_SetVertexDecl353 ();
				R_Setup_Multitexture2 ();
			}
			else
			{
				R_SetVertexDecl350 ();
				R_Setup_Multitexture ();
			}

			stateset = true;
		}

		// set base texture
		GL_BindTexture (GL_TEXTURE0_ARB, glt);

		if (fbpass)
			GL_BindTexture (GL_TEXTURE2_ARB, fullbright);

		// now draw them all in lightmap order
		for (lm = 0; lm < MAX_LIGHTMAPS; lm++)
		{
			if (!(ms = lightmap_modelsurfs[lm])) continue;

			GL_BindTexture (GL_TEXTURE1_ARB, &lightmap_textures[lm]);

			for (; ms; ms = ms->lightchain)
				R_BatchSurface (ms->surface, ms->matrix, ms->entnum);

			R_FlushSurfaces ();
			lightmap_modelsurfs[lm] = NULL;
		}
	}

	R_EndSurfaces ();

	if (stateset)
	{
		if (fbpass)
			R_Takedown_Multitexture2 ();
		else R_Takedown_Multitexture ();
	}
}


void R_RenderModelSurfs_Fullbright (void)
{
	gltexture_t *glt;
	r_modelsurf_t *ms;
	extern gltexture_t *active_gltextures;
	qbool stateset = false;

	R_BeginSurfaces ();

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		if (!(ms = glt->surfchain)) continue;

		// do water later
		if (ms->surface->flags & SURF_DRAWTURB) continue;
		if (!ms->texture->fullbright) continue;

		if (!stateset)
		{
			GL_TexEnv (GL_TEXTURE2_ARB, GL_TEXTURE_2D, GL_NONE);
			GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_NONE);
			GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);

			R_SetVertexDecl300 ();
			R_Setup_Fullbright ();
			stateset = true;
		}

		// bind texture (always)
		GL_BindTexture (GL_TEXTURE0_ARB, ms->texture->fullbright);

		for (; ms; ms = ms->surfchain)
			R_BatchSurface (ms->surface, ms->matrix, ms->entnum);

		R_FlushSurfaces ();
	}

	R_EndSurfaces ();

	if (stateset)
		R_Takedown_Fullbright ();
}


void R_RenderModelSurfs_r_fullbright_1 (void)
{
	gltexture_t *glt;
	r_modelsurf_t *ms;
	extern gltexture_t *active_gltextures;

	GL_TexEnv (GL_TEXTURE2_ARB, GL_TEXTURE_2D, GL_NONE);
	GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_NONE);
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);

	R_SetVertexDecl300 ();

	R_BeginSurfaces ();

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		if (!(ms = glt->surfchain)) continue;

		// do water later
		if (ms->surface->flags & SURF_DRAWTURB) continue;

		// bind texture (always)
		GL_BindTexture (GL_TEXTURE0_ARB, ms->texture->gltexture);

		for (; ms; ms = ms->surfchain)
			R_BatchSurface (ms->surface, ms->matrix, ms->entnum);

		R_FlushSurfaces ();
	}

	R_EndSurfaces ();
}


void R_RenderModelSurfs_ShowTris (void)
{
	gltexture_t *glt;
	r_modelsurf_t *ms;
	extern gltexture_t *active_gltextures;

	R_SetVertexDecl000 ();
	R_BeginSurfaces ();

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		if (!(ms = glt->surfchain)) continue;

		// do water later
		if (ms->surface->flags & SURF_DRAWTURB) continue;

		for (; ms; ms = ms->surfchain)
			R_BatchSurface (ms->surface, ms->matrix, ms->entnum);
	}

	R_EndSurfaces ();
}


void R_RenderModelSurfs_r_lightmap_1 (void)
{
	gltexture_t *glt;
	r_modelsurf_t *ms;
	extern gltexture_t *active_gltextures;

	int i, lm;

	// clear the lightmap chains
	for (i = 0; i < MAX_LIGHTMAPS; i++)
		lightmap_modelsurfs[i] = NULL;

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		if (!(ms = glt->surfchain)) continue;

		// do water later
		if (ms->surface->flags & SURF_DRAWTURB) continue;

		// sort the modelsurfs by lightmap (reduces state and texture changes, increases batch size)
		for (; ms; ms = ms->surfchain)
		{
			ms->lightchain = lightmap_modelsurfs[ms->surface->lightmaptexturenum];
			lightmap_modelsurfs[ms->surface->lightmaptexturenum] = ms;
		}
	}

	R_SetVertexDecl500 ();
	R_BeginSurfaces ();

	GL_TexEnv (GL_TEXTURE2_ARB, GL_TEXTURE_2D, GL_NONE);
	GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_NONE);
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);

	// now draw them all in lightmap order
	for (lm = 0; lm < MAX_LIGHTMAPS; lm++)
	{
		if (!(ms = lightmap_modelsurfs[lm])) continue;

		// bind lightmap (always)
		GL_BindTexture (GL_TEXTURE0_ARB, &lightmap_textures[lm]);
		R_BeginSurfaces ();

		for (; ms; ms = ms->lightchain)
			R_BatchSurface (ms->surface, ms->matrix, ms->entnum);

		R_FlushSurfaces ();
		lightmap_modelsurfs[lm] = NULL;
	}

	R_EndSurfaces ();
}



//==============================================================================
//
// SETUP CHAINS
//
//==============================================================================

/*
===============
R_MarkLeafs

mark surfaces based on PVS and rebuild texture chains
===============
*/
void R_MarkLeafs (void)
{
	byte		*vis;
	mnode_t		*node;
	msurface_t	**mark;
	int			i;
	qbool		nearwaterportal;
	extern		cvar_t r_lockpvs;

	// if visibility doesn't need regenerating just get out
	if (r_lockpvs.value && r_viewleaf) return;
	if (r_oldviewleaf == r_viewleaf && !vis_changed) return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	// check this leaf for water portals
	// TODO: loop through all water surfs and use distance to leaf cullbox
	nearwaterportal = false;

	for (i = 0, mark = r_viewleaf->firstmarksurface; i < r_viewleaf->nummarksurfaces; i++, mark++)
		if ((*mark)->flags & SURF_DRAWTURB)
			nearwaterportal = true;

	// choose vis data
	if (r_novis.value || r_viewleaf->contents == CONTENTS_SOLID || r_viewleaf->contents == CONTENTS_SKY)
		vis = &mod_novis[0];
	else if (nearwaterportal)
		vis = CM_FatPVS (r_origin);
	else
		vis = Mod_LeafPVS (r_viewleaf, r_worldmodel);

	for (i = 0; i < r_worldmodel->numleafs; i++)
	{
		if (vis[i >> 3] & (1 << (i & 7)))
		{
			node = (mnode_t *) &r_worldmodel->leafs[i + 1];

			do
			{
				if (node->visframe == r_visframecount)
					break;

				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}


// this can be used for both the world and for alpha bmodels ;)
void R_RenderModelSurfs (qbool showtris)
{
	if (r_showtris.value && showtris)
		R_RenderModelSurfs_ShowTris ();
	else if (r_fullbright.value || !r_worldmodel->lightdata)
	{
		R_RenderModelSurfs_r_fullbright_1 ();

		// need to add fullbrights as they may have been removed from the textures
		if (gl_fullbrights.value)
			R_RenderModelSurfs_Fullbright ();
	}
	else if (r_lightmap.value)
		R_RenderModelSurfs_r_lightmap_1 ();
	else
	{
		// this was awful stuff.  by requiring multitexture and combine modes (which you'll have if you have anything better than a tnt2)
		// we get things MUCH cleaner.  we could go the full hog here and do a 3 TMU blend for the fullbrights too... maybe later...
		R_RenderModelSurfs_Multitexture (false);

		if (gl_fullbrights.value)
		{
			if (r_world_tmus > 2)
				R_RenderModelSurfs_Multitexture (true);
			else R_RenderModelSurfs_Fullbright ();
		}
	}
}


/*
=============
R_DrawWorld
=============
*/
void R_DrawBrushModel (entity_t *e);

// to do - make this generic for bmodels too...
void R_DrawWorld (void)
{
	int i;

	if (!r_drawworld.value) return;

	VectorCopy (r_refdef2.vieworg, r_worldentity.modelorg);

	if (r_recachesurfaces)
	{
		r_num_modelsurfs = 0;

		// emit the world surfs into modelsurfs
		R_RecursiveWorldNode (r_worldmodel->nodes, 15);

		r_firstentitysurface = r_num_modelsurfs;
		r_recachesurfaces = false;
	}
	else
	{
		// begin entities on the first modelsurf after the world
		r_num_modelsurfs = r_firstentitysurface;

		// bring everything else up to date
		for (i = 0; i < r_worldmodel->numleafs; i++)
		{
			mleaf_t *leaf = &r_worldmodel->leafs[i + 1];

			if (leaf->visframe == r_visframecount && leaf->efrags)
				R_StoreEfrags (&leaf->efrags);
		}

		for (i = 0; i < r_num_modelsurfs; i++)
		{
			r_modelsurf_t *ms = r_modelsurfs[i];

			if (ms->surface->flags & SURF_DRAWSKY) r_skysurfaces = true;

			// the world never does alternate anims
			ms->texture = R_TextureAnimation (ms->surface->texinfo->texture, 0);
		}
	}

	// push the dlights here to keep framecounts consistent
	R_PushDlights (r_worldmodel->nodes);

	// add brush models to the list
	for (i = 0; i < cl_numvisedicts; i++)
	{
		entity_t *ent = &cl_visedicts[i];

		// since this is our first walk through the visedicts list we need to fix up entity alpha
		// so that anything with alpha 0 is correctly switched to alpha 255
		if (ent->alpha < 1) ent->alpha = 255;

		switch (ent->model->type)
		{
		case mod_brush:
			// this just adds to the list
			R_DrawBrushModel (ent);
			break;

		default:
			break;
		}
	}

	// render sky first to give the lightmaps more time to finish updating (if needed)
	if (r_skysurfaces)
		R_RenderModelSurfsSky (r_modelsurfs, r_num_modelsurfs);

	// sort the modelsurfs by texture, doing stuff as we go
	R_SortModelSurfs ();

	// now batch update the lightmaps after everything has been built and chained
	// this is done here because if we defer it till after the texture has been used some drivers
	// will need to stall the pipeline waiting on drawing to complete before the lightmap can update
	R_UpdateLightmaps ();

	// split out as it can be called multiple times (e.g. for r_showtris)
	R_RenderModelSurfs (false);

	if (r_showtris.value)
	{
		R_ShowTrisBegin ();
		R_RenderModelSurfs (true);
		R_ShowTrisEnd ();
	}

	// this is only done for the world refresh; bmodels with alpha are handled in here too
	R_RenderModelSurfsWater ();

	//if (rs_dynamiclightmaps) Con_Printf ("%i lightmaps\n", rs_dynamiclightmaps);
}


void R_DrawAlphaBModel (entity_t *ent)
{
	// this does nothing any more...
}

