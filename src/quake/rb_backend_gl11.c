// rb_backend_gl11.c: OpenGL 1.1 backend

#include "gl_local.h"

/*
===============
RB_GL11_SetDefaultState
===============
*/
void RB_GL11_SetDefaultState (void)
{
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
}

/*

MATRICES

*/

/*
================
RB_GL11_SetDefaultCanvas
================
*/
void RB_GL11_SetDefaultCanvas (void)
{
	extern void Draw_InvalidateState (void);
	extern canvastype currentcanvas;
	currentcanvas = CANVAS_INVALID;

	Draw_InvalidateState ();
	RB_SetCanvas (CANVAS_DEFAULT);

	// partition the depth range for 3D/2D stuff
	qglDepthRange (QGL_DEPTH_2D_BEGIN, QGL_DEPTH_2D_END);

	qglViewport (0, 0, vid.width, vid.height);

	qglMatrixMode (GL_PROJECTION);
    qglLoadIdentity ();
	qglOrtho (0, vid.width, vid.height, 0, -99999, 99999);

	qglMatrixMode (GL_MODELVIEW);
    qglLoadIdentity ();

	qglDisable (GL_DEPTH_TEST);
//	qglDepthMask (GL_FALSE);
	qglDisable (GL_CULL_FACE);

	qglEnable (GL_BLEND);
	qglDisable (GL_ALPHA_TEST);
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
	qglDepthMask (GL_TRUE);
	qglDepthFunc (GL_LEQUAL);
}

/*
===============
RB_GL11_RotateMatrixForEntity
===============
*/
void RB_GL11_RotateMatrixForEntity (vec3_t origin, vec3_t angles)
{
	if (origin[0] || origin[1] || origin[2])
		qglTranslatef (origin[0],  origin[1],  origin[2]);

	if (angles[1]) qglRotatef (angles[1],  0, 0, 1);
	if (angles[0]) qglRotatef (-angles[0],  0, 1, 0);
	if (angles[2]) qglRotatef (angles[2],  1, 0, 0);
}
