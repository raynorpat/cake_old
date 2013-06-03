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

static int r_world_tmus = 0;

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
	r_num_modelsurfs = 0;
	r_skysurfaces = false;
}


// this adds a new modelsurf to the list; called for each surf that needs rendering in a frame
// matrix can be null
void R_AllocModelSurf (msurface_t *surf, texture_t *tex, glmatrix *matrix, int entnum)
{
	// oh shit
	if (r_num_modelsurfs >= R_MAX_MODELSURFS) return;

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


// may also be used with leafs owing to common data
qbool R_CullNode (mnode_t *node)
{
	int i;
	extern mplane_t frustum[];

#if 0
	// this is just a sanity check and never happens (yes, i tested it)
	if (node->parent && node->parent->bops == FULLY_OUTSIDE_FRUSTUM)
	{
		node->bops = FULLY_OUTSIDE_FRUSTUM;
		return true;
	}
#endif

	// if the node's parent was fully inside the frustum then the node is also fully inside the frustum
	if (node->parent && node->parent->bops == FULLY_INSIDE_FRUSTUM)
	{
		node->bops = FULLY_INSIDE_FRUSTUM;
		return false;
	}

	// BoxOnPlaneSide result flags are unknown
	node->bops = 0;

	// need to check all 4 sides
	for (i = 0; i < 4; i++)
	{
		// if the parent is inside the frustum on this side then so is this node and we don't need to check
		if (node->parent && node->parent->sides[i] == INSIDE_FRUSTUM)
			continue;

		// need to check
		node->sides[i] |= BoxOnPlaneSide (node->minmaxs, node->minmaxs + 3, frustum + i);

		// if the node is outside the frustum on any side then the entire node is rejected
		if (node->sides[i] == OUTSIDE_FRUSTUM)
		{
			node->bops = FULLY_OUTSIDE_FRUSTUM;
			return true;
		}

		if (node->sides[i] == INTERSECT_FRUSTUM)
		{
			// if any side intersects the frustum we mark it all as intersecting so that we can do comparisons more cleanly
			node->bops = FULLY_INTERSECT_FRUSTUM;
			return false;
		}
	}

	// the node will be fully inside the frustum here because none of the sides evaluated to outside or intersecting
	// set the bops flag correctly as it may have been skipped above
	node->bops = FULLY_INSIDE_FRUSTUM;
	return false;
}


// theoretically we only need to do this if the PVS changes and then we can rely on culling and backfacing in the frames
// we might overflow the modelsurfs list if we did that however.
void R_RecursiveWorldNode (mnode_t *node)
{
	int			c, side;
	mplane_t	*plane;
	msurface_t	**mark;
	mleaf_t		*pleaf;
	float		dot;

loc0:;
	if (node->contents == CONTENTS_SOLID) return;		// solid
	if (node->visframe != r_visframecount) return;
	if (R_CullNode (node)) return;

	// if a leaf node, draw stuff
	if (node->contents < 0)
	{
		pleaf = (mleaf_t *) node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark)->intersect = (pleaf->bops == FULLY_INTERSECT_FRUSTUM);
				(*mark)->visframe = r_framecount;
				mark++;
			}
			while (--c);
		}

		// deal with model fragments in this leaf
		if (pleaf->efrags)
			R_StoreEfrags (&pleaf->efrags);

		return;
	}

	// node is just a decision point, so go down the apropriate sides
	// find which side of the node we are on
	plane = node->plane;

	switch (plane->type)
	{
	case PLANE_X:
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = DotProduct (modelorg, plane->normal) - plane->dist;
		break;
	}

	if (dot >= 0)
		side = 0;
	else side = 1;

	// recurse down the children, front side first
	R_RecursiveWorldNode (node->children[side]);

	// draw stuff
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
			if (surf->intersect && (node->bops == FULLY_INTERSECT_FRUSTUM))
				if (R_CullBox (surf->mins, surf->maxs)) continue;

			rs_brushpolys++; //count wpolys here

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

	// recurse down the back side
	// (in case the compiler doesn't optimize out tail recursion (it should, but you never know))
	node = node->children[!side];
	goto loc0;
}


void R_MarkLeafSurfs (mleaf_t *leaf)
{
	msurface_t **mark = leaf->firstmarksurface;
	int c = leaf->nummarksurfaces;

	if (c)
	{
		do
		{
			(*mark)->intersect = (leaf->bops == FULLY_INTERSECT_FRUSTUM);
			(*mark)->visframe = r_framecount;
			mark++;
		} while (--c);
	}
}


void R_NewRecursiveWorldNode (mnode_t *node)
{
	int c;
	int side;
	float dot;

loc0:;
	// node is just a decision point, so go down the appropriate sides
	// find which side of the node we are on
	switch (node->plane->type)
	{
	case PLANE_X:
		dot = modelorg[0] - node->plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - node->plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - node->plane->dist;
		break;
	default:
		dot = DotProduct (modelorg, node->plane->normal) - node->plane->dist;
		break;
	}

	// find which side we're on
	side = (dot >= 0 ? 0 : 1);

	// recurse down the children, front side first
	if (node->children[side]->contents == CONTENTS_SOLID) goto rrwnnofront;
	if (node->children[side]->visframe != r_visframecount) goto rrwnnofront;
	if (R_CullNode (node->children[side])) goto rrwnnofront;

	// check for a leaf
	if (node->children[side]->contents < 0)
	{
		R_MarkLeafSurfs ((mleaf_t *) node->children[side]);
		R_StoreEfrags (&((mleaf_t *) node->children[side])->efrags);
		goto rrwnnofront;
	}

	// now we can recurse
	R_NewRecursiveWorldNode (node->children[side]);

rrwnnofront:;
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
			if ((dot < 0) ^ !! (surf->flags & SURF_PLANEBACK)) continue;		// wrong side

			// this can be optimized by only checking for culling if the leaf or node containing the surf intersects the frustum
			if (surf->intersect && (node->bops == FULLY_INTERSECT_FRUSTUM))
				if (R_CullBox (surf->mins, surf->maxs)) continue;

			rs_brushpolys++; //count wpolys here

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

	// recurse down the back side
	// the compiler should be performing this optimization anyway
	node = node->children[!side];

	// check the back side
	if (node->contents == CONTENTS_SOLID) return;
	if (node->visframe != r_visframecount) return;
	if (R_CullNode (node)) return;

	// if a leaf node, draw stuff
	if (node->contents < 0)
	{
		R_MarkLeafSurfs ((mleaf_t *) node);
		R_StoreEfrags (&((mleaf_t *) node)->efrags);
		return;
	}

	goto loc0;
}


r_modelsurf_t *R_GetLightChainNext (r_modelsurf_t *ms) {return ms->lightchain;}
r_modelsurf_t *R_GetSurfChainNext (r_modelsurf_t *ms) {return ms->surfchain;}

typedef r_modelsurf_t *(*RGETMSCHAINNEXTFUNC) (r_modelsurf_t *);

int r_numworldvertexes = 0;
int r_numworldindexes = 0;
unsigned short *r_worldindexes = NULL;
glvertex_t *r_baseworldvertexes = (glvertex_t *) (((unsigned short *) scratchbuf) + 32768);
glvertex_t *r_worldvertexes = NULL;
qbool r_restart_surface = false;

void R_BeginModelSurfs (void)
{
	r_worldindexes = (unsigned short *) scratchbuf;
	r_worldvertexes = (glvertex_t *) (r_worldindexes + 32768);
	r_numworldvertexes = 0;
	r_numworldindexes = 0;
	r_restart_surface = false;
}


void R_CheckModelSurfs (msurface_t *surf)
{
	if (r_numworldindexes + surf->numglindexes >= 32768) r_restart_surface = true;
	if (r_numworldvertexes + surf->numglvertexes >= 16384) r_restart_surface = true;

	if (r_restart_surface)
	{
		GL_SetIndices (0, scratchbuf);
		GL_DrawIndexedPrimitive (GL_TRIANGLES, r_numworldindexes, r_numworldvertexes);
		R_BeginModelSurfs ();
	}
}


void R_TransferModelSurf (msurface_t *surf, glmatrix *matrix, int entnum)
{
	// regenerate and transform the vertexes
	if (matrix) GL_RegenerateVertexes (surf->model, surf, matrix);

	r_worldindexes = R_TransferIndexes (surf->glindexes, r_worldindexes, surf->numglindexes, r_numworldvertexes);

	if (surf->lightmaptexturenum)
		memcpy (r_worldvertexes, surf->glvertexes, sizeof (glvertex_t) * surf->numglvertexes);
	else
	{
		int i;
		glvertex_t *v = surf->glvertexes;

		// if LM_BLOCK_WIDTH and/or LM_BLOCK_HEIGHT change these need to be changed too
		float row = (float) (entnum >> 8) / 256.0f + (1.0f / 512.0f);
		float col = (float) (entnum & 255) / 256.0f + (1.0f / 512.0f);

		for (i = 0; i < surf->numglvertexes; i++, v++)
		{
			r_worldvertexes[i].v[0] = v->v[0];
			r_worldvertexes[i].v[1] = v->v[1];
			r_worldvertexes[i].v[2] = v->v[2];

			r_worldvertexes[i].st1[0] = v->st1[0];
			r_worldvertexes[i].st1[1] = v->st1[1];

			r_worldvertexes[i].st2[0] = col;
			r_worldvertexes[i].st2[1] = row;
		}
	}

	r_numworldindexes += surf->numglindexes;
	r_numworldvertexes += surf->numglvertexes;
	r_worldvertexes += surf->numglvertexes;
}


void R_FinishModelSurfs (void)
{
	if (r_numworldindexes)
	{
		GL_SetIndices (0, scratchbuf);
		GL_DrawIndexedPrimitive (GL_TRIANGLES, r_numworldindexes, r_numworldvertexes);
	}
}


void R_RenderModelSurfs_Generic (r_modelsurf_t *chain, RGETMSCHAINNEXTFUNC chainnextfunc)
{
	R_BeginModelSurfs ();

	// batch to 32-bit indexes
	for (; chain; chain = chainnextfunc (chain))
	{
		R_CheckModelSurfs (chain->surface);
		// R_UpdateLightmap (chain->surface->lightmaptexturenum);
		R_TransferModelSurf (chain->surface, chain->matrix, chain->entnum);
	}

	R_FinishModelSurfs ();
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


void R_EnableWorldVertexArrays (void *st1, void *st2, void *st3)
{
	GL_SetStreamSource (0, GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (glvertex_t), r_baseworldvertexes->v);
	GL_SetStreamSource (0, GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);

	if (st1)
		GL_SetStreamSource (0, GLSTREAM_TEXCOORD0, 2, GL_FLOAT, sizeof (glvertex_t), st1);
	else GL_SetStreamSource (0, GLSTREAM_TEXCOORD0, 0, GL_NONE, 0, NULL);

	if (st2)
		GL_SetStreamSource (0, GLSTREAM_TEXCOORD1, 2, GL_FLOAT, sizeof (glvertex_t), st2);
	else GL_SetStreamSource (0, GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);

	if (st3)
		GL_SetStreamSource (0, GLSTREAM_TEXCOORD2, 2, GL_FLOAT, sizeof (glvertex_t), st3);
	else GL_SetStreamSource (0, GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);
}


texture_t *lasttexture = NULL;
gltexture_t *lastlightmap = NULL;
float last_surfalpha = -1;

void R_InvalidateExtraSurf (void)
{
	lasttexture = NULL;
	lastlightmap = NULL;
	last_surfalpha = -1;
}


void R_StartExtraSurfs (void)
{
	if (lasttexture->fullbright && r_world_tmus > 2)
	{
		GL_TexEnv (GL_TEXTURE2_ARB, GL_TEXTURE_2D, GL_ADD);
		GL_BindTexture (GL_TEXTURE2_ARB, lasttexture->fullbright);
		R_EnableWorldVertexArrays (r_baseworldvertexes->st1, r_baseworldvertexes->st2, r_baseworldvertexes->st1);
	}
	else
	{
		GL_TexEnv (GL_TEXTURE2_ARB, GL_TEXTURE_2D, GL_NONE);
		R_EnableWorldVertexArrays (r_baseworldvertexes->st1, r_baseworldvertexes->st2, NULL);
	}
}


void R_ExtraSurfsBegin (void)
{
	// only bring up 2 tmus initially because we don't know if we're going to need 3 yet
	R_Setup_Multitexture ();
	R_BeginModelSurfs ();
}


void R_ExtraSurfsEnd (void)
{
	R_FinishModelSurfs ();
	R_BeginModelSurfs ();

	// need to call this because we may have had 3 tmus up
	R_Takedown_Multitexture2 ();

	// because fences may have disabled it
	qglEnable (GL_BLEND);
}


void R_ExtraSurfFBCheck (qbool alpha)
{
	// because surfs must be sorted properly for this pass we need to draw one at a time when fullbrights are present
	if (r_world_tmus < 3 && lasttexture->fullbright)
	{
		// draw the base accumulated surf
		R_FinishModelSurfs ();

		// switch the blending mode
		if (!alpha) qglEnable (GL_BLEND);
		qglBlendFunc (GL_ONE, GL_ONE);
		Fog_StartAdditive ();

		R_EnableWorldVertexArrays (r_baseworldvertexes->st1, NULL, NULL);

		GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_NONE);

		if (alpha)
		{
			GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_MODULATE);
			qglColor4f (1, 1, 1, last_surfalpha);
		}
		else GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);

		GL_BindTexture (GL_TEXTURE0_ARB, lasttexture->fullbright);

		// draw it again for fullbrights
		R_FinishModelSurfs ();

		// this is the only takedown we need here
		Fog_StopAdditive ();

		// go back to the correct blending mode
		if (!alpha) qglDisable (GL_BLEND);
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// and back to the correct texenv
		R_Setup_Multitexture ();

		// ensure that everything is invalidated
		R_InvalidateExtraSurf ();

		// and start again
		R_BeginModelSurfs ();
	}
}


void R_RenderFenceTexture (msurface_t *surf, glmatrix *matrix, int entnum)
{
	if (surf->texinfo->texture != lasttexture) r_restart_surface = true;
	if (&lightmap_textures[surf->lightmaptexturenum] != lastlightmap) r_restart_surface = true;
	if (surf->wateralpha != last_surfalpha) r_restart_surface = true;
	if (r_numworldindexes + surf->numglindexes >= 32768) r_restart_surface = true;
	if (r_numworldvertexes + surf->numglvertexes >= 16384) r_restart_surface = true;

	if (r_restart_surface)
	{
		if (surf->wateralpha > 0.99f)
			qglDisable (GL_BLEND);

		R_FinishModelSurfs ();
		R_BeginModelSurfs ();

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
	}

	R_CheckModelSurfs (surf);
	R_TransferModelSurf (surf, matrix, entnum);

	// special handling for fullbrights
	R_ExtraSurfFBCheck (surf->wateralpha < 1 ? true : false);
}


void R_RenderAlphaSurface (msurface_t *surf, glmatrix *m, int entnum)
{
	if (surf->texinfo->texture != lasttexture) r_restart_surface = true;
	if (&lightmap_textures[surf->lightmaptexturenum] != lastlightmap) r_restart_surface = true;
	if (surf->wateralpha != last_surfalpha) r_restart_surface = true;
	if (r_numworldindexes + surf->numglindexes >= 32768) r_restart_surface = true;
	if (r_numworldvertexes + surf->numglvertexes >= 16384) r_restart_surface = true;

	if (r_restart_surface)
	{
		R_FinishModelSurfs ();
		R_BeginModelSurfs ();

		GL_BindTexture (GL_TEXTURE0_ARB, surf->texinfo->texture->gltexture);
		GL_BindTexture (GL_TEXTURE1_ARB, &lightmap_textures[surf->lightmaptexturenum]);

		lasttexture = surf->texinfo->texture;
		lastlightmap = &lightmap_textures[surf->lightmaptexturenum];
		last_surfalpha = surf->wateralpha;

		R_StartExtraSurfs ();

		// this needs to be done after we enable the vertex arrays as they will switch off color
		qglColor4f (1, 1, 1, surf->wateralpha);
		GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_MODULATE);
	}

	R_CheckModelSurfs (surf);
	R_TransferModelSurf (surf, m, entnum);

	// special handling for fullbrights
	R_ExtraSurfFBCheck (true);
}


void R_RenderModelSurfs_Multitexture (qbool fbpass)
{
	gltexture_t *glt;
	r_modelsurf_t *ms;
	extern gltexture_t *active_gltextures;
	qbool stateset = false;
	int i, lm;

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
				R_EnableWorldVertexArrays (r_baseworldvertexes->st1, r_baseworldvertexes->st2, r_baseworldvertexes->st1);
				R_Setup_Multitexture2 ();
			}
			else
			{
				R_EnableWorldVertexArrays (r_baseworldvertexes->st1, r_baseworldvertexes->st2, NULL);
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

			R_RenderModelSurfs_Generic (ms, R_GetLightChainNext);

			lightmap_modelsurfs[lm] = NULL;
		}
	}

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

			R_EnableWorldVertexArrays (r_baseworldvertexes->st1, NULL, NULL);
			R_Setup_Fullbright ();
			stateset = true;
		}

		// bind texture (always)
		GL_BindTexture (GL_TEXTURE0_ARB, ms->texture->fullbright);

		R_RenderModelSurfs_Generic (ms, R_GetSurfChainNext);
	}

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

	R_EnableWorldVertexArrays (r_baseworldvertexes->st1, NULL, NULL);

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		if (!(ms = glt->surfchain)) continue;

		// do water later
		if (ms->surface->flags & SURF_DRAWTURB) continue;

		// bind texture (always)
		GL_BindTexture (GL_TEXTURE0_ARB, ms->texture->gltexture);

		R_RenderModelSurfs_Generic (ms, R_GetSurfChainNext);
	}
}


void R_RenderModelSurfs_ShowTris (void)
{
	gltexture_t *glt;
	r_modelsurf_t *ms;
	extern gltexture_t *active_gltextures;

	R_EnableWorldVertexArrays (NULL, NULL, NULL);

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		if (!(ms = glt->surfchain)) continue;

		// do water later
		if (ms->surface->flags & SURF_DRAWTURB) continue;

		R_RenderModelSurfs_Generic (ms, R_GetSurfChainNext);
	}
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

	R_EnableWorldVertexArrays (r_baseworldvertexes->st2, NULL, NULL);
	GL_TexEnv (GL_TEXTURE2_ARB, GL_TEXTURE_2D, GL_NONE);
	GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_NONE);
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);

	// now draw them all in lightmap order
	for (lm = 0; lm < MAX_LIGHTMAPS; lm++)
	{
		if (!(ms = lightmap_modelsurfs[lm])) continue;

		// bind lightmap (always)
		GL_BindTexture (GL_TEXTURE0_ARB, &lightmap_textures[lm]);

		R_RenderModelSurfs_Generic (ms, R_GetLightChainNext);
		lightmap_modelsurfs[lm] = NULL;
	}
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

	VectorCopy (r_refdef2.vieworg, modelorg);

	// emit the world surfs into modelsurfs
	R_ModelSurfsBeginFrame ();
	R_NewRecursiveWorldNode (r_worldmodel->nodes);

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

