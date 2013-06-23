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
// gl_warp.c -- warp polygons

#include "gl_local.h"
#include "rc_image.h"

cvar_t r_lavafog = {"r_lavafog", "0", true};
cvar_t r_slimefog = {"r_slimefog", "0", true};
cvar_t r_lavaalpha = {"r_lavaalpha", "1", true};
cvar_t r_telealpha = {"r_telealpha", "1", true};
cvar_t r_slimealpha = {"r_slimealpha", "1", true};
cvar_t r_lockalpha = {"r_lockalpha", "0", true};

void R_WarpRegisterCvars (void)
{
	Cvar_Register (&r_slimefog);
	Cvar_Register (&r_lavafog);
	Cvar_Register (&r_lavaalpha);
	Cvar_Register (&r_telealpha);
	Cvar_Register (&r_slimealpha);
	Cvar_Register (&r_lockalpha);
}


qbool r_glsl_reset_sky = false;
qbool r_glsl_reset_liquid = false;
qbool r_glsl_reset_liquid_fog = false;

const char *gl_liquid_vs_text =
    "#version 110\n"
	"#pragma optionNV(fastmath off)\n"
	"#pragma optionNV(fastprecision off)\n"
	"uniform float warpfactor;\n"
	"uniform float warptime;\n"
	"\n"
	"void main (void)\n"
	"{\n"
	"       gl_Position = ftransform ();\n"
	"       gl_TexCoord[0] = gl_MultiTexCoord0;\n"
	"       gl_TexCoord[1].st = ((gl_MultiTexCoord0.ts * warpfactor) + warptime) * 0.024543;\n"
	"		gl_FrontColor = gl_Color;"
	"		gl_BackColor = gl_Color;"
	"}\n";

const char *gl_liquid_fs_text =
    "#version 110\n"
	"#pragma optionNV(fastmath off)\n"
	"#pragma optionNV(fastprecision off)\n"
	"uniform sampler2D warptexture;\n"
	"uniform float warpscale;\n"
	"\n"
	"void main (void)\n"
	"{\n"
	"       vec2 st = vec2 (((sin (gl_TexCoord[1].st) * warpscale) + gl_TexCoord[0].st) * 0.01562);\n"
	"       gl_FragColor = texture2D (warptexture, st.st) * gl_Color;\n"
	"}\n";

GLhandleARB hLiquidVS = 0;
GLhandleARB hLiquidFS = 0;
GLhandleARB hLiquidProgram = 0;


const char *gl_liquid_fog_vs_text =
    "#version 110\n"
	"#pragma optionNV(fastmath off)\n"
	"#pragma optionNV(fastprecision off)\n"
	"uniform float warpfactor;\n"
	"uniform float warptime;\n"
	"varying float fogFactor;\n"
	"\n"
	"void main (void)\n"
	"{\n"
	"       gl_Position = ftransform ();\n"
	"		vec3 vVertex = vec3 (gl_ModelViewMatrix * gl_Vertex);\n"
	"		gl_FogFragCoord = length (vVertex);\n"
	"       gl_TexCoord[0] = gl_MultiTexCoord0;\n"
	"       gl_TexCoord[1].st = ((gl_MultiTexCoord0.ts * warpfactor) + warptime) * 0.024543;\n"
	"		gl_FrontColor = gl_Color;"
	"		gl_BackColor = gl_Color;"
	"		fogFactor = exp2 (-gl_Fog.density * "
	"				   gl_Fog.density * "
	"				   gl_FogFragCoord * "
	"				   gl_FogFragCoord * "
	"				   1.442695);"
	"		fogFactor = clamp (fogFactor, 0.0, 1.0);"
	"}\n";

const char *gl_liquid_fog_fs_text =
    "#version 110\n"
	"#pragma optionNV(fastmath off)\n"
	"#pragma optionNV(fastprecision off)\n"
	"uniform sampler2D warptexture;\n"
	"uniform float warpscale;\n"
	"varying float fogFactor;\n"
	"\n"
	"void main (void)\n"
	"{\n"
	"       vec2 st = vec2 (((sin (gl_TexCoord[1].st) * warpscale) + gl_TexCoord[0].st) * 0.01562);\n"
	"       gl_FragColor = mix (gl_Fog.color, texture2D (warptexture, st.st), fogFactor) * gl_Color;\n"
	"}\n";

GLhandleARB hLiquidFogVS = 0;
GLhandleARB hLiquidFogFS = 0;
GLhandleARB hLiquidFogProgram = 0;


const char *gl_sky_vs_text =
    "#version 110\n"
	"#pragma optionNV(fastmath off)\n"
	"#pragma optionNV(fastprecision off)\n"
	"uniform vec3 r_origin;\n"
	"\n"
	"void main (void)\n"
	"{\n"
	"       gl_Position = ftransform ();\n"
	"       gl_TexCoord[0].xyz = gl_Vertex.xyz - r_origin.xyz;\n"
	"       gl_TexCoord[0].z *= 3.0;\n"
	"}\n";

const char *gl_sky_fs_text =
    "#version 110\n"
	"#pragma optionNV(fastmath off)\n"
	"#pragma optionNV(fastprecision off)\n"
	"uniform sampler2D solidskytexture;\n"
	"uniform sampler2D alphaskytexture;\n"
	"uniform vec2 Scale;"
	"uniform vec4 fogColor;\n"
	"\n"
	"void main (void)\n"
	"{\n"
	"		vec3 st = vec3 (gl_TexCoord[0].xyz * (6.0 * 63.0 / length (gl_TexCoord[0].xyz)));"
	"		vec4 solidcolor = texture2D (solidskytexture, (st.st + Scale.x) * 0.0078125);"
	"		vec4 alphacolor = texture2D (alphaskytexture, (st.st + Scale.y) * 0.0078125);"
	"		vec4 finalcolor = (alphacolor * alphacolor.a) + (solidcolor * (1.0 - alphacolor.a));\n"
	"       gl_FragColor = mix (fogColor, finalcolor, fogColor.a);\n"
	"}\n";

GLhandleARB hSkyVS = 0;
GLhandleARB hSkyFS = 0;
GLhandleARB hSkyProgram = 0;

char gl_shaderlog[4096];

GLhandleARB GL_CreateProgramFromShaders (GLhandleARB hVertexShader, GLhandleARB hFragmentShader)
{
/*
	int succeeded = 0;
	GLhandleARB hProgram = qglCreateProgramObject ();

	// attach our shaders to it
	qglAttachObject (hProgram, hVertexShader);
	qglAttachObject (hProgram, hFragmentShader);

	// link the wee bastard
	qglLinkProgram (hProgram);
	qglGetObjectParameteriv (hProgram, GL_LINK_STATUS, &succeeded);

	if (!succeeded)
	{
		int len = 0;

		qglGetInfoLog (hProgram, 4095, &len, gl_shaderlog);
		return 0;
	}
	else return hProgram;
*/
	return 0;
}


GLhandleARB GL_CreateShaderFromSource (GLenum type, const char **source)
{
/*
	int succeeded = 0;
	GLhandleARB hShader = qglCreateShaderObject (type);

	qglShaderSource (hShader, 1, source, NULL);
	qglCompileShader (hShader);

	qglGetObjectParameteriv (hShader, GL_COMPILE_STATUS, &succeeded);

	if (!succeeded)
	{
		int len = 0;

		qglGetInfoLog (hShader, 4095, &len, gl_shaderlog);
		return 0;
	}
	else return hShader;
*/
	return 0;
}


qbool GL_CreateGenericProgram (const char *vstext, const char *fstext, GLhandleARB *vsh, GLhandleARB *fsh, GLhandleARB *ph)
{
/*
	// create the vertex shader
	if (!(vsh[0] = GL_CreateShaderFromSource (GL_VERTEX_SHADER, &vstext))) return false;
	if (!(fsh[0] = GL_CreateShaderFromSource (GL_FRAGMENT_SHADER, &fstext))) return false;

	// create the program
	if (!(ph[0] = GL_CreateProgramFromShaders (vsh[0], fsh[0])))
		return false;
	else return true;
*/
}


void GL_DeleteShaderObject (GLhandleARB *hObject)
{
/*
	if (qglDeleteObject && hObject[0])
		qglDeleteObject (hObject[0]);

	hObject[0] = 0;
*/
}


void GL_DestroyGenericProgram (GLhandleARB *vsh, GLhandleARB *fsh, GLhandleARB *ph)
{
/*
	// detach shaders
	if (qglDetachObject && ph[0] && vsh[0]) qglDetachObject (ph[0], vsh[0]);
	if (qglDetachObject && ph[0] && fsh[0]) qglDetachObject (ph[0], fsh[0]);

	GL_DeleteShaderObject (&vsh[0]);
	GL_DeleteShaderObject (&fsh[0]);
	GL_DeleteShaderObject (&ph[0]);

	ph[0] = vsh[0] = fsh[0] = 0;
*/
}


void R_KillGLSL (void)
{
/*
	// switch off any shaders that might be executing
	if (qglUseProgramObject)
		qglUseProgramObject (0);

	// kill even if gl_arb_shader_objects is false because gl_arb_shader_objects may have
	// been set to false if gl_arb_shader_objects fails.  destruction will validate for us
	GL_DestroyGenericProgram (&hSkyVS, &hSkyFS, &hSkyProgram);
	GL_DestroyGenericProgram (&hLiquidVS, &hLiquidFS, &hLiquidProgram);
	GL_DestroyGenericProgram (&hLiquidFogVS, &hLiquidFogFS, &hLiquidFogProgram);
*/
}


void R_LoadGLSL (void)
{
/*
	if (!gl_support_shader_objects) goto glsl_madeoffail;

	// destroy anything that might exist
	R_KillGLSL ();

	if (!GL_CreateGenericProgram (gl_sky_vs_text, gl_sky_fs_text, &hSkyVS, &hSkyFS, &hSkyProgram)) goto glsl_madeoffail;
	if (!GL_CreateGenericProgram (gl_liquid_vs_text, gl_liquid_fs_text, &hLiquidVS, &hLiquidFS, &hLiquidProgram)) goto glsl_madeoffail;

	// fucking fog
	if (!GL_CreateGenericProgram (gl_liquid_fog_vs_text, gl_liquid_fog_fs_text, &hLiquidFogVS, &hLiquidFogFS, &hLiquidFogProgram)) goto glsl_madeoffail;

	// created successfully
	// Con_Printf ("GLSL shaders created OK.\n");

	// flag to reset uniforms
	r_glsl_reset_liquid_fog = true;
	r_glsl_reset_liquid = true;
	r_glsl_reset_sky = true;
	return;

glsl_madeoffail:;
	// destroy and disable glsl use
	gl_support_shader_objects = false;
	R_KillGLSL ();
	// Con_Warning ("failed to create GLSL shaders\n");
*/
}


typedef struct r_warpmemory_s
{
	struct r_warpmemory_s *next;
	void *data;
} r_warpmemory_t;

r_warpmemory_t *warpmem = NULL;

void *R_WarpAlloc (int size)
{
	r_warpmemory_t *wm;

	// find a free slot
	for (wm = warpmem; wm; wm = wm->next)
		if (!wm->data)
			break;

	// didn't find one so create a new
	if (!wm)
	{
		wm = (r_warpmemory_t *) malloc (sizeof (r_warpmemory_t));

		wm->next = warpmem;
		warpmem = wm;
	}

	// alloc our data
	wm->data = malloc (size);
	return wm->data;
}


void R_WarpFree (void *data)
{
	r_warpmemory_t *wm;

	// find a matching slot
	for (wm = warpmem; wm; wm = wm->next)
	{
		if (!wm->data) continue;

		if (wm->data == data)
		{
			free (wm->data);
			wm->data = NULL;
			break;
		}
	}
}


void R_WarpFreeAll (void)
{
	r_warpmemory_t *wm;

	// all previously allocated slots are just marked as free and can be reused next time they're needed
	for (wm = warpmem; wm; wm = wm->next)
	{
		if (!wm->data) continue;

		free (wm->data);
		wm->data = NULL;
	}
}


float r_globalwateralpha = 0.0f;

void WaterAlpha_ParseWorldspawn (void)
{
	char key[128], value[4096];
	char *data;

	// initially no wateralpha
	r_globalwateralpha = 0.0f;

	data = COM_Parse (r_worldmodel->entities);

	if (!data)
		return; // error

	if (com_token[0] != '{')
		return; // error

	while (1)
	{
		data = COM_Parse (data);

		if (!data)
			return; // error

		if (com_token[0] == '}')
			break; // end of worldspawn

		if (com_token[0] == '_')
			strcpy (key, com_token + 1);
		else
			strcpy (key, com_token);

		while (key[strlen (key)-1] == ' ') // remove trailing spaces
			key[strlen (key)-1] = 0;

		data = COM_Parse (data);

		if (!data)
			return; // error

		strcpy (value, com_token);

		if (!strcmp ("wateralpha", key))
		{
			r_globalwateralpha = Q_atof (value);
		}
	}
}


#define R_MAXWARPINDEXES	32768
#define R_MAXWARPVERTEXES	16384

cvar_t gl_subdivide_size = {"gl_subdivide_size", "128", true};

static void R_BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int		i, j;
	float	*v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;

	for (i=0 ; i<numverts ; i++)
	{
		for (j=0 ; j<3 ; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
	}
}


typedef struct sdvert_s
{
	float xyz[3];
} sdvert_t;

static void R_SubdividePolygon (msurface_t *surf, int numverts, float *verts, sdvert_t *dstverts, unsigned short *dstindexes)
{
	int i, j, k, f, b;
	vec3_t	mins, maxs;
	vec3_t	front[64], back[64];
	float	dist[64];
	float *v, m;
	sdvert_t *dst;
	unsigned short *ndx;

	if (numverts > 60)
		Sys_Error ("SubdividePolygon: excessive numverts %i", numverts);

	R_BoundPoly (numverts, verts, mins, maxs);

	for (i=0 ; i<3 ; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5;
		m = 128 * floor (m / 128 + 0.5);

		// don't subdivide
		if (maxs[i] - m < 8) continue;
		if (m - mins[i] < 8) continue;

		// prevent scratch buffer overflow
		if (surf->numglvertexes >= R_MAXWARPVERTEXES) continue;
		if (surf->numglindexes >= R_MAXWARPINDEXES) continue;

		// cut it
		v = verts + i;

		for (j=0 ; j<numverts ; j++, v+= 3)
			dist[j] = *v - m;

		// wrap cases
		dist[numverts] = dist[0];
		v-=i;
		VectorCopy (verts, v);
		v = verts;

		f = 0; b = 0;

		for (j=0 ; j<numverts ; j++, v+= 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy (v, front[f]);
				f++;
			}

			if (dist[j] <= 0)
			{
				VectorCopy (v, back[b]);
				b++;
			}

			if (dist[j] == 0 || dist[j+1] == 0)
				continue;

			if ( (dist[j] > 0) != (dist[j+1] > 0) )
			{
				// clip point
				float frac = dist[j] / (dist[j] - dist[j + 1]);

				for (k=0 ; k<3 ; k++)
					front[f][k] = back[b][k] = v[k] + frac*(v[3+k] - v[k]);

				f++;
				b++;
			}
		}

		// subdivide
		R_SubdividePolygon (surf, f, front[0], dstverts, dstindexes);
		R_SubdividePolygon (surf, b, back[0], dstverts, dstindexes);
		return;
	}

	// set up vertexes
	dst = dstverts + surf->numglvertexes;

	// for now we're just copying out the position to a temp buffer; this will be done
	// for real (and texcoords will be generated) after we remove duplicate verts.
	for (i=0 ; i<numverts ; i++, verts+= 3)
		VectorCopy (verts, dst[i].xyz);

	// indexes
	ndx = dstindexes + surf->numglindexes;

	for (i = 2; i < numverts; i++, ndx += 3)
	{
		ndx[0] = surf->numglvertexes;
		ndx[1] = surf->numglvertexes + i - 1;
		ndx[2] = surf->numglvertexes + i;
	}

	// accumulate
	surf->numglvertexes += numverts;
	surf->numglindexes += (numverts - 2) * 3;
}


void R_SubdivideSurface (model_t *mod, msurface_t *surf)
{
	int i;
	vec3_t		verts[64];

	// verts - we set a max of 4096 in the subdivision routine so all of this is safe
	unsigned short *dstindexes = (unsigned short *) scratchbuf;
	sdvert_t *dstverts = (sdvert_t *) (dstindexes + R_MAXWARPINDEXES);
	glvertex_t *finalverts = (glvertex_t *) (dstverts + R_MAXWARPVERTEXES);

//	if (gl_support_shader_objects) return;

	// bound gl_subdivide_size
//	if (gl_subdivide_size.value < 8.0f) Cvar_SetValue (&gl_subdivide_size, 8.0f);
//	if (gl_subdivide_size.value > 1024.0f) Cvar_SetValue (&gl_subdivide_size, 1024.0f);

	// don't bother for dedicated servers
	if (dedicated) return;

	// free memory used by this surf
	if (surf->glindexes) {R_WarpFree (surf->glindexes); surf->glindexes = NULL;}
	if (surf->glvertexes) {R_WarpFree (surf->glvertexes); surf->glvertexes = NULL;}

	// reconstruct the vertexes
	surf->midpoint[0] = surf->midpoint[1] = surf->midpoint[2] = 0;

	for (i = 0; i < surf->numedges; i++)
	{
		int lindex = mod->surfedges[surf->firstedge + i];

		if (lindex > 0)
		{
			verts[i][0] = mod->vertexes[mod->edges[lindex].v[0]].position[0];
			verts[i][1] = mod->vertexes[mod->edges[lindex].v[0]].position[1];
			verts[i][2] = mod->vertexes[mod->edges[lindex].v[0]].position[2];
		}
		else
		{
			verts[i][0] = mod->vertexes[mod->edges[-lindex].v[1]].position[0];
			verts[i][1] = mod->vertexes[mod->edges[-lindex].v[1]].position[1];
			verts[i][2] = mod->vertexes[mod->edges[-lindex].v[1]].position[2];
		}

		VectorAdd (surf->midpoint, verts[i], surf->midpoint);
	}

	VectorScale (surf->midpoint, 1.0f / surf->numedges, surf->midpoint);

	// nothing to see here yet
	surf->numglvertexes = 0;
	surf->numglindexes = 0;

	// subdivide the polygon
	R_SubdividePolygon (surf, surf->numedges, verts[0], dstverts, dstindexes);

	// Con_Printf ("surf->numglvertexes: was %i ", surf->numglvertexes);

	// begin again at 0 verts
	surf->numglvertexes = 0;

	// alloc indexes
	surf->glindexes = (unsigned short *) R_WarpAlloc (surf->numglindexes * sizeof (unsigned short));

	// remove duplicate verts from the surface
	// (we could optimize these further for the vertex cache, but they're already mostly
	// optimal and doing so would only slow things down and make it complicated...)
	for (i = 0; i < surf->numglindexes; i++)
	{
		// retrieve the vert
		sdvert_t *v = &dstverts[dstindexes[i]];
		int vnum;

		for (vnum = 0; vnum < surf->numglvertexes; vnum++)
		{
			// only verts need to be compared
			if (v->xyz[0] != finalverts[vnum].v[0]) continue;
			if (v->xyz[1] != finalverts[vnum].v[1]) continue;
			if (v->xyz[2] != finalverts[vnum].v[2]) continue;

			// already exists
			surf->glindexes[i] = vnum;
			break;
		}

		if (vnum == surf->numglvertexes)
		{
			// new vert and index
			surf->glindexes[i] = surf->numglvertexes;

			finalverts[surf->numglvertexes].v[0] = v->xyz[0];
			finalverts[surf->numglvertexes].v[1] = v->xyz[1];
			finalverts[surf->numglvertexes].v[2] = v->xyz[2];

			// texcoord generation can be deferred to here...
			// cache the unwarped s/t in the second set of texcoords for reuse in warping
			finalverts[surf->numglvertexes].st2[0] = DotProduct (v->xyz, surf->texinfo->vecs[0]);
			finalverts[surf->numglvertexes].st2[1] = DotProduct (v->xyz, surf->texinfo->vecs[1]);
			surf->numglvertexes++;
		}
	}

	// Com_Printf ("is now %i (with %i indexes)\n", surf->numglvertexes, surf->numglindexes);

	// create dest buffer for vertexes (indexes has already been done)
	surf->glvertexes = (glvertex_t *) R_WarpAlloc (surf->numglvertexes * sizeof (glvertex_t));
	memcpy (surf->glvertexes, finalverts, surf->numglvertexes * sizeof (glvertex_t));

	surf->subdividesize = 128; //gl_subdivide_size.value;
}


float r_lastliquidalpha = -1;
gltexture_t *r_lastliquidtexture = NULL;
int r_lastliquidflags = 0;

void R_InvalidateLiquid (void)
{
	r_lastliquidalpha = -1;
	r_lastliquidtexture = NULL;
	r_lastliquidflags = 0;
}

r_defaultquad_t *r_warpvertexes = NULL;
unsigned short *r_warpindexes = NULL;
int r_numwarpindexes = 0;
int r_numwarpvertexes = 0;
qbool r_restart_liquid_shader = false;
float lavafog = 0;
float currentfog = 0;

void R_LiquidSetShader (void)
{
/*
	if (Fog_GetDensity () > 0)
	{
		static int u_warpfactor = -1;
		static int u_warptime = -1;
		static int u_warpscale = -1;
		static int u_warptexture = -1;

		// some drivers need this before getting uniform locations
		qglUseProgramObject (hLiquidFogProgram);

		// we may need to reset the uniform locations if video needed to be restarted
		if (r_glsl_reset_liquid_fog)
		{
			// it's a programmer error if any of these fail...
			u_warpfactor = qglGetUniformLocation (hLiquidFogProgram, "warpfactor");
			u_warptime = qglGetUniformLocation (hLiquidFogProgram, "warptime");
			u_warpscale = qglGetUniformLocation (hLiquidFogProgram, "warpscale");
			u_warptexture = qglGetUniformLocation (hLiquidFogProgram, "warptexture");

			r_glsl_reset_liquid_fog = false;
		}

		// these factors can be cvar-ized but are just hard-coded for now
		qglUniform1f (u_warpfactor, 2.0f);
		qglUniform1f (u_warptime, cl.time * 10.18591625f * 4.0f);
		qglUniform1f (u_warpscale, 8.0f);
		qglUniform1i (u_warptexture, 0);        // tmu number to use, NOT the texture object number
	}
	else
	{
		static int u_warpfactor = -1;
		static int u_warptime = -1;
		static int u_warpscale = -1;
		static int u_warptexture = -1;

		// some drivers need this before getting uniform locations
		qglUseProgramObject (hLiquidProgram);

		// we may need to reset the uniform locations if video needed to be restarted
		if (r_glsl_reset_liquid)
		{
			// it's a programmer error if any of these fail...
			u_warpfactor = qglGetUniformLocation (hLiquidProgram, "warpfactor");
			u_warptime = qglGetUniformLocation (hLiquidProgram, "warptime");
			u_warpscale = qglGetUniformLocation (hLiquidProgram, "warpscale");
			u_warptexture = qglGetUniformLocation (hLiquidProgram, "warptexture");

			r_glsl_reset_liquid = false;
		}

		// these factors can be cvar-ized but are just hard-coded for now
		qglUniform1f (u_warpfactor, 2.0f);
		qglUniform1f (u_warptime, cl.time * 10.18591625f * 4.0f);
		qglUniform1f (u_warpscale, 8.0f);
		qglUniform1i (u_warptexture, 0);        // tmu number to use, NOT the texture object number
	}
*/
}


void R_LiquidBegin (void)
{
	R_InvalidateLiquid ();

	r_warpindexes = (unsigned short *) scratchbuf;
	r_warpvertexes = (r_defaultquad_t *) (r_warpindexes + R_MAXWARPINDEXES);
	r_numwarpindexes = 0;
	r_numwarpvertexes = 0;

//	if (gl_support_shader_objects) R_LiquidSetShader ();

	lavafog = Fog_GetDensity () * r_lavafog.value;
	currentfog = Fog_GetDensity ();

	GL_SetIndices (0, scratchbuf);
	GL_SetStreamSource (0, GLSTREAM_POSITION, 3, GL_FLOAT, sizeof (r_defaultquad_t), r_warpvertexes->xyz);
	GL_SetStreamSource (0, GLSTREAM_COLOR, 4, GL_UNSIGNED_BYTE, sizeof (r_defaultquad_t), r_warpvertexes->color);
	GL_SetStreamSource (0, GLSTREAM_TEXCOORD0, 2, GL_FLOAT, sizeof (r_defaultquad_t), r_warpvertexes->st);
	GL_SetStreamSource (0, GLSTREAM_TEXCOORD1, 0, GL_NONE, 0, NULL);
	GL_SetStreamSource (0, GLSTREAM_TEXCOORD2, 0, GL_NONE, 0, NULL);
}


void R_DrawAccumulatedWarpSurfs (void)
{
	if (r_numwarpindexes)
	{
		GL_DrawIndexedPrimitive (GL_TRIANGLES, r_numwarpindexes, r_numwarpvertexes);

/*
		if (gl_support_shader_objects && r_showtris.value)
		{
			qglUseProgramObject (0);
			r_restart_liquid_shader = true;
		}
*/

		if (r_showtris.value)
		{
			int			i;
			unsigned int *ndx = (unsigned int *) scratchbuf;
			r_warpvertexes = (r_defaultquad_t *) (ndx + R_MAXWARPINDEXES);

			// reset the colours to all white for showtris
			// this is so that we don't need to mess with resetting the vertex array
			for (i = 0; i < r_numwarpvertexes; i++, r_warpvertexes++)
				r_warpvertexes->rgba = 0xffffffff;

			// this is a state change so invalidate batched up states
			GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);

			R_ShowTrisBegin ();

			GL_SetStreamSource (0, GLSTREAM_COLOR, 0, GL_NONE, 0, NULL);
			GL_DrawIndexedPrimitive (GL_TRIANGLES, r_numwarpindexes, r_numwarpvertexes);

			R_ShowTrisEnd ();
			R_LiquidBegin ();
		}
	}

	// reset the vertex array
	r_warpindexes = (unsigned short *) scratchbuf;
	r_warpvertexes = (r_defaultquad_t *) (r_warpindexes + R_MAXWARPINDEXES);
	r_numwarpindexes = 0;
	r_numwarpvertexes = 0;
}


void R_LiquidEnd (void)
{
	R_DrawAccumulatedWarpSurfs ();

//	if (gl_support_shader_objects)
//		qglUseProgramObject (0);

	r_restart_liquid_shader = false;

	R_InvalidateLiquid ();
	qglColor4f (1, 1, 1, 1);
	GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);
	qglFogf (GL_FOG_DENSITY, Fog_GetDensity () / 64.0f);
}


void R_AddLiquidToAlphaList (r_modelsurf_t *ms);

float turbsin[] =
{
#include "gl_warp_sin.h"
};

#define WARPCALC(s,t) ((s + turbsin[(int) ((t * 2) + rdt) & 255]) * 0.015625f)
#define WARPCALC2(s,t) ((s + turbsin[(int) ((t * 0.125 + cl.time) * (128.0 / M_PI)) & 255]) * 0.015625f)

void R_CheckSubdivide (msurface_t *surf, model_t *mod)
{
	if (surf->flags & SURF_DRAWTURB)
	{
		// rebuild the surface if gl_subdivide_size changes
		// done separately because it stomps on scratchbuf otherwise :P
		if ((int) surf->subdividesize != (int) 128) //gl_subdivide_size.value)
			R_SubdivideSurface (mod, surf);
	}
}


void R_RenderLiquidSurface (msurface_t *surf, glmatrix *m)
{
	int		i;
	glvertex_t *v;
	float rdt = cl.time * (128.0 / M_PI);
	qbool r_restartwarp = false;
	r_defaultquad_t *dst;
	unsigned int rgba = 0xffffffff;
	float surffog = currentfog;

	if (r_restart_liquid_shader)
	{
		R_LiquidSetShader ();
		r_restart_liquid_shader = false;
	}

	// check for restarting the warp
	if (surf->texinfo->texture->gltexture != r_lastliquidtexture)
	{
		r_lastliquidtexture = surf->texinfo->texture->gltexture;
		r_restartwarp = true;
	}

	if (surf->wateralpha != r_lastliquidalpha && surf->wateralpha < 1)
	{
		r_lastliquidalpha = surf->wateralpha;
		r_restartwarp = true;
	}

	if (surf->flags != r_lastliquidflags)
	{
		if ((surf->flags & SURF_DRAWLAVA) && !(r_lastliquidflags & SURF_DRAWLAVA))
			surffog = Fog_GetDensity () * r_lavafog.value;
		else if ((surf->flags & SURF_DRAWSLIME) && !(r_lastliquidflags & SURF_DRAWSLIME))
			surffog = Fog_GetDensity () * r_slimefog.value;
		else if (!(surf->flags & SURF_DRAWLAVA) && (r_lastliquidflags & SURF_DRAWLAVA))
			surffog = Fog_GetDensity ();
		else if (!(surf->flags & SURF_DRAWSLIME) && (r_lastliquidflags & SURF_DRAWSLIME))
			surffog = Fog_GetDensity ();

		if (surffog != currentfog)
		{
			r_restartwarp = true;
			currentfog = surffog;
		}

		r_lastliquidflags = surf->flags;
	}

	// if the surf is too big to fit we need to restart so force a flush/reset
	// we ensured that each individual surf can never be bigger than this during subdivision
	if (r_numwarpvertexes + surf->numglvertexes >= R_MAXWARPVERTEXES) r_restartwarp = true;
	if (r_numwarpindexes + surf->numglindexes >= R_MAXWARPINDEXES) r_restartwarp = true;

	if (r_restartwarp)
	{
		// draw anything that has accumulated so far
		R_DrawAccumulatedWarpSurfs ();

		// switch texture
		GL_BindTexture (GL_TEXTURE0_ARB, surf->texinfo->texture->gltexture);

		if (!gl_support_shader_objects)
		{
			if (surf->wateralpha < 1)
				GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_MODULATE);
			else GL_TexEnv (GL_TEXTURE0_ARB, GL_TEXTURE_2D, GL_REPLACE);
		}

		// set correct fogging on this surface
		qglFogf (GL_FOG_DENSITY, surffog / 64.0f);
	}

	// transfer indexes
	r_warpindexes = R_TransferIndexes (surf->glindexes, r_warpindexes, surf->numglindexes, r_numwarpvertexes);
	r_numwarpindexes += surf->numglindexes;

	// warp the texcoords
/*	if (gl_support_shader_objects)
	{
		dst = r_warpvertexes;

		for (i = 0, v = surf->glvertexes; i < surf->numglvertexes; i++, v++, dst++)
		{
			dst->st[0] = v->st1[0];
			dst->st[1] = v->st1[1];
		}
	}
	else*/ if (surf->subdividesize > 48)
	{
		dst = r_warpvertexes;

		for (i = 0, v = surf->glvertexes; i < surf->numglvertexes; i++, v++, dst++)
		{
			dst->st[0] = WARPCALC2 (v->st2[0], v->st2[1]);
			dst->st[1] = WARPCALC2 (v->st2[1], v->st2[0]);
		}
	}
	else
	{
		dst = r_warpvertexes;

		for (i = 0, v = surf->glvertexes; i < surf->numglvertexes; i++, v++, dst++)
		{
			dst->st[0] = WARPCALC (v->st2[0], v->st2[1]);
			dst->st[1] = WARPCALC (v->st2[1], v->st2[0]);
		}
	}

	// transfer vertexes
	if (m)
	{
		dst = r_warpvertexes;

		for (i = 0, v = surf->glvertexes; i < surf->numglvertexes; i++, v++, dst++)
		{
			dst->xyz[0] = v->v[0] * m->m16[0] + v->v[1] * m->m16[4] + v->v[2] * m->m16[8] + m->m16[12];
			dst->xyz[1] = v->v[0] * m->m16[1] + v->v[1] * m->m16[5] + v->v[2] * m->m16[9] + m->m16[13];
			dst->xyz[2] = v->v[0] * m->m16[2] + v->v[1] * m->m16[6] + v->v[2] * m->m16[10] + m->m16[14];
		}
	}
	else
	{
		dst = r_warpvertexes;

		for (i = 0, v = surf->glvertexes; i < surf->numglvertexes; i++, v++, dst++)
		{
			dst->xyz[0] = v->v[0];
			dst->xyz[1] = v->v[1];
			dst->xyz[2] = v->v[2];
		}
	}

	// transfer colours
	((byte *) &rgba)[3] = BYTE_CLAMPF (surf->wateralpha);
	dst = r_warpvertexes;

	for (i = 0; i < surf->numglvertexes; i++, dst++)
		dst->rgba = rgba;

	r_warpvertexes += surf->numglvertexes;
	r_numwarpvertexes += surf->numglvertexes;
}


void R_RenderModelSurfsWater (void)
{
	gltexture_t *glt;
	r_modelsurf_t *ms;
	extern gltexture_t *active_gltextures;
	qbool stateset = false;

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		if (!(ms = glt->surfchain)) continue;
		if (!(ms->surface->flags & SURF_DRAWTURB)) continue;

		// build the vertex array for this texture
		for (; ms; ms = ms->surfchain)
		{
			// override water alpha for certain surface types
			if (!r_lockalpha.value)
			{
				if (ms->surface->flags & SURF_DRAWLAVA) ms->surface->wateralpha = r_lavaalpha.value;
				if (ms->surface->flags & SURF_DRAWTELE) ms->surface->wateralpha = r_telealpha.value;
				if (ms->surface->flags & SURF_DRAWSLIME) ms->surface->wateralpha = r_slimealpha.value;
			}

			// check for alpha surfs and defer them to the alpha list
			if (ms->surface->wateralpha < 1)
			{
				R_AddLiquidToAlphaList (ms);
				continue;
			}

			// run this per-surf so that we can use common code for alpha surfs
			if (!stateset)
			{
				R_LiquidBegin ();
				stateset = true;
			}

			// no alpha
			ms->surface->wateralpha = 1.0f;
			R_RenderLiquidSurface (ms->surface, ms->matrix);
		}

		// explicitly NULL the chain
		glt->surfchain = NULL;
	}

	if (stateset) R_LiquidEnd ();
}
