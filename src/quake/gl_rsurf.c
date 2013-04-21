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
// r_surf.c: surface-related refresh code

#include "gl_local.h"

float	wateralpha;		// 1 if watervis is disabled by server, otherwise equal to r_wateralpha.value

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128

#define	MAX_LIGHTMAPS	64

unsigned int	blocklights[BLOCK_WIDTH*BLOCK_HEIGHT*3];

typedef struct glRect_s {
	unsigned char l,t,w,h;
} glRect_t;

glpoly_t	*lightmap_polys[MAX_LIGHTMAPS];
qbool		lightmap_modified[MAX_LIGHTMAPS];
glRect_t	lightmap_rectchange[MAX_LIGHTMAPS];

int			lightmap_textures[MAX_LIGHTMAPS];

int			allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte		lightmaps[4*MAX_LIGHTMAPS*BLOCK_WIDTH*BLOCK_HEIGHT];

void R_BuildLightMap (msurface_t *surf, byte *dest, int stride);

//=============================================================
// Dynamic lights

typedef struct dlightinfo_s {
	int local[2];
	int rad;
	int minlight;	// rad - minlight
	int type;
} dlightinfo_t;

static dlightinfo_t dlightlist[MAX_DLIGHTS];
static int	numdlights;

/*
===============
R_BuildDlightList
===============
*/
void R_BuildDlightList (msurface_t *surf)
{
	int			lnum;
	float		dist;
	vec3_t		impact;
	int			i;
	int			smax, tmax;
	mtexinfo_t	*tex;
	int			irad, iminlight;
	int			local[2];
	int			tdmin, sdmin, distmin;
	dlightinfo_t	*light;

	numdlights = 0;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	for (lnum=0 ; lnum<r_refdef2.numDlights ; lnum++)
	{
		if ( !(surf->dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

		dist = DotProduct (r_refdef2.dlights[lnum].origin, surf->plane->normal) -
				surf->plane->dist;
		irad = (r_refdef2.dlights[lnum].radius - fabs(dist)) * 256;
		iminlight = r_refdef2.dlights[lnum].minlight * 256;
		if (irad < iminlight)
			continue;

		iminlight = irad - iminlight;
		
		for (i=0 ; i<3 ; i++) {
			impact[i] = r_refdef2.dlights[lnum].origin[i] -
				surf->plane->normal[i]*dist;
		}
		
		local[0] = DotProduct (impact, tex->vecs[0]) +
			tex->vecs[0][3] - surf->texturemins[0];
		local[1] = DotProduct (impact, tex->vecs[1]) +
			tex->vecs[1][3] - surf->texturemins[1];
		
		// check if this dlight will touch the surface
		if (local[1] > 0) {
			tdmin = local[1] - (tmax<<4);
			if (tdmin < 0)
				tdmin = 0;
		} else
			tdmin = -local[1];

		if (local[0] > 0) {
			sdmin = local[0] - (smax<<4);
			if (sdmin < 0)
				sdmin = 0;
		} else
			sdmin = -local[0];

		if (sdmin > tdmin)
			distmin = (sdmin<<8) + (tdmin<<7);
		else
			distmin = (tdmin<<8) + (sdmin<<7);

		if (distmin < iminlight)
		{
			// save dlight info
			light = &dlightlist[numdlights];
			light->minlight = iminlight;
			light->rad = irad;
			light->local[0] = local[0];
			light->local[1] = local[1];
			light->type = r_refdef2.dlights[lnum].type;
			numdlights++;
		}
	}
}

int dlightcolor[NUM_DLIGHTTYPES][3] = {
	{ 100, 90, 80 },	// dimlight or brightlight
	{ 0, 0, 128 },		// blue
	{ 128, 0, 0 },		// red
	{ 128, 0, 128 },	// red + blue
	{ 100, 50, 10 },		// muzzleflash
	{ 100, 50, 10 },		// explosion
	{ 90, 60, 7 }		// rocket
};

/*
===============
R_AddDynamicLights

NOTE: R_BuildDlightList must be called first!
===============
*/
void R_AddDynamicLights (msurface_t *surf)
{
	int			i;
	int			smax, tmax;
	int			s, t;
	int			sd, td;
	int			_sd, _td;
	int			irad, idist, iminlight;
	dlightinfo_t	*light;
	unsigned	*dest;
	int			color[3];
	int			tmp;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	for (i=0,light=dlightlist ; i<numdlights ; i++,light++)
	{
		color[0] = dlightcolor[light->type][0];
		color[1] = dlightcolor[light->type][1];
		color[2] = dlightcolor[light->type][2];

		irad = light->rad;
		iminlight = light->minlight;

		_td = light->local[1];
		dest = blocklights;
		for (t = 0 ; t<tmax ; t++)
		{
			td = _td;
			if (td < 0)	td = -td;
			_td -= 16;
			_sd = light->local[0];
			for (s=0 ; s<smax ; s++) {
				sd = _sd < 0 ? -_sd : _sd;
				_sd -= 16;
				if (sd > td)
					idist = (sd<<8) + (td<<7);
				else
					idist = (td<<8) + (sd<<7);
				if (idist < iminlight) {
					tmp = (irad - idist) >> 7;
					dest[0] += tmp * color[0];
					dest[1] += tmp * color[1];
					dest[2] += tmp * color[2];
				}
				dest += 3;
			}
		}
	}
}

/*
================
R_RenderDynamicLightmaps
================
*/
void R_RenderDynamicLightmaps (msurface_t *fa)
{
	byte		*base;
	int			maps;
	glRect_t    *theRect;
	int			smax, tmax;
	qbool		lightstyle_modified = false;

	if (fa->flags & SURF_UNLIT) // not a lightmapped surface
		return;

	// add to lightmap chain
	fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
	lightmap_polys[fa->lightmaptexturenum] = fa->polys;

	// check for lightmap modification
	for (maps=0 ; maps<MAXLIGHTMAPS && fa->styles[maps] != 255 ; maps++)
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps]) {
			lightstyle_modified = true;
			break;
		}

	if (fa->dlightframe == r_framecount)
		R_BuildDlightList (fa);
	else
		numdlights = 0;

	if (numdlights == 0 && !fa->cached_dlight && !lightstyle_modified)
		return;
	
	if (r_dynamic.value)
	{
		lightmap_modified[fa->lightmaptexturenum] = true;
		theRect = &lightmap_rectchange[fa->lightmaptexturenum];
		if (fa->light_t < theRect->t) {
			if (theRect->h)
				theRect->h += theRect->t - fa->light_t;
			theRect->t = fa->light_t;
		}
		if (fa->light_s < theRect->l) {
			if (theRect->w)
				theRect->w += theRect->l - fa->light_s;
			theRect->l = fa->light_s;
		}
		smax = (fa->extents[0]>>4)+1;
		tmax = (fa->extents[1]>>4)+1;
		if ((theRect->w + theRect->l) < (fa->light_s + smax))
			theRect->w = (fa->light_s-theRect->l)+smax;
		if ((theRect->h + theRect->t) < (fa->light_t + tmax))
			theRect->h = (fa->light_t-theRect->t)+tmax;
		base = lightmaps + fa->lightmaptexturenum*4*BLOCK_WIDTH*BLOCK_HEIGHT;
		base += fa->light_t * BLOCK_WIDTH * 4 + fa->light_s * 4;
		R_BuildLightMap (fa, base, BLOCK_WIDTH*4);
	}
}


/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void R_BuildLightMap (msurface_t *surf, byte *dest, int stride)
{
	int			smax, tmax;
	int			t;
	int			i, j, size;
	byte		*lightmap;
	unsigned	scale;
	int			maps;
	unsigned	*bl;

	surf->cached_dlight = (surf->dlightframe == r_framecount);

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;
	lightmap = surf->samples;

	if (r_worldmodel->lightdata)
	{
		// clear to no light
		memset (&blocklights[0], 0, size * 3 * sizeof (unsigned int));

		// add all the lightmaps
		if (lightmap)
		{
			for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ; maps++)
			{
				scale = d_lightstylevalue[surf->styles[maps]];
				surf->cached_light[maps] = scale;	// 8.8 fraction

				bl = blocklights;
				for (i=0 ; i<size ; i++)
				{
					*bl++ += *lightmap++ * scale;
					*bl++ += *lightmap++ * scale;
					*bl++ += *lightmap++ * scale;
				}
			}
		}

		// add all the dynamic lights
		if (surf->dlightframe == r_framecount)
			R_AddDynamicLights (surf);
	}
	else
	{
		// set to full bright if no light data
		memset (&blocklights[0], 255, size * 3 * sizeof (unsigned int));
	}

// bound, invert, and shift
	stride -= smax * 4;
	bl = blocklights;
	for (i=0 ; i<tmax ; i++, dest += stride)
	{
		for (j=0 ; j<smax ; j++, dest += 4)
		{
/*
			if (gl_overbright.value)
			{
				t = *bl++ >> 8;if (t > 255) t = 255;dest[2] = t;
				t = *bl++ >> 8;if (t > 255) t = 255;dest[1] = t;
				t = *bl++ >> 8;if (t > 255) t = 255;dest[0] = t;
			}
			else
*/
			{
				t = *bl++ >> 7;if (t > 255) t = 255;dest[2] = t;
				t = *bl++ >> 7;if (t > 255) t = 255;dest[1] = t;
				t = *bl++ >> 7;if (t > 255) t = 255;dest[0] = t;
			}
			dest[3] = 255;
		}
	}
}

/*
===============
R_UploadLightMap

assumes lightmap texture is already bound
===============
*/
void R_UploadLightMap (int lmap)
{
	glRect_t	*theRect;

	if (!lightmap_modified[lmap])
		return;

	lightmap_modified[lmap] = false;

	theRect = &lightmap_rectchange[lmap];
	qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, BLOCK_WIDTH, theRect->h, GL_BGRA,
		  GL_UNSIGNED_INT_8_8_8_8_REV, lightmaps+(lmap* BLOCK_HEIGHT + theRect->t) *BLOCK_WIDTH*4);
	theRect->l = BLOCK_WIDTH;
	theRect->t = BLOCK_HEIGHT;
	theRect->h = 0;
	theRect->w = 0;

	//rs_dynamiclightmaps++;
}

void R_UploadLightmaps (void)
{
	int lmap;
	glRect_t	*theRect;

	for (lmap = 0; lmap < MAX_LIGHTMAPS; lmap++)
	{
		if (!lightmap_modified[lmap])
			continue;

		GL_Bind (lightmap_textures[lmap]);
		lightmap_modified[lmap] = false;

		theRect = &lightmap_rectchange[lmap];
		qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, BLOCK_WIDTH, theRect->h, GL_BGRA,
			  GL_UNSIGNED_INT_8_8_8_8_REV, lightmaps+(lmap* BLOCK_HEIGHT + theRect->t) *BLOCK_WIDTH*4);
		theRect->l = BLOCK_WIDTH;
		theRect->t = BLOCK_HEIGHT;
		theRect->h = 0;
		theRect->w = 0;

		//rs_dynamiclightmaps++;
	}
}

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation (texture_t *base)
{
	int		relative;
	int		count;

	if (currententity->frame)
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}
	
	if (!base->anim_total)
		return base;

	relative = (int)(r_refdef2.time*10) % base->anim_total;

	count = 0;	
	while (base->anim_min > relative || base->anim_max <= relative)
	{
		base = base->anim_next;
		if (!base)
			Host_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Host_Error ("R_TextureAnimation: infinite cycle");
	}

	return base;
}

/*
================
DrawGLPoly
================
*/
void DrawGLPoly (glpoly_t *p)
{
	int		i;
	float	*v;

	qglBegin (GL_POLYGON);
	v = p->verts[0];
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
	{
		qglTexCoord2f (v[3], v[4]);
		qglVertex3fv (v);
	}
	qglEnd ();
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

/*
================
R_DrawTextureChains_TextureOnly
================
*/
static void R_DrawTextureChains_TextureOnly (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	qbool	bound;

	for (i=0 ; i<r_worldmodel->numtextures ; i++)
	{
		t = r_worldmodel->textures[i];

		if (!t || !t->texturechain || t->texturechain->flags & (SURF_DRAWTURB | SURF_DRAWSKY))
			continue;

		bound = false;

		for (s = t->texturechain; s; s = s->texturechain)
		{
			if (!s->culled)
			{
				if (!bound) // only bind once we are sure we need this texture
				{
					GL_Bind ((R_TextureAnimation(t))->gl_texture->texnum);
					bound = true;
				}
				R_RenderDynamicLightmaps (s); // adds to lightmap chain

				DrawGLPoly (s->polys);

				c_brush_passes++;
			}
		}
	}
}

/*
================
R_DrawTextureChains_Glow
================
*/
static void R_DrawTextureChains_Glow (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	gltexture_t	*glt;
	qbool	bound;

	for (i=0 ; i<r_worldmodel->numtextures ; i++)
	{
		t = r_worldmodel->textures[i];

		if (!t || !t->texturechain || !(glt = R_TextureAnimation(t)->fb_texture))
			continue;

		bound = false;

		for (s = t->texturechain; s; s = s->texturechain)
		{
			if (!s->culled)
			{
				if (!bound) // only bind once we are sure we need this texture
				{
					GL_Bind (glt->texnum);
					bound = true;
				}

				DrawGLPoly (s->polys);

				c_brush_passes++;
			}
		}
	}
}

/*
================
R_DrawTextureChains_Water
================
*/
void R_DrawTextureChains_Water (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	qbool		bound;

	if (r_lightmap.value || !r_drawworld.value)
		return;

	if (r_wateralpha.value < 1.0)
	{
		qglDepthMask(GL_FALSE);
		qglEnable (GL_BLEND);
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglColor4f (1,1,1,r_wateralpha.value);
	}

	for (i=0 ; i<r_worldmodel->numtextures ; i++)
	{
		t = r_worldmodel->textures[i];
		if (!t || !t->texturechain || !(t->texturechain->flags & SURF_DRAWTURB))
			continue;
		bound = false;
		for (s = t->texturechain; s; s = s->texturechain)
		{
			if (!s->culled)
			{
				if (!bound) // only bind once we are sure we need this texture
				{
					GL_Bind (t->gl_texture->texnum);
					bound = true;
				}
				EmitWaterPolys (s);

				c_brush_passes++;
			}
		}
	}

	if (r_wateralpha.value < 1.0)
	{
		qglDepthMask(GL_TRUE);
		qglDisable (GL_BLEND);
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		qglColor3f (1,1,1);
	}
}

/*
================
R_DrawTextureChains_Multitexture
================
*/
static void R_DrawTextureChains_Multitexture (void)
{
	int			i, j;
	float		*v;
	msurface_t	*s;
	texture_t	*t;
	qbool		bound;

	for (i = 0; i < r_worldmodel->numtextures; i++)
	{
		t = r_worldmodel->textures[i];

		if (!t || !t->texturechain || t->texturechain->flags & (SURF_UNLIT))
			continue;

		bound = false;
		for (s = t->texturechain; s; s = s->texturechain)
		{
			if (!s->culled)
			{
				if (!bound) // only bind once we are sure we need this texture
				{
					GL_Bind ((R_TextureAnimation(t))->gl_texture->texnum);
					GL_EnableMultitexture(); // selects TEXTURE1
					bound = true;
				}
				R_RenderDynamicLightmaps (s);
				GL_Bind (lightmap_textures[s->lightmaptexturenum]);

				qglBegin(GL_POLYGON);
				v = s->polys->verts[0];
				for (j=0 ; j<s->polys->numverts ; j++, v+= VERTEXSIZE)
				{
					qglMultiTexCoord2f (GL_TEXTURE0_ARB, v[3], v[4]);
					qglMultiTexCoord2f (GL_TEXTURE1_ARB, v[5], v[6]);
					qglVertex3fv (v);
				}
				qglEnd ();

				c_brush_passes++;
			}
		}

		GL_DisableMultitexture(); // selects TEXTURE0
	}
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld (void)
{
	entity_t	ent;

	memset (&ent, 0, sizeof(ent));
	ent.model = r_worldmodel;

	VectorCopy (r_refdef2.vieworg, modelorg);

	currententity = &ent;

	R_UploadLightmaps ();

	if (!r_drawworld.value)
		return;

	if (r_fullbright.value)
	{
		R_DrawTextureChains_TextureOnly ();
		goto fullbrights;
	}

	GL_EnableMultitexture ();
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	GL_DisableMultitexture ();

	R_DrawTextureChains_Multitexture ();

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

fullbrights:
	if (gl_fb_bmodels.value)
	{
		qglDepthMask (GL_FALSE);
		qglEnable (GL_BLEND);
		qglBlendFunc (GL_ONE, GL_ONE);

		R_DrawTextureChains_Glow ();

		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglDisable (GL_BLEND);
		qglDepthMask (GL_TRUE);
	}
}


/*
=============================================================

	BRUSH MODEL

=============================================================
*/

/*
================
R_DrawSequentialPoly
================
*/
void R_DrawSequentialPoly (msurface_t *s)
{
	texture_t	*t;
	float		*v;
	int			i;

	t = R_TextureAnimation (s->texinfo->texture);

// fullbright
	if ((r_fullbright.value) && !(s->flags & SURF_UNLIT))
	{
		GL_Bind (t->gl_texture->texnum);
		DrawGLPoly (s->polys);
		c_brush_passes++;
		goto fullbrights;
	}

// sky poly - skip it, already handled in gl_sky.c
	if (s->flags & SURF_DRAWSKY)
		return;

// water poly
	if (s->flags & SURF_DRAWTURB)
	{
		if (r_wateralpha.value < 1)
		{
			qglDepthMask(GL_FALSE);
			qglEnable(GL_BLEND);
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			qglColor4f(1, 1, 1, r_wateralpha.value);
		}

		GL_Bind (s->texinfo->texture->gl_texture->texnum);
		EmitWaterPolys (s);
		c_brush_passes++;

		if (r_wateralpha.value < 1)
		{
			qglDepthMask(GL_TRUE);
			qglDisable(GL_BLEND);
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			qglColor3f(1, 1, 1);
		}
		return;
	}

// lightmapped poly
	qglColor3f(1, 1, 1);

	GL_DisableMultitexture(); // selects TEXTURE0
	GL_Bind (t->gl_texture->texnum);
	
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	GL_EnableMultitexture(); // selects TEXTURE1
	GL_Bind (lightmap_textures[s->lightmaptexturenum]);
	R_RenderDynamicLightmaps (s);
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	
	qglBegin(GL_POLYGON);
	v = s->polys->verts[0];
	for (i=0 ; i<s->polys->numverts ; i++, v+= VERTEXSIZE)
	{
		qglMultiTexCoord2f (GL_TEXTURE0_ARB, v[3], v[4]);
		qglMultiTexCoord2f (GL_TEXTURE1_ARB, v[5], v[6]);
		qglVertex3fv (v);
	}
	qglEnd ();
	
	GL_DisableMultitexture ();
	c_brush_passes++;

// fullbrights
fullbrights:
	if (gl_fb_bmodels.value && t->fb_texture)
	{
		qglDepthMask (GL_FALSE);
		qglEnable (GL_BLEND);
		qglBlendFunc (GL_ONE, GL_ONE);
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglColor3f (1, 1, 1);

		GL_Bind (t->fb_texture->texnum);

		DrawGLPoly (s->polys);

		qglColor3f(1, 1, 1);
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglDisable (GL_BLEND);
		qglDepthMask (GL_TRUE);

		c_brush_passes++;
	}
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel (entity_t *e)
{
	int			i;
	int			k;
	msurface_t	*psurf;
	float		dot;
	mplane_t	*pplane;
	model_t		*clmodel;

	if (R_CullModelForEntity(e))
		return;

	currententity = e;
	clmodel = e->model;

	VectorSubtract (r_refdef2.vieworg, e->origin, modelorg);
	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

// calculate dynamic lighting for bmodel if it's not an instanced model
	if (clmodel->firstmodelsurface != 0)
	{
		for (k = 0; k < r_refdef2.numDlights; k++)
		{
			R_MarkLights (&r_refdef2.dlights[k], 1<<k, clmodel->nodes + clmodel->firstnode);
		}
	}

    qglPushMatrix ();

	qglTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);
	qglRotatef (e->angles[1], 0, 0, 1);
	qglRotatef (e->angles[0], 0, 1, 0);
	qglRotatef (e->angles[2], 1, 0, 0);
	
	// draw surface
	for (i = 0; i < clmodel->nummodelsurfaces; i++, psurf++)
	{
		// find which side of the node we are on
		pplane = psurf->plane;
		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			R_DrawSequentialPoly (psurf);
			c_brush_polys++;
		}
	}

	GL_DisableMultitexture(); // selects TEXTURE0
	
	qglPopMatrix ();
}


/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

// returns a texture number and the position inside it
int AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	for (texnum=0 ; texnum<MAX_LIGHTMAPS ; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i=0 ; i<BLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (allocated[texnum][i+j] >= best)
					break;
				if (allocated[texnum][i+j] > best2)
					best2 = allocated[texnum][i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i=0 ; i<w ; i++)
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("AllocBlock: full");
	return 0;
}


mvertex_t	*r_pcurrentvertbase;
model_t		*currentmodel;

/*
================
BuildSurfaceDisplayList
================
*/
void BuildSurfaceDisplayList (msurface_t *fa)
{
	int			i, lindex, lnumverts;
	medge_t		*pedges, *r_pedge;
	int			vertpage;
	float		*vec;
	float		s, t;
	glpoly_t	*poly;

	// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	// draw texture
	poly = Hunk_Alloc (sizeof(glpoly_t) + (lnumverts-4) * VERTEXSIZE*sizeof(float));
	poly->next = fa->polys;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		// lightmap texture coordinates
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s*16;
		s += 8;
		s /= BLOCK_WIDTH*16;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t*16;
		t += 8;
		t /= BLOCK_HEIGHT*16;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	poly->numverts = lnumverts;
}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap (msurface_t *surf)
{
	int		smax, tmax;
	byte	*base;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	surf->lightmaptexturenum = AllocBlock (smax, tmax, &surf->light_s, &surf->light_t);
	base = lightmaps + surf->lightmaptexturenum*4*BLOCK_WIDTH*BLOCK_HEIGHT;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * 4;
	numdlights = 0;
	R_BuildLightMap (surf, base, BLOCK_WIDTH*4);
}


/*
==================
GL_BuildLightmaps

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void GL_BuildLightmaps (void)
{
	int		i, j;
	model_t	*m;

	memset (allocated, 0, sizeof(allocated));

	qglDeleteTextures(MAX_LIGHTMAPS, lightmap_textures);
	for (i=0; i < MAX_LIGHTMAPS; i++)
		lightmap_textures[i] = NULL;
	qglGenTextures(MAX_LIGHTMAPS, lightmap_textures); 

	for (j=1 ; j<MAX_MODELS ; j++)
	{
		m = cl.model_precache[j];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;
		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		for (i=0 ; i<m->numsurfaces ; i++)
		{
			if ( m->surfaces[i].flags & SURF_UNLIT )
				continue;
			GL_CreateSurfaceLightmap (m->surfaces + i);
			BuildSurfaceDisplayList (m->surfaces + i);
		}
	}

	// upload all lightmaps that were filled
	for (i=0 ; i<MAX_LIGHTMAPS ; i++)
	{
		if (!allocated[i][0])
			break;		// no more used
		lightmap_modified[i] = false;
		lightmap_rectchange[i].l = BLOCK_WIDTH;
		lightmap_rectchange[i].t = BLOCK_HEIGHT;
		lightmap_rectchange[i].w = 0;
		lightmap_rectchange[i].h = 0;

		GL_Bind(lightmap_textures[i]);
		qglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, lightmaps+i*BLOCK_WIDTH*BLOCK_HEIGHT*4);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
}
