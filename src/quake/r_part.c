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
void R_ParticlesBegin (void)
{
	GL_BindTexture (GL_TEXTURE0_ARB, particletexture);
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_MODULATE);

	r_num_quads = 0;
	R_EnableVertexArrays (r_default_quads->xyz, r_default_quads->color, r_default_quads->st, NULL, NULL, sizeof (r_defaultquad_t));
}


void R_ParticlesEnd (void)
{
	// draw anything left over
	if (r_num_quads)
	{
		R_DrawElements (r_num_quads * 6, r_num_quads * 4, r_quad_indexes);
		//R_DrawArrays (GL_QUADS, 0, r_num_quads * 4);
		r_num_quads = 0;
	}

	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);
}

#define PARTICLE_VERTEX(i,vert) \
	rpq[i].xyz[0] = (vert)[0]; \
	rpq[i].xyz[1] = (vert)[1]; \
	rpq[i].xyz[2] = (vert)[2];

#define PARTICLE_TEXCOORD(i,s,t) \
	rpq[i].st[0] = s; \
	rpq[i].st[1] = t;

void R_DrawParticlesForType (particle_type_t *pt)
{
	particle_t *p;
	float up[3];
	float right[3];
	float scale;
	vec3_t p_up, p_right, p_upright; // johnfitz -- p_ vectors
	r_defaultquad_t *rpq = &r_default_quads[r_num_quads * 4];
	unsigned int rgba;

	// should this be adjustable per particle???
	VectorScale (vup, 1.5, up);
	VectorScale (vright, 1.5, right);

	for (p = pt->particles; p; p = p->next, rpq += 4, r_num_quads++)
	{
		if (r_num_quads == r_max_quads)
		{
			R_DrawElements (r_num_quads * 6, r_num_quads * 4, r_quad_indexes);
			// R_DrawArrays (GL_QUADS, 0, r_num_quads * 4);
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
		((byte *) &rgba)[3] = 255;
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
