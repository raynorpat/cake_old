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
// gl_rmain.c

#include "gl_local.h"
#include "sound.h"

model_t		*r_worldmodel;
entity_t	r_worldentity;

qbool		r_cache_thrash;		// compatability

vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			c_brush_polys, c_brush_passes, c_alias_polys;

gltexture_t *playertextures[MAX_CLIENTS]; // up to 32 color translated skins

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

float	r_world_matrix[16];
float	r_base_world_matrix[16];

float 	r_fovx, r_fovy; // rendering fov may be different becuase of r_waterwarp and r_stereo

//
// screen size info
//
refdef2_t	r_refdef2;

mleaf_t		*r_viewleaf, *r_oldviewleaf;
mleaf_t		*r_viewleaf2, *r_oldviewleaf2;	// for watervis hack

int		d_lightstylevalue[256];	// 8.8 fraction of base light value

cvar_t	r_norefresh = {"r_norefresh","0"};
cvar_t	r_drawentities = {"r_drawentities","1"};
cvar_t	r_drawworld = {"r_drawworld","1"};
cvar_t	r_speeds = {"r_speeds","0"};
cvar_t	r_fullbright = {"r_fullbright","0"};
cvar_t	r_lightmap = {"r_lightmap","0"};
cvar_t	r_shadows = {"r_shadows","0"};
cvar_t	r_wateralpha = {"r_wateralpha","1"};
cvar_t	r_dynamic = {"r_dynamic","1"};
cvar_t	r_novis = {"r_novis","0"};
cvar_t	r_netgraph = {"r_netgraph","0"};
cvar_t	r_fastsky = {"r_fastsky", "0"};
cvar_t	r_stereo = {"r_stereo","0"};
cvar_t	r_stereodepth = {"r_stereodepth","128"};
cvar_t	r_waterwarp = {"r_waterwarp", "1"};

cvar_t	gl_subdivide_size = {"gl_subdivide_size", "128", CVAR_ARCHIVE};
cvar_t	gl_cull = {"gl_cull","1"};
cvar_t	gl_nocolors = {"gl_nocolors","0"};
cvar_t	gl_finish = {"gl_finish","0"};
cvar_t	gl_fullbrights = {"gl_fullbrights","1"};
cvar_t	gl_overbright = {"gl_overbright","1"};
cvar_t	gl_farclip = {"gl_farclip", "16384", CVAR_ARCHIVE};
void OnChange_lightmode_var (cvar_t *var, char *string, qbool *cancel);
cvar_t	gl_lightmode = {"gl_lightmode","1", 0, OnChange_lightmode_var};

float	lightmode = 2.0f;
void OnChange_lightmode_var (cvar_t *var, char *string, qbool *cancel)
{
	float num = Q_atof(string);
	num = bound(0, num, 4);

	// set lightmode to gl_lightmode
	if(num == 1.0f)
	{
		Cvar_SetValue (&gl_overbright, 1);
		lightmode = 2.0f;
	}
	else if(num == 2.0f)
	{
		Cvar_SetValue (&gl_overbright, 1);
		lightmode = 4.0f;
	}
	else
	{
		// disable overbrights
		Cvar_SetValue (&gl_overbright, 0);
	}

	Cvar_SetValue (var, num);
}

/*
=================
R_CullBox

Returns true if the box is completely outside the frustum
=================
*/
qbool R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;
	mplane_t *p;

	for (i=0,p=frustum ; i<4; i++,p++)
	{
		switch (p->signbits)
		{
		case 0:
			if (p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 1:
			if (p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 2:
			if (p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 3:
			if (p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 4:
			if (p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		case 5:
			if (p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		case 6:
			if (p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		case 7:
			if (p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		default:
			return false;
		}
	}

	return false;
}

/*
===============
R_CullModelForEntity

uses correct bounds based on rotation
===============
*/
qbool R_CullModelForEntity (entity_t *e)
{
	vec3_t mins, maxs;

	if (e->angles[0] || e->angles[2]) // pitch or roll
	{
		VectorAdd (e->origin, e->model->rmins, mins);
		VectorAdd (e->origin, e->model->rmaxs, maxs);
	}
	else if (e->angles[1]) // yaw
	{
		VectorAdd (e->origin, e->model->ymins, mins);
		VectorAdd (e->origin, e->model->ymaxs, maxs);
	}
	else // no rotation
	{
		VectorAdd (e->origin, e->model->mins, mins);
		VectorAdd (e->origin, e->model->maxs, maxs);
	}

	return R_CullBox (mins, maxs);
}

/*
=================
R_CullSphere

Returns true if the sphere is completely outside the frustum
=================
*/
qbool R_CullSphere (vec3_t centre, float radius)
{
	int		i;
	mplane_t *p;

	for (i=0,p=frustum ; i<4; i++,p++)
	{
		if ( DotProduct (centre, p->normal) - p->dist <= -radius )
			return true;
	}

	return false;
}

void R_RotateForEntity (entity_t *e)
{
	qglTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

	qglRotatef (e->angles[1], 0, 0, 1);
	qglRotatef (-e->angles[0], 0, 1, 0);
	qglRotatef (e->angles[2], 1, 0, 0);
}

/*
=============
GL_PolygonOffset

negative offset moves polygon closer to camera
=============
*/
void GL_PolygonOffset (int offset)
{
	if (offset > 0)
	{
		qglEnable (GL_POLYGON_OFFSET_FILL);
		qglEnable (GL_POLYGON_OFFSET_LINE);
		qglPolygonOffset(1, offset);
	}
	else if (offset < 0)
	{
		qglEnable (GL_POLYGON_OFFSET_FILL);
		qglEnable (GL_POLYGON_OFFSET_LINE);
		qglPolygonOffset(-1, offset);
	}
	else
	{
		qglDisable (GL_POLYGON_OFFSET_FILL);
		qglDisable (GL_POLYGON_OFFSET_LINE);
	}
}

//==================================================================================

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities.value)
		return;
	
	for (i = 0; i < cl_numvisedicts; i++)
	{
		currententity = &cl_visedicts[i];

		switch (currententity->model->type)
		{
			case mod_alias:
				R_DrawAliasModel (currententity);
				break;

			case mod_brush:
				R_DrawBrushModel (currententity);
				break;

			case mod_sprite:
				R_DrawSpriteModel (currententity);
				break;

			default:
				break;
		}
	}
}

/*
================
R_DrawShadows
================
*/
void R_DrawShadows (void)
{
	int i;

	if (!r_shadows.value || !r_drawentities.value || r_lightmap.value)
		return;

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = &cl_visedicts[i];

		if (currententity->model->type != mod_alias)
			continue;

		R_DrawAliasShadow (currententity);
	}
}

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

		scale /= 2.0;

		*(int *)color = d_8to24table[p->color];
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
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
	if (!v_blend[3])
		return;

	qglDisable (GL_ALPHA_TEST);
	qglEnable (GL_BLEND);
	qglDisable (GL_TEXTURE_2D);

	qglColor4fv (v_blend);

	qglBegin (GL_QUADS);
	qglVertex2f (r_refdef2.vrect.x, r_refdef2.vrect.y);
	qglVertex2f (r_refdef2.vrect.x + r_refdef2.vrect.width, r_refdef2.vrect.y);
	qglVertex2f (r_refdef2.vrect.x + r_refdef2.vrect.width, r_refdef2.vrect.y + r_refdef2.vrect.height);
	qglVertex2f (r_refdef2.vrect.x, r_refdef2.vrect.y + r_refdef2.vrect.height);
	qglEnd ();

	qglDisable (GL_BLEND);
	qglEnable (GL_TEXTURE_2D);
	qglEnable (GL_ALPHA_TEST);

	qglColor3f (1, 1, 1);
}

//==============================================================================
//
// SETUP FRAME
//
//==============================================================================

static int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test
	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}

/*
===============
TurnVector

turn forward towards side on the plane defined by forward and side
if angle = 90, the result will be equal to side
assumes side and forward are perpendicular, and normalized
to turn away from side, use a negative angle
===============
*/
#define DEG2RAD( a ) ( (a) * M_PI/180 )
void TurnVector (vec3_t out, const vec3_t forward, const vec3_t side, float angle)
{
	float scale_forward, scale_side;

	scale_forward = cos( DEG2RAD( angle ) );
	scale_side = sin( DEG2RAD( angle ) );

	out[0] = scale_forward*forward[0] + scale_side*side[0];
	out[1] = scale_forward*forward[1] + scale_side*side[1];
	out[2] = scale_forward*forward[2] + scale_side*side[2];
}

/*
===============
R_SetFrustum
===============
*/
void R_SetFrustum (float fovx, float fovy)
{
	int		i;

	if (r_stereo.value)
		fovx += 10; // HACK: so polygons don't drop out becuase of stereo skew

	TurnVector(frustum[0].normal, vpn, vright, fovx / 2 - 90); // left plane
	TurnVector(frustum[1].normal, vpn, vright, 90 - fovx / 2); // right plane
	TurnVector(frustum[2].normal, vpn, vup, 90 - fovy / 2); // bottom plane
	TurnVector(frustum[3].normal, vpn, vup, fovy / 2 - 90); // top plane

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal); // FIXME: shouldn't this always be zero?
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}

/*
=============
GL_SetFrustum
=============
*/
#define NEARCLIP 4
float frustum_skew = 0.0; // used by r_stereo
void GL_SetFrustum(float fovx, float fovy)
{
	float xmax, ymax;
	xmax = NEARCLIP * tan( fovx * M_PI / 360.0 );
	ymax = NEARCLIP * tan( fovy * M_PI / 360.0 );
	qglFrustum(-xmax + frustum_skew, xmax + frustum_skew, -ymax, ymax, NEARCLIP, gl_farclip.value);
}

/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	qglViewport (r_refdef2.vrect.x,
				vid.height - r_refdef2.vrect.y - r_refdef2.vrect.height,
				r_refdef2.vrect.width,
				r_refdef2.vrect.height);

	GL_SetFrustum (r_fovx, r_fovy);

	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity ();

	qglRotatef (-90,  1, 0, 0);		// put Z going up
	qglRotatef (90,	0, 0, 1);		// put Z going up
	qglRotatef (-r_refdef2.viewangles[2],  1, 0, 0);
	qglRotatef (-r_refdef2.viewangles[0],  0, 1, 0);
	qglRotatef (-r_refdef2.viewangles[1],  0, 0, 1);
	qglTranslatef (-r_refdef2.vieworg[0], -r_refdef2.vieworg[1], -r_refdef2.vieworg[2]);

	//qglGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	//
	// set drawing parms
	//
	if (gl_cull.value)
		qglEnable(GL_CULL_FACE);
	else
		qglDisable(GL_CULL_FACE);

	qglDisable(GL_BLEND);
	qglDisable(GL_ALPHA_TEST);
	qglEnable(GL_DEPTH_TEST);
}

void gl_main_start(void)
{
}

void gl_main_shutdown(void)
{
}

void gl_main_newmap(void)
{
	r_framecount = 1; // no dlightcache
}

void GL_Main_Init(void)
{
	Cmd_AddCommand ("screenshot", R_ScreenShot_f);
	Cmd_AddCommand ("loadsky", R_LoadSky_f);

	Cvar_Register (&r_norefresh);
	Cvar_Register (&r_lightmap);
	Cvar_Register (&r_fullbright);
	Cvar_Register (&r_drawentities);
	Cvar_Register (&r_drawworld);
	Cvar_Register (&r_shadows);
	Cvar_Register (&r_wateralpha);
	Cvar_Register (&r_dynamic);
	Cvar_Register (&r_novis);
	Cvar_Register (&r_speeds);
	Cvar_Register (&r_netgraph);
	Cvar_Register (&r_fastsky);
	Cvar_Register (&r_stereo);
	Cvar_Register (&r_stereodepth);
	Cvar_Register (&r_waterwarp);

	Cvar_Register (&gl_subdivide_size);
	Cvar_Register (&gl_cull);
	Cvar_Register (&gl_nocolors);
	Cvar_Register (&gl_finish);
	Cvar_Register (&gl_fullbrights);
	Cvar_Register (&gl_overbright);
	Cvar_Register (&gl_farclip);
	Cvar_Register (&gl_lightmode);

	R_RegisterModule("GL_Main", gl_main_start, gl_main_shutdown, gl_main_newmap);
}

extern void R_Draw_Init (void);

/*
===============
R_Init
===============
*/
void R_Init (void)
{
	RB_InitBackend ();
	GL_Main_Init ();
	TexMgr_Init ();
	R_Draw_Init ();
	Mod_Init ();
}


/*
===============
R_MarkSurfaces

mark surfaces based on PVS and rebuild texture chains
===============
*/
extern glpoly_t	*lightmap_polys[64];
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
			}
		}
	}
}

/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	qglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

/*
===============
R_SetupScene

this is the stuff that needs to be done once per eye in stereo mode
===============
*/
void R_SetupScene (void)
{
	R_PushDlights ();
	R_AnimateLight ();
	r_framecount++;
	R_SetupGL ();
}

/*
===============
R_SetupView

this is the stuff that needs to be done once per frame, even in stereo mode
===============
*/
void R_SetupView (void)
{
	vec3_t	testorigin;
	mleaf_t	*leaf;
	extern float	wateralpha;

	// use wateralpha only if the server allows
	wateralpha = r_refdef2.watervis ? r_wateralpha.value : 1;

	// build the transformation matrix for the given view angles
	VectorCopy (r_refdef2.vieworg, r_origin);
	AngleVectors (r_refdef2.viewangles, vpn, vright, vup);

	// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_oldviewleaf2 = r_viewleaf2;
	r_viewleaf = Mod_PointInLeaf (r_origin, r_worldmodel);
	r_viewleaf2 = NULL;

	// check above and below so crossing solid water doesn't draw wrong
	if (r_viewleaf->contents <= CONTENTS_WATER && r_viewleaf->contents >= CONTENTS_LAVA)
	{
		// look up a bit
		VectorCopy (r_origin, testorigin);
		testorigin[2] += 10;
		leaf = Mod_PointInLeaf (testorigin, r_worldmodel);
		if (leaf->contents == CONTENTS_EMPTY)
			r_viewleaf2 = leaf;
	}
	else if (r_viewleaf->contents == CONTENTS_EMPTY)
	{
		// look down a bit
		VectorCopy (r_origin, testorigin);
		testorigin[2] -= 10;
		leaf = Mod_PointInLeaf (testorigin, r_worldmodel);
		if (leaf->contents <= CONTENTS_WATER &&	leaf->contents >= CONTENTS_LAVA)
			r_viewleaf2 = leaf;
	}

	V_SetContentsColor (r_viewleaf->contents);
	V_PrepBlend ();

	r_cache_thrash = false;

	// calculate r_fovx and r_fovy here
	r_fovx = r_refdef2.fov_x;
	r_fovy = r_refdef2.fov_y;
	if (r_waterwarp.value)
	{
		int contents = Mod_PointInLeaf (r_origin, r_worldmodel)->contents;
		if (contents == CONTENTS_WATER || contents == CONTENTS_SLIME || contents == CONTENTS_LAVA)
		{
			// variance is a percentage of width, where width = 2 * tan(fov / 2) otherwise the effect is too dramatic at high FOV and too subtle at low FOV.  what a mess!
			r_fovx = atan(tan(DEG2RAD(r_refdef2.fov_x) / 2) * (0.97 + sin(cl.time * 1.5) * 0.03)) * 2 / (M_PI/180);
			r_fovy = atan(tan(DEG2RAD(r_refdef2.fov_y) / 2) * (1.03 - sin(cl.time * 1.5) * 0.03)) * 2 / (M_PI/180);
		}
	}

	R_SetFrustum (r_fovx, r_fovy);

	R_MarkSurfaces (); // create texture chains from PVS
	R_CullSurfaces (); // do after R_SetFrustum and R_MarkSurfaces

	R_Clear ();
}

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene (void)
{
	R_SetupScene (); 	// this does everything that should be done once per call to RenderScene

	R_DrawSky ();

	R_DrawWorld ();

	S_ExtraUpdate ();	// don't let sound get messed up if going slow

	R_DrawShadows ();	// render entity shadows

	R_DrawEntitiesOnList ();

	R_DrawTextureChains_Water (); // drawn here since they might have transparency

	R_DrawParticles ();
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	if (r_norefresh.value)
		return;

	if (!r_worldentity.model || !r_worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	RB_StartFrame ();

	R_SetupView (); // this does everything that should be done once per frame

	// stereo rendering -- full of hacky goodness
	if (r_stereo.value)
	{
		float eyesep = clamp(-8.0f, r_stereo.value, 8.0f);
		float fdepth = clamp(32.0f, r_stereodepth.value, 1024.0f);

		AngleVectors (r_refdef2.viewangles, vpn, vright, vup);

		// render left eye (red)
		qglColorMask(1, 0, 0, 1);
		VectorMA (r_refdef2.vieworg, -0.5f * eyesep, vright, r_refdef2.vieworg);
		frustum_skew = 0.5 * eyesep * NEARCLIP / fdepth;
		srand((int) (cl.time * 1000)); // sync random stuff between eyes

		R_RenderScene ();

		// render right eye (cyan)
		qglClear (GL_DEPTH_BUFFER_BIT);
		qglColorMask(0, 1, 1, 1);
		VectorMA (r_refdef2.vieworg, 1.0f * eyesep, vright, r_refdef2.vieworg);
		frustum_skew = -frustum_skew;
		srand((int) (cl.time * 1000)); // sync random stuff between eyes

		R_RenderScene ();

		// restore
		qglColorMask(1, 1, 1, 1);
		VectorMA (r_refdef2.vieworg, -0.5f * eyesep, vright, r_refdef2.vieworg);
		frustum_skew = 0.0f;
	}
	else
	{
		// render normal view
		R_RenderScene ();
	}

	RB_EndFrame ();
}
