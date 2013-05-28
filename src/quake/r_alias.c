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

extern qbool mtexenabled;

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

float	*shadedots = r_avertexnormal_dots[0];

int	lastposenum;

qbool	overbright;
qbool	shading = true;

/*
=============
GL_DrawAliasFrame
=============
*/
static void GL_DrawAliasFrame (aliashdr_t *paliashdr, int posenum)
{
	float 	vertcolor[4];
	trivertx_t	*verts;
	int		*order;
	int		count;
	float	u,v;

	if (currententity->renderfx & RF_TRANSLUCENT)
	{
		qglEnable (GL_BLEND);
		vertcolor[3] = currententity->alpha;
	}
	else
		vertcolor[3] = 1.0;

	lastposenum = posenum;

	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	while ((count = *order++))
	{
		// get the vertex count and primitive type
		if (count < 0)
		{
			count = -count;
			qglBegin (GL_TRIANGLE_FAN);
		}
		else
			qglBegin (GL_TRIANGLE_STRIP);

		do
		{
			// texture coordinates come from the draw list
			u = ((float *)order)[0];
			v = ((float *)order)[1];
			if (mtexenabled)
			{
				qglMultiTexCoord2f (GL_TEXTURE0_ARB, u, v);
				qglMultiTexCoord2f (GL_TEXTURE1_ARB, u, v);
			}
			else
			{
				qglTexCoord2f (u, v);
			}

			order += 2;

			// normals and vertexes come from the frame list
			if (shading)
			{
				vertcolor[0] = shadedots[verts->lightnormalindex] * lightcolor[0];
				vertcolor[1] = shadedots[verts->lightnormalindex] * lightcolor[1];
				vertcolor[2] = shadedots[verts->lightnormalindex] * lightcolor[2];
				qglColor4fv (vertcolor);
			}

			qglVertex3f (verts->v[0], verts->v[1], verts->v[2]);
			verts++;
		} while (--count);

		qglEnd ();
	}

	if (currententity->renderfx & RF_TRANSLUCENT)
		qglDisable (GL_BLEND);
}

/*
=================
R_SetupAliasFrame
=================
*/
static void R_SetupAliasFrame (int frame, aliashdr_t *paliashdr)
{
	int				pose, numposes;
	float			interval;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Com_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		interval = paliashdr->frames[frame].interval;
		pose += (int)(r_refdef2.time / interval) % numposes;
	}

	GL_DrawAliasFrame (paliashdr, pose);
}

/*
=================
R_SetupAliasLighting
=================
*/
static void R_SetupAliasLighting (entity_t	*e)
{
	vec3_t		dist;
	float		add;
	int			i;

	R_LightPoint (e->origin);

	// add dlights
	for (i = 0; i < r_refdef2.numDlights; i++)
	{
		VectorSubtract (e->origin, r_refdef2.dlights[i].origin, dist);
		add = r_refdef2.dlights[i].radius - VectorLength(dist);		
		if (add > 0)
			VectorMA (lightcolor, add, r_refdef2.dlights[i].color, lightcolor);
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
		if (e->model->modhint == MOD_THUNDERBOLT || e->model->modhint == MOD_FLAME)
		{
			lightcolor[0] = 256.0f;
			lightcolor[1] = 256.0f;
			lightcolor[2] = 256.0f;
		}
	}

	shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	VectorScale(lightcolor, 1.0f / 200.0f, lightcolor);
}


/*
=================
R_DrawAliasModel
=================
*/
void R_DrawAliasModel (entity_t *ent)
{
	aliashdr_t	*paliashdr;
	int			anim, skinnum;
	model_t		*clmodel = ent->model;
	gltexture_t	*texture, *fb_texture;

	// cull it
	if (R_CullModelForEntity(ent))
		return;

	VectorCopy (ent->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	//
	// locate the proper data
	//
	paliashdr = (aliashdr_t *)Mod_Extradata (ent->model);

	//
	// transform it
	//
	qglPushMatrix ();
	RB_RotateMatrixForEntity (ent->origin, ent->angles);

	if (clmodel->modhint == MOD_EYES) {
		qglTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));
		qglScalef (paliashdr->scale[0]*2, paliashdr->scale[1]*2, paliashdr->scale[2]*2); // HACK: double size of eyes, since they are really hard to see in gl
	} else {
		qglTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		qglScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	}

	//
	// random stuff
	//
	overbright = gl_overbright.value;
	shading = true;

	//
	// set up lighting
	//
	c_alias_polys += paliashdr->numtris;
	R_SetupAliasLighting(ent);

	//
	// set up textures
	//
	GL_DisableMultitexture();
	
	anim = (int)(r_refdef2.time*10) & 3;
	skinnum = ent->skinnum;
	if ((skinnum >= paliashdr->numskins) || (skinnum < 0)) {
		Com_DPrintf ("R_DrawAliasModel: no such skin # %d\n", skinnum);
		skinnum = 0;
	}
	texture = paliashdr->gl_texture[skinnum][anim];
	fb_texture = paliashdr->fb_texture[skinnum][anim];
	if (!gl_fullbrights.value)
		fb_texture = NULL;
	if (ent->colormap && (clmodel->modhint == MOD_PLAYER || ent->renderfx & RF_PLAYERMODEL) && !gl_nocolors.value)
		R_GetTranslatedPlayerSkin (ent->colormap, &texture->texnum, &fb_texture->texnum);

	//
	// draw it
	//

	// hack the depth range to prevent view model from poking into walls
	if (ent->renderfx & RF_WEAPONMODEL)
		qglDepthRange (0, 0.3);

	if (r_fullbright.value)
	{
		GL_Bind (texture->texnum);
		shading = false;
		qglColor4f(1,1,1,1);

		R_SetupAliasFrame (ent->frame, paliashdr);

		if (fb_texture)
		{
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			GL_Bind(fb_texture->texnum);

			qglEnable(GL_BLEND);
			qglBlendFunc (GL_ONE, GL_ONE);
			qglDepthMask(GL_FALSE);
			qglColor4f(1,1,1,1);

			R_SetupAliasFrame (ent->frame, paliashdr);

			qglDepthMask(GL_TRUE);
			qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			qglDisable(GL_BLEND);
		}
	}
	else if (r_lightmap.value)
	{
		qglDisable (GL_TEXTURE_2D);
		shading = false;
		qglColor4f(1,1,1,1);

		R_SetupAliasFrame (ent->frame, paliashdr);

		qglEnable (GL_TEXTURE_2D);
	}
	else if (overbright)
	{
		if  (gl_support_texture_combine && gl_support_texture_add && fb_texture) // case 1: everything in one pass
		{
			GL_Bind (texture->texnum);
			qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
			qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PRIMARY_COLOR_ARB);
			qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, r_lightscale);

			GL_EnableMultitexture(); // selects TEXTURE1
			GL_Bind (fb_texture->texnum);
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
			qglEnable(GL_BLEND);
			
			R_SetupAliasFrame (ent->frame, paliashdr);
			
			qglDisable(GL_BLEND);
			
			GL_DisableMultitexture();
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		}
		else if (gl_support_texture_combine) // case 2: overbright in one pass, then fullbright pass
		{
			// first pass
			GL_Bind(texture->texnum);
			qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
			qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PRIMARY_COLOR_ARB);
			qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, r_lightscale);

			R_SetupAliasFrame (ent->frame, paliashdr);

			qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.0f);
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

			// second pass
			if (fb_texture)
			{
				qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				GL_Bind(fb_texture->texnum);
				qglEnable(GL_BLEND);
				qglBlendFunc (GL_ONE, GL_ONE);
				qglDepthMask(GL_FALSE);
				shading = false;
				qglColor4f(1,1,1,1);

				R_SetupAliasFrame (ent->frame, paliashdr);

				qglDepthMask(GL_TRUE);
				qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				qglDisable(GL_BLEND);
				qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			}
		}
		else // case 3: overbright in two passes, then fullbright pass
		{
			// first pass
			GL_Bind(texture->texnum);
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			
			R_SetupAliasFrame (ent->frame, paliashdr);

			// second pass -- additive to double the object colors
			qglEnable(GL_BLEND);
			qglBlendFunc (GL_ONE, GL_ONE);
			qglDepthMask(GL_FALSE);

			R_SetupAliasFrame (ent->frame, paliashdr);

			qglDepthMask(GL_TRUE);
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			qglDisable(GL_BLEND);

			// third pass
			if (fb_texture)
			{
				qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				GL_Bind(fb_texture->texnum);
				qglEnable(GL_BLEND);
				qglBlendFunc (GL_ONE, GL_ONE);
				qglDepthMask(GL_FALSE);
				shading = false;
				qglColor4f(1,1,1,1);

				R_SetupAliasFrame (ent->frame, paliashdr);

				qglDepthMask(GL_TRUE);
				qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				qglDisable(GL_BLEND);
				qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			}
		}
	}
	else
	{
		if (fb_texture && gl_support_texture_add) // case 4: fullbright mask using multitexture
		{
			GL_DisableMultitexture(); // selects TEXTURE0
			GL_Bind (texture->texnum);
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

			GL_EnableMultitexture(); // selects TEXTURE1
			GL_Bind (fb_texture->texnum);
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
			qglEnable(GL_BLEND);

			R_SetupAliasFrame (ent->frame, paliashdr);

			qglDisable(GL_BLEND);
			GL_DisableMultitexture();
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		}
		else // case 5: fullbright mask without multitexture
		{
			// first pass
			GL_Bind (texture->texnum);
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			R_SetupAliasFrame (ent->frame, paliashdr);

			// second pass
			if (fb_texture)
			{
				GL_Bind(fb_texture->texnum);
				qglEnable(GL_BLEND);
				qglBlendFunc (GL_ONE, GL_ONE);
				qglDepthMask(GL_FALSE);
				shading = false;
				qglColor4f(1,1,1,1);

				R_SetupAliasFrame (ent->frame, paliashdr);

				qglDepthMask(GL_TRUE);
				qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				qglDisable(GL_BLEND);
			}
		}
	}

	// restore normal depth range
	if (ent->renderfx & RF_WEAPONMODEL)
		qglDepthRange (0, 1);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	qglColor4f (1, 1, 1, 1);
	qglDepthMask(GL_TRUE);
	qglDisable(GL_BLEND);
	qglPopMatrix ();
}

// values for shadow matrix
#define SHADOW_SKEW_X -0.7	// skew along x axis. -0.7 to mimic glquake shadows
#define SHADOW_SKEW_Y 0		// skew along y axis. 0 to mimic glquake shadows
#define SHADOW_VSCALE 0		// 0 = completely flat
#define SHADOW_HEIGHT 0.1	// how far above the floor to render the shadow

extern	vec3_t			lightspot;

/*
=============
R_DrawAliasShadow

TODO: orient shadow onto "lightplane" (a global mplane_t*)
=============
*/
void R_DrawAliasShadow (entity_t *e)
{
	float	shadowmatrix[16] = {1,				0,				0,				0,
								0,				1,				0,				0,
								SHADOW_SKEW_X,	SHADOW_SKEW_Y,	SHADOW_VSCALE,	0,
								0,				0,				SHADOW_HEIGHT,	1};
	float		lheight;
	aliashdr_t	*paliashdr;

	if (R_CullModelForEntity(e))
		return;

	if (e->renderfx & RF_WEAPONMODEL)
		return;
	
	paliashdr = (aliashdr_t *)Mod_Extradata (e->model);

	R_LightPoint (e->origin);
	lheight = currententity->origin[2] - lightspot[2];

	// set up matrix
    qglPushMatrix ();
	qglTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);
	qglTranslatef (0,0,-lheight);
	qglMultMatrixf (shadowmatrix);
	qglTranslatef (0,0,lheight);
	qglRotatef (e->angles[1],  0, 0, 1);
	qglRotatef (-e->angles[0],  0, 1, 0);
	qglRotatef (e->angles[2],  1, 0, 0);
	qglTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
	qglScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);

	// draw it
	qglDepthMask(GL_FALSE);
	qglEnable (GL_BLEND);
	GL_DisableMultitexture ();
	qglDisable (GL_TEXTURE_2D);
	shading = false;
	qglColor4f(0,0,0,0.5);

	R_SetupAliasFrame (e->frame, paliashdr);

	qglEnable (GL_TEXTURE_2D);
	qglDisable (GL_BLEND);
	qglDepthMask(GL_TRUE);

	// clean up
	qglPopMatrix ();
}

/*
=================
R_DrawAliasModel_ShowTris
=================
*/
void R_DrawAliasModel_ShowTris (entity_t *e)
{
	aliashdr_t	*paliashdr;

	if (R_CullModelForEntity(e))
		return;

	paliashdr = (aliashdr_t *)Mod_Extradata (e->model);

    qglPushMatrix ();
	RB_RotateMatrixForEntity (e->origin, e->angles);
	qglTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
	qglScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);

	shading = false;
	qglColor3f(1,1,1);
	R_SetupAliasFrame (e->frame, paliashdr);

	qglPopMatrix ();
}

