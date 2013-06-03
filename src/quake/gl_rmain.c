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
r_defaultquad2_t *r_default_quads2 = (r_defaultquad2_t *) scratchbuf;

int r_max_quads = SCRATCHBUF_SIZE / (sizeof (r_defaultquad_t) * 4);
int r_num_quads = 0;

unsigned short *r_quad_indexes = NULL;

unsigned int r_quadindexbuffer = 0;
unsigned int r_parttexcoordbuffer = 0;

void R_InitQuads (void);
void R_DrawCoronas (void);

glmatrix r_world_matrix;

void R_InitVertexBuffers (void)
{
	// create the initial indexes if needed
	if (!r_quad_indexes) R_InitQuads ();
}


void R_InitQuads (void)
{
	int i;
	unsigned short *ndx;
	unsigned int pquad;

	// don't create it more than once
	if (r_quad_indexes) return;

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


void R_DrawAliasModels (void);
void R_DrawSpriteModels (void);

void R_ShowTrisBegin (void)
{
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_NONE);
	qglColor4f (1, 1, 1, 1);
	Fog_DisableGFog ();

	if (r_showtris.value < 2)
	{
		qglDisable (GL_DEPTH_TEST);
		qglDepthMask (GL_FALSE);
	}
	else
	{
		qglEnable (GL_POLYGON_OFFSET_FILL);
		qglEnable (GL_POLYGON_OFFSET_LINE);
		qglPolygonOffset (-1, -3);
	}

	qglPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
	qglColor4f (1, 1, 1, 1);
}


void R_ShowTrisEnd (void)
{
	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

	if (r_showtris.value < 2)
	{
		qglDepthMask (GL_TRUE);
		qglEnable (GL_DEPTH_TEST);
	}
	else
	{
		qglDisable (GL_POLYGON_OFFSET_FILL);
		qglDisable (GL_POLYGON_OFFSET_LINE);
	}

	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);
	Fog_EnableGFog ();
}

model_t	*r_worldmodel;
entity_t r_worldentity;

vec3_t	modelorg, r_entorigin;

int		r_visframecount;	// bumped when going to a new PVS
int		r_framecount;		// used for dlight push checking

glmatrix r_projection_matrix;
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
cvar_t	r_lockpvs = {"r_lockpvs", "0"};
cvar_t	r_lockfrustum = {"r_lockfrustum", "0"};
cvar_t	r_lerpmodels = {"r_lerpmodels", "0"};
cvar_t	r_lerpmove = {"r_lerpmove", "1"};
cvar_t	r_nolerp_list = {"r_nolerp_list", "progs/flame.mdl,progs/flame2.mdl,progs/braztall.mdl,progs/brazshrt.mdl,progs/longtrch.mdl,progs/flame_pyre.mdl,progs/v_saw.mdl,progs/v_xfist.mdl,progs/h2stuff/newfire.mdl"};

cvar_t	gl_nocolors = {"gl_nocolors","0"};
cvar_t	gl_finish = {"gl_finish","0"};
cvar_t	gl_fullbrights = {"gl_fullbrights","1"};
cvar_t	gl_overbright = {"gl_overbright","1"};

extern cvar_t r_show_coronas;
extern cvar_t r_instancedlight;

void R_AlphaListBeginFrame (void);
void R_DrawAlphaList (void);

/*
=================
R_CullSphere

Returns true if the sphere is outside the frustum
grabbed from AlienArena 2011 source (repaying the compliment!!!)
=================
*/
qbool R_CullSphere (const float *center, const float radius, const int clipflags)
{
	int		i;
	mplane_t *p;

	for (i = 0, p = frustum; i < 4; i++, p++)
	{
		if (!(clipflags & (1 << i)))
			continue;

		if (DotProduct (center, p->normal) - p->dist <= -radius)
			return true;
	}

	return false;
}


/*
=================
R_CullBox -- johnfitz -- replaced with new function from lordhavoc

Returns true if the box is completely outside the frustum
=================
*/
qbool R_CullBox (vec3_t emins, vec3_t emaxs)
{
	int		i;
	mplane_t *p;

	for (i = 0; i < 4; i++)
	{
		p = &frustum[i];

		switch (p->signbits)
		{
		default:
		case 0:
			if (p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2] < p->dist)
				return true;
			break;
		case 1:
			if (p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2] < p->dist)
				return true;
			break;
		case 2:
			if (p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2] < p->dist)
				return true;
			break;
		case 3:
			if (p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2] < p->dist)
				return true;
			break;
		case 4:
			if (p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2] < p->dist)
				return true;
			break;
		case 5:
			if (p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2] < p->dist)
				return true;
			break;
		case 6:
			if (p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2] < p->dist)
				return true;
			break;
		case 7:
			if (p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2] < p->dist)
				return true;
			break;
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
=============
GL_SetFrustum
=============
*/
void GL_SetFrustum (float fovx, float fovy)
{
#define NEARCLIP 4
	float left, right, bottom, top;
	float nudge = 1.0 - 1.0 / (1 << 23);

	right = NEARCLIP * tan (fovx * M_PI / 360.0);
	left = -right;

	top = NEARCLIP * tan (fovy * M_PI / 360.0);
	bottom = -top;

	r_projection_matrix.m16[0] = (2 * NEARCLIP) / (right - left);
	r_projection_matrix.m16[4] = 0;
	r_projection_matrix.m16[8] = (right + left) / (right - left);
	r_projection_matrix.m16[12] = 0;

	r_projection_matrix.m16[1] = 0;
	r_projection_matrix.m16[5] = (2 * NEARCLIP) / (top - bottom);
	r_projection_matrix.m16[9] = (top + bottom) / (top - bottom);
	r_projection_matrix.m16[13] = 0;

	r_projection_matrix.m16[2] = 0;
	r_projection_matrix.m16[6] = 0;
	r_projection_matrix.m16[10] = -1 * nudge;
	r_projection_matrix.m16[14] = -2 * NEARCLIP * nudge;

	r_projection_matrix.m16[3] = 0;
	r_projection_matrix.m16[7] = 0;
	r_projection_matrix.m16[11] = -1;
	r_projection_matrix.m16[15] = 0;

	if (r_waterwarp.value && r_waterwarp.value < 2)
	{
		int contents = Mod_PointInLeaf (r_origin, r_worldmodel)->contents;

		if ((contents == CONTENTS_WATER || contents == CONTENTS_SLIME || contents == CONTENTS_LAVA) && v_blend[3])
		{
			r_projection_matrix.m16[4] = sin (cl.time * 1.125f) * 0.0666f;
			r_projection_matrix.m16[1] = cos (cl.time * 1.125f) * 0.0666f;
			GL_ScaleMatrix (&r_projection_matrix, (cos (cl.time * 0.75f) + 20.0f) * 0.05f, (sin (cl.time * 0.75f) + 20.0f) * 0.05f, 1);
		}
	}

	qglLoadMatrixf (r_projection_matrix.m16);
}


void R_ExtractFrustum (void)
{
	int i;
	glmatrix r_mvp_matrix;

	// get the frustum from the actual matrixes that were used
	GL_MultiplyMatrix (&r_mvp_matrix, &r_world_matrix, &r_projection_matrix);

	// frustum 0
	frustum[0].normal[0] = r_mvp_matrix.m16[3] - r_mvp_matrix.m16[0];
	frustum[0].normal[1] = r_mvp_matrix.m16[7] - r_mvp_matrix.m16[4];
	frustum[0].normal[2] = r_mvp_matrix.m16[11] - r_mvp_matrix.m16[8];

	// frustum 1
	frustum[1].normal[0] = r_mvp_matrix.m16[3] + r_mvp_matrix.m16[0];
	frustum[1].normal[1] = r_mvp_matrix.m16[7] + r_mvp_matrix.m16[4];
	frustum[1].normal[2] = r_mvp_matrix.m16[11] + r_mvp_matrix.m16[8];

	// frustum 2
	frustum[2].normal[0] = r_mvp_matrix.m16[3] + r_mvp_matrix.m16[1];
	frustum[2].normal[1] = r_mvp_matrix.m16[7] + r_mvp_matrix.m16[5];
	frustum[2].normal[2] = r_mvp_matrix.m16[11] + r_mvp_matrix.m16[9];

	// frustum 3
	frustum[3].normal[0] = r_mvp_matrix.m16[3] - r_mvp_matrix.m16[1];
	frustum[3].normal[1] = r_mvp_matrix.m16[7] - r_mvp_matrix.m16[5];
	frustum[3].normal[2] = r_mvp_matrix.m16[11] - r_mvp_matrix.m16[9];

	for (i = 0; i < 4; i++)
	{
		VectorNormalize (frustum[i].normal);

		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal); //FIXME: shouldn't this always be zero?
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}


/*
=============
R_SetupGL
=============
*/
int gl_vpx, gl_vpy, gl_vpwidth, gl_vpheight;

void R_SetupGL (void)
{
	// evaluate viewport size
	gl_vpx = 0;
	gl_vpy = 0;
	gl_vpwidth = vid.width;
	gl_vpheight = vid.height;
}


void R_SetViewport (void)
{
	// now set the correct viewport
	qglViewport (gl_vpx, gl_vpy, gl_vpwidth, gl_vpheight);

	// johnfitz -- rewrote this section
	qglMatrixMode (GL_PROJECTION);
	qglLoadIdentity ();
	// johnfitz

	GL_SetFrustum (r_fovx, r_fovy); // johnfitz -- use r_fov* vars

	qglMatrixMode (GL_MODELVIEW);

	GL_IdentityMatrix (&r_world_matrix);

	GL_RotateMatrix (&r_world_matrix, -90, 1, 0, 0);
	GL_RotateMatrix (&r_world_matrix, 90, 0, 0, 1);

	GL_RotateMatrix (&r_world_matrix, -r_refdef2.viewangles[2], 1, 0, 0);
	GL_RotateMatrix (&r_world_matrix, -r_refdef2.viewangles[0], 0, 1, 0);
	GL_RotateMatrix (&r_world_matrix, -r_refdef2.viewangles[1], 0, 0, 1);

	GL_TranslateMatrix (&r_world_matrix, -r_refdef2.vieworg[0], -r_refdef2.vieworg[1], -r_refdef2.vieworg[2]);

	qglLoadMatrixf (r_world_matrix.m16);

	R_ExtractFrustum ();

	// set drawing parms
	qglEnable (GL_CULL_FACE);

	qglDisable (GL_BLEND);
	qglDisable (GL_ALPHA_TEST);
	qglEnable (GL_DEPTH_TEST);
	qglDepthMask (GL_TRUE);
	qglDepthFunc (GL_LEQUAL);
}


/*
=============
R_Clear -- johnfitz -- rewritten and gutted
=============
*/
void R_Clear (void)
{
	extern int gl_stencilbits;
	unsigned int clearbits;

	// partition the depth range for 3D/2D stuff
	qglDepthRange (QGL_DEPTH_3D_BEGIN, QGL_DEPTH_3D_END);

	// always clear the depth buffer
	clearbits = GL_DEPTH_BUFFER_BIT;

	if (r_viewleaf && r_viewleaf->contents == CONTENTS_SOLID)
	{
		// when noclipping set a black clear color
		qglClearColor (0.15, 0.15, 0.15, 1);
		clearbits |= GL_COLOR_BUFFER_BIT;
	}
	else if (1)
	{
		// set the correct clear color
		qglClearColor (0.15, 0.15, 0.15, 1);
		clearbits |= GL_COLOR_BUFFER_BIT;
	}

	// if we got a stencil buffer we must always clear it, even if we're not using it
	clearbits |= GL_STENCIL_BUFFER_BIT;

	qglClear (clearbits);
}

/*
===============
R_SetupScene

this is the stuff that needs to be done once per eye in stereo mode
===============
*/
void R_SetupScene (void)
{
	R_AnimateLight ();
	r_framecount++;
}


void R_UpdateContentsColor (void)
{
	/*
	if (r_viewleaf->contents == CONTENTS_EMPTY)
	{
		// brought to you today by "disgusting hacks 'r' us"...
		extern cshift_t cshift_empty;

		cl.cshifts[CSHIFT_CONTENTS].destcolor[0] = cshift_empty.destcolor[0];
		cl.cshifts[CSHIFT_CONTENTS].destcolor[1] = cshift_empty.destcolor[1];
		cl.cshifts[CSHIFT_CONTENTS].destcolor[2] = cshift_empty.destcolor[2];
		cl.cshifts[CSHIFT_CONTENTS].percent = cshift_empty.percent;
	}
	else if (r_viewleaf->contentscolor)
	{
		// if the viewleaf has a contents colour set we just override with it
		cl.cshifts[CSHIFT_CONTENTS].destcolor[0] = r_viewleaf->contentscolor[0];
		cl.cshifts[CSHIFT_CONTENTS].destcolor[1] = r_viewleaf->contentscolor[1];
		cl.cshifts[CSHIFT_CONTENTS].destcolor[2] = r_viewleaf->contentscolor[2];

		// these now have more colour so reduce the percent to compensate
		if (r_viewleaf->contents == CONTENTS_WATER)
			cl.cshifts[CSHIFT_CONTENTS].percent = 48;
		else if (r_viewleaf->contents == CONTENTS_LAVA)
			cl.cshifts[CSHIFT_CONTENTS].percent = 112;
		else if (r_viewleaf->contents == CONTENTS_SLIME)
			cl.cshifts[CSHIFT_CONTENTS].percent = 80;
		else cl.cshifts[CSHIFT_CONTENTS].percent = 0;
	}
	else
	*/
	{
		// empty or undefined
		V_SetContentsColor (r_viewleaf->contents);
	}

	// now calc the blend
	V_PrepBlend ();
}


/*
===============
R_SetupView

this is the stuff that needs to be done once per frame, even in stereo mode
===============
*/
void R_SetupView (void)
{
	// setup fog values for this frame
	Fog_SetupFrame ();

	// build the transformation matrix for the given view angles
	VectorCopy (r_refdef2.vieworg, r_origin);
	AngleVectors (r_refdef2.viewangles, vpn, vright, vup);

	// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, r_worldmodel);

	R_UpdateContentsColor ();

	// calculate r_fovx and r_fovy here
	r_fovx = r_refdef2.fov_x;
	r_fovy = r_refdef2.fov_y;

	if (r_waterwarp.value)
	{
		int contents = Mod_PointInLeaf (r_origin, r_worldmodel)->contents;

		if ((contents == CONTENTS_WATER || contents == CONTENTS_SLIME || contents == CONTENTS_LAVA) && v_blend[3])
		{
			if (r_waterwarp.value > 2)
			{
				// variance is a percentage of width, where width = 2 * tan(fov / 2) otherwise the effect is too dramatic at high FOV and too subtle at low FOV.  what a mess!
				r_fovx = atan (tan (DEG2RAD (r_refdef2.fov_x) / 2) * (0.97 + sin (r_refdef2.time * 1.5) * 0.03)) * 2 / (M_PI/180);
				r_fovy = atan (tan (DEG2RAD (r_refdef2.fov_y) / 2) * (1.03 - sin (r_refdef2.time * 1.5) * 0.03)) * 2 / (M_PI/180);
			}
		}
	}

	R_MarkLeafs (); // create texture chains from PVS
}

//==============================================================================
//
// RENDER VIEW
//
//==============================================================================

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/

void GL_PolyBlend (void)
{
	float blendverts[] =
	{
		// note - 100 y is needed for high fov
		10, 100, 100,
		10, -100, 100,
		10, -100, -100,
		10, 100, -100
	};

	// whaddaya know - these run faster
//	if (!gl_polyblend.value) return;
	if (!v_blend[3]) return;

	qglDisable (GL_ALPHA_TEST);
	qglEnable (GL_BLEND);
	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_TEXTURE_2D);

	qglLoadIdentity ();

	// FIXME: get rid of these
	qglRotatef (-90, 1, 0, 0);	    // put Z going up
	qglRotatef (90, 0, 0, 1);	    // put Z going up

	GL_SetStreamSource (0, GLSTREAM_POSITION, 3, GL_FLOAT, 0, blendverts);
	GL_SetStreamSource (0, GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);
	GL_SetStreamSource (0, GLSTREAM_TEXCOORD0, 0, GL_NONE, 0, NULL);
	GL_SetStreamSource (0, GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
	GL_SetStreamSource (0, GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);
	GL_SetIndices (0, r_quad_indexes);

	qglColor4f (v_blend[0], v_blend[1], v_blend[2], v_blend[3]);

	GL_DrawIndexedPrimitive (GL_TRIANGLES, 6, 4);

	qglDisable (GL_BLEND);
	qglEnable (GL_TEXTURE_2D);
	qglEnable (GL_ALPHA_TEST);
	qglColor4f (1, 1, 1, 1);
}

void R_RenderScene (void)
{
	R_ModelSurfsBeginFrame ();
	R_AlphaListBeginFrame ();
	R_SetupScene (); // this does everything that should be done once per call to RenderScene
	R_SetupGL ();
	R_SetViewport ();
	R_Clear ();
	Fog_EnableGFog ();
	R_DrawWorld ();
	R_DrawAliasModels ();
	R_DrawSpriteModels ();
	R_DrawParticles ();
	R_DrawCoronas ();
	R_DrawAlphaList ();
	Fog_DisableGFog ();

	GL_PolyBlend ();
}


/*
================
R_RenderView
================
*/
void R_RenderView (void)
{
	float	time1 = 0, time2 = 0;

	if (r_norefresh.value)
		return;

	if (!r_worldentity.model || !r_worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds.value) time1 = Sys_DoubleTime ();

	// johnfitz -- rendering statistics
	// mh - always so that we can use con_printf for debug output while developing
	rs_brushpolys = rs_aliaspolys = rs_drawelements = rs_dynamiclightmaps = 0;

	R_SetupView (); // this does everything that should be done once per frame
	R_RenderScene ();

	if (r_speeds.value)
	{
		time2 = Sys_DoubleTime ();

		Com_Printf ("%3i ms  %4i wpoly   %4i epoly   %4i draw   %3i lmap\n",
			(int) ((time2 - time1) * 1000),
			rs_brushpolys,
			rs_aliaspolys,
			rs_drawelements,
			rs_dynamiclightmaps);
	}
}


void gl_main_start(void)
{
}

void gl_main_shutdown(void)
{
}

void gl_main_newmap(void)
{
}

void GL_Main_Init(void)
{
	Cmd_AddCommand ("screenshot", R_ScreenShot_f);

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
	Cvar_Register (&r_lerpmodels);
	Cvar_Register (&r_lerpmove);
	Cvar_Register (&r_nolerp_list);
	Cvar_Register (&r_lockpvs);
	Cvar_Register (&r_lockfrustum);
	Cvar_Register (&r_show_coronas);
	Cvar_Register (&r_instancedlight);

	Cvar_Register (&gl_nocolors);
	Cvar_Register (&gl_finish);
	Cvar_Register (&gl_fullbrights);
	Cvar_Register (&gl_overbright);

	R_WarpRegisterCvars ();

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
	Sky_Init ();
	Fog_Init ();
}