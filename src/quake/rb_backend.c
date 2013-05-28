// rb_backend.c: function calls to whatever renderering library we are using (OpenGL, OpenGL ES, Direct3D, etc)

#include "gl_local.h"

int			c_brush_polys, c_brush_passes, c_alias_polys;

float		r_fovx, r_fovy; // rendering fov may be different becuase of r_waterwarp and r_stereo

// rb_backend_gl11.c
extern void RB_GL11_Init (void);

extern void RB_GL11_SetDefaultCanvas (void);
extern void RB_GL11_SetCanvas (canvastype canvastype);
extern void RB_GL11_Set3DMatrix (void);

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

	// do specific renderpath init here
	switch (vid.renderpath)
	{
		case RENDERPATH_GL11:
		case RENDERPATH_GLES:
		case RENDERPATH_GL30:
			RB_GL11_Init ();
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

double	time1, time2;

void RB_StartFrame (void)
{
	// reset counters
	c_brush_polys = c_brush_passes = 0;
	c_alias_polys = 0;

	if (r_speeds.value)
	{
		qglFinish ();
		time1 = Sys_DoubleTime ();
	}
}

void RB_EndFrame (void)
{
	if (r_speeds.value)
	{
		time2 = Sys_DoubleTime ();
		Com_Printf( "%3i ms  - %4i/%4i wpoly %4i epoly\n",
			(int)((time2-time1)*1000),
			c_brush_polys,
			c_brush_passes,
			c_alias_polys );
	}
}

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

void RB_Clear (void)
{
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GLES:
	case RENDERPATH_GL30:
		qglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		break;
	}
}


/*

MATRICES

*/

void RB_SetDefaultCanvas( void )
{
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GLES:
	case RENDERPATH_GL30:
		RB_GL11_SetDefaultCanvas();
		break;
	}
}

void RB_SetCanvas (canvastype newcanvas)
{
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GLES:
	case RENDERPATH_GL30:
		RB_GL11_SetCanvas(newcanvas);
		break;
	}
}

void RB_Set3DMatrix (void)
{
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GLES:
	case RENDERPATH_GL30:
		RB_GL11_Set3DMatrix();
		break;
	}
}