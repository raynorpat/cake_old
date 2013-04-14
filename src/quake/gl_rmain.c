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

int			c_brush_polys, c_alias_polys;

gltexture_t *playertextures[MAX_CLIENTS]; // up to 32 color translated skins
gltexture_t *playerfbtextures[MAX_CLIENTS];

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

//
// screen size info
//
refdef2_t	r_refdef2;

mleaf_t		*r_viewleaf, *r_oldviewleaf;
mleaf_t		*r_viewleaf2, *r_oldviewleaf2;	// for watervis hack

texture_t	*r_notexture_mip;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value

cvar_t	r_norefresh = {"r_norefresh","0"};
cvar_t	r_drawentities = {"r_drawentities","1"};
cvar_t	r_drawflame = {"r_drawflame","1"};
cvar_t	r_speeds = {"r_speeds","0"};
cvar_t	r_fullbright = {"r_fullbright","0"};
cvar_t	r_lightmap = {"r_lightmap","0"};
cvar_t	r_shadows = {"r_shadows","0"};
cvar_t	r_wateralpha = {"r_wateralpha","1"};
cvar_t	r_dynamic = {"r_dynamic","1"};
cvar_t	r_novis = {"r_novis","0"};
cvar_t	r_netgraph = {"r_netgraph","0"};
cvar_t	r_fullbrightSkins = {"r_fullbrightSkins", "0"};
cvar_t	r_fastsky = {"r_fastsky", "0"};
cvar_t	r_skycolor = {"r_skycolor", "4"};
cvar_t	r_fastturb = {"r_fastturb", "0"};

cvar_t	gl_subdivide_size = {"gl_subdivide_size", "64", CVAR_ARCHIVE};
cvar_t	gl_clear = {"gl_clear","0"};
cvar_t	gl_cull = {"gl_cull","1"};
cvar_t	gl_smoothmodels = {"gl_smoothmodels","1"};
cvar_t	gl_polyblend = {"gl_polyblend","1"};
cvar_t	gl_playermip = {"gl_playermip","0"};
cvar_t	gl_nocolors = {"gl_nocolors","0"};
cvar_t	gl_finish = {"gl_finish","0"};
cvar_t	gl_fb_bmodels = {"gl_fb_bmodels","1"};
cvar_t	gl_fb_models = {"gl_fb_models","1"};
cvar_t	gl_lightmode = {"gl_lightmode","2"};
cvar_t	gl_solidparticles = {"gl_solidparticles", "0"};

int		lightmode = 2;

void R_MarkLeaves (void);


/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
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

//==================================================================================

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (void)
{
	int		i;
	qbool	draw_sprites;
	qbool	draw_translucent;

	if (!r_drawentities.value)
		return;

	draw_sprites = draw_translucent = false;

	for (i = 0; i < cl_numvisedicts; i++)
	{
		currententity = &cl_visedicts[i];

		switch (currententity->model->type)
		{
			case mod_alias:
				if (!(currententity->renderfx & RF_TRANSLUCENT))
					R_DrawAliasModel (currententity);
				else
					draw_translucent = true;
				break;

			case mod_brush:
				R_DrawBrushModel (currententity);
				break;

			case mod_sprite:
				draw_sprites = true;
				break;

			default:
				break;
		}
	}

	// draw sprites separately, because of alpha blending
	if (draw_sprites)
	{
		GL_SelectTexture (GL_TEXTURE0_ARB);
		qglEnable (GL_ALPHA_TEST);
//		qglDepthMask (GL_FALSE);

		for (i = 0; i < cl_numvisedicts; i++)
		{
			currententity = &cl_visedicts[i];

			if (currententity->model->type == mod_sprite)
				R_DrawSpriteModel (currententity);
		}

		qglDisable (GL_ALPHA_TEST);
//		qglDepthMask (GL_TRUE);
	}

	// draw translucent models
	if (draw_translucent)
	{
//		qglDepthMask (GL_FALSE);		// no z writes
		for (i = 0; i < cl_numvisedicts; i++)
		{
			currententity = &cl_visedicts[i];

			if (currententity->model->type == mod_alias
					&& (currententity->renderfx & RF_TRANSLUCENT))
				R_DrawAliasModel (currententity);
		}
//		qglDepthMask (GL_TRUE);
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
	if (!gl_solidparticles.value)
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

int SignbitsForPlane (mplane_t *out)
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


void R_SetFrustum (void)
{
	int		i;

	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef2.fov_x / 2 ) );

	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef2.fov_x / 2 );

	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef2.fov_y / 2 );

	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef2.fov_y / 2 ) );

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}



/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
	vec3_t	testorigin;
	mleaf_t	*leaf;
	extern float	wateralpha;

	// use wateralpha only if the server allows
	wateralpha = r_refdef2.watervis ? r_wateralpha.value : 1;

	R_AnimateLight ();

	r_framecount++;

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

	c_brush_polys = 0;
	c_alias_polys = 0;
}


void MYgluPerspective( GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar )
{
	GLdouble xmin, xmax, ymin, ymax;
	
	ymax = zNear * tan( fovy * M_PI / 360.0 );
	ymin = -ymax;
	
	xmin = ymin * aspect;
	xmax = ymax * aspect;
	
	qglFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
}


/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
	int		x, x2, y2, y, w, h;

	//
	// set up viewpoint
	//
	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	x = (r_refdef2.vrect.x * vid.realwidth) / vid.width;
	x2 = ((r_refdef2.vrect.x + r_refdef2.vrect.width) * vid.realwidth) / vid.width;
	y = ((vid.height - r_refdef2.vrect.y) * vid.realheight) / vid.height;
	y2 = ((vid.height - (r_refdef2.vrect.y + r_refdef2.vrect.height)) * vid.realheight) / vid.height;

	w = x2 - x;
	h = y - y2;

	qglViewport (x, y2, w, h);
	screenaspect = (float)r_refdef2.vrect.width/r_refdef2.vrect.height;
	MYgluPerspective (r_refdef2.fov_y,  screenaspect,  4,  4096);

	qglCullFace(GL_FRONT);

	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity ();

	qglRotatef (-90,  1, 0, 0);		// put Z going up
	qglRotatef (90,	0, 0, 1);		// put Z going up
	qglRotatef (-r_refdef2.viewangles[2],  1, 0, 0);
	qglRotatef (-r_refdef2.viewangles[0],  0, 1, 0);
	qglRotatef (-r_refdef2.viewangles[1],  0, 0, 1);
	qglTranslatef (-r_refdef2.vieworg[0], -r_refdef2.vieworg[1], -r_refdef2.vieworg[2]);

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
	r_framecount = 1;
}

void GL_Main_Init(void)
{
	Cmd_AddCommand ("screenshot", R_ScreenShot_f);
	Cmd_AddCommand ("loadsky", R_LoadSky_f);

	Cvar_Register (&r_norefresh);
	Cvar_Register (&r_lightmap);
	Cvar_Register (&r_fullbright);
	Cvar_Register (&r_drawentities);
	Cvar_Register (&r_drawflame);
	Cvar_Register (&r_shadows);
	Cvar_Register (&r_wateralpha);
	Cvar_Register (&r_dynamic);
	Cvar_Register (&r_novis);
	Cvar_Register (&r_speeds);
	Cvar_Register (&r_netgraph);
	Cvar_Register (&r_fullbrightSkins);
	Cvar_Register (&r_skycolor);
	Cvar_Register (&r_fastsky);
	Cvar_Register (&r_fastturb);

	Cvar_Register (&gl_subdivide_size);
	Cvar_Register (&gl_clear);
	Cvar_Register (&gl_cull);
	Cvar_Register (&gl_smoothmodels);
	Cvar_Register (&gl_polyblend);
	Cvar_Register (&gl_playermip);
	Cvar_Register (&gl_nocolors);
	Cvar_Register (&gl_finish);
	Cvar_Register (&gl_fb_bmodels);
	Cvar_Register (&gl_fb_models);
	Cvar_Register (&gl_lightmode);
	Cvar_Register (&gl_solidparticles);

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
	GL_Main_Init ();
	TexMgr_Init ();
	R_Draw_Init ();
	Mod_Init ();
}


/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene (void)
{
	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();	// done here so we know if we're in water

	R_DrawWorld ();		// adds static entities to the list

	S_ExtraUpdate ();	// don't let sound get messed up if going slow

	R_DrawEntitiesOnList ();

	GL_SelectTexture(GL_TEXTURE0_ARB);
}


/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	static qbool cleartogray;
	qbool	clear = false;

	if (gl_clear.value) {
		clear = true;
		if (cleartogray) {
			qglClearColor (1, 0, 0, 0);
			cleartogray = false;
		}
	}

	if (clear)
		qglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	else
		qglClear (GL_DEPTH_BUFFER_BIT);

	gldepthmax = 1;
	qglDepthFunc (GL_LEQUAL);

	qglDepthRange (0, gldepthmax);
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

	R_Clear ();

	// render normal view
	R_RenderScene ();
	R_DrawWaterSurfaces ();
	R_DrawParticles ();

	RB_EndFrame ();
}
