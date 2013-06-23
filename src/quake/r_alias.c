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

extern unsigned int r_meshindexbuffer;
extern unsigned int r_meshvertexbuffer;

#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] =
{
#include "anorms.h"
};

vec3_t	shadevector;

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


void R_MinimumLighting (float *lightcolor, float minimum)
{
	float add = minimum - (lightcolor[0] + lightcolor[1] + lightcolor[2]);

	if (add > 0.0f)
	{
		lightcolor[0] += add / 3.0f;
		lightcolor[1] += add / 3.0f;
		lightcolor[2] += add / 3.0f;
	}
}


void R_ClampLighting (float *ambientlight, float *shadelight)
{
	// correct rgb to intensity scaling factors
	float ntsc[] = {0.3f, 0.59f, 0.11f};

	// convert light values to intensity values so that we can preserve the final correct colour balance
	float ambientintensity = DotProduct (ambientlight, ntsc);
	float shadeintensity = DotProduct (shadelight, ntsc);

	if (ambientintensity > 0 && shadeintensity > 0)
	{
		// scale down now before we modify the intensities
		VectorScale (ambientlight, (1.0f / ambientintensity), ambientlight);
		VectorScale (shadelight, (1.0f / shadeintensity), shadelight);

		// clamp lighting so that it doesn't overbright as much
		if (ambientintensity > 128.0f)
			ambientintensity = 128.0f;

		if (ambientintensity + shadeintensity > 192.0f)
			shadeintensity = 192.0f - ambientintensity;

		// now scale them back up to the new values preserving colour balance
		VectorScale (ambientlight, ambientintensity, ambientlight);
		VectorScale (shadelight, shadeintensity, shadelight);
	}
}


/*
=================
R_SetupAliasLighting
=================
*/
void R_SetupAliasLighting (entity_t	*e, float *ambientlight, float *shadelight)
{
	R_LightPoint (e, shadelight);

	VectorCopy (shadelight, ambientlight);
	VectorCopy (lightspot, e->aliasstate.lightspot);

	// minimum light value on gun (24)
	if (e->renderfx & RF_WEAPONMODEL)
	{
		R_MinimumLighting (shadelight, 72.0f);
		R_MinimumLighting (ambientlight, 72.0f);
	}

	// if we're overbright we scale down by half so that the clamp doesn't flatten the light as much
	if (overbright)
	{
		VectorScale (shadelight, 0.5f, shadelight);
		VectorScale (ambientlight, 0.5f, ambientlight);
	}

	// clamp lighting so that it doesn't overbright so much
	R_ClampLighting (ambientlight, shadelight);

	// minimum light value on players (8)
	if (e->model->modhint == MOD_PLAYER || e->renderfx & RF_PLAYERMODEL)
	{
		R_MinimumLighting (shadelight, 24.0f);
		R_MinimumLighting (ambientlight, 24.0f);
	}

	// hack up the brightness when fullbrights but no overbrights (256)
	if (gl_fullbrights.value && !gl_overbright.value)
	{
		if (e->model->flags & MOD_FBRIGHTHACK)
		{
			shadelight[0] = ambientlight[0] = 256.0f;
			shadelight[1] = ambientlight[1] = 256.0f;
			shadelight[2] = ambientlight[2] = 256.0f;
		}
	}

	// take to final scale - alpha will be stored in ambientlight[3] separately
	// hack - the gun model comes out too dark with this lighting so we need to boost it a little
	if (e->renderfx & RF_WEAPONMODEL)
	{
		VectorScale (shadelight, 1.0f / 128.0f, shadelight);
		VectorScale (ambientlight, 1.0f / 128.0f, ambientlight);
	}
	else
	{
		VectorScale (shadelight, 1.0f / 200.0f, shadelight);
		VectorScale (ambientlight, 1.0f / 200.0f, ambientlight);
	}
}


// values for shadow matrix
#define SHADOW_SKEW_X -0.7	// skew along x axis. -0.7 to mimic glquake shadows
#define SHADOW_SKEW_Y 0		// skew along y axis. 0 to mimic glquake shadows
#define SHADOW_VSCALE 0		// 0 = completely flat
#define SHADOW_HEIGHT 0.1	// how far above the floor to render the shadow

float shadowmatrix16[16] =
{
	1,				0,				0,				0,
	0,				1,				0,				0,
	SHADOW_SKEW_X,	SHADOW_SKEW_Y,	SHADOW_VSCALE,	0,
	0,				0,				SHADOW_HEIGHT,	1
};

glmatrix shadowmatrix;

void R_DrawAliasShadow (entity_t *ent, aliashdr_t *hdr, aliasstate_t *state, float *dstmesh, float *shadecolor)
{
	int v;
	trivertx_t *verts1, *verts2;
	float	blend, iblend;
	qbool lerping;
	aliasmesh_t *aliasmesh;
	trivertx_t *trivert1;
	trivertx_t *trivert2;

	memcpy (shadowmatrix.m16, shadowmatrix16, sizeof (float) * 16);

	aliasmesh = (aliasmesh_t *) ((byte *) hdr + hdr->aliasmesh);

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
		for (v = 0; v < hdr->numverts; v++, dstmesh += 4, aliasmesh++)
		{
			trivert1 = &verts1[aliasmesh->vertindex];
			trivert2 = &verts2[aliasmesh->vertindex];

			dstmesh[0] = trivert1->v[0] * iblend + trivert2->v[0] * blend;
			dstmesh[1] = trivert1->v[1] * iblend + trivert2->v[1] * blend;
			dstmesh[2] = trivert1->v[2] * iblend + trivert2->v[2] * blend;
			dstmesh[3] = shadecolor[0];
		}
	}
	else
	{
		for (v = 0; v < hdr->numverts; v++, dstmesh += 4, aliasmesh++)
		{
			trivert1 = &verts1[aliasmesh->vertindex];

			dstmesh[0] = trivert1->v[0];
			dstmesh[1] = trivert1->v[1];
			dstmesh[2] = trivert1->v[2];
			dstmesh[3] = shadecolor[0];
		}
	}

	GL_SetIndices (0, ((byte *) hdr + hdr->indexes));
	GL_DrawIndexedPrimitive (GL_TRIANGLES, hdr->numindexes, hdr->numverts);
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
			glmatrix eshadow;

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
					qglColor4f (0, 0, 0, r_shadows.value);
					shadecolor[3] = BYTE_CLAMPF (r_shadows.value);

					/*
					if (gl_stencilbits)
					{
						qglEnable (GL_STENCIL_TEST);
						qglStencilFunc (GL_EQUAL, 1, 2);
						qglStencilOp (GL_KEEP, GL_KEEP, GL_INCR);
					}
					*/

					GL_SetStreamSource (0, GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (float) * 4, mesh);
					GL_SetStreamSource (0, GLSTREAM_COLOR, 4, GL_UNSIGNED_BYTE, sizeof (float) * 4, &mesh[3]);
					GL_SetStreamSource (0, GLSTREAM_TEXCOORD0, 0, GL_NONE, 0, NULL);
					GL_SetStreamSource (0, GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
					GL_SetStreamSource (0, GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);

					stateset = true;
				}

				GL_LoadMatrix (&eshadow, &r_world_matrix);

				if (state->origin[0] || state->origin[1] || state->origin[2])
					GL_TranslateMatrix (&eshadow, state->origin[0], state->origin[1], state->origin[2]);

				GL_TranslateMatrix (&eshadow, 0, 0, -lheight);
				GL_MultiplyMatrix (&eshadow, &shadowmatrix, &eshadow);
				GL_TranslateMatrix (&eshadow, 0, 0, lheight);

				if (state->angles[1]) GL_RotateMatrix (&eshadow, state->angles[1], 0, 0, 1);

				GL_TranslateMatrix (&eshadow, hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]);
				GL_ScaleMatrix (&eshadow, hdr->scale[0], hdr->scale[1], hdr->scale[2]);

				qglLoadMatrixf (eshadow.m16);

				R_DrawAliasShadow (ent, hdr, state, (float *) meshbuffer, (float *) shadecolor);
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
			qglColor4f (1, 1, 1, 1);

			qglLoadMatrixf (r_world_matrix.m16);
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

	// we get blinky with per-frame so use the whole model bbox instead
	VectorAdd (state->origin, e->model->mins, mins);
	VectorAdd (state->origin, e->model->maxs, maxs);

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
	R_SetupAliasLighting (e, state->ambientlight, state->shadelight);

	// store out the alpha value
	state->shadelight[3] = state->ambientlight[3] = ((float) e->alpha / 255.0f);

	// set up textures
	anim = (int) (cl.time * 10) & 3;
	state->tx = paliashdr->gltextures[e->skinnum][anim];

	// ensure we have a valid texture
	if (!state->tx) state->tx = notexture;
	
	if (!gl_fullbrights.value)
		state->fb = NULL;
	else
		state->fb = paliashdr->fbtextures[e->skinnum][anim];

	if (e->colormap && (e->model->modhint == MOD_PLAYER || e->renderfx & RF_PLAYERMODEL) && !gl_nocolors.value)
		R_GetTranslatedPlayerSkin (e->colormap, &state->tx, &state->fb);
}


void *r_meshbuffer = NULL;

typedef struct r_aliasmesh_s
{
	float xyz[3];
	float rgba[4];
} r_aliasmesh_t;

void R_DrawAliasModel (entity_t *ent, aliashdr_t *hdr, aliasstate_t *state, qbool showtris)
{
	int v;
	trivertx_t *verts1, *verts2;
	float	blend, iblend;
	qbool lerping;
	aliasmesh_t *mesh;
	trivertx_t *trivert1;
	trivertx_t *trivert2;
	float lerpnormal[3];
	float alias_forward[3], alias_right[3], alias_up[3];
	float r_plightvec[3], lightvec[3] = {-1, 0, 0};

	r_aliasmesh_t *r_aliasmesh;
	float *r_aliasst;

	// hack the depth range to prevent view model from poking into walls
	if (ent->renderfx & RF_WEAPONMODEL)
		qglDepthRange (QGL_DEPTH_3D_BEGIN, QGL_DEPTH_VM_END);

	ent->angles[0] = -ent->angles[0];	// stupid quake bug
	AngleVectors (ent->angles, alias_forward, alias_right, alias_up);
	ent->angles[0] = -ent->angles[0];	// stupid quake bug

	// rotate the lighting vector into the model's frame of reference
	r_plightvec[0] = DotProduct (lightvec, alias_forward);
	r_plightvec[1] = -DotProduct (lightvec, alias_right);
	r_plightvec[2] = DotProduct (lightvec, alias_up);

	// go back to the world matrix
	GL_LoadMatrix (&ent->matrix, &r_world_matrix);

	if (state->origin[0] || state->origin[1] || state->origin[2])
		GL_TranslateMatrix (&ent->matrix, state->origin[0], state->origin[1], state->origin[2]);

	if (state->angles[1]) GL_RotateMatrix (&ent->matrix, state->angles[1], 0, 0, 1);
	if (state->angles[0]) GL_RotateMatrix (&ent->matrix, -state->angles[0], 0, 1, 0);
	if (state->angles[2]) GL_RotateMatrix (&ent->matrix, state->angles[2], 1, 0, 0);

	GL_TranslateMatrix (&ent->matrix, hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]);
	GL_ScaleMatrix (&ent->matrix, hdr->scale[0], hdr->scale[1], hdr->scale[2]);

	qglLoadMatrixf (ent->matrix.m16);

	r_aliasmesh = (r_aliasmesh_t *) r_meshbuffer;
	r_aliasst = (float *) (r_aliasmesh + hdr->numverts);
	mesh = (aliasmesh_t *) ((byte *) hdr + hdr->aliasmesh);

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

	GL_SetStreamSource (0, GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (r_aliasmesh_t), r_aliasmesh->xyz);
	GL_SetStreamSource (0, GLSTREAM_COLOR, 4, GL_FLOAT, sizeof (r_aliasmesh_t), r_aliasmesh->rgba);

	if (showtris)
	{
		GL_SetStreamSource (0, GLSTREAM_TEXCOORD0, 0, GL_NONE, 0, NULL);
		GL_SetStreamSource (0, GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
	}
	else
	{
		GL_SetStreamSource (0, GLSTREAM_TEXCOORD0, 2, GL_FLOAT, 0, r_aliasst);

		if (state->fb)
			GL_SetStreamSource (0, GLSTREAM_TEXCOORD1, 2, GL_FLOAT, 0, r_aliasst);
		else GL_SetStreamSource (0, GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
	}

	GL_SetStreamSource (0, GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);

	if (lerping)
	{
		for (v = 0; v < hdr->numverts; v++, r_aliasmesh++, mesh++)
		{
			trivert1 = &verts1[mesh->vertindex];
			trivert2 = &verts2[mesh->vertindex];

			// interpolate the normals
			lerpnormal[0] = r_avertexnormals[trivert1->lightnormalindex][0] * iblend + r_avertexnormals[trivert2->lightnormalindex][0] * blend;
			lerpnormal[1] = r_avertexnormals[trivert1->lightnormalindex][1] * iblend + r_avertexnormals[trivert2->lightnormalindex][1] * blend;
			lerpnormal[2] = r_avertexnormals[trivert1->lightnormalindex][2] * iblend + r_avertexnormals[trivert2->lightnormalindex][2] * blend;

			mesh->light = DotProduct (lerpnormal, r_plightvec);

			r_aliasmesh->xyz[0] = trivert1->v[0] * iblend + trivert2->v[0] * blend;
			r_aliasmesh->xyz[1] = trivert1->v[1] * iblend + trivert2->v[1] * blend;
			r_aliasmesh->xyz[2] = trivert1->v[2] * iblend + trivert2->v[2] * blend;
		}
	}
	else
	{
		for (v = 0; v < hdr->numverts; v++, r_aliasmesh++, mesh++)
		{
			trivert1 = &verts1[mesh->vertindex];

			mesh->light = DotProduct (r_avertexnormals[trivert1->lightnormalindex], r_plightvec);

			r_aliasmesh->xyz[0] = trivert1->v[0];
			r_aliasmesh->xyz[1] = trivert1->v[1];
			r_aliasmesh->xyz[2] = trivert1->v[2];
		}
	}

	// reset these for lighting
	r_aliasmesh = (r_aliasmesh_t *) r_meshbuffer;
	r_aliasst = (float *) (r_aliasmesh + hdr->numverts);
	mesh = (aliasmesh_t *) ((byte *) hdr + hdr->aliasmesh);

	if (showtris)
	{
		for (v = 0; v < hdr->numverts; v++, r_aliasmesh++, mesh++)
		{
			r_aliasmesh->rgba[0] = 1;
			r_aliasmesh->rgba[1] = 1;
			r_aliasmesh->rgba[2] = 1;
			r_aliasmesh->rgba[3] = 1;
		}
	}
	else
	{
		for (v = 0; v < hdr->numverts; v++, r_aliasmesh++, mesh++, r_aliasst += 2)
		{
			r_aliasst[0] = mesh->st[0];
			r_aliasst[1] = mesh->st[1];

			r_aliasmesh->rgba[0] = state->ambientlight[0];
			r_aliasmesh->rgba[1] = state->ambientlight[1];
			r_aliasmesh->rgba[2] = state->ambientlight[2];
			r_aliasmesh->rgba[3] = state->ambientlight[3];

			if (mesh->light < 0)
			{
				r_aliasmesh->rgba[0] -= mesh->light * state->shadelight[0];
				r_aliasmesh->rgba[1] -= mesh->light * state->shadelight[1];
				r_aliasmesh->rgba[2] -= mesh->light * state->shadelight[2];
			}
		}
	}

	// for testing of light shading
	// glDisable (GL_TEXTURE_2D);
	GL_SetIndices (0, ((byte *) hdr + hdr->indexes));
	GL_DrawIndexedPrimitive (GL_TRIANGLES, hdr->numindexes, hdr->numverts);
	// glEnable (GL_TEXTURE_2D);

	// restore normal depth range
	if (ent->renderfx & RF_WEAPONMODEL)
		qglDepthRange (QGL_DEPTH_3D_BEGIN, QGL_DEPTH_3D_END);
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

	qglColor4f (1, 1, 1, 1);

	// go back to the world matrix
	qglLoadMatrixf (r_world_matrix.m16);
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
