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

static void *r_currentindexes = NULL;

void GL_SetIndices (int indexBuffer, void *indexes)
{
	r_currentindexes = indexes;
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


typedef void (*PFNGLARRAYPOINTERFUNC) (int, GLenum, GLsizei, const GLvoid *);

// unfortunately OpenGL weenie-ism means that glNormalPointer, glEdgeFlagPointer and
// possibly some others take different params, so we need to wrap these.  bah.
void GL_VertexPointer (int size, GLenum type, GLsizei stride, const GLvoid *ptr) {qglVertexPointer (size, type, stride, ptr);}
void GL_ColorPointer (int size, GLenum type, GLsizei stride, const GLvoid *ptr) {qglColorPointer (size, type, stride, ptr);}
void GL_TexCoordPointer (int size, GLenum type, GLsizei stride, const GLvoid *ptr) {qglTexCoordPointer (size, type, stride, ptr);}

typedef struct gl_stream_s
{
	int vbonum;
	int desc;
	int size;
	GLenum type;
	int stride;
	void *ptr;
	PFNGLARRAYPOINTERFUNC ptrfunc;
	GLenum tmu;
	GLenum mode;
} gl_stream_t;

static gl_stream_t gl_streams[5] =
{
	{0, GLSTREAM_POSITION, 0, GL_NONE, 0, NULL, GL_VertexPointer, GL_NONE, GL_VERTEX_ARRAY},
	{0, GLSTREAM_COLOR, 0, GL_NONE, 0, NULL, GL_ColorPointer, GL_NONE, GL_COLOR_ARRAY},
	{0, GLSTREAM_TEXCOORD0, 0, GL_NONE, 0, NULL, GL_TexCoordPointer, GL_TEXTURE0_ARB, GL_TEXTURE_COORD_ARRAY},
	{0, GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL, GL_TexCoordPointer, GL_TEXTURE1_ARB, GL_TEXTURE_COORD_ARRAY},
	{0, GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL, GL_TexCoordPointer, GL_TEXTURE2_ARB, GL_TEXTURE_COORD_ARRAY}
};

static int gl_activestreams = 0;
static GLenum r_clientactivetexture = GL_INVALID_VALUE;

static void GL_ClientActiveTexture (gl_stream_t *stream)
{
	if (stream->tmu != r_clientactivetexture)
	{
		qglClientActiveTexture (stream->tmu);
		r_clientactivetexture = stream->tmu;
		gl_activestreams &= ~stream->desc;
	}
}


// note that NULL may be a valid ptr if a VBO is used?
// 0 is also a valid stride for tightly packed vertexes, so we need to check type == GL_NONE for disabled
void GL_SetStreamSource (int vbonum, int streamnum, int size, GLenum type, int stride, void *ptr)
{
	gl_stream_t *stream = NULL;

	switch (streamnum)
	{
	case GLSTREAM_POSITION:  stream = &gl_streams[0]; break;
	case GLSTREAM_COLOR:     stream = &gl_streams[1]; break;
	case GLSTREAM_TEXCOORD0: stream = &gl_streams[2]; break;
	case GLSTREAM_TEXCOORD1: stream = &gl_streams[3]; break;
	case GLSTREAM_TEXCOORD2: stream = &gl_streams[4]; break;
	default: return;	// unknown
	}

	if (type == GL_NONE)
	{
		if (gl_activestreams & stream->desc)
		{
			if (stream->tmu != GL_NONE)
				GL_ClientActiveTexture (stream);

			qglDisableClientState (stream->mode);
			gl_activestreams &= ~stream->desc;
		}

		// clear down the data to force a full update next time this stream is used
		stream->size = 0;
		stream->type = GL_NONE;
		stream->stride = 0;
		stream->ptr = NULL;
	}
	else
	{
		if (stream->tmu != GL_NONE)
			GL_ClientActiveTexture (stream);

		if (!(gl_activestreams & stream->desc))
		{
			qglEnableClientState (stream->mode);
			gl_activestreams |= stream->desc;
		}

		if (stream->size != size || stream->type != type || stream->stride != stride || stream->ptr != ptr)
		{
			stream->ptrfunc (size, type, stride, ptr);

			stream->size = size;
			stream->type = type;
			stream->stride = stride;
			stream->ptr = ptr;
		}
	}
}


void GL_DrawPrimitive (GLenum mode, int firstvert, int numverts)
{
	if (gl_streams[0].ptr)
	{
		if (qglLockArraysEXT && qglUnlockArraysEXT)
			qglLockArraysEXT (firstvert, numverts);

		qglDrawArrays (mode, firstvert, numverts);

		if (qglLockArraysEXT && qglUnlockArraysEXT)
			qglUnlockArraysEXT ();

		rs_drawelements++;
	}
}


void GL_DrawIndexedPrimitive (GLenum mode, int numIndexes, int numVertexes)
{
	if (gl_streams[0].ptr)
	{
		if (qglLockArraysEXT && qglUnlockArraysEXT)
			qglLockArraysEXT (0, numVertexes);

		qglDrawElements (mode, numIndexes, GL_UNSIGNED_SHORT, r_currentindexes);

		if (qglLockArraysEXT && qglUnlockArraysEXT)
			qglUnlockArraysEXT ();

		rs_drawelements++;
	}
}


void GL_UnbindBuffers (void)
{
	// unbind all buffer objects on a mode switch
	GL_SetIndices (0, NULL);
	GL_SetStreamSource (0, GLSTREAM_POSITION, 0, GL_NONE, 0, NULL);
	GL_SetStreamSource (0, GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);
	GL_SetStreamSource (0, GLSTREAM_TEXCOORD0, 0, GL_NONE, 0, NULL);
	GL_SetStreamSource (0, GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
	GL_SetStreamSource (0, GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);
}

