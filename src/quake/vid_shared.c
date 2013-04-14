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
// vid_shared.c -- shared video functionality

#include "gl_local.h"

// if window is hidden, don't update screen
int vid_hidden = true;
// if window is not the active window, don't hog as much CPU time,
// let go of the mouse, turn off sound, and restore system gamma ramps...
int vid_activewindow = true;
// whether to allow use of hwgamma (disabled when window is inactive)
int vid_allowhwgamma = false;

// we don't know until we try it!
int vid_hardwaregammasupported = true;
// whether hardware gamma ramps are currently in effect
int vid_usinghwgamma = false;

unsigned short vid_gammaramps[768];
unsigned short vid_systemgammaramps[768];

cvar_t vid_fullscreen = {"vid_fullscreen", "0", CVAR_ARCHIVE};
cvar_t vid_width = {"vid_width", "1024", CVAR_ARCHIVE};
cvar_t vid_height = {"vid_height", "768", CVAR_ARCHIVE};
cvar_t vid_mouse = {"vid_mouse", "1", CVAR_ARCHIVE};
cvar_t vid_vsync = {"vid_vsync", "1", CVAR_ARCHIVE};

cvar_t v_gamma = {"v_gamma", "1", CVAR_ARCHIVE};
cvar_t v_contrast = {"v_contrast", "1", CVAR_ARCHIVE};
cvar_t v_brightness = {"v_brightness", "0", CVAR_ARCHIVE};
cvar_t v_overbrightbits = {"v_overbrightbits", "0", CVAR_ARCHIVE};
cvar_t v_hwgamma = {"v_hwgamma", "1", CVAR_ARCHIVE};

// brand of graphics chip
const char *gl_vendor;
// graphics chip model and other information
const char *gl_renderer;
// begins with 1.0.0, 1.1.0, 1.2.0, 1.2.1, 1.3.0, 1.3.1, or 1.4.0
const char *gl_version;
// extensions list, space separated
const char *gl_extensions;
// WGL, GLX, or AGL
const char *gl_platform;
// another extensions list, containing platform-specific extensions that are
// not in the main list
const char *gl_platformextensions;
// name of driver library (opengl32.dll, libGL.so.1, or whatever)
char gl_driver[256];

// GL_ARB_multitexture
int gl_textureunits = 0;
// GL_EXT_compiled_vertex_array
int gl_supportslockarrays = false;
// GLX_SGI_video_sync or WGL_EXT_swap_control
int gl_videosyncavailable = false;
// GL_SGIS_texture_edge_clamp
int gl_support_clamptoedge = false;

// GL_ARB_multitexture
void (GLAPIENTRY *qglMultiTexCoord2f) (GLenum, GLfloat, GLfloat);
void (GLAPIENTRY *qglActiveTexture) (GLenum);
void (GLAPIENTRY *qglClientActiveTexture) (GLenum);

// GL_EXT_compiled_vertex_array
void (GLAPIENTRY *qglLockArraysEXT) (GLint first, GLint count);
void (GLAPIENTRY *qglUnlockArraysEXT) (void);

// general GL functions

void (GLAPIENTRY *qglClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);

void (GLAPIENTRY *qglClear)(GLbitfield mask);

void (GLAPIENTRY *qglAlphaFunc)(GLenum func, GLclampf ref);
void (GLAPIENTRY *qglBlendFunc)(GLenum sfactor, GLenum dfactor);
void (GLAPIENTRY *qglCullFace)(GLenum mode);

void (GLAPIENTRY *qglEnable)(GLenum cap);
void (GLAPIENTRY *qglDisable)(GLenum cap);

void (GLAPIENTRY *qglEnableClientState)(GLenum cap);
void (GLAPIENTRY *qglDisableClientState)(GLenum cap);

//void (GLAPIENTRY *qglGetBooleanv)(GLenum pname, GLboolean *params);
//void (GLAPIENTRY *qglGetDoublev)(GLenum pname, GLdouble *params);
//void (GLAPIENTRY *qglGetFloatv)(GLenum pname, GLfloat *params);
void (GLAPIENTRY *qglGetIntegerv)(GLenum pname, GLint *params);

GLenum (GLAPIENTRY *qglGetError)(void);
const GLubyte* (GLAPIENTRY *qglGetString)(GLenum name);
void (GLAPIENTRY *qglFinish)(void);
void (GLAPIENTRY *qglFlush)(void);

void (GLAPIENTRY *qglClearDepth)(GLclampd depth);
void (GLAPIENTRY *qglDepthFunc)(GLenum func);
void (GLAPIENTRY *qglDepthMask)(GLboolean flag);
void (GLAPIENTRY *qglDepthRange)(GLclampd near_val, GLclampd far_val);
void (GLAPIENTRY *qglColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);

void (GLAPIENTRY *qglDrawRangeElements)(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);
void (GLAPIENTRY *qglDrawElements)(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
void (GLAPIENTRY *qglVertexPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
void (GLAPIENTRY *qglNormalPointer)(GLenum type, GLsizei stride, const GLvoid *ptr);
void (GLAPIENTRY *qglColorPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
void (GLAPIENTRY *qglTexCoordPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
void (GLAPIENTRY *qglArrayElement)(GLint i);

void (GLAPIENTRY *qglColor3ubv)(const GLubyte* v);
void (GLAPIENTRY *qglColor3f)(GLfloat red, GLfloat green, GLfloat blue);
void (GLAPIENTRY *qglColor4ubv)(const GLubyte* v);
void (GLAPIENTRY *qglColor4f)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void (GLAPIENTRY *qglColor4fv)(const GLfloat* v);
void (GLAPIENTRY *qglTexCoord2f)(GLfloat s, GLfloat t);
void (GLAPIENTRY *qglVertex2f)(GLfloat x, GLfloat y);
void (GLAPIENTRY *qglVertex3f)(GLfloat x, GLfloat y, GLfloat z);
void (GLAPIENTRY *qglVertex3fv)(const GLfloat* v);
void (GLAPIENTRY *qglBegin)(GLenum mode);
void (GLAPIENTRY *qglEnd)(void);

void (GLAPIENTRY *qglMatrixMode)(GLenum mode);
void (GLAPIENTRY *qglOrtho)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
void (GLAPIENTRY *qglFrustum)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
void (GLAPIENTRY *qglViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
void (GLAPIENTRY *qglPushMatrix)(void);
void (GLAPIENTRY *qglPopMatrix)(void);
void (GLAPIENTRY *qglLoadIdentity)(void);
//void (GLAPIENTRY *qglLoadMatrixd)(const GLdouble *m);
void (GLAPIENTRY *qglLoadMatrixf)(const GLfloat *m);
//void (GLAPIENTRY *qglMultMatrixd)(const GLdouble *m);
//void (GLAPIENTRY *qglMultMatrixf)(const GLfloat *m);
//void (GLAPIENTRY *qglRotated)(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
void (GLAPIENTRY *qglRotatef)(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
//void (GLAPIENTRY *qglScaled)(GLdouble x, GLdouble y, GLdouble z);
void (GLAPIENTRY *qglScalef)(GLfloat x, GLfloat y, GLfloat z);
//void (GLAPIENTRY *qglTranslated)(GLdouble x, GLdouble y, GLdouble z);
void (GLAPIENTRY *qglTranslatef)(GLfloat x, GLfloat y, GLfloat z);

void (GLAPIENTRY *qglReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);

void (GLAPIENTRY *qglStencilFunc)(GLenum func, GLint ref, GLuint mask);
void (GLAPIENTRY *qglStencilMask)(GLuint mask);
void (GLAPIENTRY *qglStencilOp)(GLenum fail, GLenum zfail, GLenum zpass);
void (GLAPIENTRY *qglClearStencil)(GLint s);

void (GLAPIENTRY *qglTexEnvf)(GLenum target, GLenum pname, GLfloat param);
void (GLAPIENTRY *qglTexEnvi)(GLenum target, GLenum pname, GLint param);
void (GLAPIENTRY *qglTexParameterf)(GLenum target, GLenum pname, GLfloat param);
//void (GLAPIENTRY *qglTexParameterfv)(GLenum target, GLenum pname, GLfloat *params);
void (GLAPIENTRY *qglTexParameteri)(GLenum target, GLenum pname, GLint param);

void (GLAPIENTRY *qglGenTextures)(GLsizei n, GLuint *textures);
void (GLAPIENTRY *qglDeleteTextures)(GLsizei n, const GLuint *textures);
void (GLAPIENTRY *qglBindTexture)(GLenum target, GLuint texture);
//void (GLAPIENTRY *qglPrioritizeTextures)(GLsizei n, const GLuint *textures, const GLclampf *priorities);
//GLboolean (GLAPIENTRY *qglAreTexturesResident)(GLsizei n, const GLuint *textures, GLboolean *residences);
GLboolean (GLAPIENTRY *qglIsTexture)(GLuint texture);
//void (GLAPIENTRY *qglPixelStoref)(GLenum pname, GLfloat param);
void (GLAPIENTRY *qglPixelStorei)(GLenum pname, GLint param);

void (GLAPIENTRY *qglTexImage1D)(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void (GLAPIENTRY *qglTexImage2D)(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void (GLAPIENTRY *qglTexSubImage1D)(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels);
void (GLAPIENTRY *qglTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void (GLAPIENTRY *qglCopyTexImage1D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border);
void (GLAPIENTRY *qglCopyTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
void (GLAPIENTRY *qglCopyTexSubImage1D)(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
void (GLAPIENTRY *qglCopyTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);

void (GLAPIENTRY *qglTexImage3D)(GLenum target, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void (GLAPIENTRY *qglTexSubImage3D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels);
void (GLAPIENTRY *qglCopyTexSubImage3D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);

void (GLAPIENTRY *qglScissor)(GLint x, GLint y, GLsizei width, GLsizei height);

void (GLAPIENTRY *qglPolygonOffset)(GLfloat factor, GLfloat units);

int GL_CheckExtension(const char *minglver_or_ext, const dllfunction_t *funcs, char *disableparm, int silent)
{
	int failed = false;
	const dllfunction_t *func;
	struct { int major, minor; } min_version, curr_version;
	int ext;

	if(sscanf(minglver_or_ext, "%d.%d", &min_version.major, &min_version.minor) == 2)
		ext = 0; // opengl version
	else if(minglver_or_ext[0] != toupper(minglver_or_ext[0]))
		ext = -1; // pseudo name
	else
		ext = 1; // extension name

	if (ext)
		Com_DPrintf("checking for %s...  ", minglver_or_ext);
	else
		Com_DPrintf("checking for OpenGL %s core features...  ", minglver_or_ext);

	for (func = funcs; func && func->name; func++)
		*func->funcvariable = NULL;

	if (disableparm && COM_CheckParm(disableparm))
	{
		Com_DPrintf("disabled by commandline\n");
		return false;
	}

	if (ext == 1) // opengl extension
	{
		if (!strstr(gl_extensions ? gl_extensions : "", minglver_or_ext) && !strstr(gl_platformextensions ? gl_platformextensions : "", minglver_or_ext))
		{
			Com_DPrintf("not detected\n");
			return false;
		}
	}

	if(ext == 0) // opengl version
	{
		if (sscanf(gl_version, "%d.%d", &curr_version.major, &curr_version.minor) < 2)
			curr_version.major = curr_version.minor = 1;

		if (curr_version.major < min_version.major || (curr_version.major == min_version.major && curr_version.minor < min_version.minor))
		{
			Com_DPrintf("not detected (OpenGL %d.%d loaded)\n", curr_version.major, curr_version.minor);
			return false;
		}
	}

	for (func = funcs; func && func->name != NULL; func++)
	{
		// functions are cleared before all the extensions are evaluated
		if (!(*func->funcvariable = (void *) GL_GetProcAddress(func->name)))
		{
			if (!silent)
				Com_DPrintf("missing function \"%s\" - broken driver!\n", func->name);
			failed = true;
		}
	}

	// delay the return so it prints all missing functions
	if (failed)
		return false;
	
	Com_DPrintf("enabled\n");
	return true;
}

static dllfunction_t opengl110funcs[] =
{
	{"glClearColor", (void **) &qglClearColor},
	{"glClear", (void **) &qglClear},
	{"glAlphaFunc", (void **) &qglAlphaFunc},
	{"glBlendFunc", (void **) &qglBlendFunc},
	{"glCullFace", (void **) &qglCullFace},
	{"glEnable", (void **) &qglEnable},
	{"glDisable", (void **) &qglDisable},
	{"glEnableClientState", (void **) &qglEnableClientState},
	{"glDisableClientState", (void **) &qglDisableClientState},
//	{"glGetBooleanv", (void **) &qglGetBooleanv},
//	{"glGetDoublev", (void **) &qglGetDoublev},
//	{"glGetFloatv", (void **) &qglGetFloatv},
	{"glGetIntegerv", (void **) &qglGetIntegerv},
	{"glGetError", (void **) &qglGetError},
	{"glGetString", (void **) &qglGetString},
	{"glFinish", (void **) &qglFinish},
	{"glFlush", (void **) &qglFlush},
	{"glClearDepth", (void **) &qglClearDepth},
	{"glDepthFunc", (void **) &qglDepthFunc},
	{"glDepthMask", (void **) &qglDepthMask},
	{"glDepthRange", (void **) &qglDepthRange},
	{"glDrawElements", (void **) &qglDrawElements},
	{"glColorMask", (void **) &qglColorMask},
	{"glVertexPointer", (void **) &qglVertexPointer},
	{"glNormalPointer", (void **) &qglNormalPointer},
	{"glColorPointer", (void **) &qglColorPointer},
	{"glTexCoordPointer", (void **) &qglTexCoordPointer},
	{"glArrayElement", (void **) &qglArrayElement},
	{"glColor3ubv", (void **) &qglColor3ubv},
	{"glColor3f", (void **) &qglColor3f},
	{"glColor4ubv", (void **) &qglColor4ubv},
	{"glColor4f", (void **) &qglColor4f},
	{"glColor4fv", (void **) &qglColor4fv},
	{"glTexCoord2f", (void **) &qglTexCoord2f},
	{"glVertex2f", (void **) &qglVertex2f},
	{"glVertex3f", (void **) &qglVertex3f},
	{"glVertex3fv", (void **) &qglVertex3fv},
	{"glBegin", (void **) &qglBegin},
	{"glEnd", (void **) &qglEnd},
	{"glMatrixMode", (void **) &qglMatrixMode},
	{"glOrtho", (void **) &qglOrtho},
	{"glFrustum", (void **) &qglFrustum},
	{"glViewport", (void **) &qglViewport},
	{"glPushMatrix", (void **) &qglPushMatrix},
	{"glPopMatrix", (void **) &qglPopMatrix},
	{"glLoadIdentity", (void **) &qglLoadIdentity},
//	{"glLoadMatrixd", (void **) &qglLoadMatrixd},
	{"glLoadMatrixf", (void **) &qglLoadMatrixf},
//	{"glMultMatrixd", (void **) &qglMultMatrixd},
//	{"glMultMatrixf", (void **) &qglMultMatrixf},
//	{"glRotated", (void **) &qglRotated},
	{"glRotatef", (void **) &qglRotatef},
//	{"glScaled", (void **) &qglScaled},
	{"glScalef", (void **) &qglScalef},
//	{"glTranslated", (void **) &qglTranslated},
	{"glTranslatef", (void **) &qglTranslatef},
	{"glReadPixels", (void **) &qglReadPixels},
	{"glStencilFunc", (void **) &qglStencilFunc},
	{"glStencilMask", (void **) &qglStencilMask},
	{"glStencilOp", (void **) &qglStencilOp},
	{"glClearStencil", (void **) &qglClearStencil},
	{"glTexEnvf", (void **) &qglTexEnvf},
	{"glTexEnvi", (void **) &qglTexEnvi},
	{"glTexParameterf", (void **) &qglTexParameterf},
//	{"glTexParameterfv", (void **) &qglTexParameterfv},
	{"glTexParameteri", (void **) &qglTexParameteri},
//	{"glPixelStoref", (void **) &qglPixelStoref},
	{"glPixelStorei", (void **) &qglPixelStorei},
	{"glGenTextures", (void **) &qglGenTextures},
	{"glDeleteTextures", (void **) &qglDeleteTextures},
	{"glBindTexture", (void **) &qglBindTexture},
//	{"glPrioritizeTextures", (void **) &qglPrioritizeTextures},
//	{"glAreTexturesResident", (void **) &qglAreTexturesResident},
	{"glIsTexture", (void **) &qglIsTexture},
	{"glTexImage1D", (void **) &qglTexImage1D},
	{"glTexImage2D", (void **) &qglTexImage2D},
	{"glTexSubImage1D", (void **) &qglTexSubImage1D},
	{"glTexSubImage2D", (void **) &qglTexSubImage2D},
	{"glCopyTexImage1D", (void **) &qglCopyTexImage1D},
	{"glCopyTexImage2D", (void **) &qglCopyTexImage2D},
	{"glCopyTexSubImage1D", (void **) &qglCopyTexSubImage1D},
	{"glCopyTexSubImage2D", (void **) &qglCopyTexSubImage2D},
	{"glScissor", (void **) &qglScissor},
	{"glPolygonOffset", (void **) &qglPolygonOffset},
	{NULL, NULL}
};

static dllfunction_t multitexturefuncs[] =
{
	{"glMultiTexCoord2fARB", (void **) &qglMultiTexCoord2f},
	{"glActiveTextureARB", (void **) &qglActiveTexture},
	{"glClientActiveTextureARB", (void **) &qglClientActiveTexture},
	{NULL, NULL}
};

static dllfunction_t compiledvertexarrayfuncs[] =
{
	{"glLockArraysEXT", (void **) &qglLockArraysEXT},
	{"glUnlockArraysEXT", (void **) &qglUnlockArraysEXT},
	{NULL, NULL}
};

void VID_CheckExtensions(void)
{
	vid.renderpath = RENDERPATH_GL11;

	// reset all the gl extension variables here
	// this should match the declarations
	gl_textureunits = 1;
	gl_supportslockarrays = false;
	gl_support_clamptoedge = false;

	if (!GL_CheckExtension("glbase", opengl110funcs, NULL, false))
		Sys_Error("OpenGL 1.1.0 functions not found");

	Com_DPrintf ("GL_VENDOR: %s\n", gl_vendor);
	Com_DPrintf ("GL_RENDERER: %s\n", gl_renderer);
	Com_DPrintf ("GL_VERSION: %s\n", gl_version);
	Com_DPrintf ("GL_EXTENSIONS: %s\n", gl_extensions);
	Com_DPrintf ("%s_EXTENSIONS: %s\n", gl_platform, gl_platformextensions);

	if (GL_CheckExtension("GL_ARB_multitexture", multitexturefuncs, "-nomtex", false))
		qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &gl_textureunits);

	gl_supportslockarrays = GL_CheckExtension("GL_EXT_compiled_vertex_array", compiledvertexarrayfuncs, "-nocva", false);
	gl_support_clamptoedge = GL_CheckExtension("GL_EXT_texture_edge_clamp", NULL, "-noedgeclamp", false) || GL_CheckExtension("GL_SGIS_texture_edge_clamp", NULL, "-noedgeclamp", false);
}

static void BuildGammaTable16(float prescale, float gamma, float scale, float base, unsigned short *out)
{
	int i, adjusted;
	double invgamma, d;

	gamma = bound(0.1, gamma, 5.0);
	if (gamma == 1)
	{
		for (i = 0;i < 256;i++)
		{
			adjusted = (int) (i * 256.0 * prescale * scale + base * 65535.0);
			out[i] = bound(0, adjusted, 65535);
		}
	}
	else
	{
		invgamma = 1.0 / gamma;
		prescale /= 255.0f;
		for (i = 0;i < 256;i++)
		{
			d = pow((double) i * prescale, invgamma) * scale + base;
			adjusted = (int) (65535.0 * d);
			out[i] = bound(0, adjusted, 65535);
		}
	}
}

static float cachegamma, cachebrightness, cachecontrast;
static int cacheoverbrightbits = -1, cachehwgamma;
#define BOUNDCVAR(cvar, m1, m2) c = &(cvar);f = bound(m1, c->value, m2);if (c->value != f) Cvar_SetValue(c, f);
void VID_UpdateGamma(qbool force)
{
	cvar_t *c;
	float f;

	// don't mess with gamma tables if running dedicated
	if (dedicated)
		return;

	if (!force
	 && vid_usinghwgamma == (vid_allowhwgamma && v_hwgamma.value)
	 && v_overbrightbits.value == cacheoverbrightbits
	 && v_gamma.value == cachegamma
	 && v_contrast.value == cachecontrast
	 && v_brightness.value == cachebrightness)
		return;

	if (vid_allowhwgamma && v_hwgamma.value)
	{
		if (!vid_usinghwgamma)
		{
			vid_usinghwgamma = true;
			vid_hardwaregammasupported = VID_GetGamma(vid_systemgammaramps);
		}

		BOUNDCVAR(v_gamma, 0.1, 5);cachegamma = v_gamma.value;
		BOUNDCVAR(v_contrast, 1, 5);cachecontrast = v_contrast.value;
		BOUNDCVAR(v_brightness, 0, 0.8);cachebrightness = v_brightness.value;
		cacheoverbrightbits = v_overbrightbits.value;
		cachehwgamma = v_hwgamma.value;

		BuildGammaTable16((float) (1 << cacheoverbrightbits), cachegamma, cachecontrast, cachebrightness, vid_gammaramps);
		BuildGammaTable16((float) (1 << cacheoverbrightbits), cachegamma, cachecontrast, cachebrightness, vid_gammaramps + 256);
		BuildGammaTable16((float) (1 << cacheoverbrightbits), cachegamma, cachecontrast, cachebrightness, vid_gammaramps + 512);

		vid_hardwaregammasupported = VID_SetGamma(vid_gammaramps);
	}
	else
	{
		if (vid_usinghwgamma)
		{
			vid_usinghwgamma = false;
			vid_hardwaregammasupported = VID_SetGamma(vid_systemgammaramps);
		}
	}
}

void VID_RestoreSystemGamma(void)
{
	if (vid_usinghwgamma)
	{
		vid_usinghwgamma = false;
		VID_SetGamma(vid_systemgammaramps);
	}
}

int current_vid_fullscreen;
int current_vid_width;
int current_vid_height;
extern int VID_InitMode (int fullscreen, int width, int height);
int VID_Mode(int fullscreen, int width, int height)
{
	if (fullscreen)
		Com_Printf("Video: %dx%d fullscreen\n", width, height);
	else
		Com_Printf("Video: %dx%d windowed\n", width, height);

	if (VID_InitMode(fullscreen, width, height))
	{
		current_vid_fullscreen = fullscreen;
		current_vid_width = width;
		current_vid_height = height;
		Cvar_SetValue(&vid_fullscreen, fullscreen);
		Cvar_SetValue(&vid_width, width);
		Cvar_SetValue(&vid_height, height);
		return true;
	}
	else
	{
		return false;
	}
}

void VID_Shared_Init(void)
{
	int i;

	Cvar_Register(&v_gamma);
	Cvar_Register(&v_brightness);
	Cvar_Register(&v_contrast);
	Cvar_Register(&v_hwgamma);
	Cvar_Register(&v_overbrightbits);

	Cvar_Register(&vid_fullscreen);
	Cvar_Register(&vid_width);
	Cvar_Register(&vid_height);
	Cvar_Register(&vid_mouse);
	Cvar_Register(&vid_vsync);

	Cmd_AddCommand("vid_restart", VID_Restart_f);
	
	// interpret command-line parameters
	if ((i = COM_CheckParm("-window")) != 0)
		Cvar_SetValue(&vid_fullscreen, 0);
	if ((i = COM_CheckParm("-fullscreen")) != 0)
		Cvar_SetValue(&vid_fullscreen, 1);
	if ((i = COM_CheckParm("-width")) != 0)
		Cvar_Set(&vid_width, com_argv[i+1]);
	if ((i = COM_CheckParm("-height")) != 0)
		Cvar_Set(&vid_height, com_argv[i+1]);
}

static void VID_OpenSystems(void)
{
	R_Modules_Start();
}

static void VID_CloseSystems(void)
{
	R_Modules_Shutdown();
}

void VID_Restart_f(void)
{
	Cmd_ExecuteString("disconnect");

	Com_Printf("VID_Restart: changing from %s %dx%d, to %s %dx%d.\n",
		current_vid_fullscreen ? "fullscreen" : "window", current_vid_width, current_vid_height, 
		vid_fullscreen.value ? "fullscreen" : "window", vid_width.value, vid_height.value);
	VID_CloseSystems();
	VID_Shutdown();
	if (!VID_Mode(vid_fullscreen.value, vid_width.value, vid_height.value))
	{
		Com_Printf("Video mode change failed\n");
		if (!VID_Mode(current_vid_fullscreen, current_vid_width, current_vid_height))
			Sys_Error("Unable to restore to last working video mode\n");
	}
	Cache_Flush ();
	VID_OpenSystems();
}

// this is only called once by Host_StartVideo
void VID_Start(void)
{
	Com_Printf("Starting video system\n");
	if (!VID_Mode(vid_fullscreen.value, vid_width.value, vid_height.value))
	{
		Com_Printf("Desired video mode failed, trying fallbacks...\n");
		if (vid_fullscreen.value)
		{
			if (!VID_Mode(true, 640, 480))
				if (!VID_Mode(false, 640, 480))
					Sys_Error("Video modes failed\n");
		}
		else
		{
			Sys_Error("Windowed video failed\n");
		}
	}

	VID_OpenSystems();
}
