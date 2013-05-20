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
// r_world.c: world model rendering

#include "gl_local.h"

extern glpoly_t	*lightmap_polys[MAX_LIGHTMAPS];

//==============================================================================
//
// SETUP CHAINS
//
//==============================================================================

/*
===============
R_MarkSurfaces

mark surfaces based on PVS and rebuild texture chains
===============
*/
void R_MarkSurfaces (void)
{
	byte		*vis;
	mleaf_t		*leaf;
	mnode_t		*node;
	msurface_t	*surf, **mark;
	int			i, j;
	byte		solid[MAX_MAP_LEAFS/8];

	// clear lightmap chains
	memset (lightmap_polys, 0, sizeof(lightmap_polys));

	// choose vis data
	if (r_novis.value || r_viewleaf->contents == CONTENTS_SOLID || r_viewleaf->contents == CONTENTS_SKY)
	{
		vis = solid;
		memset (solid, 0xff, (r_worldmodel->numleafs+7)>>3);
	}
	else
	{
		vis = Mod_LeafPVS (r_viewleaf, r_worldmodel);
		if (r_viewleaf2)
		{
			int			i, count;
			unsigned	*src, *dest;

			// merge visibility data for two leafs
			count = (r_worldmodel->numleafs+7)>>3;
			memcpy (solid, vis, count);
			src = (unsigned *) Mod_LeafPVS (r_viewleaf2, r_worldmodel);
			dest = (unsigned *) solid;
			count = (count + 3)>>2;
			for (i=0 ; i<count ; i++)
				*dest++ |= *src++;
			vis = solid;
		}
	}

	// if surface chains don't need regenerating, just add static entities and return
	if (r_oldviewleaf == r_viewleaf && r_oldviewleaf2 == r_viewleaf2)
	{
		leaf = &r_worldmodel->leafs[1];
		for (i=0 ; i<r_worldmodel->numleafs ; i++, leaf++)
			if (vis[i>>3] & (1<<(i&7)))
				if (leaf->efrags)
					R_StoreEfrags (&leaf->efrags);
		return;
	}

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	// iterate through leaves, marking surfaces
	leaf = &r_worldmodel->leafs[1];
	for (i=0 ; i<r_worldmodel->numleafs ; i++, leaf++)
	{
		if (vis[i>>3] & (1<<(i&7)))
		{
			if (leaf->contents != CONTENTS_SKY)
				for (j=0, mark = leaf->firstmarksurface; j<leaf->nummarksurfaces; j++, mark++)
					(*mark)->visframe = r_visframecount;

			// add static models
			if (leaf->efrags)
				R_StoreEfrags (&leaf->efrags);
		}
	}

	// set all chains to null
	for (i=0 ; i<r_worldmodel->numtextures ; i++)
		if (r_worldmodel->textures[i])
 			r_worldmodel->textures[i]->texturechain = NULL;

	// rebuild chains

#if 1
	// iterate through surfaces one node at a time to rebuild chains
	// need to do it this way if we want to work with tyrann's skip removal tool
	// becuase his tool doesn't actually remove the surfaces from the bsp surfaces lump
	// nor does it remove references to them in each leaf's marksurfaces list
	for (i=0, node = r_worldmodel->nodes ; i<r_worldmodel->numnodes ; i++, node++)
	{
		for (j=0, surf=&r_worldmodel->surfaces[node->firstsurface] ; j<node->numsurfaces ; j++, surf++)
		{
			if (surf->visframe == r_visframecount)
			{
				surf->texturechain = surf->texinfo->texture->texturechain;
				surf->texinfo->texture->texturechain = surf;
			}
		}
	}
#else
	// the old way
	surf = &r_worldmodel->surfaces[r_worldmodel->firstmodelsurface];
	for (i=0 ; i<r_worldmodel->nummodelsurfaces ; i++, surf++)
	{
		if (surf->visframe == r_visframecount)
		{
			surf->texturechain = surf->texinfo->texture->texturechain;
			surf->texinfo->texture->texturechain = surf;
		}
	}
#endif
}

/*
================
R_BackFaceCull

returns true if the surface is facing away from vieworg
================
*/
qbool R_BackFaceCull (msurface_t *surf)
{
	double dot;

	switch (surf->plane->type)
	{
	case PLANE_X:
		dot = r_refdef2.vieworg[0] - surf->plane->dist;
		break;
	case PLANE_Y:
		dot = r_refdef2.vieworg[1] - surf->plane->dist;
		break;
	case PLANE_Z:
		dot = r_refdef2.vieworg[2] - surf->plane->dist;
		break;
	default:
		dot = DotProduct (r_refdef2.vieworg, surf->plane->normal) - surf->plane->dist;
		break;
	}

	if ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK))
		return true;

	return false;
}

/*
================
R_CullSurfaces
================
*/
void R_CullSurfaces (void)
{
	msurface_t *s;
	int i;

	if (!r_drawworld.value)
		return;

	s = &r_worldmodel->surfaces[r_worldmodel->firstmodelsurface];
	for (i=0 ; i<r_worldmodel->nummodelsurfaces ; i++, s++)
	{
		if (s->visframe == r_visframecount)
		{
			if (R_CullBox(s->mins, s->maxs) || R_BackFaceCull (s))
			{
				s->culled = true;
			}
			else
			{
				s->culled = false;
				c_brush_polys++; // count wpolys here
				if (s->texinfo->texture->warpimage)
					s->texinfo->texture->update_warp = true;
			}
		}
	}
}

/*
================
R_BuildLightmapChains
================
*/
void R_BuildLightmapChains (void)
{
	msurface_t *s;
	int i;

	// clear lightmap chains
	memset (lightmap_polys, 0, sizeof(lightmap_polys));

	// now rebuild them
	s = &r_worldmodel->surfaces[r_worldmodel->firstmodelsurface];
	for (i=0 ; i<r_worldmodel->nummodelsurfaces ; i++, s++)
	{
		if (s->visframe == r_visframecount && !R_CullBox(s->mins, s->maxs) && !R_BackFaceCull (s))
			R_RenderDynamicLightmaps (s);
	}
}

//==============================================================================
//
// DRAW CHAINS
//
//==============================================================================

/*
================
R_DrawTextureChains_ShowTris
================
*/
void R_DrawTextureChains_ShowTris (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	glpoly_t	*p;

	for (i=0 ; i<r_worldmodel->numtextures ; i++)
	{
		t = r_worldmodel->textures[i];
		if (!t)
			continue;

		if (r_oldwater.value && t->texturechain && (t->texturechain->flags & SURF_DRAWTURB))
		{
			for (s = t->texturechain; s; s = s->texturechain)
			{
				if (!s->culled)
				{
					for (p = s->polys->next; p; p = p->next)
					{
						DrawGLTriangleFan (p);
					}
				}
			}
		}
		else
		{
			for (s = t->texturechain; s; s = s->texturechain)
			{
				if (!s->culled)
				{
					DrawGLTriangleFan (s->polys);
				}
			}
		}
	}
}

/*
================
R_DrawTextureChains_Glow
================
*/
void R_DrawTextureChains_Glow (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	gltexture_t	*glt;
	qbool	bound;

	for (i=0 ; i<r_worldmodel->numtextures ; i++)
	{
		t = r_worldmodel->textures[i];

		if (!t || !t->texturechain || !(glt = R_TextureAnimation(t,0)->fb_texture))
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
R_DrawTextureChains_Multitexture
================
*/
void R_DrawTextureChains_Multitexture (void)
{
	int			i, j;
	msurface_t	*s;
	texture_t	*t;
	float		*v;
	qbool		bound;

	for (i = 0; i < r_worldmodel->numtextures; i++)
	{
		t = r_worldmodel->textures[i];

		if (!t || !t->texturechain || t->texturechain->flags & (SURF_DRAWTILED | SURF_NOTEXTURE))
			continue;

		bound = false;
		for (s = t->texturechain; s; s = s->texturechain)
		{
			if (!s->culled)
			{
				if (!bound) // only bind once we are sure we need this texture
				{
					GL_Bind ((R_TextureAnimation(t,0))->gl_texture->texnum);
					GL_EnableMultitexture(); // selects TEXTURE1
					bound = true;
				}
				R_RenderDynamicLightmaps (s);
				GL_Bind (lightmap_textures[s->lightmaptexturenum]->texnum);
				R_UploadLightmap(s->lightmaptexturenum);
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
================
R_DrawTextureChains_NoTexture

draws surfs whose textures were missing from the BSP
================
*/
void R_DrawTextureChains_NoTexture (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	qbool		bound;

	for (i=0 ; i<r_worldmodel->numtextures ; i++)
	{
		t = r_worldmodel->textures[i];

		if (!t || !t->texturechain || !(t->texturechain->flags & SURF_NOTEXTURE))
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
				DrawGLPoly (s->polys);
				c_brush_passes++;
			}
		}
	}
}

/*
================
R_DrawTextureChains_TextureOnly
================
*/
void R_DrawTextureChains_TextureOnly (void)
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
					GL_Bind ((R_TextureAnimation(t,0))->gl_texture->texnum);
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
R_DrawTextureChains_Water
================
*/
void R_DrawTextureChains_Water (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	glpoly_t	*p;
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

	if (r_oldwater.value)
	{
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
					for (p = s->polys->next; p; p = p->next)
					{
						DrawWaterPoly (p);
						c_brush_passes++;
					}
				}
			}
		}
	}
	else
	{
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
						GL_Bind (t->warpimage->texnum);
						bound = true;
					}
					DrawGLPoly (s->polys);
					c_brush_passes++;
				}
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
R_DrawTextureChains_White

draw sky and water as white polys when r_lightmap is 1
================
*/
void R_DrawTextureChains_White (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;

	qglDisable (GL_TEXTURE_2D);
	for (i=0 ; i<r_worldmodel->numtextures ; i++)
	{
		t = r_worldmodel->textures[i];

		if (!t || !t->texturechain || !(t->texturechain->flags & SURF_DRAWTILED))
			continue;

		for (s = t->texturechain; s; s = s->texturechain)
		{
			if (!s->culled)
			{
				DrawGLPoly (s->polys);
				c_brush_passes++;
			}
		}
	}
	qglEnable (GL_TEXTURE_2D);
}

/*
================
R_DrawLightmapChains
================
*/
void R_DrawLightmapChains (void)
{
	int			i, j;
	glpoly_t	*p;
	float		*v;

	for (i=0 ; i<MAX_LIGHTMAPS ; i++)
	{
		if (!lightmap_polys[i])
			continue;

		GL_Bind (lightmap_textures[i]->texnum);
		R_UploadLightmap(i);
		for (p = lightmap_polys[i]; p; p=p->chain)
		{
			qglBegin (GL_POLYGON);
			v = p->verts[0];
			for (j=0 ; j<p->numverts ; j++, v+= VERTEXSIZE)
			{
				qglTexCoord2f (v[5], v[6]);
				qglVertex3fv (v);
			}
			qglEnd ();

			c_brush_passes++;
		}
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

	if (!r_drawworld.value)
		return;

// r_fullbright
	if (r_fullbright.value)
	{
		R_DrawTextureChains_TextureOnly ();
		goto fullbrights;
	}

// r_lightmap
	if (r_lightmap.value)
	{
		R_BuildLightmapChains ();

		if (!gl_overbright.value)
		{
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			qglColor4f(0.5, 0.5, 0.5, 1);
		}

		R_DrawLightmapChains ();

		if (!gl_overbright.value)
		{
			qglColor4f(1,1,1,1);
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		}

		R_DrawTextureChains_White ();
		return;
	}

// no texture
	R_DrawTextureChains_NoTexture ();

// diffuse * lightmap
	if (gl_overbright.value)
	{
		if (gl_support_texture_combine) // case 1: texture and lightmap in one pass, overbright using texture combiners
		{
			GL_EnableMultitexture ();
			qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
			qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
			qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
			qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, r_lightscale);

			GL_DisableMultitexture ();

			R_DrawTextureChains_Multitexture ();

			GL_EnableMultitexture ();
			qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.0f);
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			
			GL_DisableMultitexture ();
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		}
		else // case 2: texture in one pass, lightmap in second pass using 2x modulation blend func
		{
			R_DrawTextureChains_TextureOnly ();

			qglDepthMask (GL_FALSE);
			qglEnable (GL_BLEND);
			qglBlendFunc (GL_DST_COLOR, GL_SRC_COLOR); // 2x modulate

			R_DrawLightmapChains ();

			qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			qglDisable (GL_BLEND);
			qglDepthMask (GL_TRUE);
		}
	}
	else // case 3: texture and lightmap in one pass, regular modulation
	{
		GL_EnableMultitexture ();
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		GL_DisableMultitexture ();

		R_DrawTextureChains_Multitexture ();

		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	}

fullbrights:
	if (gl_fullbrights.value)
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
