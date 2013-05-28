// rb_backend_gl11.c: OpenGL 1.1 backend

#include "gl_local.h"

/*
===============
RB_GL11_Init
===============
*/
void RB_GL11_Init (void)
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
================
RB_GL11_Set2DProjections

Setup as if the screen was 320*200
================
*/
void RB_GL11_Set2DProjections (void)
{
	extern canvastype currentcanvas;

	currentcanvas = CANVAS_INVALID;
	GL_SetCanvas (CANVAS_DEFAULT);

	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);
	qglDisable (GL_BLEND);
	qglEnable (GL_ALPHA_TEST);

	qglColor4f (1, 1, 1, 1);
}

