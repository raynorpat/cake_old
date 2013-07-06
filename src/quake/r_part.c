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
// r_part.c - GL rendering of particles

#include "gl_local.h"

float texcoordfactor = 0.5f;

/*
===============
R_DrawParticles
===============
*/
extern unsigned int r_parttexcoordbuffer;


float up[3];
float right[3];

int r_particleframe = -1;

void R_ParticlesBegin (void)
{
	VectorScale (vup, 1.5, up);
	VectorScale (vright, 1.5, right);

	GL_BindTexture (GL_TEXTURE0_ARB, particletexture);

	r_num_quads = 0;

	GL_SetStreamSource (GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (r_defaultquad_t), r_default_quads->xyz);
	GL_SetStreamSource (GLSTREAM_COLOR, 4, GL_UNSIGNED_BYTE, sizeof (r_defaultquad_t), r_default_quads->color);
	GL_SetStreamSource (GLSTREAM_TEXCOORD0, 2, GL_FLOAT, sizeof (r_defaultquad_t), r_default_quads->st);
	GL_SetStreamSource (GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
	GL_SetStreamSource (GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);

	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_MODULATE);
	qglDepthMask (GL_TRUE);
}


void R_ParticlesEnd (void)
{
	// draw anything left over
	if (r_num_quads)
	{
		GL_SetIndices (r_quad_indexes);
		GL_DrawIndexedPrimitive (GL_TRIANGLES, r_num_quads * 6, r_num_quads * 4);

		r_num_quads = 0;
	}

	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);
	qglDepthMask (GL_FALSE);
}


#define PARTICLE_VERTEX(i,vert) \
	rpq[i].xyz[0] = (vert)[0]; \
	rpq[i].xyz[1] = (vert)[1]; \
	rpq[i].xyz[2] = (vert)[2];

#define PARTICLE_VERTEX2(xyz,vert) \
	xyz[0] = (vert)[0]; \
	xyz[1] = (vert)[1]; \
	xyz[2] = (vert)[2];

#define PARTICLE_TEXCOORD(i,s,t) \
	rpq[i].st[0] = s; \
	rpq[i].st[1] = t;

void R_DrawParticlesForType (particle_type_t *pt)
{
	particle_t *p;

	float scale;
	vec3_t p_up, p_right, p_upright; // johnfitz -- p_ vectors

	r_defaultquad_t *rpq;

	unsigned int rgba;

	rpq = &r_default_quads[r_num_quads * 4];

	for (p = pt->particles; p; p = p->next, rpq += 4, r_num_quads++)
	{
		// don't crash (255 is alpha)
		if (p->color < 0 || p->color > 254) continue;
		if (p->alpha < 0) continue;

		if (r_num_quads >= r_max_quads)
		{
			GL_SetIndices (r_quad_indexes);
			GL_DrawIndexedPrimitive (GL_TRIANGLES, r_num_quads * 6, r_num_quads * 4);

			rpq = r_default_quads;
			r_num_quads = 0;
		}

		// hack a scale up to keep particles from disapearing
		scale = (p->org[0] - r_origin[0]) * vpn[0] +
				(p->org[1] - r_origin[1]) * vpn[1] +
				(p->org[2] - r_origin[2]) * vpn[2];

		if (scale < 20)
			scale = 0.5 + 0.04;
		else
			scale = 0.5 + scale * 0.002;

		VectorMA (p->org, scale, up, p_up);
		VectorMA (p->org, scale, right, p_right);
		VectorMA (p_up, scale, right, p_upright);

		// set correct particle colour
		rgba = d_8to24table_rgba[(int) p->color];
		((byte *) &rgba)[3] = BYTE_CLAMPF (p->alpha);

		rpq[0].rgba = rpq[1].rgba = rpq[2].rgba = rpq[3].rgba = rgba;
	
		PARTICLE_VERTEX (0, p->org);
		PARTICLE_TEXCOORD (0, 0, 0);
	
		PARTICLE_VERTEX (1, p_up);
		PARTICLE_TEXCOORD (1, texcoordfactor, 0);

		PARTICLE_VERTEX (2, p_upright);
		PARTICLE_TEXCOORD (2, texcoordfactor, texcoordfactor);
	
		PARTICLE_VERTEX (3, p_right);
		PARTICLE_TEXCOORD (3, 0, texcoordfactor);
	}
}


void R_AddParticlesToAlphaList (particle_type_t *pt);

void R_DrawParticles (void)
{
	extern particle_type_t *active_particle_types;
	particle_type_t *pt;

	if (!active_particle_types)
		return;

	for (pt = active_particle_types; pt; pt = pt->next)
	{
		if (!pt->particles)
			continue;

		R_AddParticlesToAlphaList (pt);
	}
}

void CL_WipeParticles (void)
{
	extern particle_type_t *active_particle_types, *free_particle_types;
	extern particle_t *free_particles;

	// these need to be wiped immediately on going to a new server
	active_particle_types = NULL;
	free_particle_types = NULL;
	free_particles = NULL;
}
