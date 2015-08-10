/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// gl_sky.c -- sky polygons

#include "gl_local.h"
#include "rc_image.h"

void R_InitializeSkySphere (void);

extern	model_t	*loadmodel;

gltexture_t	*solidskytexture, *alphaskytexture;
char skybox_name[256] = {0};
extern GLuint r_surfacevbo;

qbool sky_initialized = false;

int		skytexorder[6] = {0, 2, 1, 3, 4, 5}; // for skybox


void GL_SkyBindSkybox (int layer)
{
}


//==============================================================================
//
//  INIT
//
//==============================================================================

/*
=============
Sky_LoadTexture

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void Sky_LoadTexture (texture_t *mt, byte *data)
{
	char		texturename[64];
	int			i, j;
	byte		*src;
	static byte	front_data[128*128]; //FIXME: Hunk_Alloc
	static byte	back_data[128*128]; //FIXME: Hunk_Alloc

	src = data;

	// extract back layer and upload
	for (i=0 ; i<128 ; i++)
	{
		for (j=0 ; j<128 ; j++)
			back_data[(i*128) + j] = src[i*256 + j + 128];
	}

	sprintf(texturename, "%s:%s_back", loadmodel->name, mt->name);
	solidskytexture = TexMgr_LoadImage (loadmodel, texturename, 128, 128, SRC_INDEXED, back_data, "", (unsigned)back_data, TEXPREF_MIPMAP);

	// extract front layer and upload
	for (i=0 ; i<128 ; i++)
	{
		for (j=0 ; j<128 ; j++)
		{
			front_data[(i*128) + j] = src[i*256 + j];

			// we need to store the original alpha colour so that we can properly set r_skyalpha
			if (front_data[(i*128) + j] == 0)
			{
				front_data[(i*128) + j] = 255;
			}
		}
	}

	sprintf(texturename, "%s:%s_front", loadmodel->name, mt->name);
	alphaskytexture = TexMgr_LoadImage (loadmodel, texturename, 128, 128, SRC_INDEXED, front_data, "", (unsigned)front_data, TEXPREF_MIPMAP);
}


// ==============================================================================
//
//  LOAD SKYBOX AS A CUBEMAP - DO NOT MESS WITH THIS; IT WORKS ON ATI NOW
//
// ==============================================================================

GLuint skyCubeTexture = 0;

void Sky_ClearSkybox (void)
{
	// purge old texture
	if (skyCubeTexture)
	{
		qglDeleteTextures (1, &skyCubeTexture);
		skyCubeTexture = 0;
	}

	skybox_name[0] = 0;
}


void Sky_ReloadSkybox (void)
{
	char name[256];

	// copy out the skybox name and invalidate it to force a reload
	strcpy (name, skybox_name);
	Sky_ClearSkybox ();
	Sky_LoadCubeMap (name);
}


/*
==================
Sky_LoadCubeMap
==================
*/
char *suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
int loadorder[6] = {0, 2, 1, 3, 4, 5};

/*
GLenum cubefaces[6] =
{
	GL_TEXTURE_CUBE_MAP_POSITIVE_X,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
};
*/

void D3D_FlipTexels (unsigned int *texels, int width, int height)
{
	int x, y;

	for (x = 0; x < width; x++)
	{
		for (y = 0; y < (height / 2); y++)
		{
			int pos1 = y * width + x;
			int pos2 = (height - 1 - y) * width + x;

			unsigned int temp = texels[pos1];
			texels[pos1] = texels[pos2];
			texels[pos2] = temp;
		}
	}
}


void D3D_MirrorTexels (unsigned int *texels, int width, int height)
{
	int x, y;

	for (x = 0; x < (width / 2); x++)
	{
		for (y = 0; y < height; y++)
		{
			int pos1 = y * width + x;
			int pos2 = y * width + (width - 1 - x);

			unsigned int temp = texels[pos1];
			texels[pos1] = texels[pos2];
			texels[pos2] = temp;
		}
	}
}


void D3D_RotateTexelsInPlace (unsigned int *texels, int size)
{
	int i, j;

	for (i = 0; i < size; i++)
	{
		for (j = i; j < size; j++)
		{
			int pos1 = i * size + j;
			int pos2 = j * size + i;

			unsigned int temp = texels[pos1];

			texels[pos1] = texels[pos2];
			texels[pos2] = temp;
		}
	}
}


void GL_Resample32BitTexture (unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight);

void Sky_LoadCubeMap (char *name)
{
#if 0
	int		i, mark, width[6], height[6];
	char	filename[MAX_OSPATH];
	byte	*data[6];
	qbool nonefound = true;
	int largest;

	// so that I can call Sky_LoadCubeMap (NULL) to flush a texture.
	if (!name)
	{
		// purge old texture
		Sky_ClearSkybox ();
		return;
	}

	// no change
	if (strcmp (skybox_name, name) == 0)
		return;

	// purge old texture
	Sky_ClearSkybox ();

	// turn off skybox if sky is set to ""
	if (name[0] == 0)
	{
		skybox_name[0] = 0;
		return;
	}

	mark = Hunk_LowMark ();

	// skybox faces must all be square and the same dimension so track the largest
	largest = 0;

	// load textures
	for (i=0 ; i<6 ; i++)
	{
		sprintf (filename, "gfx/env/%s%s", name, suf[i]);
		data[i] = Image_LoadImage (filename, &width[i], &height[i]);

		if (data[i])
		{
			// skybox faces must all be square and the same dimension so track the largest
			if (width[i] > largest) largest = width[i];
			if (height[i] > largest) largest = height[i];
		}
		else width[i] = height[i] = 0;
	}

	// fixme - this could get a mite cleaner
	if (largest > 0)
	{
		// now let's see what we got
		byte *cubebuffer = NULL;

		glBindTexture (GL_TEXTURE_2D, 0);
		glDisable (GL_TEXTURE_2D);
		glEnable (GL_TEXTURE_CUBE_MAP);

		glGenTextures (1, &skyCubeTexture);
		glBindTexture (GL_TEXTURE_CUBE_MAP, skyCubeTexture);

		glTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		// ATI strikes again - cubemaps should be loaded in a specific order
		for (i = 0; i < 6; i++)
		{
			if (!data[loadorder[i]])
			{
				if (!cubebuffer) cubebuffer = (byte *) Hunk_Alloc (largest * largest * 4);

				memset (cubebuffer, 0, largest * largest * 4);
				data[loadorder[i]] = cubebuffer;
				width[loadorder[i]] = largest;
				height[loadorder[i]] = largest;
			}

			if (width[loadorder[i]] != largest || height[loadorder[i]] != largest)
			{
				if (!cubebuffer) cubebuffer = (byte *) Hunk_Alloc (largest * largest * 4);

				// upsize to cube buffer and set back
				GL_Resample32BitTexture ((unsigned *) data[loadorder[i]], width[loadorder[i]], height[loadorder[i]], (unsigned *) cubebuffer, largest, largest);

				data[loadorder[i]] = cubebuffer;
				width[loadorder[i]] = largest;
				height[loadorder[i]] = largest;
			}

			switch (loadorder[i])
			{
			case 0:
				D3D_RotateTexelsInPlace ((unsigned int *) data[0], width[0]);
				break;

			case 1:
				D3D_FlipTexels ((unsigned int *) data[1], width[1], height[1]);
				break;

			case 2:
				D3D_RotateTexelsInPlace ((unsigned int *) data[2], width[2]);
				D3D_MirrorTexels ((unsigned int *) data[2], width[2], height[2]);
				D3D_FlipTexels ((unsigned int *) data[2], width[2], height[2]);
				break;

			case 3:
				D3D_MirrorTexels ((unsigned int *) data[3], width[3], height[3]);
				break;

			case 4:
				D3D_RotateTexelsInPlace ((unsigned int *) data[4], width[4]);
				break;

			case 5:
				D3D_RotateTexelsInPlace ((unsigned int *) data[5], width[5]);
				break;
			}

			// standard face
			glTexImage2D (cubefaces[i], 0, GL_RGBA8, largest, largest, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data[loadorder[i]]);

			nonefound = false;
		}

		glBindTexture (GL_TEXTURE_CUBE_MAP, 0);
		glDisable (GL_TEXTURE_CUBE_MAP);
		glEnable (GL_TEXTURE_2D);
		GL_BindTexture (GL_TEXTURE0, NULL);
	}

	Hunk_FreeToLowMark (mark);

	if (nonefound) // go back to scrolling sky if skybox is totally missing
	{
		Sky_ClearSkybox ();
		Com_Printf ("Couldn't load %s\n", name);
		return;
	}

	strcpy (skybox_name, name);
	Com_DPrintf ("loaded skybox %s OK\n", name);
#endif
}


/*
=================
Sky_NewMap
=================
*/
char *sbkeys[] =
{
	"sky",
	"skyname",
	"q1sky",
	"skybox",
	NULL
};

void Sky_NewMap (void)
{
	char	key[128], value[4096];
	char	*data;
	int		i;

	// initially no sky
	Sky_ClearSkybox ();

	// reload the sphere
	R_InitializeSkySphere ();

	// read worldspawn (this is so ugly, and shouldn't it be done on the server?)
	data = r_worldmodel->entities;
	if (!data)
		return;

	data = COM_Parse(data);
	if (!data) // should never happen
		return; // error
	if (com_token[0] != '{') // should never happen
		return; // error
	while (1)
	{
		data = COM_Parse(data);
		if (!data)
			return; // error
		if (com_token[0] == '}')
			break; // end of worldspawn
		if (com_token[0] == '_')
			strcpy(key, com_token + 1);
		else
			strcpy(key, com_token);
		while (key[strlen(key)-1] == ' ') // remove trailing spaces
			key[strlen(key)-1] = 0;
		data = COM_Parse(data);
		if (!data)
			return; // error
		strcpy(value, com_token);

		// better key testing
		for (i = 0; ; i++)
		{
			// no more
			if (!sbkeys[i]) break;

			if (!strcmp (sbkeys[i], key))
			{
				skybox_name[0] = 0;
				Sky_LoadCubeMap (value);
			}
		}
	}
}

/*
=================
Sky_SkyCommand_f
=================
*/
void Sky_SkyCommand_f (void)
{
	switch (Cmd_Argc())
	{
	case 1:
		// give the correct command
		Com_Printf ("\"%s\" is \"%s\"\n", Cmd_Argv (0), skybox_name);
		break;

	case 2:
		Sky_LoadCubeMap (Cmd_Argv (1));
		break;

	default:
		// give the correct command
		Com_Printf ("usage: %s <skyname>\n", Cmd_Argv (0));
	}
}

/*
=============
Sky_Init
=============
*/
void Sky_Init (void)
{
	Cmd_AddCommand ("sky", Sky_SkyCommand_f);
	Cmd_AddCommand ("loadsky", Sky_SkyCommand_f);
}


//==============================================================================
//
//  RENDER CLOUDS
//
//==============================================================================

// let's distinguish properly between preproccessor defines and variables
// (LH apparently doesn't believe in this, but I do)
#define SKYGRID_SIZE 16
#define SKYGRID_SIZE_PLUS_1 (SKYGRID_SIZE + 1)
#define SKYGRID_RECIP (1.0f / (SKYGRID_SIZE))
#define SKYSPHERE_NUMVERTS (SKYGRID_SIZE_PLUS_1 * SKYGRID_SIZE_PLUS_1)
#define SKYSPHERE_NUMTRIS (SKYGRID_SIZE * SKYGRID_SIZE * 2)
#define SKYSPHERE_NUMINDEXES (SKYGRID_SIZE * SKYGRID_SIZE * 6)


typedef struct dpskyvert_s
{
	float xyz[3];
	float st[2];
} dpskyvert_t;

dpskyvert_t r_dpskyverts[SKYSPHERE_NUMVERTS];
unsigned short r_dpskyindexes[SKYSPHERE_NUMINDEXES];
qbool r_skysphere_initialized = false;

void R_InitializeSkySphere (void)
{
	int i, j, vert;
	float a, b, x, ax, ay, v[3], length;
	float dx, dy, dz;
	unsigned short *e = r_dpskyindexes;

	dx = 16;
	dy = 16;
	dz = 16 / 3;

	for (vert = 0, j = 0; j <= SKYGRID_SIZE; j++)
	{
		a = j * SKYGRID_RECIP;
		ax = cos (a * M_PI * 2);
		ay = -sin (a * M_PI * 2);

		for (i = 0; i <= SKYGRID_SIZE; i++, vert++)
		{
			b = i * SKYGRID_RECIP;
			x = cos ((b + 0.5) * M_PI);

			v[0] = ax * x * dx;
			v[1] = ay * x * dy;
			v[2] = -sin ((b + 0.5) * M_PI) * dz;

			// same calculation as classic Q1 sky but projected onto an actual physical sphere
			// (rather than on flat surfs) and calced as if from an origin of [0,0,0] to prevent
			// the heaving and buckling effect
			length = 3.0f / sqrt (v[0] * v[0] + v[1] * v[1] + (v[2] * v[2] * 9));

			r_dpskyverts[vert].st[0] = v[0] * length;
			r_dpskyverts[vert].st[1] = v[1] * length;

			r_dpskyverts[vert].xyz[0] = v[0];
			r_dpskyverts[vert].xyz[1] = v[1];
			r_dpskyverts[vert].xyz[2] = v[2];
		}
	}

	for (j = 0; j < SKYGRID_SIZE; j++)
	{
		for (i = 0; i < SKYGRID_SIZE; i++)
		{
			*e++ =  j * SKYGRID_SIZE_PLUS_1 + i;
			*e++ =  j * SKYGRID_SIZE_PLUS_1 + i + 1;
			*e++ = (j + 1) * SKYGRID_SIZE_PLUS_1 + i;

			*e++ =  j * SKYGRID_SIZE_PLUS_1 + i + 1;
			*e++ = (j + 1) * SKYGRID_SIZE_PLUS_1 + i + 1;
			*e++ = (j + 1) * SKYGRID_SIZE_PLUS_1 + i;
		}
	}
}


void R_ScrollSkyMatrix (float speed)
{
	float scroll = cl.time * speed;
	glmatrix texmatrix;

	scroll -= (int) scroll & ~127;
	scroll /= 128.0f;

	qglMatrixMode (GL_TEXTURE);
	GL_IdentityMatrix (&texmatrix);
	GL_TranslateMatrix (&texmatrix, scroll, scroll, 0);
	qglLoadMatrixf (texmatrix.m16);
	qglMatrixMode (GL_MODELVIEW);
}


void Sky_RenderLayers (void)
{
	glmatrix skymatrix;

	// standard layers
	GL_BindTexture (GL_TEXTURE0_ARB, solidskytexture);
	R_ScrollSkyMatrix (8);

	GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_DECAL);
	GL_BindTexture (GL_TEXTURE1_ARB, alphaskytexture);
	R_ScrollSkyMatrix (16);

	GL_IdentityMatrix (&skymatrix);
	GL_TranslateMatrix (&skymatrix, -r_origin[0], -r_origin[1], -r_origin[2]);
	GL_ScaleMatrix (&skymatrix, 65536, 65536, 65536);
	qglMultMatrixf (skymatrix.m16);

	GL_SetStreamSource (GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (dpskyvert_t), r_dpskyverts->xyz);
	GL_SetStreamSource (GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);
	GL_SetStreamSource (GLSTREAM_TEXCOORD0, 2, GL_FLOAT, sizeof (dpskyvert_t), r_dpskyverts->st);
	GL_SetStreamSource (GLSTREAM_TEXCOORD1, 2, GL_FLOAT, sizeof (dpskyvert_t), r_dpskyverts->st);
	GL_SetStreamSource (GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);

	GL_SetIndices (r_dpskyindexes);
	GL_DrawIndexedPrimitive (GL_TRIANGLES, SKYSPHERE_NUMINDEXES, SKYSPHERE_NUMVERTS);

	GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_NONE);

	qglMatrixMode (GL_TEXTURE);
	qglLoadIdentity ();
	qglMatrixMode (GL_MODELVIEW);

	GL_ActiveTexture (GL_TEXTURE0_ARB);

	qglMatrixMode (GL_TEXTURE);
	qglLoadIdentity ();
	qglMatrixMode (GL_MODELVIEW);

	qglLoadMatrixf (r_world_matrix.m16);
}


//==============================================================================
//
//  RENDER SKYBOX
//
//==============================================================================

void Sky_DrawCubeMap (r_modelsurf_t **mslist, int numms)
{
#if 0
	int i;
	msurface_t *surf;
	r_modelsurf_t *ms;
	qbool stateset = false;

	// cubemapped sky doesn't need depth clipping or other hackery, is always perfectly positioned, and never has bugs
	for (i = 0; i < numms; i++)
	{
		ms = mslist[i];

		if (!((surf = ms->surface)->flags & SURF_DRAWSKY)) continue;

		if (!stateset)
		{
			if (gl_support_arb_vertex_buffer_object)
			{
				GL_BindBuffer (GL_ARRAY_BUFFER_ARB, r_surfacevbo);

				GL_SetStreamSource (GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (glvertex_t), (void *) (0));
				GL_SetStreamSource (GLSTREAM_TEXCOORD0, 3, GL_FLOAT, sizeof (glvertex_t), (void *) (0));
			}
			else
			{
				glvertex_t *base = (glvertex_t *) scratchbuf;

				GL_SetStreamSource (GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (glvertex_t), base->v);
				GL_SetStreamSource (GLSTREAM_TEXCOORD0, 3, GL_FLOAT, sizeof (glvertex_t), base->v);
			}

			GL_SetStreamSource (GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);
			GL_SetStreamSource (GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
			GL_SetStreamSource (GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);

			R_BeginSurfaces ();

			// texmgr is fragile and can't handle cubemaps so we must do it manually
			qglBindTexture (GL_TEXTURE_2D, 0);
			qglDisable (GL_TEXTURE_2D);
			qglEnable (GL_TEXTURE_CUBE_MAP);
			qglBindTexture (GL_TEXTURE_CUBE_MAP, skyCubeTexture);

			qglMatrixMode (GL_TEXTURE);
			qglLoadIdentity ();
			qglTranslatef (-r_origin[0], -r_origin[1], -r_origin[2]);
			qglMatrixMode (GL_MODELVIEW);

			stateset = true;
		}

		R_BatchSurface (ms->surface, ms->matrix, ms->entnum);
	}

	if (stateset)
	{
		R_EndSurfaces ();

		qglMatrixMode (GL_TEXTURE);
		qglLoadIdentity ();
		qglMatrixMode (GL_MODELVIEW);

		// texmgr is fragile and can't handle cubemaps so we must do it manually
		qglBindTexture (GL_TEXTURE_CUBE_MAP, 0);
		qglDisable (GL_TEXTURE_CUBE_MAP);
		qglEnable (GL_TEXTURE_2D);

		// ensure that we bind a 2D texture here
		GL_BindTexture (GL_TEXTURE0_ARB, NULL);
	}
#endif
}


//==============================================================================
//
//  WHOLE SKY
//
//==============================================================================

#define SKY_DEPTHCLIP		1
#define SKY_FOG_LAYER		2
#define SKY_SHOWTRIS		3
#define SKY_GLSL			4

void Sky_GLSLBegin (void)
{
/*
	float speedscale[2];

	static int u_rorigin = -1;
	static int u_solidskytexture = -1;
	static int u_alphaskytexture = -1;
	static int u_scale = -1;
	static int u_fogcolor = -1;
	extern qbool r_glsl_reset_sky;
	float defaultfogcolor[4] = {1, 1, 1, 1};

	qglUseProgramObject (hSkyProgram);

	if (r_glsl_reset_sky)
	{
		u_rorigin = qglGetUniformLocation (hSkyProgram, "r_origin");
		u_scale = qglGetUniformLocation (hSkyProgram, "Scale");
		u_fogcolor = qglGetUniformLocation (hSkyProgram, "fogColor");
		u_solidskytexture = qglGetUniformLocation (hSkyProgram, "solidskytexture");
		u_alphaskytexture = qglGetUniformLocation (hSkyProgram, "alphaskytexture");

		r_glsl_reset_sky = false;
	}

	speedscale[0] = cl.time * 8.0f;
	speedscale[1] = cl.time * 16.0f;

	speedscale[0] -= (int) speedscale[0] & ~127;
	speedscale[1] -= (int) speedscale[1] & ~127;

	qglUniform3fv (u_rorigin, 1, r_origin);
	qglUniform2fv (u_scale, 1, speedscale);
	qglUniform1i (u_solidskytexture, 0);        // tmu number to use, NOT the texture object number
	qglUniform1i (u_alphaskytexture, 1);        // tmu number to use, NOT the texture object number

	if (Fog_GetDensity () > 0 && r_skyfog.value > 0)
	{
		float *c = Fog_GetColor ();

		// every time the illogical order of this has caught me out... grrr...
		VectorCopy (c, defaultfogcolor);
		defaultfogcolor[3] = 1.0f - r_skyfog.value;

		qglUniform4fv (u_fogcolor, 1, defaultfogcolor);
	}
	else qglUniform4fv (u_fogcolor, 1, defaultfogcolor);

	GL_BindTexture (GL_TEXTURE0_ARB, solidskytexture);
	GL_BindTexture (GL_TEXTURE1_ARB, alphaskytexture);
*/
}


void Sky_GLSLEnd (void)
{
/*
	qglUseProgramObject (0);

	GL_TexEnv (GL_TEXTURE1_ARB, GL_TEXTURE_2D, GL_NONE);
	GL_ActiveTexture (GL_TEXTURE0_ARB);
*/
}


void Sky_DepthClipBegin (void)
{
	qglColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	qglColor4f (0, 0, 0, 0);
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_NONE);
}


void Sky_DepthClipEnd (void)
{
	qglColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	qglColor4f (1, 1, 1, 1);
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);

	// invert the depth function so that the sky will actually draw
	qglDepthFunc (GL_GEQUAL);
	qglDepthMask (0);
}


void Sky_DepthClipEnd2 (void)
{
	// restore the correct depth func
	qglDepthFunc (GL_LEQUAL);
	qglDepthMask (1);
}


void Sky_FogBegin (void)
{
	float *c;

	c = Fog_GetColor();
	qglEnable (GL_BLEND);
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_NONE);
	qglColor4f (c[0], c[1], c[2], clamp (0.0, r_skyfog.value, 1.0));
}


void Sky_FogEnd (void)
{
	qglColor4f (1, 1, 1, 1);
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);
	qglDisable (GL_BLEND);
}


void R_RenderModelSurfsSkyPass (r_modelsurf_t **mslist, int numms, int skytype)
{
	int i;
	r_modelsurf_t *ms;
	qbool stateset = false;

	R_BeginSurfaces ();

	for (i = 0; i < numms; i++)
	{
		ms = mslist[i];

		if (!(ms->surface->flags & SURF_DRAWSKY)) continue;

		if (!stateset)
		{
			if (gl_support_arb_vertex_buffer_object)
			{
				GL_BindBuffer (GL_ARRAY_BUFFER_ARB, r_surfacevbo);

				if (skytype == SKY_FOG_LAYER || skytype == SKY_DEPTHCLIP || skytype == SKY_SHOWTRIS || skytype == SKY_GLSL)
				{
					GL_SetStreamSource (GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (glvertex_t), (void *) (0));
					GL_SetStreamSource (GLSTREAM_TEXCOORD0, 0, GL_NONE, 0, NULL);
				}
				else
				{
					GL_SetStreamSource (GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (glvertex_t), (void *) (0));
					GL_SetStreamSource (GLSTREAM_TEXCOORD0, 3, GL_FLOAT, sizeof (glvertex_t), (void *) (0));
				}
			}
			else
			{
				glvertex_t *base = (glvertex_t *) scratchbuf;

				if (skytype == SKY_FOG_LAYER || skytype == SKY_DEPTHCLIP || skytype == SKY_SHOWTRIS || skytype == SKY_GLSL)
				{
					GL_SetStreamSource (GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (glvertex_t), base->v);
					GL_SetStreamSource (GLSTREAM_TEXCOORD0, 0, GL_NONE, 0, NULL);
				}
				else
				{
					GL_SetStreamSource (GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (glvertex_t), base->v);
					GL_SetStreamSource (GLSTREAM_TEXCOORD0, 3, GL_FLOAT, sizeof (glvertex_t), base->v);
				}
			}

			GL_SetStreamSource (GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);
			GL_SetStreamSource (GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
			GL_SetStreamSource (GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);

			if (skytype == SKY_FOG_LAYER)
				Sky_FogBegin ();
			else if (skytype == SKY_DEPTHCLIP)
				Sky_DepthClipBegin ();
			else if (skytype == SKY_SHOWTRIS)
				R_ShowTrisBegin ();
			else if (skytype == SKY_GLSL)
				Sky_GLSLBegin ();

			stateset = true;
		}

		R_BatchSurface (ms->surface, ms->matrix, ms->entnum);
	}

	R_EndSurfaces ();

	if (stateset)
	{
		if (skytype == SKY_FOG_LAYER)
			Sky_FogEnd ();
		else if (skytype == SKY_SHOWTRIS)
			R_ShowTrisEnd ();
		else if (skytype == SKY_DEPTHCLIP)
		{
			Sky_DepthClipEnd ();
			Sky_RenderLayers ();
			Sky_DepthClipEnd2 ();
		}
		else if (skytype == SKY_GLSL)
			Sky_GLSLEnd ();
	}
}


void R_RenderModelSurfsSky (r_modelsurf_t **mslist, int numms)
{
	Fog_DisableGFog ();

	if (skybox_name[0])
	{
		// we have working skybox cubemap code now
		Sky_DrawCubeMap (mslist, numms);
	}
//	else if (gl_support_shader_objects)
//	{
//		R_RenderModelSurfsSkyPass (mslist, numms, SKY_GLSL);
//		goto glsl_sky;
//	}
	else R_RenderModelSurfsSkyPass (mslist, numms, SKY_DEPTHCLIP);

	// same pass for all sky fog just using the base verts
	if (Fog_GetDensity () > 0 && r_skyfog.value > 0)
		R_RenderModelSurfsSkyPass (mslist, numms, SKY_FOG_LAYER);

//glsl_sky:;
	if (r_showtris.value)
		R_RenderModelSurfsSkyPass (mslist, numms, SKY_SHOWTRIS);

	Fog_EnableGFog ();
}

