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
#include "sound.h"
#include "cdaudio.h"

// GL_ARB_multitexture
int gl_textureunits = 1;
int gl_maxtexturesize = 256;
// GL_EXT_compiled_vertex_array
int gl_supportslockarrays = false;
// GLX_SGI_video_sync or WGL_EXT_swap_control
int gl_videosyncavailable = false;
// GLX_EXT_swap_control_tear or WGL_EXT_swap_control_tear
int gl_videosynccontroltearavailable = false;
// GL_SGIS_texture_edge_clamp
int gl_support_clamptoedge = false;
// GL_ARB_texture_env_combine
int gl_support_texture_combine = false;
// GL_ARB_texture_env_add
int gl_support_texture_add = false;
// GL_ARB_texture_non_power_of_two
int gl_support_texture_npot = false;
// GL_ARB_shader_objects
int gl_support_shader_objects = false;
// GL_ARB_shading_language_100
int gl_support_shading_language_100 = false;
// GL_ARB_vertex_shader
int gl_support_vertex_shader = false;
// GL_ARB_fragment_shader
int gl_support_fragment_shader = false;
// gl_support_arb_vertex_buffer_object
int gl_support_arb_vertex_buffer_object = false;
// GL_ARB_occlusion_query
int gl_support_arb_occlusion_query = false;

// if window is hidden, don't update screen
qbool vid_hidden = true;
// if window is not the active window, don't hog as much CPU time,
// let go of the mouse, turn off sound, and restore system gamma ramps...
qbool vid_activewindow = true;
// whether to allow use of hwgamma (disabled when window is inactive)
qbool vid_allowhwgamma = false;

// we don't know until we try it!
qbool vid_hardwaregammasupported = true;
// whether hardware gamma ramps are currently in effect
qbool vid_usinghwgamma = false;

unsigned short vid_gammaramps[768];
unsigned short vid_systemgammaramps[768];

cvar_t vid_fullscreen = {"vid_fullscreen", "0", CVAR_ARCHIVE};
cvar_t vid_width = {"vid_width", "1280", CVAR_ARCHIVE};
cvar_t vid_height = {"vid_height", "720", CVAR_ARCHIVE};
cvar_t vid_conwidth = {"vid_conwidth", "640", CVAR_ARCHIVE};
cvar_t vid_conheight = {"vid_conheight", "360", CVAR_ARCHIVE};
cvar_t vid_pixelheight = {"vid_pixelheight", "1", CVAR_ARCHIVE};
cvar_t vid_refreshrate = {"vid_refreshrate", "60", CVAR_ARCHIVE};
cvar_t vid_userefreshrate = {"vid_userefreshrate", "0", CVAR_ARCHIVE};
cvar_t vid_samples = {"vid_samples", "1", CVAR_ARCHIVE};

cvar_t vid_vsync = {"vid_vsync", "0", CVAR_ARCHIVE};
cvar_t vid_mouse = {"vid_mouse", "1", CVAR_ARCHIVE};
cvar_t vid_minwidth = {"vid_minwidth", "0"};
cvar_t vid_minheight = {"vid_minheight", "0"};

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

void (GLAPIENTRY *qglDrawArrays)(GLenum mode, GLint first, GLsizei count);
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
void (GLAPIENTRY *qglTexCoord2fv)(const GLfloat *v);
void (GLAPIENTRY *qglTexCoord3fv)(const GLfloat *v);
void (GLAPIENTRY *qglVertex2f)(GLfloat x, GLfloat y);
void (GLAPIENTRY *qglVertex3f)(GLfloat x, GLfloat y, GLfloat z);
void (GLAPIENTRY *qglVertex3fv)(const GLfloat* v);
void (GLAPIENTRY *qglBegin)(GLenum mode);
void (GLAPIENTRY *qglEnd)(void);

void (GLAPIENTRY *qglFogf)(GLenum pname, GLfloat param);
void (GLAPIENTRY *qglFogfv)(GLenum pname, const GLfloat *params);
void (GLAPIENTRY *qglFogi)(GLenum pname, GLint param);

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
void (GLAPIENTRY *qglMultMatrixf)(const GLfloat *m);
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
void (GLAPIENTRY *qglPolygonMode)(GLenum face , GLenum mode);

void (GLAPIENTRY *qglDeleteObjectARB)(GLhandleARB obj);
GLhandleARB (GLAPIENTRY *qglGetHandleARB)(GLenum pname);
void (GLAPIENTRY *qglDetachObjectARB)(GLhandleARB containerObj, GLhandleARB attachedObj);
GLhandleARB (GLAPIENTRY *qglCreateShaderObjectARB)(GLenum shaderType);
void (GLAPIENTRY *qglShaderSourceARB)(GLhandleARB shaderObj, GLsizei count, const GLcharARB **string, const GLint *length);
void (GLAPIENTRY *qglCompileShaderARB)(GLhandleARB shaderObj);
GLhandleARB (GLAPIENTRY *qglCreateProgramObjectARB)(void);
void (GLAPIENTRY *qglAttachObjectARB)(GLhandleARB containerObj, GLhandleARB obj);
void (GLAPIENTRY *qglLinkProgramARB)(GLhandleARB programObj);
void (GLAPIENTRY *qglUseProgramObjectARB)(GLhandleARB programObj);
void (GLAPIENTRY *qglValidateProgramARB)(GLhandleARB programObj);
void (GLAPIENTRY *qglUniform1fARB)(GLint location, GLfloat v0);
void (GLAPIENTRY *qglUniform2fARB)(GLint location, GLfloat v0, GLfloat v1);
void (GLAPIENTRY *qglUniform3fARB)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
void (GLAPIENTRY *qglUniform4fARB)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
void (GLAPIENTRY *qglUniform1iARB)(GLint location, GLint v0);
void (GLAPIENTRY *qglUniform2iARB)(GLint location, GLint v0, GLint v1);
void (GLAPIENTRY *qglUniform3iARB)(GLint location, GLint v0, GLint v1, GLint v2);
void (GLAPIENTRY *qglUniform4iARB)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
void (GLAPIENTRY *qglUniform1fvARB)(GLint location, GLsizei count, const GLfloat *value);
void (GLAPIENTRY *qglUniform2fvARB)(GLint location, GLsizei count, const GLfloat *value);
void (GLAPIENTRY *qglUniform3fvARB)(GLint location, GLsizei count, const GLfloat *value);
void (GLAPIENTRY *qglUniform4fvARB)(GLint location, GLsizei count, const GLfloat *value);
void (GLAPIENTRY *qglUniform1ivARB)(GLint location, GLsizei count, const GLint *value);
void (GLAPIENTRY *qglUniform2ivARB)(GLint location, GLsizei count, const GLint *value);
void (GLAPIENTRY *qglUniform3ivARB)(GLint location, GLsizei count, const GLint *value);
void (GLAPIENTRY *qglUniform4ivARB)(GLint location, GLsizei count, const GLint *value);
void (GLAPIENTRY *qglUniformMatrix2fvARB)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void (GLAPIENTRY *qglUniformMatrix3fvARB)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void (GLAPIENTRY *qglUniformMatrix4fvARB)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void (GLAPIENTRY *qglGetObjectParameterfvARB)(GLhandleARB obj, GLenum pname, GLfloat *params);
void (GLAPIENTRY *qglGetObjectParameterivARB)(GLhandleARB obj, GLenum pname, GLint *params);
void (GLAPIENTRY *qglGetInfoLogARB)(GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *infoLog);
void (GLAPIENTRY *qglGetAttachedObjectsARB)(GLhandleARB containerObj, GLsizei maxCount, GLsizei *count, GLhandleARB *obj);
GLint (GLAPIENTRY *qglGetUniformLocationARB)(GLhandleARB programObj, const GLcharARB *name);
void (GLAPIENTRY *qglGetActiveUniformARB)(GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
void (GLAPIENTRY *qglGetUniformfvARB)(GLhandleARB programObj, GLint location, GLfloat *params);
void (GLAPIENTRY *qglGetUniformivARB)(GLhandleARB programObj, GLint location, GLint *params);
void (GLAPIENTRY *qglGetShaderSourceARB)(GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *source);

//void (GLAPIENTRY *qglVertexAttrib1fARB)(GLuint index, GLfloat v0);
//void (GLAPIENTRY *qglVertexAttrib1sARB)(GLuint index, GLshort v0);
//void (GLAPIENTRY *qglVertexAttrib1dARB)(GLuint index, GLdouble v0);
//void (GLAPIENTRY *qglVertexAttrib2fARB)(GLuint index, GLfloat v0, GLfloat v1);
//void (GLAPIENTRY *qglVertexAttrib2sARB)(GLuint index, GLshort v0, GLshort v1);
//void (GLAPIENTRY *qglVertexAttrib2dARB)(GLuint index, GLdouble v0, GLdouble v1);
//void (GLAPIENTRY *qglVertexAttrib3fARB)(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2);
//void (GLAPIENTRY *qglVertexAttrib3sARB)(GLuint index, GLshort v0, GLshort v1, GLshort v2);
//void (GLAPIENTRY *qglVertexAttrib3dARB)(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2);
//void (GLAPIENTRY *qglVertexAttrib4fARB)(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
//void (GLAPIENTRY *qglVertexAttrib4sARB)(GLuint index, GLshort v0, GLshort v1, GLshort v2, GLshort v3);
//void (GLAPIENTRY *qglVertexAttrib4dARB)(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3);
//void (GLAPIENTRY *qglVertexAttrib4NubARB)(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
//void (GLAPIENTRY *qglVertexAttrib1fvARB)(GLuint index, const GLfloat *v);
//void (GLAPIENTRY *qglVertexAttrib1svARB)(GLuint index, const GLshort *v);
//void (GLAPIENTRY *qglVertexAttrib1dvARB)(GLuint index, const GLdouble *v);
//void (GLAPIENTRY *qglVertexAttrib2fvARB)(GLuint index, const GLfloat *v);
//void (GLAPIENTRY *qglVertexAttrib2svARB)(GLuint index, const GLshort *v);
//void (GLAPIENTRY *qglVertexAttrib2dvARB)(GLuint index, const GLdouble *v);
//void (GLAPIENTRY *qglVertexAttrib3fvARB)(GLuint index, const GLfloat *v);
//void (GLAPIENTRY *qglVertexAttrib3svARB)(GLuint index, const GLshort *v);
//void (GLAPIENTRY *qglVertexAttrib3dvARB)(GLuint index, const GLdouble *v);
//void (GLAPIENTRY *qglVertexAttrib4fvARB)(GLuint index, const GLfloat *v);
//void (GLAPIENTRY *qglVertexAttrib4svARB)(GLuint index, const GLshort *v);
//void (GLAPIENTRY *qglVertexAttrib4dvARB)(GLuint index, const GLdouble *v);
//void (GLAPIENTRY *qglVertexAttrib4ivARB)(GLuint index, const GLint *v);
//void (GLAPIENTRY *qglVertexAttrib4bvARB)(GLuint index, const GLbyte *v);
//void (GLAPIENTRY *qglVertexAttrib4ubvARB)(GLuint index, const GLubyte *v);
//void (GLAPIENTRY *qglVertexAttrib4usvARB)(GLuint index, const GLushort *v);
//void (GLAPIENTRY *qglVertexAttrib4uivARB)(GLuint index, const GLuint *v);
//void (GLAPIENTRY *qglVertexAttrib4NbvARB)(GLuint index, const GLbyte *v);
//void (GLAPIENTRY *qglVertexAttrib4NsvARB)(GLuint index, const GLshort *v);
//void (GLAPIENTRY *qglVertexAttrib4NivARB)(GLuint index, const GLint *v);
//void (GLAPIENTRY *qglVertexAttrib4NubvARB)(GLuint index, const GLubyte *v);
//void (GLAPIENTRY *qglVertexAttrib4NusvARB)(GLuint index, const GLushort *v);
//void (GLAPIENTRY *qglVertexAttrib4NuivARB)(GLuint index, const GLuint *v);
void (GLAPIENTRY *qglVertexAttribPointerARB)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
void (GLAPIENTRY *qglEnableVertexAttribArrayARB)(GLuint index);
void (GLAPIENTRY *qglDisableVertexAttribArrayARB)(GLuint index);
void (GLAPIENTRY *qglBindAttribLocationARB)(GLhandleARB programObj, GLuint index, const GLcharARB *name);
void (GLAPIENTRY *qglGetActiveAttribARB)(GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
GLint (GLAPIENTRY *qglGetAttribLocationARB)(GLhandleARB programObj, const GLcharARB *name);
//void (GLAPIENTRY *qglGetVertexAttribdvARB)(GLuint index, GLenum pname, GLdouble *params);
//void (GLAPIENTRY *qglGetVertexAttribfvARB)(GLuint index, GLenum pname, GLfloat *params);
//void (GLAPIENTRY *qglGetVertexAttribivARB)(GLuint index, GLenum pname, GLint *params);
//void (GLAPIENTRY *qglGetVertexAttribPointervARB)(GLuint index, GLenum pname, GLvoid **pointer);

// gl_support_arb_vertex_buffer_object
void (GLAPIENTRY *qglBindBufferARB) (GLenum target, GLuint buffer);
void (GLAPIENTRY *qglDeleteBuffersARB) (GLsizei n, const GLuint *buffers);
void (GLAPIENTRY *qglGenBuffersARB) (GLsizei n, GLuint *buffers);
GLboolean (GLAPIENTRY *qglIsBufferARB) (GLuint buffer);
GLvoid* (GLAPIENTRY *qglMapBufferARB) (GLenum target, GLenum access);
GLboolean (GLAPIENTRY *qglUnmapBufferARB) (GLenum target);
void (GLAPIENTRY *qglBufferDataARB) (GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage);
void (GLAPIENTRY *qglBufferSubDataARB) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data);

void (GLAPIENTRY *qglGenQueriesARB)(GLsizei n, GLuint *ids);
void (GLAPIENTRY *qglDeleteQueriesARB)(GLsizei n, const GLuint *ids);
GLboolean (GLAPIENTRY *qglIsQueryARB)(GLuint qid);
void (GLAPIENTRY *qglBeginQueryARB)(GLenum target, GLuint qid);
void (GLAPIENTRY *qglEndQueryARB)(GLenum target);
void (GLAPIENTRY *qglGetQueryivARB)(GLenum target, GLenum pname, GLint *params);
void (GLAPIENTRY *qglGetQueryObjectivARB)(GLuint qid, GLenum pname, GLint *params);
void (GLAPIENTRY *qglGetQueryObjectuivARB)(GLuint qid, GLenum pname, GLuint *params);

// OpenGL 3.0 core functions
void (GLAPIENTRY *qglGenerateMipmap)(GLenum target);

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

	if (!silent)
	{
		if (ext)
			Com_DPrintf("checking for %s...  ", minglver_or_ext);
		else
			Com_DPrintf("checking for OpenGL %s core features...  ", minglver_or_ext);
	}

	for (func = funcs; func && func->name; func++)
		*func->funcvariable = NULL;

	if (disableparm && COM_CheckParm(disableparm))
	{
		if (!silent)
			Com_DPrintf("disabled by commandline\n");
		return false;
	}

	if (ext == 1) // opengl extension
	{
		if (!strstr(gl_extensions ? gl_extensions : "", minglver_or_ext) && !strstr(gl_platformextensions ? gl_platformextensions : "", minglver_or_ext))
		{
			if (!silent)
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
	
	if (!silent)
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
	{"glDrawArrays", (void **) &qglDrawArrays},
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
	{"glTexCoord2fv", (void **) &qglTexCoord2fv},
	{"glTexCoord3fv", (void **) &qglTexCoord3fv},
	{"glVertex2f", (void **) &qglVertex2f},
	{"glVertex3f", (void **) &qglVertex3f},
	{"glVertex3fv", (void **) &qglVertex3fv},
	{"glBegin", (void **) &qglBegin},
	{"glEnd", (void **) &qglEnd},
	{"glFogf", (void **) &qglFogf},
	{"glFogfv", (void **) &qglFogfv},
	{"glFogi", (void **) &qglFogi},
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
	{"glMultMatrixf", (void **) &qglMultMatrixf},
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
	{"glPolygonMode", (void **) &qglPolygonMode},
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

static dllfunction_t shaderobjectsfuncs[] =
{
	{"glDeleteObjectARB", (void **) &qglDeleteObjectARB},
	{"glGetHandleARB", (void **) &qglGetHandleARB},
	{"glDetachObjectARB", (void **) &qglDetachObjectARB},
	{"glCreateShaderObjectARB", (void **) &qglCreateShaderObjectARB},
	{"glShaderSourceARB", (void **) &qglShaderSourceARB},
	{"glCompileShaderARB", (void **) &qglCompileShaderARB},
	{"glCreateProgramObjectARB", (void **) &qglCreateProgramObjectARB},
	{"glAttachObjectARB", (void **) &qglAttachObjectARB},
	{"glLinkProgramARB", (void **) &qglLinkProgramARB},
	{"glUseProgramObjectARB", (void **) &qglUseProgramObjectARB},
	{"glValidateProgramARB", (void **) &qglValidateProgramARB},
	{"glUniform1fARB", (void **) &qglUniform1fARB},
	{"glUniform2fARB", (void **) &qglUniform2fARB},
	{"glUniform3fARB", (void **) &qglUniform3fARB},
	{"glUniform4fARB", (void **) &qglUniform4fARB},
	{"glUniform1iARB", (void **) &qglUniform1iARB},
	{"glUniform2iARB", (void **) &qglUniform2iARB},
	{"glUniform3iARB", (void **) &qglUniform3iARB},
	{"glUniform4iARB", (void **) &qglUniform4iARB},
	{"glUniform1fvARB", (void **) &qglUniform1fvARB},
	{"glUniform2fvARB", (void **) &qglUniform2fvARB},
	{"glUniform3fvARB", (void **) &qglUniform3fvARB},
	{"glUniform4fvARB", (void **) &qglUniform4fvARB},
	{"glUniform1ivARB", (void **) &qglUniform1ivARB},
	{"glUniform2ivARB", (void **) &qglUniform2ivARB},
	{"glUniform3ivARB", (void **) &qglUniform3ivARB},
	{"glUniform4ivARB", (void **) &qglUniform4ivARB},
	{"glUniformMatrix2fvARB", (void **) &qglUniformMatrix2fvARB},
	{"glUniformMatrix3fvARB", (void **) &qglUniformMatrix3fvARB},
	{"glUniformMatrix4fvARB", (void **) &qglUniformMatrix4fvARB},
	{"glGetObjectParameterfvARB", (void **) &qglGetObjectParameterfvARB},
	{"glGetObjectParameterivARB", (void **) &qglGetObjectParameterivARB},
	{"glGetInfoLogARB", (void **) &qglGetInfoLogARB},
	{"glGetAttachedObjectsARB", (void **) &qglGetAttachedObjectsARB},
	{"glGetUniformLocationARB", (void **) &qglGetUniformLocationARB},
	{"glGetActiveUniformARB", (void **) &qglGetActiveUniformARB},
	{"glGetUniformfvARB", (void **) &qglGetUniformfvARB},
	{"glGetUniformivARB", (void **) &qglGetUniformivARB},
	{"glGetShaderSourceARB", (void **) &qglGetShaderSourceARB},
	{NULL, NULL}
};

static dllfunction_t vertexshaderfuncs[] =
{
//	{"glVertexAttrib1fARB", (void **) &qglVertexAttrib1fARB},
//	{"glVertexAttrib1sARB", (void **) &qglVertexAttrib1sARB},
//	{"glVertexAttrib1dARB", (void **) &qglVertexAttrib1dARB},
//	{"glVertexAttrib2fARB", (void **) &qglVertexAttrib2fARB},
//	{"glVertexAttrib2sARB", (void **) &qglVertexAttrib2sARB},
//	{"glVertexAttrib2dARB", (void **) &qglVertexAttrib2dARB},
//	{"glVertexAttrib3fARB", (void **) &qglVertexAttrib3fARB},
//	{"glVertexAttrib3sARB", (void **) &qglVertexAttrib3sARB},
//	{"glVertexAttrib3dARB", (void **) &qglVertexAttrib3dARB},
//	{"glVertexAttrib4fARB", (void **) &qglVertexAttrib4fARB},
//	{"glVertexAttrib4sARB", (void **) &qglVertexAttrib4sARB},
//	{"glVertexAttrib4dARB", (void **) &qglVertexAttrib4dARB},
//	{"glVertexAttrib4NubARB", (void **) &qglVertexAttrib4NubARB},
//	{"glVertexAttrib1fvARB", (void **) &qglVertexAttrib1fvARB},
//	{"glVertexAttrib1svARB", (void **) &qglVertexAttrib1svARB},
//	{"glVertexAttrib1dvARB", (void **) &qglVertexAttrib1dvARB},
//	{"glVertexAttrib2fvARB", (void **) &qglVertexAttrib1fvARB},
//	{"glVertexAttrib2svARB", (void **) &qglVertexAttrib1svARB},
//	{"glVertexAttrib2dvARB", (void **) &qglVertexAttrib1dvARB},
//	{"glVertexAttrib3fvARB", (void **) &qglVertexAttrib1fvARB},
//	{"glVertexAttrib3svARB", (void **) &qglVertexAttrib1svARB},
//	{"glVertexAttrib3dvARB", (void **) &qglVertexAttrib1dvARB},
//	{"glVertexAttrib4fvARB", (void **) &qglVertexAttrib1fvARB},
//	{"glVertexAttrib4svARB", (void **) &qglVertexAttrib1svARB},
//	{"glVertexAttrib4dvARB", (void **) &qglVertexAttrib1dvARB},
//	{"glVertexAttrib4ivARB", (void **) &qglVertexAttrib1ivARB},
//	{"glVertexAttrib4bvARB", (void **) &qglVertexAttrib1bvARB},
//	{"glVertexAttrib4ubvARB", (void **) &qglVertexAttrib1ubvARB},
//	{"glVertexAttrib4usvARB", (void **) &qglVertexAttrib1usvARB},
//	{"glVertexAttrib4uivARB", (void **) &qglVertexAttrib1uivARB},
//	{"glVertexAttrib4NbvARB", (void **) &qglVertexAttrib1NbvARB},
//	{"glVertexAttrib4NsvARB", (void **) &qglVertexAttrib1NsvARB},
//	{"glVertexAttrib4NivARB", (void **) &qglVertexAttrib1NivARB},
//	{"glVertexAttrib4NubvARB", (void **) &qglVertexAttrib1NubvARB},
//	{"glVertexAttrib4NusvARB", (void **) &qglVertexAttrib1NusvARB},
//	{"glVertexAttrib4NuivARB", (void **) &qglVertexAttrib1NuivARB},
	{"glVertexAttribPointerARB", (void **) &qglVertexAttribPointerARB},
	{"glEnableVertexAttribArrayARB", (void **) &qglEnableVertexAttribArrayARB},
	{"glDisableVertexAttribArrayARB", (void **) &qglDisableVertexAttribArrayARB},
	{"glBindAttribLocationARB", (void **) &qglBindAttribLocationARB},
	{"glGetActiveAttribARB", (void **) &qglGetActiveAttribARB},
	{"glGetAttribLocationARB", (void **) &qglGetAttribLocationARB},
//	{"glGetVertexAttribdvARB", (void **) &qglGetVertexAttribdvARB},
//	{"glGetVertexAttribfvARB", (void **) &qglGetVertexAttribfvARB},
//	{"glGetVertexAttribivARB", (void **) &qglGetVertexAttribivARB},
//	{"glGetVertexAttribPointervARB", (void **) &qglGetVertexAttribPointervARB},
	{NULL, NULL}
};

static dllfunction_t vbofuncs[] =
{
	{"glBindBufferARB"    , (void **) &qglBindBufferARB},
	{"glDeleteBuffersARB" , (void **) &qglDeleteBuffersARB},
	{"glGenBuffersARB"    , (void **) &qglGenBuffersARB},
	{"glIsBufferARB"      , (void **) &qglIsBufferARB},
	{"glMapBufferARB"     , (void **) &qglMapBufferARB},
	{"glUnmapBufferARB"   , (void **) &qglUnmapBufferARB},
	{"glBufferDataARB"    , (void **) &qglBufferDataARB},
	{"glBufferSubDataARB" , (void **) &qglBufferSubDataARB},
	{NULL, NULL}
};

static dllfunction_t occlusionqueryfuncs[] =
{
	{"glGenQueriesARB",              (void **) &qglGenQueriesARB},
	{"glDeleteQueriesARB",           (void **) &qglDeleteQueriesARB},
	{"glIsQueryARB",                 (void **) &qglIsQueryARB},
	{"glBeginQueryARB",              (void **) &qglBeginQueryARB},
	{"glEndQueryARB",                (void **) &qglEndQueryARB},
	{"glGetQueryivARB",              (void **) &qglGetQueryivARB},
	{"glGetQueryObjectivARB",        (void **) &qglGetQueryObjectivARB},
	{"glGetQueryObjectuivARB",       (void **) &qglGetQueryObjectuivARB},
	{NULL, NULL}
};

static dllfunction_t opengl30funcs[] =
{
	{"glGenerateMipmap", (void **) &qglGenerateMipmap },
	{NULL, NULL}
};

void VID_CheckExtensions(void)
{
	vid.renderpath = RENDERPATH_GL11;

	// reset all the gl extension variables here
	// this should match the declarations
	gl_textureunits = 1;
	gl_maxtexturesize = 256;
	gl_supportslockarrays = false;
	gl_support_clamptoedge = false;
	gl_support_texture_combine = false;
	gl_support_texture_add = false;
	gl_support_texture_npot = false;
	gl_support_shader_objects = false;
	gl_support_shading_language_100 = false;
	gl_support_vertex_shader = false;
	gl_support_fragment_shader = false;
	gl_support_arb_vertex_buffer_object = false;
	gl_support_arb_occlusion_query = false;

	if (!GL_CheckExtension("glbase", opengl110funcs, NULL, false))
		Sys_Error("OpenGL 1.1 functions not found");

	Com_DPrintf ("GL_VENDOR: %s\n", gl_vendor);
	Com_DPrintf ("GL_RENDERER: %s\n", gl_renderer);
	Com_DPrintf ("GL_VERSION: %s\n", gl_version);
	Com_DPrintf ("GL_EXTENSIONS: %s\n", gl_extensions);
	Com_DPrintf ("%s_EXTENSIONS: %s\n", gl_platform, gl_platformextensions);

	if (GL_CheckExtension("GL_ARB_multitexture", multitexturefuncs, "-nomtex", false))
	{
		qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &gl_textureunits);
		qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl_maxtexturesize);
	}

	gl_supportslockarrays = GL_CheckExtension("GL_EXT_compiled_vertex_array", compiledvertexarrayfuncs, "-nocva", false);
	gl_support_clamptoedge = GL_CheckExtension("GL_EXT_texture_edge_clamp", NULL, "-noedgeclamp", false) || GL_CheckExtension("GL_SGIS_texture_edge_clamp", NULL, "-noedgeclamp", false);
	gl_support_texture_combine = GL_CheckExtension("GL_ARB_texture_env_combine", NULL, "-notexcombine", false) || GL_CheckExtension("GL_EXT_texture_env_combine", NULL, "-notexcombine", false);
	gl_support_texture_add = GL_CheckExtension("GL_ARB_texture_env_add", NULL, "-notexadd", false) || GL_CheckExtension("GL_EXT_texture_env_add", NULL, "-notexadd", false);
	gl_support_texture_npot = GL_CheckExtension("GL_ARB_texture_non_power_of_two", NULL, "-nonpottex", false) && (gl_maxtexturesize >= 8192);
//	gl_support_arb_vertex_buffer_object = GL_CheckExtension("gl_support_arb_vertex_buffer_object", vbofuncs, "-novbo", false);
//	if ((gl_support_shader_objects = GL_CheckExtension("GL_ARB_shader_objects", shaderobjectsfuncs, "-noshaderobjects", false)))
//		if ((gl_support_shading_language_100 = GL_CheckExtension("GL_ARB_shading_language_100", NULL, "-noshadinglanguage100", false)))
//			if ((gl_support_vertex_shader = GL_CheckExtension("GL_ARB_vertex_shader", vertexshaderfuncs, "-novertexshader", false)))
//				gl_support_fragment_shader = GL_CheckExtension("GL_ARB_fragment_shader", NULL, "-nofragmentshader", false);
	gl_support_arb_occlusion_query = GL_CheckExtension("GL_ARB_occlusion_query", occlusionqueryfuncs, "-noocclusionquery", false);

	if (GL_CheckExtension("gl3.0", opengl30funcs, NULL, false) && gl_support_fragment_shader && gl_support_arb_vertex_buffer_object && gl_support_arb_occlusion_query)
		vid.renderpath = RENDERPATH_GL30;
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

extern int VID_InitMode (int fullscreen, int width, int height, int refreshrate);
int VID_Mode(int fullscreen, int width, int height, int refreshrate)
{
	Com_Printf("Video: %s %dx%dx%dhz\n", fullscreen ? "fullscreen" : "window", width, height, refreshrate);
	if (VID_InitMode(fullscreen, width, height, vid_userefreshrate.value ? max(1, refreshrate) : 0))
	{
		vid.fullscreen = fullscreen;
		vid.width = width;
		vid.height = height;
		vid.refreshrate = refreshrate;
		vid.userefreshrate = vid_userefreshrate.value;
		Cvar_SetValue(&vid_fullscreen, fullscreen);
		Cvar_SetValue(&vid_width, width);
		Cvar_SetValue(&vid_height, height);
		if(vid_userefreshrate.value)
			Cvar_SetValue(&vid_refreshrate, refreshrate);
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
	Cvar_Register(&vid_conwidth);
	Cvar_Register(&vid_conheight);
	Cvar_Register(&vid_pixelheight);
	Cvar_Register(&vid_refreshrate);
	Cvar_Register(&vid_userefreshrate);
	Cvar_Register(&vid_vsync);
	Cvar_Register(&vid_samples);
	Cvar_Register(&vid_mouse);
	Cvar_Register(&vid_minwidth);
	Cvar_Register(&vid_minheight);

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
	S_Startup();
	CDAudio_Startup();
}

static void VID_CloseSystems(void)
{
	CDAudio_Shutdown();
	S_Shutdown();
	R_Modules_Shutdown();
}

void VID_Restart_f(void)
{
	Cmd_ExecuteString("disconnect");

	Com_Printf("VID_Restart: changing from %s %dx%d, to %s %dx%d.\n",
		vid.fullscreen ? "fullscreen" : "window", vid.width, vid.height, 
		vid_fullscreen.value ? "fullscreen" : "window", (int)vid_width.value, (int)vid_height.value);
	VID_CloseSystems();
	VID_Shutdown();
	if (!VID_Mode(vid_fullscreen.value, vid_width.value, vid_height.value, vid_refreshrate.value))
	{
		Com_Printf("Video mode change failed\n");
		if (!VID_Mode(vid.fullscreen, vid.width, vid.height, vid.refreshrate))
			Sys_Error("Unable to restore to last working video mode\n");
	}
	Cache_Flush ();
	VID_OpenSystems();
}

// this is only called once by Host_StartVideo
void VID_Start(void)
{
	Com_DPrintf("Starting video system\n");
	if (!VID_Mode(vid_fullscreen.value, vid_width.value, vid_height.value, vid_refreshrate.value))
	{
		Com_Printf("Desired video mode failed, trying fallbacks...\n");
		if (vid_fullscreen.value)
		{
			if (!VID_Mode(true, 640, 480, 60))
				if (!VID_Mode(false, 640, 480, 60))
					Sys_Error("Video modes failed\n");
		}
		else
		{
			Sys_Error("Windowed video failed\n");
		}
	}

	VID_OpenSystems();
}

int VID_SortModes_Compare(const void *a_, const void *b_)
{
	vid_mode_t *a = (vid_mode_t *) a_;
	vid_mode_t *b = (vid_mode_t *) b_;
	if(a->width > b->width)
		return +1;
	if(a->width < b->width)
		return -1;
	if(a->height > b->height)
		return +1;
	if(a->height < b->height)
		return -1;
	if(a->refreshrate > b->refreshrate)
		return +1;
	if(a->refreshrate < b->refreshrate)
		return -1;
	if(a->bpp > b->bpp)
		return +1;
	if(a->bpp < b->bpp)
		return -1;
	if(a->pixelheight_num * b->pixelheight_denom > a->pixelheight_denom * b->pixelheight_num)
		return +1;
	if(a->pixelheight_num * b->pixelheight_denom < a->pixelheight_denom * b->pixelheight_num)
		return -1;
	return 0;
}

size_t VID_SortModes(vid_mode_t *modes, size_t count, qbool usebpp, qbool userefreshrate, qbool useaspect)
{
	size_t i;
	if(count == 0)
		return 0;
	// 1. sort them
	qsort(modes, count, sizeof(*modes), VID_SortModes_Compare);
	// 2. remove duplicates
	for(i = 0; i < count; ++i)
	{
		if(modes[i].width && modes[i].height)
		{
			if(i == 0)
				continue;
			if(modes[i].width != modes[i-1].width)
				continue;
			if(modes[i].height != modes[i-1].height)
				continue;
			if(userefreshrate)
				if(modes[i].refreshrate != modes[i-1].refreshrate)
					continue;
			if(usebpp)
				if(modes[i].bpp != modes[i-1].bpp)
					continue;
			if(useaspect)
				if(modes[i].pixelheight_num * modes[i-1].pixelheight_denom != modes[i].pixelheight_denom * modes[i-1].pixelheight_num)
					continue;
		}
		// a dupe, or a bogus mode!
		if(i < count-1)
			memmove(&modes[i], &modes[i+1], sizeof(*modes) * (count-1 - i));
		--i; // check this index again, as mode i+1 is now here
		--count;
	}
	return count;
}
