// rb_backend.c: function calls to whatever renderering library we are using (OpenGL, OpenGL ES, Direct3D, etc)

#include "gl_local.h"

unsigned int	indexesArray[MAX_ARRAY_INDEXES];
vec4_t	vertexArray[MAX_ARRAY_VERTS];
vec3_t	normalsArray[MAX_ARRAY_VERTS];

vec2_t	inCoordsData[MAX_ARRAY_TEX_COORDS];
vec2_t	inLightmapCoordsData[MAX_ARRAY_TEX_COORDS];
vec4_t	inColorsData[MAX_ARRAY_COLORS];
int		numVerts, numIndexes, numCoords, numColors;

static	vec2_t	coordsArray[MAX_ARRAY_TEX_COORDS];
static	vec2_t	coordsArrayMtex[MAX_ARRAY_TEX_COORDS];
static	byte_vec4_t	colorArray[MAX_ARRAY_COLORS];
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

void RB_InitBackend (void)
{
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

	GL_SelectTexture ( GL_TEXTURE1_ARB );
	qglTexCoordPointer( 2, GL_FLOAT, 0, coordsArrayMtex );

	GL_SelectTexture ( GL_TEXTURE0_ARB );
	qglVertexPointer( 3, GL_FLOAT, 16, vertexArray );	// padded for SIMD
	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );
	qglTexCoordPointer( 2, GL_FLOAT, 0, coordsArray );

	qglEnableClientState( GL_VERTEX_ARRAY );
}

void RB_StartFrame (void)
{
	r_numverts = 0;
	r_numtris = 0;
	r_numflushes = 0;
}

void RB_EndFrame (void)
{
	if (r_speeds.value)
	{
		Com_Printf( "%4i wpoly %4i verts %4i tris %4i flushes\n",
			c_brush_polys, 
			r_numverts,
			r_numtris,
			r_numflushes ); 
	}
}

void R_LockArrays (void)
{
	if ( r_arrays_locked )
		return;

	if ( !gl_supportslockarrays )
		return;

	if ( qglLockArraysEXT != 0 ) {
		qglLockArraysEXT( 0, numVerts );
		r_arrays_locked = true;
	}
}

void R_UnlockArrays (void)
{
	if ( !r_arrays_locked )
		return;

	if ( qglUnlockArraysEXT != 0 ) {
		qglUnlockArraysEXT();
		r_arrays_locked = false;
	}
}

qbool R_MeshWillNotFit ( int numvertexes, int numindexes )
{
	return ( numVerts + numvertexes > MAX_ARRAY_VERTS ||
		numIndexes + numindexes > MAX_ARRAY_INDEXES );
}

/*
==============
R_DrawTriangleStrips

This function looks for and sends tristrips.
Original Code by Stephen C. Taylor (Aftershock 3D rendering engine)
==============
*/
static void R_DrawTriangleStrips ( void )
{
    int toggle;
    unsigned int a, b, c, *index;

	c = 0;
    index = indexesArray;
    while ( c < numIndexes ) {
		toggle = 1;

		qglBegin( GL_TRIANGLE_STRIP );
		
		qglArrayElement( index[0] );
		qglArrayElement( b = index[1] );
		qglArrayElement( a = index[2] );

		c += 3;
		index += 3;

		while ( c < numIndexes ) {
			if ( a != index[0] || b != index[1] ) {
				break;
			}

			if ( toggle ) {
				qglArrayElement( b = index[2] );
			} else {
				qglArrayElement( a = index[2] );
			}

			c += 3;
			index += 3;
			toggle = !toggle;
		}

		qglEnd();
    }
}

void R_ClearArrays (void)
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

void R_FlushArrays (void)
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

	if ( !r_arrays_locked ) {
		R_DrawTriangleStrips ();
	}
	else {
		qglDrawElements( GL_TRIANGLES, numIndexes, GL_UNSIGNED_INT,
			indexesArray );
	}
	r_numtris += numIndexes / 3;

	if ( numColors > 1 ) {
		qglDisableClientState( GL_COLOR_ARRAY );
	}

	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );

	numColors = 0;
	currentColor = inColorsData[0];
	currentCoords = inCoordsData[0];
	currentLightmapCoords = inLightmapCoordsData[0];

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

	if ( !r_arrays_locked ) {
		R_DrawTriangleStrips ();
	} else {
		qglDrawElements( GL_TRIANGLES, numIndexes, GL_UNSIGNED_INT,
			indexesArray );
	}
	r_numtris += numIndexes / 3;

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

	r_numflushes++;
}

/*
** R_DrawTriangleOutlines
*/
void R_DrawTriangleOutlines (void)
{
	if ( !developer.value ) {
		return;
	}

	qglDisable( GL_TEXTURE_2D );
	qglDisable( GL_DEPTH_TEST );
	qglColor4f( 1, 1, 1, 1 );
	qglDisable( GL_BLEND );
//	qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

	R_FlushArrays();

//	qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	qglEnable( GL_DEPTH_TEST );
	qglEnable( GL_TEXTURE_2D );
	qglEnable( GL_BLEND );
}
