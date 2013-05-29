/*
Copyright (C) 1996-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "gl_local.h"

int r_vastate = 0;
int r_vertexsize = 0;
GLenum r_clientactivetexture = GL_INVALID_VALUE;
cvar_t r_primitives = {"r_primitives", "2", CVAR_ARCHIVE};

static GLuint r_currentindexbuffer = 0;


#define VA_VERTEX	1
#define VA_COLOR	2
#define VA_TEX0		4
#define VA_TEX1		8
#define VA_TEX2		16


void GL_ClientActiveTexture (GLenum tex)
{
	if (tex != r_clientactivetexture)
	{
		qglClientActiveTexture (tex);
		r_clientactivetexture = tex;
	}
}


float *r_vertexpointer = NULL;
byte *r_colorpointer = NULL;
float *r_texcoordpointer[3] = {NULL, NULL, NULL};

void R_VertexPointer (int vertexsize, float *ptr)
{
	if (!(r_vastate & VA_VERTEX))
		qglEnableClientState (GL_VERTEX_ARRAY);

	if (r_vertexpointer != ptr)
	{
		qglVertexPointer (3, GL_FLOAT, vertexsize, ptr);
		r_vertexpointer = ptr;
	}
}

void R_ColorPointer (int vertexsize, byte *ptr)
{
	if (!(r_vastate & VA_COLOR))
		qglEnableClientState (GL_COLOR_ARRAY);

	if (r_colorpointer != ptr)
	{
		qglColorPointer (4, GL_UNSIGNED_BYTE, vertexsize, ptr);
		r_colorpointer = ptr;
	}
}

void R_TexCoordPointer (int tmu, int state, int vertexsize, float *ptr)
{
	if (!(r_vastate & state))
	{
		GL_ClientActiveTexture (GL_TEXTURE0_ARB + tmu);
		qglEnableClientState (GL_TEXTURE_COORD_ARRAY);
	}

	if (r_texcoordpointer[tmu] != ptr)
	{
		GL_ClientActiveTexture (GL_TEXTURE0_ARB + tmu);

		// this is a hack for the skybox cubemap
		if (ptr == r_vertexpointer)
			qglTexCoordPointer (3, GL_FLOAT, vertexsize, ptr);
		else qglTexCoordPointer (2, GL_FLOAT, vertexsize, ptr);

		r_texcoordpointer[tmu] = ptr;
	}
}


void R_EnableVertexArrays (float *v, byte *c, float *st1, float *st2, float *st3, int vertexsize)
{
	int newstate = 0;

	if (r_vertexsize != vertexsize)
	{
		// clear the pointers so the cached versions won't be used
		r_vertexpointer = NULL;
		r_colorpointer = NULL;
		r_texcoordpointer[0] = r_texcoordpointer[1] = r_texcoordpointer[2] = NULL;
		r_vertexsize = vertexsize;
	}

	if (v)
	{
		R_VertexPointer (vertexsize, v);
		newstate |= VA_VERTEX;
	}

	if (c)
	{
		R_ColorPointer (vertexsize, c);
		newstate |= VA_COLOR;
	}

	if (st1)
	{
		R_TexCoordPointer (0, VA_TEX0, vertexsize, st1);
		newstate |= VA_TEX0;
	}

	if (st2)
	{
		R_TexCoordPointer (1, VA_TEX1, vertexsize, st2);
		newstate |= VA_TEX1;
	}

	if (st3)
	{
		R_TexCoordPointer (2, VA_TEX2, vertexsize, st3);
		newstate |= VA_TEX2;
	}

	// see if there is anything to take down
	r_vastate &= ~newstate;
	R_DisableVertexArrays ();
	r_vastate = newstate;
}


void R_DisableVertexArrays (void)
{
	if (r_vastate & VA_TEX2)
	{
		GL_ClientActiveTexture (GL_TEXTURE2_ARB);
		qglDisableClientState (GL_TEXTURE_COORD_ARRAY);
		r_texcoordpointer[2] = NULL;
	}

	if (r_vastate & VA_TEX1)
	{
		GL_ClientActiveTexture (GL_TEXTURE1_ARB);
		qglDisableClientState (GL_TEXTURE_COORD_ARRAY);
		r_texcoordpointer[1] = NULL;
	}

	if (r_vastate & VA_TEX0)
	{
		GL_ClientActiveTexture (GL_TEXTURE0_ARB);
		qglDisableClientState (GL_TEXTURE_COORD_ARRAY);
		r_texcoordpointer[0] = NULL;
	}

	if (r_vastate & VA_COLOR)
	{
		qglDisableClientState (GL_COLOR_ARRAY);
		r_colorpointer = NULL;

		// current colour is undefined after this
		GL_Color (1, 1, 1, 1);
	}

	if (r_vastate & VA_VERTEX)
	{
		qglDisableClientState (GL_VERTEX_ARRAY);
		r_vertexpointer = NULL;
	}

	if (gl_support_arb_vertex_buffer_object)
	{
		// unbind any index buffer in use
		if (r_currentindexbuffer)
		{
			qglBindBufferARB (GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
			r_currentindexbuffer = 0;
		}
	}

	r_vastate = 0;
	// r_clientactivetexture = GL_INVALID_VALUE;
}


unsigned short *R_TransferIndexes (unsigned short *src, unsigned short *dst, int numindexes, int offset)
{
	int i;

	// fast-transfer the indexes
	while (numindexes > 15)
	{
		dst[0] = src[0] + offset;
		dst[1] = src[1] + offset;
		dst[2] = src[2] + offset;
		dst[3] = src[3] + offset;
		dst[4] = src[4] + offset;
		dst[5] = src[5] + offset;
		dst[6] = src[6] + offset;
		dst[7] = src[7] + offset;
		dst[8] = src[8] + offset;
		dst[9] = src[9] + offset;
		dst[10] = src[10] + offset;
		dst[11] = src[11] + offset;
		dst[12] = src[12] + offset;
		dst[13] = src[13] + offset;
		dst[14] = src[14] + offset;
		dst[15] = src[15] + offset;

		dst += 16;
		src += 16;
		numindexes -= 16;
	}

	while (numindexes > 7)
	{
		dst[0] = src[0] + offset;
		dst[1] = src[1] + offset;
		dst[2] = src[2] + offset;
		dst[3] = src[3] + offset;
		dst[4] = src[4] + offset;
		dst[5] = src[5] + offset;
		dst[6] = src[6] + offset;
		dst[7] = src[7] + offset;

		dst += 8;
		src += 8;
		numindexes -= 8;
	}

	while (numindexes > 3)
	{
		dst[0] = src[0] + offset;
		dst[1] = src[1] + offset;
		dst[2] = src[2] + offset;
		dst[3] = src[3] + offset;

		dst += 4;
		src += 4;
		numindexes -= 4;
	}

	for (i = 0; i < numindexes; i++)
		dst[i] = src[i] + offset;

	return (dst + numindexes);
}


/*
=========================================================================================================================================

			RENDERING

	All primitive rendering in the engine goes through here.  This was shamelessly copied from Q3A and allows for comparison
	of different drawing types and selection of the fastest, via the r_primitives cvar.

	1: using glBegin/glEnd with glArrayElement
	2: using glDrawElements or glDrawArrays
	3: using glBegin/glEnd/glTexCoord, etc

=========================================================================================================================================
*/
void APIENTRY R_ArrayElementDiscrete (GLint index)
{
	int vertoffset = ((r_vertexsize * index) >> 2);

	if (r_colorpointer)
	{
		// need to fix up the offset here.....
		float *ptr = r_vertexpointer + vertoffset;
		qglColor4ubv ((byte *) (ptr + 3));
	}

	if (r_texcoordpointer[1] || r_texcoordpointer[2])
	{
		if (r_texcoordpointer[2])
		{
			float *ptr = r_texcoordpointer[2] + vertoffset;
			qglMultiTexCoord2f (GL_TEXTURE2_ARB, ptr[0], ptr[1]);
		}

		if (r_texcoordpointer[1])
		{
			float *ptr = r_texcoordpointer[1] + vertoffset;
			qglMultiTexCoord2f (GL_TEXTURE1_ARB, ptr[0], ptr[1]);
		}

		if (r_texcoordpointer[0])
		{
			float *ptr = r_texcoordpointer[0] + vertoffset;
			qglMultiTexCoord2f (GL_TEXTURE0_ARB, ptr[0], ptr[1]);
		}
	}
	else
	{
		if (r_texcoordpointer[0] == r_vertexpointer)
		{
			float *ptr = r_vertexpointer + vertoffset;
			qglTexCoord3fv (ptr);
		}
		else if (r_texcoordpointer[0])
		{
			float *ptr = r_texcoordpointer[0] + vertoffset;
			qglTexCoord2fv (ptr);
		}
	}

	if (r_vertexpointer)
	{
		float *ptr = r_vertexpointer + vertoffset;
		qglVertex3fv (ptr);
	}
}


void R_DrawStripElements (int numIndexes, const unsigned short *indexes, void (APIENTRY *element) (GLint))
{
	int i;
	int last[3] = {-1, -1, -1};
	qbool even;

	if (numIndexes <= 0)
		return;

	qglBegin (GL_TRIANGLE_STRIP);
	rs_drawelements++;

	// prime the strip
	element (indexes[0]);
	element (indexes[1]);
	element (indexes[2]);

	last[0] = indexes[0];
	last[1] = indexes[1];
	last[2] = indexes[2];

	even = false;

	for (i = 3; i < numIndexes; i += 3)
	{
		// odd numbered triangle in potential strip
		if (!even)
		{
			// check previous triangle to see if we're continuing a strip
			if ((indexes[i + 0] == last[2]) && (indexes[i + 1] == last[1]))
			{
				element (indexes[i + 2]);
				even = true;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				qglEnd();
				qglBegin (GL_TRIANGLE_STRIP);
				rs_drawelements++;
				element (indexes[i + 0]);
				element (indexes[i + 1]);
				element (indexes[i + 2]);
				even = false;
			}
		}
		else
		{
			// check previous triangle to see if we're continuing a strip
			if ((last[2] == indexes[i + 1]) && (last[0] == indexes[i + 0]))
			{
				element (indexes[i + 2]);
				even = false;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				qglEnd();
				qglBegin (GL_TRIANGLE_STRIP);
				rs_drawelements++;
				element (indexes[i + 0]);
				element (indexes[i + 1]);
				element (indexes[i + 2]);
				even = false;
			}
		}

		// cache the last three vertices
		last[0] = indexes[i + 0];
		last[1] = indexes[i + 1];
		last[2] = indexes[i + 2];
	}

	qglEnd();
}


static int r_drawtype = -1;

void R_GetPrimitiveType (void)
{
	int primitives = (int) r_primitives.value;

	// default is to use triangles if compiled vertex arrays are present
	if (primitives == 0)
	{
		if (gl_supportslockarrays)
			primitives = 2;
		else primitives = 1;
	}

	if (primitives != r_drawtype)
	{
		if (primitives == 2)
			Com_Printf ("Using glDrawElements\n");
		else if (primitives == 1)
			Com_Printf ("Using glArrayElement\n");
		else if (primitives == 3)
			Com_Printf ("Using R_ArrayElementDiscrete\n");
		else Com_Printf ("Using glDrawElements\n");

		r_drawtype = primitives;
	}
}


void R_DrawArraysImmediate (GLenum mode, int firstvert, int numverts, void (APIENTRY *element) (GLint))
{
	int i;

	qglBegin (mode);

	for (i = 0; i < numverts; i++)
		element (firstvert + i);

	qglEnd ();
	rs_drawelements++;
}


void R_DrawArrays (GLenum mode, int firstvert, int numverts)
{
	if (gl_support_arb_vertex_buffer_object)
	{
		// unbind any index buffer in use
		if (r_currentindexbuffer)
		{
			qglBindBufferARB (GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
			r_currentindexbuffer = 0;
		}
	}

	if (r_drawtype == 3)
	{
		R_DrawArraysImmediate (mode, firstvert, numverts, R_ArrayElementDiscrete);
		return;
	}

	if (r_drawtype == 1)
		R_DrawArraysImmediate (mode, firstvert, numverts, qglArrayElement);
	else
	{
		qglDrawArrays (mode, firstvert, numverts);
		rs_drawelements++;
	}
}


void R_DrawElements (int numIndexes, int numVertexes, const unsigned short *indexes)
{
	if (gl_support_arb_vertex_buffer_object)
	{
		// unbind any index buffer in use
		if (r_currentindexbuffer)
		{
			qglBindBufferARB (GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
			r_currentindexbuffer = 0;
		}
	}

	if (r_drawtype == 3)
	{
		R_DrawStripElements (numIndexes, indexes, R_ArrayElementDiscrete);
		return;
	}

	if (r_drawtype == 1)
		R_DrawStripElements (numIndexes, indexes, qglArrayElement);
	else
	{
		qglDrawElements (GL_TRIANGLES, numIndexes, GL_UNSIGNED_SHORT, indexes);
		rs_drawelements++;
	}
}


void R_DrawBufferElements (int firstIndex, int numIndexes, int indexBuffer)
{
	if (gl_support_arb_vertex_buffer_object)
	{
		// update any index buffer in use
		if (r_currentindexbuffer != indexBuffer)
		{
			qglBindBufferARB (GL_ELEMENT_ARRAY_BUFFER_ARB, indexBuffer);
			r_currentindexbuffer = indexBuffer;
		}

		qglDrawElements (GL_TRIANGLES, numIndexes, GL_UNSIGNED_SHORT, (void *) firstIndex);
		rs_drawelements++;
	}
}
