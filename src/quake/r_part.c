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

/*
===============
R_DrawParticles
===============
*/
void R_DrawParticles (void)
{
	int				i;
	byte			color[4];
	vec3_t			up, right, p_up, p_right, p_upright;
	float			scale;
	particle_t		*p;

	VectorScale (vup, 1.5, up);
	VectorScale (vright, 1.5, right);

	GL_Bind (particletexture->texnum);
	
	qglEnable (GL_BLEND);
	qglDepthMask (GL_FALSE);
	qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	qglBegin (GL_QUADS);
	for (i = 0, p = r_refdef2.particles; i < r_refdef2.numParticles; i++, p++)
	{
		// hack a scale up to keep particles from disapearing
		scale = (p->org[0] - r_origin[0]) * vpn[0]
				  + (p->org[1] - r_origin[1]) * vpn[1]
				  + (p->org[2] - r_origin[2]) * vpn[2];
		if (scale < 20)
			scale = 1 + 0.08;
		else
			scale = 1 + scale * 0.004;

		scale /= 2.0; // quad is half the size of triangle

		*(int *)color = d_8to24table_rgba[p->color];
		color[3] = p->alpha * 255;

		qglColor4ubv (color);

		qglTexCoord2f (0, 0);
		qglVertex3fv (p->org);

		qglTexCoord2f (0.5, 0);
		VectorMA (p->org, scale, up, p_up);
		qglVertex3fv (p_up);

		qglTexCoord2f (0.5,0.5);
		VectorMA (p_up, scale, right, p_upright);
		qglVertex3fv (p_upright);

		qglTexCoord2f (0,0.5);
		VectorMA (p->org, scale, right, p_right);
		qglVertex3fv (p_right);
	}
	qglEnd ();

	qglDisable (GL_BLEND);
	qglDepthMask (GL_TRUE);
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	qglColor3f (1, 1, 1);
}

/*
===============
R_DrawParticles_ShowTris
===============
*/
void R_DrawParticles_ShowTris (void)
{
	int				i;
	particle_t		*p;
	float			scale;
	vec3_t			up, right, p_up, p_right, p_upright;

	VectorScale (vup, 1.5, up);
	VectorScale (vright, 1.5, right);

	for (i = 0, p = r_refdef2.particles; i < r_refdef2.numParticles; i++, p++)
	{
		qglBegin (GL_TRIANGLE_FAN);

		// hack a scale up to keep particles from disapearing
		scale = (p->org[0] - r_origin[0]) * vpn[0]
			  + (p->org[1] - r_origin[1]) * vpn[1]
			  + (p->org[2] - r_origin[2]) * vpn[2];
		if (scale < 20)
			scale = 1 + 0.08;
		else
			scale = 1 + scale * 0.004;

		scale /= 2.0; // quad is half the size of triangle

		qglVertex3fv (p->org);

		VectorMA (p->org, scale, up, p_up);
		qglVertex3fv (p_up);

		VectorMA (p_up, scale, right, p_upright);
		qglVertex3fv (p_upright);

		VectorMA (p->org, scale, right, p_right);
		qglVertex3fv (p_right);

		qglEnd ();
	}
}
