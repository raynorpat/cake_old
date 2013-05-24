// rb_backend.c: function calls to whatever renderering library we are using (OpenGL, OpenGL ES, Direct3D, etc)

#include "gl_local.h"

unsigned int	indexesArray[MAX_ARRAY_INDEXES];
vec4_t	vertexArray[MAX_ARRAY_VERTS];
vec3_t	normalsArray[MAX_ARRAY_VERTS];

vec2_t	inCoordsData[MAX_ARRAY_TEX_COORDS];
vec2_t	inLightmapCoordsData[MAX_ARRAY_TEX_COORDS];
vec4_t	inColorsData[MAX_ARRAY_COLORS];
int		numVerts, numIndexes, numCoords, numColors;

vec2_t	coordsArray[MAX_ARRAY_TEX_COORDS];
vec2_t	coordsArrayMtex[MAX_ARRAY_TEX_COORDS];
byte_vec4_t	colorArray[MAX_ARRAY_COLORS];
static	qbool	r_arrays_locked;

qbool r_blocked;

float	*currentVertex;
float	*currentNormal;
float	*currentCoords;
float	*currentLightmapCoords;
float	*currentColor;

unsigned int	r_numverts;
unsigned int	r_numtris;
unsigned int	r_numflushes;

unsigned int quad_elems[6] = { 0, 1, 2, 0, 2, 3 };

static void rb_backend_start(void)
{
	Com_Printf("Renderer backend started.\n");

	numVerts = 0;
	numIndexes = 0;
	numCoords = 0;
    numColors = 0;

	currentVertex = vertexArray[0];
	currentNormal = normalsArray[0];

	currentCoords = inCoordsData[0];
	currentLightmapCoords = inLightmapCoordsData[0];
	currentColor = inColorsData[0];

	r_arrays_locked = false;
	r_blocked = false;

	memset (vertexArray, 0, MAX_ARRAY_VERTS * sizeof(vec4_t));
	memset (normalsArray, 0, MAX_ARRAY_VERTS * sizeof(vec3_t));
	memset (coordsArray, 0, MAX_ARRAY_TEX_COORDS * sizeof(vec2_t));
	memset (coordsArrayMtex, 0, MAX_ARRAY_TEX_COORDS * sizeof(vec2_t));
	memset (indexesArray, 0, MAX_ARRAY_INDEXES * sizeof(int));
	memset (colorArray, 0, MAX_ARRAY_COLORS * sizeof(byte_vec4_t));

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
	r_numverts = 0;
	r_numtris = 0;
	r_numflushes = 0;

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
	// unlock arrays if any
	R_UnlockArrays ();

	if (r_speeds.value)
	{
		time2 = Sys_DoubleTime ();
		Com_Printf( "%3i ms  - %4i/%4i wpoly %4i epoly %4i verts %4i tris %4i flushes\n",
			(int)((time2-time1)*1000),
			c_brush_polys,
			c_brush_passes,
			c_alias_polys,
			r_numverts,
			r_numtris,
			r_numflushes );
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


void R_LockArrays (void)
{
	if ( r_arrays_locked )
		return;

	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GLES:
	case RENDERPATH_GL30:
		{
			if ( !gl_supportslockarrays )
				return;

			if ( qglLockArraysEXT != 0 ) {
				qglLockArraysEXT( 0, numVerts );
				r_arrays_locked = true;
			}
			break;
		}
	case RENDERPATH_D3D:
		Sys_Error( "TODO: D3D" );
		break;
	}
}

void R_UnlockArrays (void)
{
	if ( !r_arrays_locked )
		return;

	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GLES:
	case RENDERPATH_GL30:
		{
			if ( qglUnlockArraysEXT != 0 ) {
				qglUnlockArraysEXT();
				r_arrays_locked = false;
			}
			break;
		}
	case RENDERPATH_D3D:
		Sys_Error( "TODO: D3D" );
		break;
	}
}

void R_ClearArrays ( void )
{
	numVerts = 0;
	numIndexes = 0;
	numCoords = 0;

	currentVertex = vertexArray[0];
	currentNormal = normalsArray[0];

	currentCoords = inCoordsData[0];
	currentLightmapCoords = inLightmapCoordsData[0];

	r_blocked = false;
}

void R_FlushArrays ( void )
{
	if ( !numVerts || !numIndexes ) {
		return;
	}

	if ( numColors > 1 ) {
		qglEnableClientState( GL_COLOR_ARRAY );
	} else if ( numColors == 1 ) {
		qglColor4ubv ( colorArray[0] );
	}

	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );

	qglDrawElements( GL_TRIANGLES, numIndexes, GL_UNSIGNED_INT,	indexesArray );

	if ( numColors > 1 ) {
		qglDisableClientState( GL_COLOR_ARRAY );
	}

	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );

	numColors = 0;
	currentColor = inColorsData[0];
	currentCoords = inCoordsData[0];
	currentLightmapCoords = inLightmapCoordsData[0];

	r_numtris += numIndexes / 3;
	r_numflushes++;
}

void R_FlushArraysMtex ( int tex1, int tex2 )
{
	if ( !numVerts || !numIndexes ) {
		return;
	}

	GL_MBind( GL_TEXTURE0_ARB, tex1 );

	if ( numColors > 1 ) {
		qglEnableClientState( GL_COLOR_ARRAY );
	} else if ( numColors == 1 ) {
		qglColor4ubv ( colorArray[0] );
	}

	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );

	GL_MBind( GL_TEXTURE1_ARB, tex2 );

	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );

	qglDrawElements( GL_TRIANGLES, numIndexes, GL_UNSIGNED_INT, indexesArray );

	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );

	GL_SelectTexture ( GL_TEXTURE0_ARB );

	if ( numColors > 1 ) {
		qglDisableClientState( GL_COLOR_ARRAY );
	}

	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );

	numColors = 0;
	currentColor = inColorsData[0];
	currentCoords = inCoordsData[0];
	currentLightmapCoords = inLightmapCoordsData[0];

	r_numtris += numIndexes / 3;
	r_numflushes++;
}

void R_VertexTCBase ( int tc_gen, qbool mtex )
{
	int i;

	for ( i = 0; i < numVerts; i++  ) {
		if ( tc_gen == 1 ) {
			if ( mtex ) {
				coordsArrayMtex[i][0] = inLightmapCoordsData[i][0];
				coordsArrayMtex[i][1] = inLightmapCoordsData[i][1];
			} else {
				coordsArray[i][0] = inLightmapCoordsData[i][0];
				coordsArray[i][1] = inLightmapCoordsData[i][1];
			}
		} else if ( tc_gen == 0 ) {
			if ( mtex ) {
				coordsArrayMtex[i][0] = inCoordsData[i][0];
				coordsArrayMtex[i][1] = inCoordsData[i][1];
			} else {
				coordsArray[i][0] = inCoordsData[i][0];
				coordsArray[i][1] = inCoordsData[i][1];
			}
		}
	}
}
