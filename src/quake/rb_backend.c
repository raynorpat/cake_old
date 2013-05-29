// rb_backend.c: function calls to whatever renderering library we are using (OpenGL, OpenGL ES, Direct3D, etc)

#include "gl_local.h"

int			rs_brushpolys, rs_aliaspolys, rs_drawelements;
int			rs_dynamiclightmaps;

float		r_fovx, r_fovy; // rendering fov may be different becuase of r_waterwarp and r_stereo

// rb_backend_gl11.c
extern void RB_GL11_SetDefaultState (void);

extern void RB_GL11_SetDefaultCanvas (void);
extern void RB_GL11_SetCanvas (canvastype canvastype);
extern void RB_GL11_Set3DMatrix (void);
extern void RB_GL11_RotateMatrixForEntity (vec3_t origin, vec3_t angles);

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

double	time1, time2;

void RB_StartFrame (void)
{
	// reset counters
	rs_brushpolys = rs_aliaspolys = rs_drawelements = rs_dynamiclightmaps = 0;

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
		Com_Printf ("%3i ms  %4i wpoly   %4i epoly   %4i draw   %3i lmap\n",
			(int) ((time2 - time1) * 1000),
			rs_brushpolys,
			rs_aliaspolys,
			rs_drawelements,
			rs_dynamiclightmaps);
	}
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

void RB_Clear (void)
{
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GLES:
	case RENDERPATH_GL30:
		{	
			// partition the depth range for 3D/2D stuff
			qglDepthRange (QGL_DEPTH_3D_BEGIN, QGL_DEPTH_3D_END);

			// set a black clear color
			qglClearColor (0, 0, 0, 1);

			// clear
			qglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
			break;
		}
	}
}

/*
=============
RB_PolygonOffset

negative offset moves polygon closer to camera
=============
*/
void RB_PolygonOffset (int offset)
{
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GLES:
	case RENDERPATH_GL30:
		{
			if (offset > 0)
			{
				qglEnable (GL_POLYGON_OFFSET_FILL);
				qglEnable (GL_POLYGON_OFFSET_LINE);
				qglPolygonOffset(1, offset);
			}
			else if (offset < 0)
			{
				qglEnable (GL_POLYGON_OFFSET_FILL);
				qglEnable (GL_POLYGON_OFFSET_LINE);
				qglPolygonOffset(-1, offset);
			}
			else
			{
				qglDisable (GL_POLYGON_OFFSET_FILL);
				qglDisable (GL_POLYGON_OFFSET_LINE);
			}
		}
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

void RB_RotateMatrixForEntity (vec3_t origin, vec3_t angles)
{
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GLES:
	case RENDERPATH_GL30:
		RB_GL11_RotateMatrixForEntity(origin, angles);
		break;
	}
}
