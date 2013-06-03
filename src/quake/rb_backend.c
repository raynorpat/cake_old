// rb_backend.c: function calls to whatever renderering library we are using (OpenGL, OpenGL ES, Direct3D, etc)

#include "gl_local.h"

int			rs_brushpolys, rs_aliaspolys, rs_drawelements;
int			rs_dynamiclightmaps;

float		r_fovx, r_fovy; // rendering fov may be different becuase of r_waterwarp and r_stereo

// rb_backend_gl11.c
extern void RB_GL11_SetDefaultState (void);

// ===================================================================

static void rb_backend_start(void)
{
	Com_Printf("Renderer backend: ");
	switch (vid.renderpath)
	{
		case RENDERPATH_GL11:
			Com_Printf("Fixed-Function OpenGL 1.1 with Extensions\n");
			break;
		case RENDERPATH_GLES:
			Com_Printf("Fixed-Function OpenGL ES 1.1\n");
			break;
		case RENDERPATH_GL30:
			Com_Printf("OpenGL 3.0\n");
			break;
	}

	// make sure we set the default state
	switch (vid.renderpath)
	{
		case RENDERPATH_GL11:
		case RENDERPATH_GLES:
		case RENDERPATH_GL30:
			RB_GL11_SetDefaultState ();
			break;
	}
}

static void rb_backend_shutdown(void)
{
	Com_Printf("Renderer backend shutting down.\n");
}

static void rb_backend_newmap(void)
{
}

void RB_InitBackend(void)
{
	R_RegisterModule("RB_Backend", rb_backend_start, rb_backend_shutdown, rb_backend_newmap);
}

// ===================================================================

void RB_Finish(void)
{
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GLES:
	case RENDERPATH_GL30:
		qglFinish();
		break;
	}
}
