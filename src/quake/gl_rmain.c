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

r_defaultquad_t *r_default_quads = (r_defaultquad_t *) scratchbuf;
int r_max_quads = SCRATCHBUF_SIZE / (sizeof (r_defaultquad_t) * 4);
int r_num_quads = 0;
unsigned short *r_quad_indexes = NULL;


void R_InitQuads (void)
{
	int i;
	unsigned short *ndx;
	unsigned int pquad;

	// clamp to shorts
	if (r_max_quads >= 16384) r_max_quads = 16384;

	r_quad_indexes = (unsigned short *) Hunk_Alloc (r_max_quads * sizeof (unsigned short) * 6);
	ndx = r_quad_indexes;

	// set up the indexes one time only
	for (i = 0, pquad = 0; i < r_max_quads; i++, ndx += 6, pquad += 4)
	{
		ndx[0] = pquad + 0;
		ndx[1] = pquad + 1;
		ndx[2] = pquad + 2;

		ndx[3] = pquad + 0;
		ndx[4] = pquad + 2;
		ndx[5] = pquad + 3;
	}
}

model_t	*r_worldmodel;
entity_t r_worldentity;

vec3_t	modelorg, r_entorigin;
entity_t *currententity;

int		r_visframecount;	// bumped when going to a new PVS
int		r_framecount;		// used for dlight push checking

mplane_t frustum[4];

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
refdef2_t r_refdef2;

mleaf_t	*r_viewleaf, *r_oldviewleaf;
mleaf_t	*r_viewleaf2, *r_oldviewleaf2;	// for watervis hack

int		d_lightstylevalue[256];	// 8.8 fraction of base light value

cvar_t	r_norefresh = {"r_norefresh","0"};
cvar_t	r_drawentities = {"r_drawentities","1"};
cvar_t	r_drawworld = {"r_drawworld","1"};
cvar_t	r_speeds = {"r_speeds","0"};
cvar_t	r_fullbright = {"r_fullbright","0"};
cvar_t	r_lightmap = {"r_lightmap","0"};
cvar_t	r_shadows = {"r_shadows","0"};
cvar_t	r_wateralpha = {"r_wateralpha","1"};
cvar_t	r_waterwarp = {"r_waterwarp", "1"};
cvar_t	r_dynamic = {"r_dynamic","1"};
cvar_t	r_novis = {"r_novis","0"};
cvar_t	r_netgraph = {"r_netgraph","0"};
cvar_t	r_fastsky = {"r_fastsky", "0"};
cvar_t	r_sky_quality = {"r_sky_quality", "12"};
cvar_t	r_skyalpha = {"r_skyalpha", "1"};
cvar_t	r_skyfog = {"r_skyfog","0.5"};
cvar_t	r_stereo = {"r_stereo","0"};
cvar_t	r_stereodepth = {"r_stereodepth","128"};
cvar_t	r_showtris = {"r_showtris","0"};

cvar_t	gl_nocolors = {"gl_nocolors","0"};
cvar_t	gl_finish = {"gl_finish","0"};
cvar_t	gl_fullbrights = {"gl_fullbrights","1"};
cvar_t	gl_overbright = {"gl_overbright","1"};
cvar_t	gl_farclip = {"gl_farclip", "16384", CVAR_ARCHIVE};

float	r_lightscale = 4.0f; // overbright light scale

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
static void TurnVector (vec3_t out, const vec3_t forward, const vec3_t side, float angle)
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
static void R_SetFrustum (float fovx, float fovy)
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

	RB_Set3DMatrix ();
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
	extern float wateralpha;

	// use wateralpha only if the server allows
	wateralpha = r_refdef2.watervis ? r_wateralpha.value : 1;

	// setup fog values for this frame
	Fog_SetupFrame ();

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

	// the overbright scale needs to retain 1 in the alpha channel
	if (gl_overbright.value >= 1)
		r_lightscale = 4;
	else if (gl_overbright.value < 1)
		r_lightscale = 2;

	if (r_lightmap.value)
		r_lightscale *= 0.5f;

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

	R_MarkSurfaces ();			// create texture chains from PVS
	R_CullSurfaces ();			// do after R_SetFrustum and R_MarkSurfaces

	RB_Clear ();
}

//==============================================================================
//
// RENDER VIEW
//
//==============================================================================

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
R_ShowTris
================
*/
void R_ShowTris (void)
{
	int i;

	if (r_showtris.value < 1 || r_showtris.value > 2)
		return;

	if (r_showtris.value == 1)
		qglDisable (GL_DEPTH_TEST);
	qglPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
	RB_PolygonOffset (OFFSET_SHOWTRIS);
	qglDisable (GL_TEXTURE_2D);
	qglColor3f (1,1,1);
//	qglEnable (GL_BLEND);
//	qglBlendFunc (GL_ONE, GL_ONE);

	if (r_drawworld.value)
		R_DrawTextureChains_ShowTris ();

	if (r_drawentities.value)
	{
		for (i=0 ; i<cl_numvisedicts ; i++)
		{
			currententity = &cl_visedicts[i];

			switch (currententity->model->type)
			{
			case mod_brush:
				R_DrawBrushModel_ShowTris (currententity);
				break;
			case mod_alias:
				R_DrawAliasModel_ShowTris (currententity);
				break;
			case mod_sprite:
				R_DrawSpriteModel (currententity);
				break;
			default:
				break;
			}
		}
	}

	R_DrawParticles_ShowTris ();

//	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//	qglDisable (GL_BLEND);
	qglColor3f (1,1,1);
	qglEnable (GL_TEXTURE_2D);
	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	RB_PolygonOffset (OFFSET_NONE);
	if (r_showtris.value == 1)
		qglEnable (GL_DEPTH_TEST);
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

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene (void)
{
	R_SetupScene (); 				// this does everything that should be done once per call to RenderScene

	Fog_EnableGFog ();

	Sky_DrawSky ();

	R_DrawWorld ();

	R_DrawShadows ();				// render entity shadows

	R_DrawEntitiesOnList ();

	R_DrawTextureChains_Water ();	// drawn here since they might have transparency

	R_DrawParticles ();

	Fog_DisableGFog ();

	R_ShowTris ();
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
		extern float frustum_skew;

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


void gl_main_start(void)
{
}

void gl_main_shutdown(void)
{
}

void gl_main_newmap(void)
{
	r_framecount = 1; // no dlightcache

	Sky_NewMap ();
	Fog_NewMap ();
}

void GL_Main_Init(void)
{
	R_InitQuads ();

	Cmd_AddCommand ("screenshot", R_ScreenShot_f);
	Cmd_AddCommand ("loadsky", R_LoadSky_f);

	Cvar_Register (&r_norefresh);
	Cvar_Register (&r_lightmap);
	Cvar_Register (&r_fullbright);
	Cvar_Register (&r_drawentities);
	Cvar_Register (&r_drawworld);
	Cvar_Register (&r_shadows);
	Cvar_Register (&r_wateralpha);
	Cvar_Register (&r_waterwarp);
	Cvar_Register (&r_dynamic);
	Cvar_Register (&r_novis);
	Cvar_Register (&r_speeds);
	Cvar_Register (&r_netgraph);
	Cvar_Register (&r_fastsky);
	Cvar_Register (&r_sky_quality);
	Cvar_Register (&r_skyalpha);
	Cvar_Register (&r_skyfog);
	Cvar_Register (&r_stereo);
	Cvar_Register (&r_stereodepth);
	Cvar_Register (&r_showtris);
	Cvar_Register (&r_primitives);

	Cvar_Register (&gl_nocolors);
	Cvar_Register (&gl_finish);
	Cvar_Register (&gl_fullbrights);
	Cvar_Register (&gl_overbright);
	Cvar_Register (&gl_farclip);

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
	Fog_Init ();
}