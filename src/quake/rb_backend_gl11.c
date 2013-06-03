// rb_backend_gl11.c: OpenGL 1.1 backend

#include "gl_local.h"

void R_LMFindOptimalTSIMode (void);

/*
===============
RB_GL11_SetDefaultState
===============
*/
void RB_GL11_SetDefaultState (void)
{
	qglClearColor (0.15, 0.15, 0.15, 0);

	qglCullFace(GL_FRONT);

	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);
	qglEnable(GL_ALPHA_TEST);
	qglAlphaFunc(GL_GREATER, 0.666);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDepthFunc (GL_LEQUAL);

	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

	// find the mode we're going to use for updating light
	R_LMFindOptimalTSIMode ();

	// recreate any vertex buffers in use; this isn't needed on my nvidia, but might be on other hardware
	R_CreateMeshIndexBuffer ();

	// need to call this here too, even if we don't have the extensions available, to let things settle down nicely
	R_InitVertexBuffers ();

	// this deletes texture objects and PBOs used for lightmaps so that the renderer can create them on demand the next time they are needed
	// needs to be done before image reloading otherwise things go wacko (why?)
	GL_DeleteAllLightmaps ();

	// unbind all vertex and index arrays
	GL_UnbindBuffers ();

	// unbind all textures
	GL_UnbindTextures ();
}
