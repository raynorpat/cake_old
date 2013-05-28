// rb_backend.c: function calls to whatever renderering library we are using (OpenGL, OpenGL ES, Direct3D, etc)

#include "gl_local.h"

int	c_brush_polys, c_brush_passes, c_alias_polys;

static void rb_backend_start(void)
{
	Com_Printf("Renderer backend started.\n");

	// do specific renderpath init here
	switch (vid.renderpath)
	{
		case RENDERPATH_GL11:
		case RENDERPATH_GLES:
		case RENDERPATH_GL30:
			Com_Printf("Backend: OpenGL\n");
			RB_GL11_Init ();
			break;
		case RENDERPATH_D3D:
			Com_Printf("Backend: Direct3D\n");
			Sys_Error("TODO: D3D");
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
	case RENDERPATH_D3D:
		// no finish/flush in d3d?
		break;
	}
}


void RB_Set2DProjections( void )
{
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GLES:
	case RENDERPATH_GL30:
		RB_GL11_Set2DProjections();
		break;
	case RENDERPATH_D3D:
		Sys_Error( "TODO: D3D" );
		break;
	}
}
