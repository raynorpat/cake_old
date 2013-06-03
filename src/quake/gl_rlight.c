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
// r_light.c

#include "gl_local.h"

void r_light_start(void)
{
}

void r_light_shutdown(void)
{
}

void r_light_newmap(void)
{
	int i;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264; // normal light value
}

void R_Light_Init(void)
{
	R_RegisterModule("R_Light", r_light_start, r_light_shutdown, r_light_newmap);
}

int r_numcoronaverts = 0;
int r_numcoronaindexes = 0;


typedef struct r_corona_s
{
	float xyz[3];
	byte rgb[4];
} r_corona_t;


unsigned short *r_coronaindexes = NULL;
r_corona_t *r_coronaverts = NULL;


void R_RenderCoronaVerts (void)
{
	if (r_numcoronaindexes)
		GL_DrawIndexedPrimitive (GL_TRIANGLES, r_numcoronaindexes, r_numcoronaverts);

	r_numcoronaindexes = 0;
	r_numcoronaverts = 0;
	r_coronaindexes = (unsigned short *) scratchbuf;
	r_coronaverts = (r_corona_t *) (r_coronaindexes + 16384);
}


float coronasin[17];
float coronacos[17];
qbool firstcorona = true;

void R_RenderCorona (dlight_t *light)
{
	int		i, k;
	vec3_t	v;
	float	rad;

	if (firstcorona)
	{
		// cache these to save a few CPU cycles when we have a lot of coronas going off
		for (i = 16; i >= 0; i--)
		{
			float a = i / 16.0f * M_PI * 2;

			coronasin[i] = sin (a);
			coronacos[i] = cos (a);
		}

		firstcorona = false;
	}

	if (r_numcoronaindexes > 16000 || r_numcoronaverts > 8000)
		R_RenderCoronaVerts ();

	// reduce the corona size so that it coexists more peacefully with proper light
	rad = light->radius * 0.15;
	VectorSubtract (light->origin, r_origin, v);

	if (VectorLength (v) < rad)
	{
		// view is inside the dlight
		// don;t bother adjusting the blend here because it screws things up
		return;
	}

	// let's unwind some loops a little...
	r_coronaverts[0].xyz[0] = light->origin[0] - vpn[0] * rad;
	r_coronaverts[0].xyz[1] = light->origin[1] - vpn[1] * rad;
	r_coronaverts[0].xyz[2] = light->origin[2] - vpn[2] * rad;

	// because we allowed lighting to go higher than 256 we need to scale the colour back further
	r_coronaverts[0].rgb[0] = light->color[0] > 5100 ? 255 : light->color[0] / 20;
	r_coronaverts[0].rgb[1] = light->color[1] > 5100 ? 255 : light->color[1] / 20;
	r_coronaverts[0].rgb[2] = light->color[2] > 5100 ? 255 : light->color[2] / 20;
	r_coronaverts[0].rgb[3] = 255;	// ati throws a hairy fit with a 3-component color array

	for (i = 16, k = 1; i >= 0; i--, k++)
	{
		// let's unwind some more loops a little...
		r_coronaverts[k].xyz[0] = light->origin[0] + vright[0] * coronacos[i] * rad + vup[0] * coronasin[i] * rad;
		r_coronaverts[k].xyz[1] = light->origin[1] + vright[1] * coronacos[i] * rad + vup[1] * coronasin[i] * rad;
		r_coronaverts[k].xyz[2] = light->origin[2] + vright[2] * coronacos[i] * rad + vup[2] * coronasin[i] * rad;

		r_coronaverts[k].rgb[0] = 0;
		r_coronaverts[k].rgb[1] = 0;
		r_coronaverts[k].rgb[2] = 0;
		r_coronaverts[k].rgb[3] = 255;	// ati throws a hairy fit with a 3-component color array
	}

	// this is a trifan with 18 vertexes so set it up right
	for (i = 2; i < 18; i++, r_coronaindexes += 3, r_numcoronaindexes += 3)
	{
		r_coronaindexes[0] = r_numcoronaverts;
		r_coronaindexes[1] = r_numcoronaverts + i - 1;
		r_coronaindexes[2] = r_numcoronaverts + i;
	}

	r_numcoronaverts += 18;
	r_coronaverts += 18;
}


void R_AddCoronaToAlphaList (dlight_t *l);

cvar_t r_show_coronas = {"r_show_coronas", "1", CVAR_ARCHIVE};

void R_CoronasBegin (void)
{
    Fog_DisableGFog ();

	qglDisable (GL_TEXTURE_2D);
	qglBlendFunc (GL_ONE, GL_ONE);

	// this doesn't render at this stage but does set up the pointers/etc
	// needs to be done before we set up our vertex arrays so that r_coronaindexes and r_coronaverts point to the right things
	R_RenderCoronaVerts ();

	// now we set up our data streams
	GL_SetIndices (0, r_coronaindexes);

	// ati throws a hairy fit with a 3-component color array
	GL_SetStreamSource (0, GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (r_corona_t), r_coronaverts->xyz);
	GL_SetStreamSource (0, GLSTREAM_COLOR, 4, GL_UNSIGNED_BYTE, sizeof (r_corona_t), r_coronaverts->rgb);
	GL_SetStreamSource (0, GLSTREAM_TEXCOORD0, 0, GL_NONE, 0, NULL);
	GL_SetStreamSource (0, GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
	GL_SetStreamSource (0, GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);
}


void R_CoronasEnd (void)
{
	// this one just renders; there's no more need for the pointers
	R_RenderCoronaVerts ();

	// fixme - make fog an enable-only thing and enable/disable during setup
    Fog_EnableGFog ();

	qglColor3f (1, 1, 1);
	qglEnable (GL_TEXTURE_2D);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


void R_DrawCoronas (void)
{
	int i;
	dlight_t *l = r_refdef2.dlights;

	if (!r_show_coronas.value) return;

	for (i = 0; i < r_refdef2.numDlights; i++, l++)
	{
		if (l->die < r_refdef2.time || !l->radius)
			continue;

		R_AddCoronaToAlphaList (l);
	}
}


/*
==================
R_AnimateLight
==================
*/
void R_AnimateLight (void)
{
	int			i,j,k;

	// light animations
	// 'm' is normal light, 'a' is no light, 'z' is double bright
	i = (int)(r_refdef2.time*10);
	for (j=0 ; j<MAX_LIGHTSTYLES ; j++)
	{
		if (!r_refdef2.lightstyles[j].length)
		{
			d_lightstylevalue[j] = 256;
			continue;
		}

		k = i % r_refdef2.lightstyles[j].length;
		k = r_refdef2.lightstyles[j].map[k] - 'a';

		d_lightstylevalue[j] = k * 22;
	}	
}


/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

/*
=============
R_MarkLights
=============
*/
void R_MarkLights (dlight_t *light, int num, mnode_t *node)
{
	mplane_t	*splitplane;
	float		dist, l, maxdist;
	msurface_t	*surf;
	int			i, j, s, t;
	vec3_t		impact;

loc0:;
	while (1)
	{
		if (node->visframe != r_visframecount) return;
		if (node->contents < 0) return;

		splitplane = node->plane;
		dist = DotProduct (light->origin, splitplane->normal) - splitplane->dist;
	
		if (dist > light->radius)
		{
			node = node->children[0];
			continue;
		}
	
		if (dist < -light->radius)
		{
			node = node->children[1];
			continue;
		}
	
		break;
	}
		
	// mark the polygons
	surf = r_worldmodel->surfaces + node->firstsurface;
	maxdist = light->radius*light->radius;

	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		// not visible
		if (surf->visframe != r_framecount) continue;

		// no lights on these
		if (surf->flags & SURF_DRAWTURB) continue;
		if (surf->flags & SURF_DRAWSKY) continue;

		for (j=0 ; j<3 ; j++)
			impact[j] = light->origin[j] - surf->plane->normal[j]*dist;

		// clamp center of light to corner and check brightness
		l = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];
		s = l+0.5;if (s < 0) s = 0;else if (s > surf->extents[0]) s = surf->extents[0];
		s = l - s;
		l = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];
		t = l+0.5;if (t < 0) t = 0;else if (t > surf->extents[1]) t = surf->extents[1];
		t = l - t;

		if ((s*s+t*t+dist*dist) < maxdist)
		{
			if (surf->dlightframe != r_framecount)
			{
				// first time hit
				surf->dlightbits[0] = surf->dlightbits[1] = surf->dlightbits[2] = surf->dlightbits[3] = 0;
				surf->dlightframe = r_framecount;
			}

			// mark the surf for this dlight
			surf->dlightbits[num >> 5] |= 1 << (num & 31);
		}
	}

	if (node->children[0]->contents >= 0)
	{
		if (node->children[1]->contents >= 0)
		{
			// must do both nodes so need to recurse here
			R_MarkLights (light, num, node->children[0]);
		}
		else
		{
			node = node->children[0];
			goto loc0;
		}
	}

	if (node->children[1]->contents >= 0)
	{
		node = node->children[1];
		goto loc0;
	}
}


/*
=============
R_PushDlights
=============
*/
void R_PushDlights (mnode_t *headnode)
{
	int		i;
	dlight_t	*l;

	// dlight push was moved to R_DrawWorld so the framecount hack is no longer needed
	l = r_refdef2.dlights;

	for (i = 0; i < r_refdef2.numDlights; i++, l++)
	{
		if (l->die < cl.time || !l->radius)
			continue;

		R_MarkLights (l, i, headnode);
	}
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

mplane_t		*lightplane;
vec3_t			lightspot;
msurface_t		*lightsurf;

int RecursiveLightPoint (vec3_t color, mnode_t *node, vec3_t start, vec3_t end)
{
	float		front, back, frac;
	vec3_t		mid;

loc0:;
	// check for a hit
	if (node->contents < 0) return node->contents;
	
	// calculate mid point
	if (node->plane->type < 3)
	{
		front = start[node->plane->type] - node->plane->dist;
		back = end[node->plane->type] - node->plane->dist;
	}
	else
	{
		front = DotProduct(start, node->plane->normal) - node->plane->dist;
		back = DotProduct(end, node->plane->normal) - node->plane->dist;
	}

	// LordHavoc: optimized recursion
	if ((back < 0) == (front < 0))
	{
		node = node->children[front < 0];
		goto loc0;
	}
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;
	
	// go down front side	
	if (RecursiveLightPoint (color, node->children[front < 0], start, mid) > 0)
		return true;	// hit something
	else
	{
		int i, ds, dt;
		msurface_t *surf;
		
		// check for impact on this node
		VectorCopy (mid, lightspot);
		lightplane = node->plane;

		surf = r_worldmodel->surfaces + node->firstsurface;

		for (i=0 ; i<node->numsurfaces ; i++, surf++)
		{
			if (surf->flags & SURF_DRAWTILED)
				continue;	// no lightmaps

			ds = (int) ((float) DotProduct (mid, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3]);
			dt = (int) ((float) DotProduct (mid, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3]);

			if (ds < surf->texturemins[0] || dt < surf->texturemins[1])
				continue;
		
			ds -= surf->texturemins[0];
			dt -= surf->texturemins[1];
		
			if (ds > surf->extents[0] || dt > surf->extents[1])
				continue;

			if (surf->samples)
			{
				// LordHavoc: enhanced to interpolate lighting
				byte *lightmap;
				int maps, line3, dsfrac = ds & 15, dtfrac = dt & 15, r00 = 0, g00 = 0, b00 = 0, r01 = 0, g01 = 0, b01 = 0, r10 = 0, g10 = 0, b10 = 0, r11 = 0, g11 = 0, b11 = 0;
				float scale;
				line3 = ((surf->extents[0]>>4)+1)*3;

				lightmap = surf->samples + ((dt>>4) * ((surf->extents[0]>>4)+1) + (ds>>4))*3; // LordHavoc: *3 for color

				for (maps = 0; maps < MAX_SURFACE_STYLES && surf->styles[maps] != 255; maps++)
				{
					scale = (float) d_lightstylevalue[surf->styles[maps]] * 1.0 / 256.0;
					r00 += (float) lightmap[      0] * scale;g00 += (float) lightmap[      1] * scale;b00 += (float) lightmap[2] * scale;
					r01 += (float) lightmap[      3] * scale;g01 += (float) lightmap[      4] * scale;b01 += (float) lightmap[5] * scale;
					r10 += (float) lightmap[line3+0] * scale;g10 += (float) lightmap[line3+1] * scale;b10 += (float) lightmap[line3+2] * scale;
					r11 += (float) lightmap[line3+3] * scale;g11 += (float) lightmap[line3+4] * scale;b11 += (float) lightmap[line3+5] * scale;
					lightmap += ((surf->extents[0]>>4)+1) * ((surf->extents[1]>>4)+1)*3; // LordHavoc: *3 for colored lighting
				}

				color[0] += (float) ((int) ((((((((r11-r10) * dsfrac) >> 4) + r10)-((((r01-r00) * dsfrac) >> 4) + r00)) * dtfrac) >> 4) + ((((r01-r00) * dsfrac) >> 4) + r00)));
				color[1] += (float) ((int) ((((((((g11-g10) * dsfrac) >> 4) + g10)-((((g01-g00) * dsfrac) >> 4) + g00)) * dtfrac) >> 4) + ((((g01-g00) * dsfrac) >> 4) + g00)));
				color[2] += (float) ((int) ((((((((b11-b10) * dsfrac) >> 4) + b10)-((((b01-b00) * dsfrac) >> 4) + b00)) * dtfrac) >> 4) + ((((b01-b00) * dsfrac) >> 4) + b00)));
			}

			lightsurf = surf;
			return 1; // success
		}

		// go down back side
		return RecursiveLightPoint (color, node->children[front >= 0], mid, end);
	}
}

/*
=============
R_LightPoint
=============
*/
int R_LightPoint (entity_t *e, float *lightcolor)
{
	vec3_t		dist;
	float		add;
	int			i;
	vec3_t		end;
	
	if (!r_worldmodel->lightdata)
	{
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 255;
		return 255;
	}

	end[0] = e->origin[0];
	end[1] = e->origin[1];
	end[2] = e->origin[2] - 8192;

	lightcolor[0] = lightcolor[1] = lightcolor[2] = 0;

	RecursiveLightPoint (lightcolor, r_worldmodel->nodes, e->origin, end);

	// add dlights
	// mh - dynamic lights should only be added to mdls if r_dynamic is on
	if (r_dynamic.value)
	{
		// variable dlight scale - cool!!!
		float dlscale = (1.0f / 255.0f) * r_dynamic.value;

		for (i = 0; i < r_refdef2.numDlights; i++)
		{
			if (r_refdef2.dlights[i].die >= cl.time)
			{
				VectorSubtract (e->origin, r_refdef2.dlights[i].origin, dist);
				add = r_refdef2.dlights[i].radius - VectorLength (dist);

				if (add > 0)
				{
					lightcolor[0] += (add * r_refdef2.dlights[i].color[0]) * dlscale;
					lightcolor[1] += (add * r_refdef2.dlights[i].color[1]) * dlscale;
					lightcolor[2] += (add * r_refdef2.dlights[i].color[2]) * dlscale;
				}
			}
		}
	}

	return ((lightcolor[0] + lightcolor[1] + lightcolor[2]) * (1.0f / 3.0f));
}
