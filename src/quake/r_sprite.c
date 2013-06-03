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
// r_sprite.c - sprite model rendering

#include "gl_local.h"

GLuint r_lastspritetexture = 0;
r_defaultquad_t *r_sprite_quads = NULL;
r_defaultquad2_t *r_sprite_quads2 = NULL;

extern unsigned int r_quadindexbuffer;
extern unsigned int r_parttexcoordbuffer;

void R_InvalidateSprite (void)
{
	// used when we want to force a state update
	r_lastspritetexture = 0;
}


void R_SpriteBegin (void)
{
	r_num_quads = 0;
	r_sprite_quads = r_default_quads;
	r_sprite_quads2 = r_default_quads2;

	GL_SetIndices (0, r_quad_indexes);

	GL_SetStreamSource (0, GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (r_defaultquad_t), r_default_quads->xyz);
	GL_SetStreamSource (0, GLSTREAM_COLOR, 4, GL_UNSIGNED_BYTE, sizeof (r_defaultquad_t), r_default_quads->color);
	GL_SetStreamSource (0, GLSTREAM_TEXCOORD0, 2, GL_FLOAT, sizeof (r_defaultquad_t), r_default_quads->st);
	GL_SetStreamSource (0, GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
	GL_SetStreamSource (0, GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);

	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_MODULATE);
}


void R_SpriteEnd (void)
{
	if (r_num_quads)
	{
		GL_DrawIndexedPrimitive (GL_TRIANGLES, r_num_quads * 6, r_num_quads * 4);
		r_num_quads = 0;
		r_sprite_quads = r_default_quads;
		r_sprite_quads2 = r_default_quads2;
	}

	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);
}


void R_SpriteVertex (r_defaultquad_t *rsq, float *point, float *color, float alpha, float s, float t)
{
	rsq->xyz[0] = point[0];
	rsq->xyz[1] = point[1];
	rsq->xyz[2] = point[2];

	rsq->color[0] = BYTE_CLAMPF (color[0]);
	rsq->color[1] = BYTE_CLAMPF (color[1]);
	rsq->color[2] = BYTE_CLAMPF (color[2]);
	rsq->color[3] = BYTE_CLAMPF (alpha);

	rsq->st[0] = s;
	rsq->st[1] = t;
}


void R_SpriteVertex2 (r_defaultquad2_t *rsq, float *point, float *color, float alpha, float s, float t)
{
	rsq->xyz[0] = point[0];
	rsq->xyz[1] = point[1];
	rsq->xyz[2] = point[2];

	rsq->color[0] = BYTE_CLAMPF (color[0]);
	rsq->color[1] = BYTE_CLAMPF (color[1]);
	rsq->color[2] = BYTE_CLAMPF (color[2]);
	rsq->color[3] = BYTE_CLAMPF (alpha);
}


void R_RenderSpritePolygon (mspriteframe_t *frame, float *origin, float *up, float *right, float *color, float alpha)
{
	float point[3];

	VectorMA (origin, frame->up, up, point);
	VectorMA (point, frame->left, right, point);
	R_SpriteVertex (&r_sprite_quads[0], point, color, alpha, 0, 0);

	VectorMA (origin, frame->up, up, point);
	VectorMA (point, frame->right, right, point);
	R_SpriteVertex (&r_sprite_quads[1], point, color, alpha, 1, 0);

	VectorMA (origin, frame->down, up, point);
	VectorMA (point, frame->right, right, point);
	R_SpriteVertex (&r_sprite_quads[2], point, color, alpha, 1, 1);

	VectorMA (origin, frame->down, up, point);
	VectorMA (point, frame->left, right, point);
	R_SpriteVertex (&r_sprite_quads[3], point, color, alpha, 0, 1);

	r_sprite_quads += 4;
	r_sprite_quads2 += 4;
	r_num_quads++;
}


/*
================
R_GetSpriteFrame
================
*/
static mspriteframe_t *R_GetSpriteFrame (entity_t *e)
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int				i, numframes, frame;
	float			*pintervals, fullinterval, targettime, time;

	psprite = (msprite_t *) e->model->cache.data;
	frame = e->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Com_Printf ("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = r_refdef2.time;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}


/*
=================
R_DrawSpriteModel
=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	vec3_t			v_forward, v_right, v_up;
	msprite_t		*psprite;
	mspriteframe_t	*frame;
	float			*s_up, *s_right;
	float			angle, sr, cr;
	float			defaultcolor[3] = {1, 1, 1};
	float			*color = defaultcolor;

	frame = R_GetSpriteFrame (e);
	psprite = (msprite_t *) e->model->cache.data;

	switch(psprite->type)
	{
	case SPR_VP_PARALLEL_UPRIGHT: // faces view plane, up is towards the heavens
		v_up[0] = 0;
		v_up[1] = 0;
		v_up[2] = 1;
		s_up = v_up;
		s_right = vright;
		break;

	case SPR_FACING_UPRIGHT: // faces camera origin, up is towards the heavens
		VectorSubtract(e->origin, r_origin, v_forward);
		v_forward[2] = 0;
		VectorNormalizeFast (v_forward);
		v_right[0] = v_forward[1];
		v_right[1] = -v_forward[0];
		v_right[2] = 0;
		v_up[0] = 0;
		v_up[1] = 0;
		v_up[2] = 1;
		s_up = v_up;
		s_right = v_right;
		break;

	case SPR_VP_PARALLEL: // faces view plane, up is towards the top of the screen
		s_up = vup;
		s_right = vright;
		break;

	case SPR_ORIENTED: // pitch yaw roll are independent of camera
		AngleVectors (e->angles, v_forward, v_right, v_up);
		s_up = v_up;
		s_right = v_right;
		break;

	case SPR_VP_PARALLEL_ORIENTED: // faces view plane, but obeys roll value
		angle = e->angles[ROLL] * M_PI_DIV_180;
		sr = sin(angle);
		cr = cos(angle);
		v_right[0] = vright[0] * cr + vup[0] * sr;
		v_right[1] = vright[1] * cr + vup[1] * sr;
		v_right[2] = vright[2] * cr + vup[2] * sr;
		v_up[0] = vright[0] * -sr + vup[0] * cr;
		v_up[1] = vright[1] * -sr + vup[1] * cr;
		v_up[2] = vright[2] * -sr + vup[2] * cr;
		s_up = v_up;
		s_right = v_right;
		break;

	default:
		return;
	}

	// if it overflows invalidate the cached state to force a flush and reset
	if (r_num_quads == r_max_quads)
		r_lastspritetexture = 0;

	if (frame->gl_texture->texnum != r_lastspritetexture)
	{
		if (r_num_quads)
		{
			GL_DrawIndexedPrimitive (GL_TRIANGLES, r_num_quads * 6, r_num_quads * 4);
			r_num_quads = 0;
			r_sprite_quads = r_default_quads;
			r_sprite_quads2 = r_default_quads2;
		}

		// only if it changes
		GL_BindTexture (GL_TEXTURE0_ARB, frame->gl_texture);

		r_lastspritetexture = frame->gl_texture->texnum;
	}

	R_RenderSpritePolygon (frame, e->origin, s_up, s_right, color, (float) e->alpha / 255.0f);
}


void R_DrawSpriteModels (void)
{
	int i;
	entity_t *e;

	if (!r_drawentities.value)
		return;

	// sprites just go onto the alpha list for later drawing
	for (i = 0; i < cl_numvisedicts; i++)
	{
		e = &cl_visedicts[i];

		if (e->model->type != mod_sprite)
			continue;

		// we can't sort by texture as sprites may be interleaved with different object types at different depths
		R_AddEntityToAlphaList (e);
	}
}


