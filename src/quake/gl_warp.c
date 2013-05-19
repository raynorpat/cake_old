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
// gl_warp.c -- sky and water polygons

#include "gl_local.h"
#include "rc_image.h"

//==============================================================================
//
//  OLD-STYLE WATER
//
//==============================================================================

extern	model_t		*loadmodel;

static msurface_t	*warpface;

float turbsin[] =
{
	#include "gl_warp_sin.h"
};

#define	SUBDIVIDE_SIZE 128

#define TURBSCALE (256.0 / (2 * M_PI))

#define WARPCALC(s,t) ((s + turbsin[(int)((t*0.125+cl.time)*(128.0/M_PI)) & 255]) * (1.0/SUBDIVIDE_SIZE))

void BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int		i, j;
	float	*v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i=0 ; i<numverts ; i++)
		for (j=0 ; j<3 ; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

void SubdividePolygon (int numverts, float *verts)
{
	int		i, j, k;
	vec3_t	mins, maxs;
	float	m;
	float	*v;
	vec3_t	front[64], back[64];
	int		f, b;
	float	dist[64];
	float	frac;
	glpoly_t *poly;
	float	s, t;

	if (numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	BoundPoly (numverts, verts, mins, maxs);

	for (i=0 ; i<3 ; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5;
		m = SUBDIVIDE_SIZE * floor (m/SUBDIVIDE_SIZE + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j=0 ; j<numverts ; j++, v+= 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v-=i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j=0 ; j<numverts ; j++, v+= 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j+1] == 0)
				continue;
			if ( (dist[j] > 0) != (dist[j+1] > 0) )
			{
				// clip point
				frac = dist[j] / (dist[j] - dist[j+1]);
				for (k=0 ; k<3 ; k++)
					front[f][k] = back[b][k] = v[k] + frac*(v[3+k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon (f, front[0]);
		SubdividePolygon (b, back[0]);
		return;
	}

	poly = (glpoly_t *) Hunk_Alloc (sizeof(glpoly_t) + (numverts-4) * VERTEXSIZE*sizeof(float));
	poly->next = warpface->polys->next;
	warpface->polys->next = poly;
	poly->numverts = numverts;
	for (i=0 ; i<numverts ; i++, verts+= 3)
	{
		VectorCopy (verts, poly->verts[i]);
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
	}
}

/*
================
GL_SubdivideSurface
================
*/
void GL_SubdivideSurface (msurface_t *fa)
{
	vec3_t		verts[64];
	int			i;

	warpface = fa;

	// the first poly in the chain is the undivided poly for newwater rendering.
	// grab the verts from that.
	for (i=0; i<fa->polys->numverts; i++)
		VectorCopy (fa->polys->verts[i], verts[i]);

	SubdividePolygon (fa->polys->numverts, verts[0]);
}

/*
================
DrawWaterPoly
================
*/
void DrawWaterPoly (glpoly_t *p)
{
	float	*v;
	int		i;
	float		s, t, os, ot;

	qglBegin (GL_TRIANGLE_FAN);
	v = p->verts[0];
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
	{
		os = v[3];
		ot = v[4];

		s = os + turbsin[(int)((ot*0.125+r_refdef2.time) * TURBSCALE) & 255];
		s *= (1.0/SUBDIVIDE_SIZE);

		t = ot + turbsin[(int)((os*0.125+r_refdef2.time) * TURBSCALE) & 255];
		t *= (1.0/SUBDIVIDE_SIZE);

		qglTexCoord2f (s, t);
		qglVertex3fv (v);
	}
	qglEnd ();
}

//==============================================================================
//
//  RENDER-TO-FRAMEBUFFER WATER
//
//==============================================================================

/*
=============
R_UpdateWarpTextures 

each frame, update warping textures
=============
*/
void R_UpdateWarpTextures (void)
{
	texture_t *tx;
	int i;
	float x, y, x2, warptess;

	if (r_oldwater.value || cl.paused || r_lightmap.value)
		return;

	warptess = 128.0/clamp(3.0, floor(r_waterquality.value), 64.0);

	for (i=0; i<r_worldmodel->numtextures; i++)
	{
		if (!(tx = r_worldmodel->textures[i]))
			continue;

		if (!tx->update_warp)
			continue;

		// render warp
		GL_SetCanvas (CANVAS_WARPIMAGE);
		GL_Bind (tx->gl_texture->texnum);
		for (x=0.0; x<128.0; x=x2)
		{
			x2 = x + warptess;
			qglBegin (GL_TRIANGLE_STRIP);
			for (y=0.0; y<128.01; y+=warptess) // .01 for rounding errors
			{
				qglTexCoord2f (WARPCALC(x,y), WARPCALC(y,x));
				qglVertex2f (x,y);
				qglTexCoord2f (WARPCALC(x2,y), WARPCALC(y,x2));
				qglVertex2f (x2,y);
			}
			qglEnd();
		}

		// copy to texture
		GL_Bind (tx->warpimage->texnum);
		qglCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 0, vid.realheight-gl_warpimagesize, gl_warpimagesize, gl_warpimagesize);

		tx->update_warp = false;
	}

	// if warp render went down into sbar territory, we need to be sure to refresh it next frame
	if (gl_warpimagesize + sb_lines > vid.realheight)
		Sbar_Changed ();
}
