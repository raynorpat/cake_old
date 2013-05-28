// rb_backend_gl11.c: OpenGL 1.1 backend

#include "gl_local.h"

/*
===============
RB_GL11_SetDefaultState
===============
*/
void RB_GL11_SetDefaultState (void)
{
	qglClearColor (1,0,0,0);

	qglCullFace(GL_FRONT);

	qglEnable(GL_TEXTURE_2D);

	qglEnable(GL_ALPHA_TEST);
	qglAlphaFunc(GL_GREATER, 0.666);

	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	qglDepthFunc (GL_LEQUAL);
	qglDepthRange (0, 1);
}

/*

MATRICES

*/

canvastype currentcanvas = CANVAS_NONE; // for RB_GL11_SetCanvas

/*
================
RB_GL11_SetDefaultCanvas
================
*/
void RB_GL11_SetDefaultCanvas (void)
{
	currentcanvas = CANVAS_INVALID;
	RB_SetCanvas (CANVAS_DEFAULT);

	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);
	qglDisable (GL_BLEND);
	qglEnable (GL_ALPHA_TEST);

	qglColor4f (1, 1, 1, 1);
}

/*
================
RB_GL11_SetCanvas
================
*/
void RB_GL11_SetCanvas (canvastype newcanvas)
{
	extern vrect_t scr_vrect;
	extern cvar_t scr_menuscale;
	extern cvar_t scr_sbarscale;
	extern cvar_t scr_crosshairscale;
	float s;
	int lines;

	if (newcanvas == currentcanvas)
		return;

	currentcanvas = newcanvas;

	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();

	switch(newcanvas)
	{
	case CANVAS_DEFAULT:
		qglOrtho (0, vid.width, vid.height, 0, -99999, 99999);
		qglViewport (0, 0, vid.width, vid.height);
		break;
	case CANVAS_CONSOLE:
		lines = vid.height - (scr_con_current * vid.height / vid.height);
		qglOrtho (0, vid.width, vid.height + lines, lines, -99999, 99999);
		qglViewport (0, 0, vid.width, vid.height);
		break;
	case CANVAS_MENU:
		s = min ((float)vid.width / 320.0, (float)vid.height / 200.0);
		s = clamp (1.0, scr_menuscale.value, s);
		qglOrtho (0, 320, 200, 0, -99999, 99999);
		qglViewport ((vid.width - 320*s) / 2, (vid.height - 200*s) / 2, 320*s, 200*s);
		break;
	case CANVAS_SBAR:
		s = clamp (1.0, scr_sbarscale.value, (float)vid.width / 320.0);
		qglOrtho (0, 320, 48, 0, -99999, 99999);
		qglViewport ((vid.width - 320*s) / 2, 0, 320*s, 48*s);
		break;
	case CANVAS_WARPIMAGE:
		qglOrtho (0, 128, 0, 128, -99999, 99999);
		qglViewport (0, vid.height-gl_warpimagesize, gl_warpimagesize, gl_warpimagesize);
		break;
	case CANVAS_CROSSHAIR: //0,0 is center of viewport
		s = clamp (1.0, scr_crosshairscale.value, 10.0);
		qglOrtho (scr_vrect.width/-2/s, scr_vrect.width/2/s, scr_vrect.height/2/s, scr_vrect.height/-2/s, -99999, 99999);
		qglViewport (scr_vrect.x, vid.height - scr_vrect.y - scr_vrect.height, scr_vrect.width & ~1, scr_vrect.height & ~1);
		break;
	case CANVAS_BOTTOMLEFT: //used by devstats
		s = (float)vid.width/vid.width;
		qglOrtho (0, 320, 200, 0, -99999, 99999);
		qglViewport (0, 0, 320*s, 200*s);
		break;
	case CANVAS_BOTTOMRIGHT: //used by fps
		s = (float)vid.width/vid.width;
		qglOrtho (0, 320, 200, 0, -99999, 99999);
		qglViewport (vid.width-320*s, 0, 320*s, 200*s);
		break;
	default:
		Sys_Error ("RB_GL11_SetCanvas: bad canvas type");
	}

	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();
}

float frustum_skew = 0.0; // used by r_stereo
static void GL_SetFrustum(float fovx, float fovy)
{
	float xmax, ymax;
	xmax = NEARCLIP * tan( fovx * M_PI / 360.0 );
	ymax = NEARCLIP * tan( fovy * M_PI / 360.0 );
	qglFrustum(-xmax + frustum_skew, xmax + frustum_skew, -ymax, ymax, NEARCLIP, gl_farclip.value);
}

/*
=============
RB_Set3DMatrix
=============
*/
void RB_GL11_Set3DMatrix (void)
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

	// set drawing parms
	qglEnable(GL_CULL_FACE);
	qglDisable(GL_BLEND);
	qglDisable(GL_ALPHA_TEST);
	qglEnable(GL_DEPTH_TEST);
}

/*
===============
RB_GL11_RotateMatrixForEntity
===============
*/
void RB_GL11_RotateMatrixForEntity (vec3_t origin, vec3_t angles)
{
	qglTranslatef (origin[0],  origin[1],  origin[2]);

	qglRotatef (angles[1],  0, 0, 1);
	qglRotatef (-angles[0],  0, 1, 0);
	qglRotatef (angles[2],  1, 0, 0);
}