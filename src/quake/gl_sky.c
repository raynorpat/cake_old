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
gltexture_t *skybox_textures[6];
char skybox_name[256] = {0};

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


/*
==================
Sky_LoadSkyBox
==================
*/
char *suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};

void Sky_LoadSkyBox (char *name)
{
	int		i, mark, width, height;
	char	filename[MAX_OSPATH];
	byte	*data;
	qbool nonefound = true;

	if (strcmp (skybox_name, name) == 0)
		return; // no change

	// purge old textures
	for (i = 0; i < 6; i++)
	{
		if (skybox_textures[i] && skybox_textures[i] != notexture)
			TexMgr_FreeTexture (skybox_textures[i]);

		skybox_textures[i] = NULL;
	}

	// turn off skybox if sky is set to ""
	if (name[0] == 0)
	{
		skybox_name[0] = 0;
		return;
	}

	// load textures
	for (i=0 ; i<6 ; i++)
	{
		mark = Hunk_LowMark ();
		sprintf (filename, "gfx/env/%s%s", name, suf[i]);
		data = Image_LoadImage (filename, &width, &height);

		if (data)
		{
			skybox_textures[i] = TexMgr_LoadImage (r_worldmodel, filename, width, height, SRC_RGBA, data, filename, 0, TEXPREF_NONE);
			nonefound = false;
		}
		else
		{
			Com_Printf ("Couldn't load %s\n", filename);
			skybox_textures[i] = notexture;
		}

		Hunk_FreeToLowMark (mark);
	}

	if (nonefound) // go back to scrolling sky if skybox is totally missing
	{
		for (i = 0; i < 6; i++)
		{
			if (skybox_textures[i] && skybox_textures[i] != notexture)
				TexMgr_FreeTexture (skybox_textures[i]);

			skybox_textures[i] = NULL;
		}

		Com_Printf ("Couldn't load %s\n", name);
		skybox_name[0] = 0;
		return;
	}

	strcpy (skybox_name, name);
	Com_DPrintf ("loaded skybox %s OK\n", name);
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
	skybox_name[0] = 0;

	// clear textures too
	for (i = 0; i < 6; i++)
		skybox_textures[i] = NULL;

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
				Sky_LoadSkyBox (value);
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
		Sky_LoadSkyBox (Cmd_Argv (1));
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

	GL_SetStreamSource (0, GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (dpskyvert_t), r_dpskyverts->xyz);
	GL_SetStreamSource (0, GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);
	GL_SetStreamSource (0, GLSTREAM_TEXCOORD0, 2, GL_FLOAT, sizeof (dpskyvert_t), r_dpskyverts->st);
	GL_SetStreamSource (0, GLSTREAM_TEXCOORD1, 2, GL_FLOAT, sizeof (dpskyvert_t), r_dpskyverts->st);
	GL_SetStreamSource (0, GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);

	GL_SetIndices (0, r_dpskyindexes);
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

float skymins[2][6], skymaxs[2][6];

int	st_to_vec[6][3] =
{
	{3, -1, 2},
	{-3, 1, 2},
	{1, 3, 2},
	{-1, -3, 2},
 	{-2, -1, 3},		// straight up
 	{2, -1, -3}		// straight down
};


void Sky_EmitSkyBoxVertex (float *dest, float s, float t, int axis)
{
	vec3_t b;
	int j, k;
	float w, h;

	// because we're using infinite projection we can just use a REALLY large far clip
	b[0] = s * 65536.0f / sqrt (3.0);
	b[1] = t * 65536.0f / sqrt (3.0);
	b[2] = 65536.0f / sqrt (3.0);

	for (j = 0; j < 3; j++)
	{
		k = st_to_vec[axis][j];

		if (k < 0)
			dest[j] = -b[-k - 1];
		else dest[j] = b[k - 1];

		dest[j] += r_origin[j];
	}

	// convert from range [-1, 1] to [0, 1]
	dest[3] = (s + 1) * 0.5;
	dest[4] = (t + 1) * 0.5;

	// avoid bilerp seam
	w = skybox_textures[skytexorder[axis]]->width;
	h = skybox_textures[skytexorder[axis]]->height;

	dest[3] = dest[3] * (w - 1) / w + 0.5 / w;
	dest[4] = dest[4] * (h - 1) / h + 0.5 / h;

	dest[4] = 1.0 - dest[4];
}


void Sky_RenderSkybox (void)
{
	int i;
	float skyquad[5 * 4];
	qbool stateset = false;

	for (i = 0; i < 6; i++)
	{
		if (!skybox_textures[i]) continue;
		if (skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i]) continue;

		GL_BindTexture (GL_TEXTURE0_ARB, skybox_textures[skytexorder[i]]);

#if 1 // FIXME: this is to avoid tjunctions until i can do it the right way
		skymins[0][i] = -1;
		skymins[1][i] = -1;
		skymaxs[0][i] = 1;
		skymaxs[1][i] = 1;
#endif

		if (!stateset)
		{
			GL_SetStreamSource (0, GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (float) * 5, &skyquad[0]);
			GL_SetStreamSource (0, GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);
			GL_SetStreamSource (0, GLSTREAM_TEXCOORD0, 2, GL_FLOAT, sizeof (float) * 5, &skyquad[3]);
			GL_SetStreamSource (0, GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
			GL_SetStreamSource (0, GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);
			GL_SetIndices (0, r_quad_indexes);

			stateset = true;
		}

		Sky_EmitSkyBoxVertex (&skyquad[0], skymins[0][i], skymins[1][i], i);
		Sky_EmitSkyBoxVertex (&skyquad[5], skymins[0][i], skymaxs[1][i], i);
		Sky_EmitSkyBoxVertex (&skyquad[10], skymaxs[0][i], skymaxs[1][i], i);
		Sky_EmitSkyBoxVertex (&skyquad[15], skymaxs[0][i], skymins[1][i], i);

		GL_DrawIndexedPrimitive (GL_TRIANGLES, 6, 4);
	}
}


#define MAX_CLIP_VERTS 64

int	vec_to_st[6][3] =
{
	{-2, 3, 1},
	{2, 3, -1},
	{1, 3, 2},
	{-1, 3, -2},
	{-2, -1, 3},
	{-2, 1, -3}
};


vec3_t	skyclip[6] =
{
	{1, 1, 0},
	{1, -1, 0},
	{0, -1, 1},
	{0, 1, 1},
	{1, 0, 1},
	{-1, 0, 1}
};


void Sky_ProjectPoly (int nump, vec3_t vecs)
{
	int		i, j;
	vec3_t	v, av;
	float	s, t, dv;
	int		axis;
	float	*vp;

	// decide which face it maps to
	VectorCopy (vec3_origin, v);

	for (i = 0, vp = vecs; i < nump; i++, vp += 3)
		VectorAdd (vp, v, v);

	av[0] = fabs (v[0]);
	av[1] = fabs (v[1]);
	av[2] = fabs (v[2]);

	if (av[0] > av[1] && av[0] > av[2])
	{
		if (v[0] < 0)
			axis = 1;
		else axis = 0;
	}
	else if (av[1] > av[2] && av[1] > av[0])
	{
		if (v[1] < 0)
			axis = 3;
		else axis = 2;
	}
	else
	{
		if (v[2] < 0)
			axis = 5;
		else axis = 4;
	}

	// project new texture coords
	for (i = 0; i < nump; i++, vecs += 3)
	{
		j = vec_to_st[axis][2];

		if (j > 0)
			dv = vecs[j - 1];
		else dv = -vecs[-j - 1];

		j = vec_to_st[axis][0];

		if (j < 0)
			s = -vecs[-j - 1] / dv;
		else s = vecs[j - 1] / dv;

		j = vec_to_st[axis][1];

		if (j < 0)
			t = -vecs[-j - 1] / dv;
		else t = vecs[j - 1] / dv;

		if (s < skymins[0][axis]) skymins[0][axis] = s;
		if (t < skymins[1][axis]) skymins[1][axis] = t;
		if (s > skymaxs[0][axis]) skymaxs[0][axis] = s;
		if (t > skymaxs[1][axis]) skymaxs[1][axis] = t;
	}
}


void Sky_ClipPoly (int nump, vec3_t vecs, int stage)
{
	float	*norm;
	float	*v;
	qbool	front, back;
	float	d, e;
	float	dists[MAX_CLIP_VERTS];
	int		sides[MAX_CLIP_VERTS];
	vec3_t	newv[2][MAX_CLIP_VERTS];
	int		newc[2];
	int		i, j;

	if (nump > MAX_CLIP_VERTS - 2)
		Sys_Error ("Sky_ClipPoly: MAX_CLIP_VERTS");

	if (stage == 6) // fully clipped
	{
		Sky_ProjectPoly (nump, vecs);
		return;
	}

	front = back = false;
	norm = skyclip[stage];

	for (i = 0, v = vecs; i < nump; i++, v += 3)
	{
		d = DotProduct (v, norm);

		if (d > ON_EPSILON)
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < ON_EPSILON)
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else sides[i] = SIDE_ON;

		dists[i] = d;
	}

	if (!front || !back)
	{
		// not clipped
		Sky_ClipPoly (nump, vecs, stage + 1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs + (i * 3)));
	newc[0] = newc[1] = 0;

	for (i = 0, v = vecs; i < nump; i++, v += 3)
	{
		switch (sides[i])
		{
		case SIDE_FRONT:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			break;

		case SIDE_BACK:
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;

		case SIDE_ON:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i + 1]);

		for (j = 0; j < 3; j++)
		{
			e = v[j] + d * (v[j + 3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}

		newc[0]++;
		newc[1]++;
	}

	// continue
	Sky_ClipPoly (newc[0], newv[0][0], stage + 1);
	Sky_ClipPoly (newc[1], newv[1][0], stage + 1);
}


void Sky_UpdateBounds (r_modelsurf_t **mslist, int numms)
{
	int i, j;
	msurface_t *surf;
	r_modelsurf_t *ms;
	vec3_t verts[MAX_CLIP_VERTS];

	// reset bounds; ensure that this is large enough for our infinite projection
	for (i = 0; i < 6; i++)
	{
		skymins[0][i] = skymins[1][i] = 99999999;
		skymaxs[0][i] = skymaxs[1][i] = -99999999;
	}

	for (i = 0; i < numms; i++)
	{
		ms = mslist[i];

		if (!((surf = ms->surface)->flags & SURF_DRAWSKY)) continue;

		for (j = 0; j < surf->numglvertexes; j++)
			VectorSubtract (surf->glvertexes[j].v, r_origin, verts[j]);

		Sky_ClipPoly (surf->numglvertexes, verts[0], 0);
	}
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
	int i, j;
	r_modelsurf_t *ms;
	qbool stateset = false;

	int numglindexes = 0;
	int numglvertexes = 0;
	unsigned short *glindexes = (unsigned short *) scratchbuf;
	float *glvertexes = (float *) (glindexes + 32768);
	qbool r_restart_sky = false;

	for (i = 0; i < numms; i++)
	{
		ms = mslist[i];

		if (!(ms->surface->flags & SURF_DRAWSKY)) continue;

		if (!stateset)
		{
			GL_SetIndices (0, scratchbuf);

			if (skytype == SKY_FOG_LAYER || skytype == SKY_DEPTHCLIP || skytype == SKY_SHOWTRIS || skytype == SKY_GLSL)
			{
				GL_SetStreamSource (0, GLSTREAM_POSITION, 3, GL_FLOAT, 0, glvertexes);
				GL_SetStreamSource (0, GLSTREAM_TEXCOORD0, 0, GL_NONE, 0, NULL);
			}
			else
			{
				GL_SetStreamSource (0, GLSTREAM_POSITION, 3, GL_FLOAT, 0, glvertexes);
				GL_SetStreamSource (0, GLSTREAM_TEXCOORD0, 3, GL_FLOAT, 0, glvertexes);
			}

			GL_SetStreamSource (0, GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);
			GL_SetStreamSource (0, GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
			GL_SetStreamSource (0, GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);

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

		if (numglindexes + ms->surface->numglindexes >= 32768) r_restart_sky = true;
		if (numglvertexes + ms->surface->numglvertexes >= 16384) r_restart_sky = true;

		if (r_restart_sky)
		{
			GL_DrawIndexedPrimitive (GL_TRIANGLES, numglindexes, numglvertexes);

			numglindexes = 0;
			numglvertexes = 0;
			glindexes = (unsigned short *) scratchbuf;
			glvertexes = (float *) (glindexes + 32768);
			r_restart_sky = false;
		}

		// regenerate the vertexes, transforming if required
		if (ms->matrix)
			GL_RegenerateVertexes (ms->surface->model, ms->surface, ms->matrix);

		glindexes = R_TransferIndexes (ms->surface->glindexes, glindexes, ms->surface->numglindexes, numglvertexes);

		for (j = 0; j < ms->surface->numglvertexes; j++, glvertexes += 3)
		{
			glvertexes[0] = ms->surface->glvertexes[j].v[0];
			glvertexes[1] = ms->surface->glvertexes[j].v[1];
			glvertexes[2] = ms->surface->glvertexes[j].v[2];
		}

		numglindexes += ms->surface->numglindexes;
		numglvertexes += ms->surface->numglvertexes;
	}

	if (numglindexes)
		GL_DrawIndexedPrimitive (GL_TRIANGLES, numglindexes, numglvertexes);

	if (stateset)
	{
		if (skytype == SKY_FOG_LAYER)
			Sky_FogEnd ();
		else if (skytype == SKY_SHOWTRIS)
			R_ShowTrisEnd ();
		else if (skytype == SKY_DEPTHCLIP)
		{
			Sky_DepthClipEnd ();

			if (skybox_name[0])
				Sky_RenderSkybox ();
			else Sky_RenderLayers ();

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
		Sky_UpdateBounds (mslist, numms);
		R_RenderModelSurfsSkyPass (mslist, numms, SKY_DEPTHCLIP);
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

glsl_sky:;
	if (r_showtris.value)
		R_RenderModelSurfsSkyPass (mslist, numms, SKY_SHOWTRIS);

	Fog_EnableGFog ();
}
