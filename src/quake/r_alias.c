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
// r_alias.c - rendering of alias models (*.mdl)

#include "gl_local.h"

#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

vec3_t	shadevector;

extern vec3_t lightcolor;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
;

extern vec3_t lightspot;

qbool	overbright;

/*
=================
R_SetupAliasFrame
=================
*/
void R_SetupAliasFrame (entity_t *e, aliashdr_t *paliashdr, int frame, aliasstate_t *state)
{
	int posenum, numposes;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Com_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	posenum = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		e->lerptime = paliashdr->frames[frame].interval;
		posenum += (int) (r_refdef2.time / e->lerptime) % numposes;
	}
	else
		e->lerptime = 0.1;

	if (e->lerpflags & LERP_RESETANIM) // kill any lerp in progress
	{
		e->lerpstart = 0;
		e->previouspose = posenum;
		e->currentpose = posenum;
		e->lerpflags -= LERP_RESETANIM;
	}
	else if (e->currentpose != posenum) // pose changed, start new lerp
	{
		if (e->lerpflags & LERP_RESETANIM2) // defer lerping one more time
		{
			e->lerpstart = 0;
			e->previouspose = posenum;
			e->currentpose = posenum;
			e->lerpflags -= LERP_RESETANIM2;
		}
		else
		{
			e->lerpstart = r_refdef2.time;
			e->previouspose = e->currentpose;
			e->currentpose = posenum;
		}
	}

	// set up values
	if (r_lerpmodels.value && ! (e->model->flags & MOD_NOLERP && r_lerpmodels.value != 2))
	{
		if (e->lerpflags & LERP_FINISH && numposes == 1)
			state->blend = clamp (0, (r_refdef2.time - e->lerpstart) / (e->lerpfinish - e->lerpstart), 1);
		else
			state->blend = clamp (0, (r_refdef2.time - e->lerpstart) / e->lerptime, 1);

		state->pose1 = e->previouspose;
		state->pose2 = e->currentpose;
	}
	else // don't lerp
	{
		state->blend = 1;
		state->pose1 = posenum;
		state->pose2 = posenum;
	}
}

/*
=================
R_SetupEntityTransform

set up transform part of state
=================
*/
void R_SetupEntityTransform (entity_t *e, aliasstate_t *state)
{
	float blend;
	vec3_t d;
	int i;

	// if LERP_RESETMOVE, kill any lerps in progress
	if (e->lerpflags & LERP_RESETMOVE)
	{
		e->movelerpstart = 0;
		VectorCopy (e->origin, e->previousorigin);
		VectorCopy (e->origin, e->currentorigin);
		VectorCopy (e->angles, e->previousangles);
		VectorCopy (e->angles, e->currentangles);
		e->lerpflags -= LERP_RESETMOVE;
	}
	else if (!VectorCompare (e->origin, e->currentorigin) || !VectorCompare (e->angles, e->currentangles)) // origin/angles changed, start new lerp
	{
		e->movelerpstart = r_refdef2.time;
		VectorCopy (e->currentorigin, e->previousorigin);
		VectorCopy (e->origin,  e->currentorigin);
		VectorCopy (e->currentangles, e->previousangles);
		VectorCopy (e->angles,  e->currentangles);
	}

	// set up values
	if (r_lerpmove.value && !(e->renderfx & RF_WEAPONMODEL) && e->lerpflags & LERP_MOVESTEP)
	{
		if (e->lerpflags & LERP_FINISH)
			blend = clamp (0, (r_refdef2.time - e->movelerpstart) / (e->lerpfinish - e->movelerpstart), 1);
		else blend = clamp (0, (r_refdef2.time - e->movelerpstart) / 0.1, 1);

		// translation
		VectorSubtract (e->currentorigin, e->previousorigin, d);
		state->origin[0] = e->previousorigin[0] + d[0] * blend;
		state->origin[1] = e->previousorigin[1] + d[1] * blend;
		state->origin[2] = e->previousorigin[2] + d[2] * blend;

		// rotation
		VectorSubtract (e->currentangles, e->previousangles, d);

		for (i = 0; i < 3; i++)
		{
			if (d[i] > 180)
				d[i] -= 360;
			if (d[i] < -180)
				d[i] += 360;
		}

		state->angles[0] = e->previousangles[0] + d[0] * blend;
		state->angles[1] = e->previousangles[1] + d[1] * blend;
		state->angles[2] = e->previousangles[2] + d[2] * blend;
	}
	else // don't lerp
	{
		VectorCopy (e->origin, state->origin);
		VectorCopy (e->angles, state->angles);
	}
}

/*
=================
R_SetupAliasLighting
=================
*/
void R_SetupAliasLighting (entity_t	*e)
{
	vec3_t		dist;
	float		add;
	int			i;

	R_LightPoint (e->origin);
	VectorCopy (lightspot, e->aliasstate.lightspot);

	// add dlights
	if (r_dynamic.value)
	{
		float dlscale = (1.0f / 255.0f) * r_dynamic.value;

		for (i = 0; i < r_refdef2.numDlights; i++)	
		{
			VectorSubtract (e->origin, r_refdef2.dlights[i].origin, dist);
			add = r_refdef2.dlights[i].radius - VectorLength(dist);
	
			if (add > 0)
			{
				lightcolor[0] += (add * r_refdef2.dlights[i].color[0]) * dlscale;
				lightcolor[1] += (add * r_refdef2.dlights[i].color[1]) * dlscale;
				lightcolor[2] += (add * r_refdef2.dlights[i].color[2]) * dlscale;
			}
		}
	}

	// minimum light value on gun (24)
	if (e->renderfx & RF_WEAPONMODEL)
	{
		add = 72.0f - (lightcolor[0] + lightcolor[1] + lightcolor[2]);

		if (add > 0.0f)
		{
			lightcolor[0] += add / 3.0f;
			lightcolor[1] += add / 3.0f;
			lightcolor[2] += add / 3.0f;
		}
	}

	// minimum light value on players (8)
	if (e->model->modhint == MOD_PLAYER || e->renderfx & RF_PLAYERMODEL)
	{
		add = 24.0f - (lightcolor[0] + lightcolor[1] + lightcolor[2]);

		if (add > 0.0f)
		{
			lightcolor[0] += add / 3.0f;
			lightcolor[1] += add / 3.0f;
			lightcolor[2] += add / 3.0f;
		}
	}

	// clamp lighting so it doesn't overbright as much (96)
	if (overbright)
	{
		add = 288.0f / (lightcolor[0] + lightcolor[1] + lightcolor[2]);

		if (add < 1.0f)
			VectorScale(lightcolor, add, lightcolor);
	}

	// hack up the brightness when fullbrights but no overbrights (256)
	if (gl_fullbrights.value && !gl_overbright.value)
	{
		if (e->model->flags & MOD_FBRIGHTHACK)
		{
			lightcolor[0] = 256.0f;
			lightcolor[1] = 256.0f;
			lightcolor[2] = 256.0f;
		}
	}

	e->aliasstate.shadedots = r_avertexnormal_dots[((int) (e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	VectorScale(lightcolor, 1.0f / 200.0f, lightcolor);
}


// values for shadow matrix
#define SHADOW_SKEW_X -0.7	// skew along x axis. -0.7 to mimic glquake shadows
#define SHADOW_SKEW_Y 0		// skew along y axis. 0 to mimic glquake shadows
#define SHADOW_VSCALE 0		// 0 = completely flat
#define SHADOW_HEIGHT 0.1	// how far above the floor to render the shadow

float shadowmatrix[16] =
{
	1,				0,				0,				0,
	0,				1,				0,				0,
	SHADOW_SKEW_X,	SHADOW_SKEW_Y,	SHADOW_VSCALE,	0,
	0,				0,				SHADOW_HEIGHT,	1
};

void R_DrawAliasShadow (entity_t *ent, aliashdr_t *hdr, aliasstate_t *state, float *mesh, float *shadecolor)
{
	int v;
	trivertx_t *verts1, *verts2;
	float	blend, iblend;
	qbool lerping;
	meshdesc_t *desc;
	trivertx_t *trivert1;
	trivertx_t *trivert2;

	desc = (meshdesc_t *) ((byte *) hdr + hdr->meshdescs);

	if (state->pose1 != state->pose2)
	{
		lerping = true;

		verts1 = (trivertx_t *) ((byte *) hdr + hdr->vertexes + hdr->framevertexsize * state->pose1);
		verts2 = (trivertx_t *) ((byte *) hdr + hdr->vertexes + hdr->framevertexsize * state->pose2);

		blend = state->blend;
		iblend = 1.0f - blend;
	}
	else // poses the same means either 1. the entity has paused its animation, or 2. r_lerpmodels is disabled
	{
		lerping = false;
	
		verts1 = (trivertx_t *) ((byte *) hdr + hdr->vertexes + hdr->framevertexsize * state->pose1);
		verts2 = verts1;

		blend = iblend = 0; // avoid bogus compiler warning
	}

	if (lerping)
	{
		for (v = 0; v < hdr->numverts; v++, mesh += 4, desc++)
		{
			trivert1 = &verts1[desc->vertindex];
			trivert2 = &verts2[desc->vertindex];

			mesh[0] = trivert1->v[0] * iblend + trivert2->v[0] * blend;
			mesh[1] = trivert1->v[1] * iblend + trivert2->v[1] * blend;
			mesh[2] = trivert1->v[2] * iblend + trivert2->v[2] * blend;
			mesh[3] = shadecolor[0];
		}
	}
	else
	{
		for (v = 0; v < hdr->numverts; v++, mesh += 4, desc++)
		{
			trivert1 = &verts1[desc->vertindex];

			mesh[0] = trivert1->v[0];
			mesh[1] = trivert1->v[1];
			mesh[2] = trivert1->v[2];
			mesh[3] = shadecolor[0];
		}
	}

	R_DrawElements (hdr->numindexes, hdr->numverts, (unsigned short *) ((byte *) hdr + hdr->indexes));
}


void R_DrawAliasShadows (entity_t **ents, int numents, void *meshbuffer)
{
	if (r_shadows.value > 0.01f)
	{
		int i;
		qbool stateset = false;
		byte shadecolor[4] = {0, 0, 0, 128};
//		extern int gl_stencilbits;

		for (i = 0; i < numents; i++)
		{
			entity_t *ent = ents[i];

			if (!(ent->model->flags & MOD_NOSHADOW))
			{
				aliasstate_t *state = &ent->aliasstate;
				aliashdr_t *hdr = Mod_Extradata (ent->model);
				float lheight = state->origin[2] - state->lightspot[2];

				if (!stateset)
				{
					float *mesh = (float *) meshbuffer;

					qglDepthMask (GL_FALSE);
					qglEnable (GL_BLEND);
					GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_NONE);
					GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_NONE);
					GL_Color (0, 0, 0, r_shadows.value);
					shadecolor[3] = BYTE_CLAMPF (r_shadows.value);

					/*
					if (gl_stencilbits)
					{
						qglEnable (GL_STENCIL_TEST);
						qglStencilFunc (GL_EQUAL, 1, 2);
						qglStencilOp (GL_KEEP, GL_KEEP, GL_INCR);
					}
					*/

					R_EnableVertexArrays (mesh, (byte *) &mesh[3], NULL, NULL, NULL, sizeof (float) * 4);
					stateset = true;
				}

				qglPushMatrix ();

				if (state->origin[0] || state->origin[1] || state->origin[2])
					qglTranslatef (state->origin[0], state->origin[1], state->origin[2]);

				qglTranslatef (0, 0, -lheight);
				qglMultMatrixf (shadowmatrix);
				qglTranslatef (0, 0, lheight);

				if (state->angles[1]) qglRotatef (state->angles[1], 0, 0, 1);

				qglTranslatef (hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]);
				qglScalef (hdr->scale[0], hdr->scale[1], hdr->scale[2]);

				R_DrawAliasShadow (ent, hdr, state, (float *) meshbuffer, (float *) shadecolor);

				qglPopMatrix ();
			}
		}

		if (stateset)
		{
			/*
			if (gl_stencilbits)
				qglDisable (GL_STENCIL_TEST);
			*/

			GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);
			qglDisable (GL_BLEND);
			qglDepthMask (GL_TRUE);
			GL_Color (1, 1, 1, 1);
		}
	}
}


void R_SetupAliasModel (entity_t *e)
{
	aliashdr_t	*paliashdr;
	int			anim;
	aliasstate_t	*state = &e->aliasstate;
	vec3_t mins, maxs;

	memset (state, 0, sizeof (aliasstate_t));

	// initially not visible
	e->visframe = -1;

	// setup pose/lerp data -- do it first so we don't miss updates due to culling
	paliashdr = (aliashdr_t *) Mod_Extradata (e->model);
	R_SetupAliasFrame (e, paliashdr, e->frame, state);
	R_SetupEntityTransform (e, state);

	// use proper per-frame bboxes (to do - fix for rotation)
	VectorAdd (state->origin, paliashdr->frames[e->frame].mins, mins);
	VectorAdd (state->origin, paliashdr->frames[e->frame].maxs, maxs);

	// cull it (the viewmodel is never culled)
	if (r_shadows.value > 0.01f)
	{
		if (!(e->renderfx & RF_WEAPONMODEL))
		{
			if (R_CullBox (mins, maxs))
				e->visframe = -1;
			else
				e->visframe = r_framecount;
		}
	}
	else
	{
		if (!(e->renderfx & RF_WEAPONMODEL))
		{
			if (R_CullBox (mins, maxs))
				return;
		}

		// the ent is visible now
		e->visframe = r_framecount;
	}

	// set up lighting
	overbright = gl_overbright.value;
	rs_aliaspolys += paliashdr->numtris;
	R_SetupAliasLighting (e);

	// store out the lighting
	state->shadelight[0] = lightcolor[0];
	state->shadelight[1] = lightcolor[1];
	state->shadelight[2] = lightcolor[2];
	state->shadelight[3] = e->alpha;

	// set up textures
	anim = (int) (cl.time * 10) & 3;
	state->tx = paliashdr->gltextures[e->skinnum][anim];
	
	if (!gl_fullbrights.value)
		state->fb = NULL;
	else
		state->fb = paliashdr->fbtextures[e->skinnum][anim];

	if (e->colormap && (e->model->modhint == MOD_PLAYER || e->renderfx & RF_PLAYERMODEL) && !gl_nocolors.value)
		R_GetTranslatedPlayerSkin (e->colormap, &state->tx->texnum, &state->fb->texnum);
}


void *r_meshbuffer = NULL;

void R_DrawAliasModel (entity_t *ent, aliashdr_t *hdr, aliasstate_t *state, qbool showtris)
{
	int v;
	trivertx_t *verts1, *verts2;
	float	blend, iblend;
	qbool lerping;
	meshdesc_t *desc;
	trivertx_t *trivert1;
	trivertx_t *trivert2;
	r_defaultquad_t *r_aliasmesh;
	extern int r_meshindexbuffer;

	qglPushMatrix ();
	RB_RotateMatrixForEntity (state->origin, state->angles);
	qglTranslatef (hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]);
	qglScalef (hdr->scale[0], hdr->scale[1], hdr->scale[2]);

	// hack the depth range to prevent view model from poking into walls
	if (ent->renderfx & RF_WEAPONMODEL)
		qglDepthRange (QGL_DEPTH_3D_BEGIN, QGL_DEPTH_VM_END);

	r_aliasmesh = (r_defaultquad_t *) r_meshbuffer;
	desc = (meshdesc_t *) ((byte *) hdr + hdr->meshdescs);

	if (state->pose1 != state->pose2)
	{
		lerping = true;

		verts1 = (trivertx_t *) ((byte *) hdr + hdr->vertexes + hdr->framevertexsize * state->pose1);
		verts2 = (trivertx_t *) ((byte *) hdr + hdr->vertexes + hdr->framevertexsize * state->pose2);

		blend = state->blend;
		iblend = 1.0f - blend;
	}
	else // poses the same means either 1. the entity has paused its animation, or 2. r_lerpmodels is disabled
	{
		lerping = false;

		verts1 = (trivertx_t *) ((byte *) hdr + hdr->vertexes + hdr->framevertexsize * state->pose1);
		verts2 = verts1;

		blend = iblend = 0; // avoid bogus compiler warning
	}

	if (showtris)
		R_EnableVertexArrays (r_aliasmesh->xyz, r_aliasmesh->color, NULL, NULL, NULL, sizeof (r_defaultquad_t));
	else if (state->fb)
		R_EnableVertexArrays (r_aliasmesh->xyz, r_aliasmesh->color, r_aliasmesh->st, r_aliasmesh->st, NULL, sizeof (r_defaultquad_t));
	else
		R_EnableVertexArrays (r_aliasmesh->xyz, r_aliasmesh->color, r_aliasmesh->st, NULL, NULL, sizeof (r_defaultquad_t));

	if (lerping)
	{
		for (v = 0; v < hdr->numverts; v++, r_aliasmesh++, desc++)
		{
			trivert1 = &verts1[desc->vertindex];
			trivert2 = &verts2[desc->vertindex];

			desc->light = (state->shadedots[trivert1->lightnormalindex] * iblend + state->shadedots[trivert2->lightnormalindex] * blend);

			r_aliasmesh->xyz[0] = trivert1->v[0] * iblend + trivert2->v[0] * blend;
			r_aliasmesh->xyz[1] = trivert1->v[1] * iblend + trivert2->v[1] * blend;
			r_aliasmesh->xyz[2] = trivert1->v[2] * iblend + trivert2->v[2] * blend;
		}
	}
	else
	{
		for (v = 0; v < hdr->numverts; v++, r_aliasmesh++, desc++)
		{
			trivert1 = &verts1[desc->vertindex];

			desc->light = state->shadedots[trivert1->lightnormalindex];

			r_aliasmesh->xyz[0] = trivert1->v[0];
			r_aliasmesh->xyz[1] = trivert1->v[1];
			r_aliasmesh->xyz[2] = trivert1->v[2];
		}
	}

	// reset these for lighting
	r_aliasmesh = (r_defaultquad_t *) r_meshbuffer;
	desc = (meshdesc_t *) ((byte *) hdr + hdr->meshdescs);

	if (showtris)
	{
		for (v = 0; v < hdr->numverts; v++, r_aliasmesh++, desc++)
		{
			r_aliasmesh->st[0] = desc->st[0];
			r_aliasmesh->st[1] = desc->st[1];
			r_aliasmesh->rgba = 0xffffffff;
		}
	}
	else
	{
		for (v = 0; v < hdr->numverts; v++, r_aliasmesh++, desc++)
		{
			r_aliasmesh->st[0] = desc->st[0];
			r_aliasmesh->st[1] = desc->st[1];

			r_aliasmesh->color[0] = BYTE_CLAMPF (desc->light * state->shadelight[0]);
			r_aliasmesh->color[1] = BYTE_CLAMPF (desc->light * state->shadelight[1]);
			r_aliasmesh->color[2] = BYTE_CLAMPF (desc->light * state->shadelight[2]);
			r_aliasmesh->color[3] = ent->alpha;
		}
	}

	if (gl_support_arb_vertex_buffer_object && r_meshindexbuffer)
		R_DrawBufferElements (hdr->firstindex, hdr->numindexes, r_meshindexbuffer);
	else
		R_DrawElements (hdr->numindexes, hdr->numverts, (unsigned short *) ((byte *) hdr + hdr->indexes));

	// restore normal depth range
	if (ent->renderfx & RF_WEAPONMODEL)
		qglDepthRange (QGL_DEPTH_3D_BEGIN, QGL_DEPTH_3D_END);

	qglPopMatrix ();
}


void R_DrawAliasBatchPass (entity_t **ents, int numents, qbool showtris)
{
	int i;
	gltexture_t *lasttexture = NULL;
	gltexture_t *lastfullbright = NULL;

	if (!numents)
		return;

	if (showtris)
		GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);
	else if (gl_overbright.value)
		GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_RGB_SCALE_ARB);
	else
		GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_MODULATE);

	for (i = 0; i < numents; i++)
	{
		entity_t *ent = ents[i];
		aliasstate_t *state = &ent->aliasstate;
		aliashdr_t *hdr = Mod_Extradata (ent->model);

		// we need a separate test for culling here as r_shadows mode doesn't cull
		if (ent->visframe != r_framecount)
			continue;

		if (!showtris && ((state->tx != lasttexture) || (state->fb != lastfullbright)))
		{
			if (state->fb)
			{
				GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_ADD);
				GL_BindTexture (GL_TEXTURE1_ARB, state->fb);
			}
			else
				GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_NONE);

			GL_BindTexture (GL_TEXTURE0_ARB, state->tx);

			lasttexture = state->tx;
			lastfullbright = state->fb;
		}

		// OK, this works better in GL... go figure...
		R_DrawAliasModel (ent, hdr, state, showtris);
	}

	GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_NONE);
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);

	GL_Color (1, 1, 1, 1);
}


void R_DrawAliasBatches (entity_t **ents, int numents, void *meshbuffer)
{
	r_meshbuffer = meshbuffer;
	R_DrawAliasBatchPass (ents, numents, false);

	if (r_showtris.value)
	{
		R_ShowTrisBegin ();
		R_DrawAliasBatchPass (ents, numents, true);
		R_ShowTrisEnd ();
	}
}


int R_DrawAliasModels_Sort (entity_t **e1, entity_t **e2)
{
	// sort by texture (the same model can have different textures)
	return (int) (e1[0]->aliasstate.tx - e2[0]->aliasstate.tx);
}


void R_AddEntityToAlphaList (entity_t *ent);

void R_DrawAliasModels (void)
{
	int i;
	int numalias;

	// temp buffer for storing ents for sorting
	entity_t **ents = (entity_t **) scratchbuf;

	if (!r_drawentities.value)
		return;

	for (i = 0, numalias = 0; i < cl_numvisedicts; i++)
	{
		entity_t *ent = &cl_visedicts[i];

		if (!ent->model)
			continue;
		if (ent->model->type != mod_alias)
			continue;

		if (ent->alpha < 1)
			ent->alpha = 255;
		else if (ent->alpha < 255)
		{
			R_AddEntityToAlphaList (ent);
			continue;
		}

		R_SetupAliasModel (ent);

		// if it was culled or otherwise removed from the renderer we don't bother with it
		// (unless it's got shadows enabled in which case we have to :( )
		if (ent->visframe != r_framecount && r_shadows.value < 0.01f)
			continue;

		// store this model
		ents[numalias++] = ent;
	}

	if (!numalias)
		return;

	// sort the models by texture so that we can batch the bastards
	qsort (ents, numalias, sizeof (entity_t *), (int (*) (const void *, const void *)) R_DrawAliasModels_Sort);

	R_DrawAliasBatches (ents, numalias, (void *) (ents + numalias));
	R_DrawAliasShadows (ents, numalias, (void *) (ents + numalias));
}
