// rb_backend_ogl.c: function calls to OpenGL

#include "gl_local.h"

extern vec2_t		coordsArray[MAX_ARRAY_TEX_COORDS];
extern vec2_t		coordsArrayMtex[MAX_ARRAY_TEX_COORDS];
extern byte_vec4_t	colorArray[MAX_ARRAY_COLORS];

/*
===============
RB_GL_Init
===============
*/
void RB_GL_Init (void)
{
	qglClearColor (1,0,0,0);
	qglCullFace(GL_FRONT);
	qglEnable(GL_TEXTURE_2D);

	qglEnable(GL_ALPHA_TEST);
	qglAlphaFunc(GL_GREATER, 0.666);

//	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
//	glShadeModel (GL_FLAT);

	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

//	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	GL_SelectTexture ( GL_TEXTURE1_ARB );
	qglTexCoordPointer( 2, GL_FLOAT, 0, coordsArrayMtex );

	GL_SelectTexture ( GL_TEXTURE0_ARB );
	qglVertexPointer( 3, GL_FLOAT, 16, vertexArray );	// padded for SIMD
	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );
	qglTexCoordPointer( 2, GL_FLOAT, 0, coordsArray );

	qglEnableClientState( GL_VERTEX_ARRAY );
}
