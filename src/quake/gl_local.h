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
// gl_local.h -- private refresh defs
#ifndef _GL_LOCAL_H_
#define _GL_LOCAL_H_

#include "quakedef.h"		// FIXME
#include "gl_model.h"

#ifdef _WIN32
#include <windows.h>
#endif

//====================================================

// wgl uses APIENTRY
#ifndef APIENTRY
#define APIENTRY
#endif

// for platforms (wgl) that do not use GLAPIENTRY
#ifndef GLAPIENTRY
#define GLAPIENTRY APIENTRY
#endif

#ifndef GL_PROJECTION
#include <stddef.h>

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef void GLvoid;
// 1-byte signed
typedef signed char GLbyte;
// 2-byte signed
typedef short GLshort;
// 4-byte signed
typedef int GLint;
// 1-byte unsigned
typedef unsigned char GLubyte;
// 2-byte unsigned
typedef unsigned short GLushort;
// 4-byte unsigned
typedef unsigned int GLuint;
// 4-byte signed
typedef int GLsizei;
// single precision float
typedef float GLfloat;
// single precision float in [0,1]
typedef float GLclampf;
// double precision float
typedef double GLdouble;
// double precision float in [0,1]
typedef double GLclampd;
// int whose size is the same as a pointer (?)
typedef ptrdiff_t GLintptrARB;
// int whose size is the same as a pointer (?)
typedef ptrdiff_t GLsizeiptrARB;

#define GL_MODELVIEW				0x1700
#define GL_PROJECTION				0x1701
#define GL_TEXTURE				0x1702
#define GL_MATRIX_MODE				0x0BA0
#define GL_MODELVIEW_MATRIX			0x0BA6
#define GL_PROJECTION_MATRIX			0x0BA7
#define GL_TEXTURE_MATRIX			0x0BA8

#define GL_DONT_CARE				0x1100
#define GL_FASTEST					0x1101
#define GL_NICEST					0x1102

#define GL_DEPTH_TEST				0x0B71

#define GL_CULL_FACE				0x0B44

#define GL_BLEND				0x0BE2
#define GL_ALPHA_TEST			0x0BC0

#define GL_ZERO					0x0
#define GL_ONE					0x1
#define GL_SRC_COLOR				0x0300
#define GL_ONE_MINUS_SRC_COLOR			0x0301
#define GL_DST_COLOR				0x0306
#define GL_ONE_MINUS_DST_COLOR			0x0307
#define GL_SRC_ALPHA				0x0302
#define GL_ONE_MINUS_SRC_ALPHA			0x0303
#define GL_DST_ALPHA				0x0304
#define GL_ONE_MINUS_DST_ALPHA			0x0305
#define GL_SRC_ALPHA_SATURATE			0x0308
#define GL_CONSTANT_COLOR			0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR		0x8002
#define GL_CONSTANT_ALPHA			0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA		0x8004

#define GL_TEXTURE_ENV				0x2300
#define GL_TEXTURE_ENV_MODE			0x2200
#define GL_TEXTURE_1D				0x0DE0
#define GL_TEXTURE_2D				0x0DE1
#define GL_TEXTURE_WRAP_S			0x2802
#define GL_TEXTURE_WRAP_T			0x2803
#define GL_TEXTURE_WRAP_R			0x8072
#define GL_TEXTURE_BORDER_COLOR			0x1004
#define GL_TEXTURE_MAG_FILTER			0x2800
#define GL_TEXTURE_MIN_FILTER			0x2801
#define GL_PACK_ALIGNMENT			0x0D05
#define GL_UNPACK_ALIGNMENT			0x0CF5
#define GL_UNPACK_ROW_LENGTH		0x0CF2
#define GL_TEXTURE_BINDING_1D                   0x8068
#define GL_TEXTURE_BINDING_2D                   0x8069

#define GL_NEAREST				0x2600
#define GL_LINEAR				0x2601
#define GL_NEAREST_MIPMAP_NEAREST		0x2700
#define GL_NEAREST_MIPMAP_LINEAR		0x2702
#define GL_LINEAR_MIPMAP_NEAREST		0x2701
#define GL_LINEAR_MIPMAP_LINEAR			0x2703

#define GL_LINE					0x1B01
#define GL_FILL					0x1B02

extern int gl_support_anisotropy;
extern int gl_max_anisotropy;
#define GL_TEXTURE_MAX_ANISOTROPY_EXT		0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT	0x84FF

#define GL_ADD					0x0104
#define GL_DECAL				0x2101
#define GL_MODULATE				0x2100

#define GL_REPEAT				0x2901
#define GL_CLAMP				0x2900

#define GL_POINTS				0x0000
#define GL_LINES				0x0001
#define GL_LINE_LOOP			0x0002
#define GL_LINE_STRIP			0x0003
#define GL_TRIANGLES			0x0004
#define GL_TRIANGLE_STRIP		0x0005
#define GL_TRIANGLE_FAN			0x0006
#define GL_QUADS				0x0007
#define GL_QUAD_STRIP			0x0008
#define GL_POLYGON				0x0009

#define GL_FALSE				0x0
#define GL_TRUE					0x1

#define GL_BYTE					0x1400
#define GL_UNSIGNED_BYTE			0x1401
#define GL_SHORT				0x1402
#define GL_UNSIGNED_SHORT			0x1403
#define GL_INT					0x1404
#define GL_UNSIGNED_INT				0x1405
#define GL_FLOAT				0x1406
#define GL_DOUBLE				0x140A
#define GL_2_BYTES				0x1407
#define GL_3_BYTES				0x1408
#define GL_4_BYTES				0x1409

#define GL_VERTEX_ARRAY				0x8074
#define GL_NORMAL_ARRAY				0x8075
#define GL_COLOR_ARRAY				0x8076
//#define GL_INDEX_ARRAY				0x8077
#define GL_TEXTURE_COORD_ARRAY			0x8078
//#define GL_EDGE_FLAG_ARRAY			0x8079

#define GL_NONE					0
#define GL_FRONT_LEFT			0x0400
#define GL_FRONT_RIGHT			0x0401
#define GL_BACK_LEFT			0x0402
#define GL_BACK_RIGHT			0x0403
#define GL_FRONT				0x0404
#define GL_BACK					0x0405
#define GL_LEFT					0x0406
#define GL_RIGHT				0x0407
#define GL_FRONT_AND_BACK		0x0408
#define GL_AUX0					0x0409
#define GL_AUX1					0x040A
#define GL_AUX2					0x040B
#define GL_AUX3					0x040C

#define GL_VENDOR				0x1F00
#define GL_RENDERER				0x1F01
#define GL_VERSION				0x1F02
#define GL_EXTENSIONS				0x1F03

#define GL_NO_ERROR 				0x0
#define GL_INVALID_VALUE			0x0501
#define GL_INVALID_ENUM				0x0500
#define GL_INVALID_OPERATION			0x0502
#define GL_STACK_OVERFLOW			0x0503
#define GL_STACK_UNDERFLOW			0x0504
#define GL_OUT_OF_MEMORY			0x0505

#define GL_DITHER				0x0BD0
#define GL_RGB					0x1907
#define GL_RGBA					0x1908

#define GL_MAX_TEXTURE_SIZE			0x0D33

#define GL_NEVER				0x0200
#define GL_LESS					0x0201
#define GL_EQUAL				0x0202
#define GL_LEQUAL				0x0203
#define GL_GREATER				0x0204
#define GL_NOTEQUAL				0x0205
#define GL_GEQUAL				0x0206
#define GL_ALWAYS				0x0207
#define GL_DEPTH_TEST				0x0B71

#define GL_RED_SCALE				0x0D14
#define GL_GREEN_SCALE				0x0D18
#define GL_BLUE_SCALE				0x0D1A
#define GL_ALPHA_SCALE				0x0D1C

#define GL_DEPTH_BUFFER_BIT			0x00000100
#define GL_ACCUM_BUFFER_BIT			0x00000200
#define GL_STENCIL_BUFFER_BIT			0x00000400
#define GL_COLOR_BUFFER_BIT			0x00004000

#define GL_STENCIL_TEST				0x0B90
#define GL_KEEP					0x1E00
#define GL_REPLACE				0x1E01
#define GL_INCR					0x1E02
#define GL_DECR					0x1E03

#define GL_POLYGON_OFFSET_FACTOR          0x8038
#define GL_POLYGON_OFFSET_UNITS           0x2A00
#define GL_POLYGON_OFFSET_POINT           0x2A01
#define GL_POLYGON_OFFSET_LINE            0x2A02
#define GL_POLYGON_OFFSET_FILL            0x8037

#define GL_POINT_SMOOTH                         0x0B10
#define GL_LINE_SMOOTH                          0x0B20
#define GL_POLYGON_SMOOTH                       0x0B41

#define GL_POLYGON_STIPPLE                0x0B42

#define GL_CLIP_PLANE0                    0x3000
#define GL_CLIP_PLANE1                    0x3001
#define GL_CLIP_PLANE2                    0x3002
#define GL_CLIP_PLANE3                    0x3003
#define GL_CLIP_PLANE4                    0x3004
#define GL_CLIP_PLANE5                    0x3005

#endif

// GL_ARB_multitexture
extern int gl_textureunits;
extern int gl_maxtexturesize;
extern void (GLAPIENTRY *qglMultiTexCoord2f) (GLenum, GLfloat, GLfloat);
extern void (GLAPIENTRY *qglActiveTexture) (GLenum);
extern void (GLAPIENTRY *qglClientActiveTexture) (GLenum);
#ifndef GL_ACTIVE_TEXTURE_ARB
#define GL_ACTIVE_TEXTURE_ARB			0x84E0
#define GL_CLIENT_ACTIVE_TEXTURE_ARB	0x84E1
#define GL_MAX_TEXTURE_UNITS_ARB		0x84E2
#define GL_TEXTURE0_ARB					0x84C0
#define GL_TEXTURE1_ARB					0x84C1
#define GL_TEXTURE2_ARB					0x84C2
#define GL_TEXTURE3_ARB					0x84C3
#define GL_TEXTURE4_ARB					0x84C4
#define GL_TEXTURE5_ARB					0x84C5
#define GL_TEXTURE6_ARB					0x84C6
#define GL_TEXTURE7_ARB					0x84C7
#define GL_TEXTURE8_ARB					0x84C8
#define GL_TEXTURE9_ARB					0x84C9
#define GL_TEXTURE10_ARB				0x84CA
#define GL_TEXTURE11_ARB				0x84CB
#define GL_TEXTURE12_ARB				0x84CC
#define GL_TEXTURE13_ARB				0x84CD
#define GL_TEXTURE14_ARB				0x84CE
#define GL_TEXTURE15_ARB				0x84CF
#define GL_TEXTURE16_ARB				0x84D0
#define GL_TEXTURE17_ARB				0x84D1
#define GL_TEXTURE18_ARB				0x84D2
#define GL_TEXTURE19_ARB				0x84D3
#define GL_TEXTURE20_ARB				0x84D4
#define GL_TEXTURE21_ARB				0x84D5
#define GL_TEXTURE22_ARB				0x84D6
#define GL_TEXTURE23_ARB				0x84D7
#define GL_TEXTURE24_ARB				0x84D8
#define GL_TEXTURE25_ARB				0x84D9
#define GL_TEXTURE26_ARB				0x84DA
#define GL_TEXTURE27_ARB				0x84DB
#define GL_TEXTURE28_ARB				0x84DC
#define GL_TEXTURE29_ARB				0x84DD
#define GL_TEXTURE30_ARB				0x84DE
#define GL_TEXTURE31_ARB				0x84DF
#endif

// GL_EXT_compiled_vertex_array
extern int gl_supportslockarrays;
extern void (GLAPIENTRY *qglLockArraysEXT) (GLint first, GLint count);
extern void (GLAPIENTRY *qglUnlockArraysEXT) (void);

#ifndef GL_MAX_ELEMENTS_VERTICES
#define GL_MAX_ELEMENTS_VERTICES		0x80E8
#endif
#ifndef GL_MAX_ELEMENTS_INDICES
#define GL_MAX_ELEMENTS_INDICES			0x80E9
#endif

// GL_SGIS_texture_edge_clamp or GL_EXT_texture_edge_clamp
extern int gl_support_clamptoedge;
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

// GL_ARB_texture_env_combine
extern int gl_support_texture_combine;
#ifndef GL_ARB_texture_env_combine
#define GL_COMBINE_ARB                    0x8570
#define GL_COMBINE_RGB_ARB                0x8571
#define GL_COMBINE_ALPHA_ARB              0x8572
#define GL_SOURCE0_RGB_ARB                0x8580
#define GL_SOURCE1_RGB_ARB                0x8581
#define GL_SOURCE2_RGB_ARB                0x8582
#define GL_SOURCE0_ALPHA_ARB              0x8588
#define GL_SOURCE1_ALPHA_ARB              0x8589
#define GL_SOURCE2_ALPHA_ARB              0x858A
#define GL_OPERAND0_RGB_ARB               0x8590
#define GL_OPERAND1_RGB_ARB               0x8591
#define GL_OPERAND2_RGB_ARB               0x8592
#define GL_OPERAND0_ALPHA_ARB             0x8598
#define GL_OPERAND1_ALPHA_ARB             0x8599
#define GL_OPERAND2_ALPHA_ARB             0x859A
#define GL_RGB_SCALE_ARB                  0x8573
#define GL_ADD_SIGNED_ARB                 0x8574
#define GL_INTERPOLATE_ARB                0x8575
#define GL_SUBTRACT_ARB                   0x84E7
#define GL_CONSTANT_ARB                   0x8576
#define GL_PRIMARY_COLOR_ARB              0x8577
#define GL_PREVIOUS_ARB                   0x8578
#endif

// GL_ARB_texture_env_add
extern int gl_support_texture_add;

// GL_ARB_texture_non_power_of_two
extern int gl_support_texture_npot;

#ifndef GL_VERSION_1_2
#define GL_UNSIGNED_BYTE_3_3_2            0x8032
#define GL_UNSIGNED_SHORT_4_4_4_4         0x8033
#define GL_UNSIGNED_SHORT_5_5_5_1         0x8034
#define GL_UNSIGNED_INT_8_8_8_8           0x8035
#define GL_UNSIGNED_INT_10_10_10_2        0x8036
#define GL_TEXTURE_BINDING_3D             0x806A
#define GL_PACK_SKIP_IMAGES               0x806B
#define GL_PACK_IMAGE_HEIGHT              0x806C
#define GL_UNPACK_SKIP_IMAGES             0x806D
#define GL_UNPACK_IMAGE_HEIGHT            0x806E
#define GL_TEXTURE_3D                     0x806F
#define GL_PROXY_TEXTURE_3D               0x8070
#define GL_TEXTURE_DEPTH                  0x8071
#define GL_TEXTURE_WRAP_R                 0x8072
#define GL_MAX_3D_TEXTURE_SIZE            0x8073
#define GL_UNSIGNED_BYTE_2_3_3_REV        0x8362
#define GL_UNSIGNED_SHORT_5_6_5           0x8363
#define GL_UNSIGNED_SHORT_5_6_5_REV       0x8364
#define GL_UNSIGNED_SHORT_4_4_4_4_REV     0x8365
#define GL_UNSIGNED_SHORT_1_5_5_5_REV     0x8366
#define GL_UNSIGNED_INT_8_8_8_8_REV       0x8367
#define GL_UNSIGNED_INT_2_10_10_10_REV    0x8368
#define GL_BGR                            0x80E0
#define GL_BGRA                           0x80E1
#define GL_RGB10_A2 					  0x8059
#define GL_MAX_ELEMENTS_VERTICES          0x80E8
#define GL_MAX_ELEMENTS_INDICES           0x80E9
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_TEXTURE_MIN_LOD                0x813A
#define GL_TEXTURE_MAX_LOD                0x813B
#define GL_TEXTURE_BASE_LEVEL             0x813C
#define GL_TEXTURE_MAX_LEVEL              0x813D
#define GL_SMOOTH_POINT_SIZE_RANGE        0x0B12
#define GL_SMOOTH_POINT_SIZE_GRANULARITY  0x0B13
#define GL_SMOOTH_LINE_WIDTH_RANGE        0x0B22
#define GL_SMOOTH_LINE_WIDTH_GRANULARITY  0x0B23
#define GL_ALIASED_LINE_WIDTH_RANGE       0x846E
#define GL_RESCALE_NORMAL                 0x803A
#define GL_LIGHT_MODEL_COLOR_CONTROL      0x81F8
#define GL_SINGLE_COLOR                   0x81F9
#define GL_SEPARATE_SPECULAR_COLOR        0x81FA
#define GL_ALIASED_POINT_SIZE_RANGE       0x846D
#define GL_FOG                            0x0B60
#define GL_FOG_DENSITY                    0x0B62
#define GL_FOG_START                      0x0B63
#define GL_FOG_END                        0x0B64
#define GL_FOG_MODE                       0x0B65
#define GL_FOG_COLOR                      0x0B66
#define GL_EXP                            0x0800
#define GL_EXP2                           0x0801
#endif

// gl_support_arb_vertex_buffer_object
extern int gl_support_arb_vertex_buffer_object;
#ifndef GL_ARRAY_BUFFER_ARB
#define GL_ARRAY_BUFFER_ARB               0x8892
#define GL_ELEMENT_ARRAY_BUFFER_ARB       0x8893
#define GL_ARRAY_BUFFER_BINDING_ARB       0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB 0x8895
#define GL_VERTEX_ARRAY_BUFFER_BINDING_ARB 0x8896
#define GL_NORMAL_ARRAY_BUFFER_BINDING_ARB 0x8897
#define GL_COLOR_ARRAY_BUFFER_BINDING_ARB 0x8898
#define GL_INDEX_ARRAY_BUFFER_BINDING_ARB 0x8899
#define GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB 0x889A
#define GL_EDGE_FLAG_ARRAY_BUFFER_BINDING_ARB 0x889B
#define GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING_ARB 0x889C
#define GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING_ARB 0x889D
#define GL_WEIGHT_ARRAY_BUFFER_BINDING_ARB 0x889E
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB 0x889F
#define GL_STREAM_DRAW_ARB                0x88E0
#define GL_STREAM_READ_ARB                0x88E1
#define GL_STREAM_COPY_ARB                0x88E2
#define GL_STATIC_DRAW_ARB                0x88E4
#define GL_STATIC_READ_ARB                0x88E5
#define GL_STATIC_COPY_ARB                0x88E6
#define GL_DYNAMIC_DRAW_ARB               0x88E8
#define GL_DYNAMIC_READ_ARB               0x88E9
#define GL_DYNAMIC_COPY_ARB               0x88EA
#define GL_READ_ONLY_ARB                  0x88B8
#define GL_WRITE_ONLY_ARB                 0x88B9
#define GL_READ_WRITE_ARB                 0x88BA
#define GL_BUFFER_SIZE_ARB                0x8764
#define GL_BUFFER_USAGE_ARB               0x8765
#define GL_BUFFER_ACCESS_ARB              0x88BB
#define GL_BUFFER_MAPPED_ARB              0x88BC
#define GL_BUFFER_MAP_POINTER_ARB         0x88BD
#endif
extern void (GLAPIENTRY *qglBindBufferARB) (GLenum target, GLuint buffer);
extern void (GLAPIENTRY *qglDeleteBuffersARB) (GLsizei n, const GLuint *buffers);
extern void (GLAPIENTRY *qglGenBuffersARB) (GLsizei n, GLuint *buffers);
extern GLboolean (GLAPIENTRY *qglIsBufferARB) (GLuint buffer);
extern GLvoid* (GLAPIENTRY *qglMapBufferARB) (GLenum target, GLenum access);
extern GLboolean (GLAPIENTRY *qglUnmapBufferARB) (GLenum target);
extern void (GLAPIENTRY *qglBufferDataARB) (GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage);
extern void (GLAPIENTRY *qglBufferSubDataARB) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data);

extern void (GLAPIENTRY *qglScissor)(GLint x, GLint y, GLsizei width, GLsizei height);

extern void (GLAPIENTRY *qglClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);

extern void (GLAPIENTRY *qglClear)(GLbitfield mask);

extern void (GLAPIENTRY *qglAlphaFunc)(GLenum func, GLclampf ref);
extern void (GLAPIENTRY *qglBlendFunc)(GLenum sfactor, GLenum dfactor);
extern void (GLAPIENTRY *qglCullFace)(GLenum mode);

extern void (GLAPIENTRY *qglEnable)(GLenum cap);
extern void (GLAPIENTRY *qglDisable)(GLenum cap);

extern void (GLAPIENTRY *qglEnableClientState)(GLenum cap);
extern void (GLAPIENTRY *qglDisableClientState)(GLenum cap);

//extern void (GLAPIENTRY *qglGetBooleanv)(GLenum pname, GLboolean *params);
//extern void (GLAPIENTRY *qglGetDoublev)(GLenum pname, GLdouble *params);
//extern void (GLAPIENTRY *qglGetFloatv)(GLenum pname, GLfloat *params);
extern void (GLAPIENTRY *qglGetIntegerv)(GLenum pname, GLint *params);

extern GLenum (GLAPIENTRY *qglGetError)(void);
extern const GLubyte* (GLAPIENTRY *qglGetString)(GLenum name);
extern void (GLAPIENTRY *qglFinish)(void);
extern void (GLAPIENTRY *qglFlush)(void);

extern void (GLAPIENTRY *qglClearDepth)(GLclampd depth);
extern void (GLAPIENTRY *qglDepthFunc)(GLenum func);
extern void (GLAPIENTRY *qglDepthMask)(GLboolean flag);
extern void (GLAPIENTRY *qglDepthRange)(GLclampd near_val, GLclampd far_val);
extern void (GLAPIENTRY *qglColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);

extern void (GLAPIENTRY *qglDrawArrays)(GLenum mode, GLint first, GLsizei count);
extern void (GLAPIENTRY *qglDrawElements)(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
extern void (GLAPIENTRY *qglVertexPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
extern void (GLAPIENTRY *qglNormalPointer)(GLenum type, GLsizei stride, const GLvoid *ptr);
extern void (GLAPIENTRY *qglColorPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
extern void (GLAPIENTRY *qglTexCoordPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
extern void (GLAPIENTRY *qglArrayElement)(GLint i);

extern void (GLAPIENTRY *qglColor3ubv)(const GLubyte* v);
extern void (GLAPIENTRY *qglColor3f)(GLfloat red, GLfloat green, GLfloat blue);
extern void (GLAPIENTRY *qglColor4ubv)(const GLubyte* v);
extern void (GLAPIENTRY *qglColor4f)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
extern void (GLAPIENTRY *qglColor4fv)(const GLfloat* v);
extern void (GLAPIENTRY *qglTexCoord2f)(GLfloat s, GLfloat t);
extern void (GLAPIENTRY *qglTexCoord2fv)(const GLfloat *v);
extern void (GLAPIENTRY *qglTexCoord3fv)(const GLfloat *v);
extern void (GLAPIENTRY *qglVertex2f)(GLfloat x, GLfloat y);
extern void (GLAPIENTRY *qglVertex3f)(GLfloat x, GLfloat y, GLfloat z);
extern void (GLAPIENTRY *qglVertex3fv)(const GLfloat* v);
extern void (GLAPIENTRY *qglBegin)(GLenum mode);
extern void (GLAPIENTRY *qglEnd)(void);

extern void (GLAPIENTRY *qglFogf)(GLenum pname, GLfloat param);
extern void (GLAPIENTRY *qglFogfv)(GLenum pname, const GLfloat *params);
extern void (GLAPIENTRY *qglFogi)(GLenum pname, GLint param);

extern void (GLAPIENTRY *qglMatrixMode)(GLenum mode);
extern void (GLAPIENTRY *qglOrtho)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
extern void (GLAPIENTRY *qglFrustum)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
extern void (GLAPIENTRY *qglViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
extern void (GLAPIENTRY *qglPushMatrix)(void);
extern void (GLAPIENTRY *qglPopMatrix)(void);
extern void (GLAPIENTRY *qglLoadIdentity)(void);
//extern void (GLAPIENTRY *qglLoadMatrixd)(const GLdouble *m);
extern void (GLAPIENTRY *qglLoadMatrixf)(const GLfloat *m);
//extern void (GLAPIENTRY *qglMultMatrixd)(const GLdouble *m);
extern void (GLAPIENTRY *qglMultMatrixf)(const GLfloat *m);
//extern void (GLAPIENTRY *qglRotated)(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
extern void (GLAPIENTRY *qglRotatef)(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
//extern void (GLAPIENTRY *qglScaled)(GLdouble x, GLdouble y, GLdouble z);
extern void (GLAPIENTRY *qglScalef)(GLfloat x, GLfloat y, GLfloat z);
//extern void (GLAPIENTRY *qglTranslated)(GLdouble x, GLdouble y, GLdouble z);
extern void (GLAPIENTRY *qglTranslatef)(GLfloat x, GLfloat y, GLfloat z);

extern void (GLAPIENTRY *qglReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);

extern void (GLAPIENTRY *qglStencilFunc)(GLenum func, GLint ref, GLuint mask);
extern void (GLAPIENTRY *qglStencilMask)(GLuint mask);
extern void (GLAPIENTRY *qglStencilOp)(GLenum fail, GLenum zfail, GLenum zpass);
extern void (GLAPIENTRY *qglClearStencil)(GLint s);

extern void (GLAPIENTRY *qglTexEnvf)(GLenum target, GLenum pname, GLfloat param);
extern void (GLAPIENTRY *qglTexEnvi)(GLenum target, GLenum pname, GLint param);
extern void (GLAPIENTRY *qglTexParameterf)(GLenum target, GLenum pname, GLfloat param);
//extern void (GLAPIENTRY *qglTexParameterfv)(GLenum target, GLenum pname, GLfloat *params);
extern void (GLAPIENTRY *qglTexParameteri)(GLenum target, GLenum pname, GLint param);

extern void (GLAPIENTRY *qglGenTextures)(GLsizei n, GLuint *textures);
extern void (GLAPIENTRY *qglDeleteTextures)(GLsizei n, const GLuint *textures);
extern void (GLAPIENTRY *qglBindTexture)(GLenum target, GLuint texture);
//extern void (GLAPIENTRY *qglPrioritizeTextures)(GLsizei n, const GLuint *textures, const GLclampf *priorities);
//extern GLboolean (GLAPIENTRY *qglAreTexturesResident)(GLsizei n, const GLuint *textures, GLboolean *residences);
extern GLboolean (GLAPIENTRY *qglIsTexture)(GLuint texture);
//extern void (GLAPIENTRY *qglPixelStoref)(GLenum pname, GLfloat param);
extern void (GLAPIENTRY *qglPixelStorei)(GLenum pname, GLint param);

extern void (GLAPIENTRY *qglTexImage1D)(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
extern void (GLAPIENTRY *qglTexImage2D)(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
extern void (GLAPIENTRY *qglTexSubImage1D)(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels);
extern void (GLAPIENTRY *qglTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
extern void (GLAPIENTRY *qglCopyTexImage1D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border);
extern void (GLAPIENTRY *qglCopyTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
extern void (GLAPIENTRY *qglCopyTexSubImage1D)(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
extern void (GLAPIENTRY *qglCopyTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);

extern void (GLAPIENTRY *qglPolygonOffset)(GLfloat factor, GLfloat units);
extern void (GLAPIENTRY *qglPolygonMode)(GLenum face , GLenum mode);

// GL_ARB_shader_objects
extern int gl_support_shader_objects;
#ifndef GL_PROGRAM_OBJECT_ARB
// 1-byte character string
typedef char GLcharARB;
// 4-byte integer handle to a shader object or program object
typedef unsigned int GLhandleARB;
#endif
extern void (GLAPIENTRY *qglDeleteObjectARB)(GLhandleARB obj);
extern GLhandleARB (GLAPIENTRY *qglGetHandleARB)(GLenum pname);
extern void (GLAPIENTRY *qglDetachObjectARB)(GLhandleARB containerObj, GLhandleARB attachedObj);
extern GLhandleARB (GLAPIENTRY *qglCreateShaderObjectARB)(GLenum shaderType);
extern void (GLAPIENTRY *qglShaderSourceARB)(GLhandleARB shaderObj, GLsizei count, const GLcharARB **string, const GLint *length);
extern void (GLAPIENTRY *qglCompileShaderARB)(GLhandleARB shaderObj);
extern GLhandleARB (GLAPIENTRY *qglCreateProgramObjectARB)(void);
extern void (GLAPIENTRY *qglAttachObjectARB)(GLhandleARB containerObj, GLhandleARB obj);
extern void (GLAPIENTRY *qglLinkProgramARB)(GLhandleARB programObj);
extern void (GLAPIENTRY *qglUseProgramObjectARB)(GLhandleARB programObj);
extern void (GLAPIENTRY *qglValidateProgramARB)(GLhandleARB programObj);
extern void (GLAPIENTRY *qglUniform1fARB)(GLint location, GLfloat v0);
extern void (GLAPIENTRY *qglUniform2fARB)(GLint location, GLfloat v0, GLfloat v1);
extern void (GLAPIENTRY *qglUniform3fARB)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
extern void (GLAPIENTRY *qglUniform4fARB)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
extern void (GLAPIENTRY *qglUniform1iARB)(GLint location, GLint v0);
extern void (GLAPIENTRY *qglUniform2iARB)(GLint location, GLint v0, GLint v1);
extern void (GLAPIENTRY *qglUniform3iARB)(GLint location, GLint v0, GLint v1, GLint v2);
extern void (GLAPIENTRY *qglUniform4iARB)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
extern void (GLAPIENTRY *qglUniform1fvARB)(GLint location, GLsizei count, const GLfloat *value);
extern void (GLAPIENTRY *qglUniform2fvARB)(GLint location, GLsizei count, const GLfloat *value);
extern void (GLAPIENTRY *qglUniform3fvARB)(GLint location, GLsizei count, const GLfloat *value);
extern void (GLAPIENTRY *qglUniform4fvARB)(GLint location, GLsizei count, const GLfloat *value);
extern void (GLAPIENTRY *qglUniform1ivARB)(GLint location, GLsizei count, const GLint *value);
extern void (GLAPIENTRY *qglUniform2ivARB)(GLint location, GLsizei count, const GLint *value);
extern void (GLAPIENTRY *qglUniform3ivARB)(GLint location, GLsizei count, const GLint *value);
extern void (GLAPIENTRY *qglUniform4ivARB)(GLint location, GLsizei count, const GLint *value);
extern void (GLAPIENTRY *qglUniformMatrix2fvARB)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern void (GLAPIENTRY *qglUniformMatrix3fvARB)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern void (GLAPIENTRY *qglUniformMatrix4fvARB)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern void (GLAPIENTRY *qglGetObjectParameterfvARB)(GLhandleARB obj, GLenum pname, GLfloat *params);
extern void (GLAPIENTRY *qglGetObjectParameterivARB)(GLhandleARB obj, GLenum pname, GLint *params);
extern void (GLAPIENTRY *qglGetInfoLogARB)(GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *infoLog);
extern void (GLAPIENTRY *qglGetAttachedObjectsARB)(GLhandleARB containerObj, GLsizei maxCount, GLsizei *count, GLhandleARB *obj);
extern GLint (GLAPIENTRY *qglGetUniformLocationARB)(GLhandleARB programObj, const GLcharARB *name);
extern void (GLAPIENTRY *qglGetActiveUniformARB)(GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
extern void (GLAPIENTRY *qglGetUniformfvARB)(GLhandleARB programObj, GLint location, GLfloat *params);
extern void (GLAPIENTRY *qglGetUniformivARB)(GLhandleARB programObj, GLint location, GLint *params);
extern void (GLAPIENTRY *qglGetShaderSourceARB)(GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *source);
extern void (GLAPIENTRY *qglPolygonStipple)(const GLubyte *mask);
#ifndef GL_PROGRAM_OBJECT_ARB
#define GL_PROGRAM_OBJECT_ARB					0x8B40
#define GL_OBJECT_TYPE_ARB						0x8B4E
#define GL_OBJECT_SUBTYPE_ARB					0x8B4F
#define GL_OBJECT_DELETE_STATUS_ARB				0x8B80
#define GL_OBJECT_COMPILE_STATUS_ARB			0x8B81
#define GL_OBJECT_LINK_STATUS_ARB				0x8B82
#define GL_OBJECT_VALIDATE_STATUS_ARB			0x8B83
#define GL_OBJECT_INFO_LOG_LENGTH_ARB			0x8B84
#define GL_OBJECT_ATTACHED_OBJECTS_ARB			0x8B85
#define GL_OBJECT_ACTIVE_UNIFORMS_ARB			0x8B86
#define GL_OBJECT_ACTIVE_UNIFORM_MAX_LENGTH_ARB	0x8B87
#define GL_OBJECT_SHADER_SOURCE_LENGTH_ARB		0x8B88
#define GL_SHADER_OBJECT_ARB					0x8B48
#define GL_FLOAT								0x1406
#define GL_FLOAT_VEC2_ARB						0x8B50
#define GL_FLOAT_VEC3_ARB						0x8B51
#define GL_FLOAT_VEC4_ARB						0x8B52
#define GL_INT									0x1404
#define GL_INT_VEC2_ARB							0x8B53
#define GL_INT_VEC3_ARB							0x8B54
#define GL_INT_VEC4_ARB							0x8B55
#define GL_BOOL_ARB								0x8B56
#define GL_BOOL_VEC2_ARB						0x8B57
#define GL_BOOL_VEC3_ARB						0x8B58
#define GL_BOOL_VEC4_ARB						0x8B59
#define GL_FLOAT_MAT2_ARB						0x8B5A
#define GL_FLOAT_MAT3_ARB						0x8B5B
#define GL_FLOAT_MAT4_ARB						0x8B5C
#define GL_SAMPLER_1D_ARB						0x8B5D
#define GL_SAMPLER_2D_ARB						0x8B5E
#define GL_SAMPLER_3D_ARB						0x8B5F
#define GL_SAMPLER_CUBE_ARB						0x8B60
#define GL_SAMPLER_1D_SHADOW_ARB				0x8B61
#define GL_SAMPLER_2D_SHADOW_ARB				0x8B62
#define GL_SAMPLER_2D_RECT_ARB					0x8B63
#define GL_SAMPLER_2D_RECT_SHADOW_ARB			0x8B64
#endif

// GL_ARB_vertex_shader
extern int gl_support_vertex_shader;
//extern void (GLAPIENTRY *qglVertexAttrib1fARB)(GLuint index, GLfloat v0);
//extern void (GLAPIENTRY *qglVertexAttrib1sARB)(GLuint index, GLshort v0);
//extern void (GLAPIENTRY *qglVertexAttrib1dARB)(GLuint index, GLdouble v0);
//extern void (GLAPIENTRY *qglVertexAttrib2fARB)(GLuint index, GLfloat v0, GLfloat v1);
//extern void (GLAPIENTRY *qglVertexAttrib2sARB)(GLuint index, GLshort v0, GLshort v1);
//extern void (GLAPIENTRY *qglVertexAttrib2dARB)(GLuint index, GLdouble v0, GLdouble v1);
//extern void (GLAPIENTRY *qglVertexAttrib3fARB)(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2);
//extern void (GLAPIENTRY *qglVertexAttrib3sARB)(GLuint index, GLshort v0, GLshort v1, GLshort v2);
//extern void (GLAPIENTRY *qglVertexAttrib3dARB)(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2);
//extern void (GLAPIENTRY *qglVertexAttrib4fARB)(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
//extern void (GLAPIENTRY *qglVertexAttrib4sARB)(GLuint index, GLshort v0, GLshort v1, GLshort v2, GLshort v3);
//extern void (GLAPIENTRY *qglVertexAttrib4dARB)(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3);
//extern void (GLAPIENTRY *qglVertexAttrib4NubARB)(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
//extern void (GLAPIENTRY *qglVertexAttrib1fvARB)(GLuint index, const GLfloat *v);
//extern void (GLAPIENTRY *qglVertexAttrib1svARB)(GLuint index, const GLshort *v);
//extern void (GLAPIENTRY *qglVertexAttrib1dvARB)(GLuint index, const GLdouble *v);
//extern void (GLAPIENTRY *qglVertexAttrib2fvARB)(GLuint index, const GLfloat *v);
//extern void (GLAPIENTRY *qglVertexAttrib2svARB)(GLuint index, const GLshort *v);
//extern void (GLAPIENTRY *qglVertexAttrib2dvARB)(GLuint index, const GLdouble *v);
//extern void (GLAPIENTRY *qglVertexAttrib3fvARB)(GLuint index, const GLfloat *v);
//extern void (GLAPIENTRY *qglVertexAttrib3svARB)(GLuint index, const GLshort *v);
//extern void (GLAPIENTRY *qglVertexAttrib3dvARB)(GLuint index, const GLdouble *v);
//extern void (GLAPIENTRY *qglVertexAttrib4fvARB)(GLuint index, const GLfloat *v);
//extern void (GLAPIENTRY *qglVertexAttrib4svARB)(GLuint index, const GLshort *v);
//extern void (GLAPIENTRY *qglVertexAttrib4dvARB)(GLuint index, const GLdouble *v);
//extern void (GLAPIENTRY *qglVertexAttrib4ivARB)(GLuint index, const GLint *v);
//extern void (GLAPIENTRY *qglVertexAttrib4bvARB)(GLuint index, const GLbyte *v);
//extern void (GLAPIENTRY *qglVertexAttrib4ubvARB)(GLuint index, const GLubyte *v);
//extern void (GLAPIENTRY *qglVertexAttrib4usvARB)(GLuint index, const GLushort *v);
//extern void (GLAPIENTRY *qglVertexAttrib4uivARB)(GLuint index, const GLuint *v);
//extern void (GLAPIENTRY *qglVertexAttrib4NbvARB)(GLuint index, const GLbyte *v);
//extern void (GLAPIENTRY *qglVertexAttrib4NsvARB)(GLuint index, const GLshort *v);
//extern void (GLAPIENTRY *qglVertexAttrib4NivARB)(GLuint index, const GLint *v);
//extern void (GLAPIENTRY *qglVertexAttrib4NubvARB)(GLuint index, const GLubyte *v);
//extern void (GLAPIENTRY *qglVertexAttrib4NusvARB)(GLuint index, const GLushort *v);
//extern void (GLAPIENTRY *qglVertexAttrib4NuivARB)(GLuint index, const GLuint *v);
extern void (GLAPIENTRY *qglVertexAttribPointerARB)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
extern void (GLAPIENTRY *qglEnableVertexAttribArrayARB)(GLuint index);
extern void (GLAPIENTRY *qglDisableVertexAttribArrayARB)(GLuint index);
extern void (GLAPIENTRY *qglBindAttribLocationARB)(GLhandleARB programObj, GLuint index, const GLcharARB *name);
extern void (GLAPIENTRY *qglGetActiveAttribARB)(GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
extern GLint (GLAPIENTRY *qglGetAttribLocationARB)(GLhandleARB programObj, const GLcharARB *name);
//extern void (GLAPIENTRY *qglGetVertexAttribdvARB)(GLuint index, GLenum pname, GLdouble *params);
//extern void (GLAPIENTRY *qglGetVertexAttribfvARB)(GLuint index, GLenum pname, GLfloat *params);
//extern void (GLAPIENTRY *qglGetVertexAttribivARB)(GLuint index, GLenum pname, GLint *params);
//extern void (GLAPIENTRY *qglGetVertexAttribPointervARB)(GLuint index, GLenum pname, GLvoid **pointer);
#ifndef GL_VERTEX_SHADER_ARB
#define GL_VERTEX_SHADER_ARB						0x8B31
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB		0x8B4A
#define GL_MAX_VARYING_FLOATS_ARB					0x8B4B
#define GL_MAX_VERTEX_ATTRIBS_ARB					0x8869
#define GL_MAX_TEXTURE_IMAGE_UNITS_ARB				0x8872
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB		0x8B4C
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB		0x8B4D
#define GL_MAX_TEXTURE_COORDS_ARB					0x8871
#define GL_VERTEX_PROGRAM_POINT_SIZE_ARB			0x8642
#define GL_VERTEX_PROGRAM_TWO_SIDE_ARB				0x8643
#define GL_OBJECT_ACTIVE_ATTRIBUTES_ARB				0x8B89
#define GL_OBJECT_ACTIVE_ATTRIBUTE_MAX_LENGTH_ARB	0x8B8A
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED_ARB			0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE_ARB				0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE_ARB			0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE_ARB				0x8625
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED_ARB		0x886A
#define GL_CURRENT_VERTEX_ATTRIB_ARB				0x8626
#define GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB			0x8645
#define GL_FLOAT									0x1406
#define GL_FLOAT_VEC2_ARB							0x8B50
#define GL_FLOAT_VEC3_ARB							0x8B51
#define GL_FLOAT_VEC4_ARB							0x8B52
#define GL_FLOAT_MAT2_ARB							0x8B5A
#define GL_FLOAT_MAT3_ARB							0x8B5B
#define GL_FLOAT_MAT4_ARB							0x8B5C
#endif

// GL_ARB_fragment_shader
extern int gl_support_fragment_shader;
#ifndef GL_FRAGMENT_SHADER_ARB
#define GL_FRAGMENT_SHADER_ARB						0x8B30
#define GL_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB		0x8B49
#define GL_MAX_TEXTURE_COORDS_ARB					0x8871
#define GL_MAX_TEXTURE_IMAGE_UNITS_ARB				0x8872
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT_ARB		0x8B8B
#endif

// GL_ARB_shading_language_100
extern int gl_support_shading_language_100;
#ifndef GL_SHADING_LANGUAGE_VERSION_ARB
#define GL_SHADING_LANGUAGE_VERSION_ARB				0x8B8C
#endif

// GL_ARB_occlusion_query
extern int gl_support_arb_occlusion_query;
extern void (GLAPIENTRY *qglGenQueriesARB)(GLsizei n, GLuint *ids);
extern void (GLAPIENTRY *qglDeleteQueriesARB)(GLsizei n, const GLuint *ids);
extern GLboolean (GLAPIENTRY *qglIsQueryARB)(GLuint qid);
extern void (GLAPIENTRY *qglBeginQueryARB)(GLenum target, GLuint qid);
extern void (GLAPIENTRY *qglEndQueryARB)(GLenum target);
extern void (GLAPIENTRY *qglGetQueryivARB)(GLenum target, GLenum pname, GLint *params);
extern void (GLAPIENTRY *qglGetQueryObjectivARB)(GLuint qid, GLenum pname, GLint *params);
extern void (GLAPIENTRY *qglGetQueryObjectuivARB)(GLuint qid, GLenum pname, GLuint *params);
#ifndef GL_SAMPLES_PASSED_ARB
#define GL_SAMPLES_PASSED_ARB                             0x8914
#define GL_QUERY_COUNTER_BITS_ARB                         0x8864
#define GL_CURRENT_QUERY_ARB                              0x8865
#define GL_QUERY_RESULT_ARB                               0x8866
#define GL_QUERY_RESULT_AVAILABLE_ARB                     0x8867
#endif

#ifdef _WIN32
extern int (WINAPI *qwglChoosePixelFormat)(HDC, CONST PIXELFORMATDESCRIPTOR *);
extern int (WINAPI *qwglDescribePixelFormat)(HDC, int, UINT, LPPIXELFORMATDESCRIPTOR);
//extern int (WINAPI *qwglGetPixelFormat)(HDC);
extern BOOL (WINAPI *qwglSetPixelFormat)(HDC, int, CONST PIXELFORMATDESCRIPTOR *);
extern BOOL (WINAPI *qwglSwapBuffers)(HDC);
extern HGLRC (WINAPI *qwglCreateContext)(HDC);
extern BOOL (WINAPI *qwglDeleteContext)(HGLRC);
extern PROC (WINAPI *qwglGetProcAddress)(LPCSTR);
extern BOOL (WINAPI *qwglMakeCurrent)(HDC, HGLRC);
extern BOOL (WINAPI *qwglSwapIntervalEXT)(int interval);
#endif

// OpenGL 3.0 core functions
extern void (GLAPIENTRY *qglGenerateMipmap)(GLenum target);

// partitioned depth range slices for 2d/3d stuff
#define QGL_DEPTH_2D_BEGIN		0.0f
#define QGL_DEPTH_2D_END		0.005f
#define QGL_DEPTH_3D_BEGIN		QGL_DEPTH_2D_END
#define QGL_DEPTH_3D_END		1.0f
#define QGL_DEPTH_VM_END		0.3f

//====================================================

// for restoring post-transform
extern glmatrix r_world_matrix;

#define ALIAS_BASE_SIZE_RATIO	(1.0 / 11.0) // normalizing factor so player model works out to about 1 pixel per triangle

extern model_t		*r_worldmodel;
extern entity_t		r_worldentity;
extern vec3_t		r_entorigin;
extern int			r_visframecount;
extern int			r_framecount;
extern mplane_t		frustum[4];

//
// view origin
//
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

//
// screen size info
//
extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern	texture_t	*r_notexture_mip;
extern	int		d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	cvar_t	r_norefresh;
extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawworld;
extern	cvar_t	r_speeds;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_lightmap;
extern	cvar_t	r_shadows;
extern	cvar_t	r_wateralpha;
extern	cvar_t	r_waterwarp;
extern	cvar_t	r_dynamic;
extern	cvar_t	r_novis;
extern	cvar_t	r_netgraph;
extern	cvar_t	r_fastsky;
extern	cvar_t	r_sky_quality;
extern	cvar_t	r_skyalpha;
extern	cvar_t	r_skyfog;
extern	cvar_t	r_lerpmodels;
extern	cvar_t	r_lerpmove;

extern	cvar_t	gl_nocolors;
extern	cvar_t	gl_finish;
extern	cvar_t	gl_fullbrights;
extern	cvar_t	gl_overbright;

#define TEXPREF_NONE			0x0000
#define TEXPREF_MIPMAP			0x0001	// generate mipmaps
#define TEXPREF_LINEAR			0x0002	// force linear
#define TEXPREF_NEAREST			0x0004	// force nearest
#define TEXPREF_REPEAT			0x0008	// force repeat instead of clamp
#define TEXPREF_OVERWRITE		0x0010	// overwrite existing same-name texture
#define TEXPREF_NOPICMIP		0x0020	// always load full-sized
#define TEXPREF_FULLBRIGHT		0x0040	// use fullbright palette
#define TEXPREF_NOBRIGHT		0x0080	// use nobright mask palette
#define TEXPREF_CUBEMAP			0x0100	// so that texmgr doesn't fuck with cubemap textures

enum srcformat {SRC_INDEXED, SRC_LIGHTMAP, SRC_RGBA};

#define MAX_TEXTURE_SIZE		2048

typedef struct gltexture_s
{
	// managed by texture manager
	unsigned int		texnum;
	struct gltexture_s	*next;
	model_t				*owner;

	// managed by image loading
	char				name[64];
	unsigned int		width; // size of image as it exists in opengl
	unsigned int		height; // size of image as it exists in opengl
	unsigned int		flags;
	char				source_file[MAX_OSPATH]; // filepath to data source, or "" if source is in memory
	unsigned int		source_offset; // byte offset into file, or memory address
	enum srcformat		source_format; // format of pixel data (indexed, lightmap, or rgba)
	unsigned int		source_width; // size of image in source data
	unsigned int		source_height; // size of image in source data
	unsigned short		source_crc; // generated by source data before modifications
	char				shirt; // 0-13 shirt color, or -1 if never colormapped
	char				pants; // 0-13 pants color, or -1 if never colormapped

	//used for rendering
	struct r_modelsurf_s *surfchain;
	int					visframe; // matches r_framecount if texture was bound this frame
} gltexture_t;

extern gltexture_t *notexture, *particletexture;

void TexMgr_Init (void);

gltexture_t *TexMgr_FindTexture (model_t *owner, char *name);
gltexture_t *TexMgr_NewTexture (void);
void TexMgr_FreeTexture (gltexture_t *kill);
void TexMgr_FreeTextures (int flags, int mask);
void TexMgr_FreeTexturesForOwner (model_t *owner);

gltexture_t *TexMgr_LoadImage (model_t *owner, char *name, int width, int height, enum srcformat format,
							   byte *data, char *source_file, unsigned source_offset, unsigned flags);

void TexMgr_ReloadImage (gltexture_t *glt, int shirt, int pants);

#define MAX_LIGHTMAPS 256

texture_t *R_TextureAnimation (texture_t *base, int frame);



void GL_BindTexture (GLenum tmu, gltexture_t *texture);

float *Fog_GetColor (void);
float Fog_GetDensity (void);
void Fog_SetupFrame (void);
void Fog_EnableGFog (void);
void Fog_DisableGFog (void);
void Fog_StartAdditive (void);
void Fog_StopAdditive (void);

void Fog_Init (void);
void Fog_NewMap (void);

void R_AddVisEdict (entity_t *e);

qbool R_CullBox (vec3_t mins, vec3_t maxs);

void R_ScreenShot_f (void);

void R_AnimateLight (void);
void R_MarkLeafs (void);
void R_StoreEfrags (efrag_t **ppefrag);
void R_MarkLights (dlight_t *light, int num, mnode_t *node);

void R_DrawParticles (void);
void R_DrawWorld (void);
void R_DrawBrushModel (entity_t *e);
void R_DrawSpriteModel (entity_t *e);
void R_DrawParticlesForType (particle_type_t *pt);

void GL_BuildLightmaps (void);

int R_LightPoint (entity_t *e, float *lightcolor);

void R_SubdivideSurface (model_t *mod, msurface_t *surf);
void R_BuildLightMap (msurface_t *surf, byte *dest, int stride);
void R_RenderDynamicLightmaps (msurface_t *surf);

void GL_DrawAliasShadow (entity_t *e);
void GL_MakeAliasModelDisplayLists (stvert_t *stverts, dtriangle_t *triangles);

void Sky_Init (void);
void Sky_NewMap (void);
void Sky_LoadTexture (texture_t *mt, byte *data);
void Sky_LoadCubeMap (char *name);
void Sky_ClearSkybox (void);

void R_GetTranslatedPlayerSkin (int colormap, gltexture_t **texture, gltexture_t **fb_texture);
void R_FlushTranslations (void);

typedef struct skin_s {
	char            name[16];
	qbool           failedload; // the name isn't a valid skin
	cache_user_t    cache;
} skin_t;

void Skin_Find (char *skinname, struct skin_s **sk);
byte *Skin_Cache (struct skin_s *skin);
void Skin_Flush (void);

void GL_PixelStore (int rowalign, int rowlen);

#define BYTE_CLAMP(a) ((a)>255?255:((a)<0?0:(a)))
#define BYTE_CLAMPF(a) ((a)>1?255:((a)<0?0:(a) * 255.0f))




void R_AddEntityToAlphaList (entity_t *ent);

unsigned short *R_TransferIndexes (unsigned short *src, unsigned short *dst, int numindexes, int offset);
void GL_DrawIndexedPrimitive (GLenum mode, int numIndexes, int numVertexes);
void GL_DrawPrimitive (GLenum mode, int firstvert, int numverts);
void GL_SetIndices (void *indexes);
void GL_SetStreamSource (int streamnum, int size, GLenum type, int stride, void *ptr);

void GL_UnbindBuffers (void);
void GL_BindBuffer (GLenum mode, GLuint buffer);

// set these up so that we can do bitwise on them some time
#define GLSTREAM_POSITION		1
#define GLSTREAM_COLOR			2
#define GLSTREAM_TEXCOORD0		4
#define GLSTREAM_TEXCOORD1		8
#define GLSTREAM_TEXCOORD2		16

void R_ShowTrisBegin (void);
void R_ShowTrisEnd (void);
extern cvar_t r_showtris;


void R_BeginSurfaces (void);
void R_BatchSurface (msurface_t *surf, glmatrix *matrix, int entnum);
void R_EndSurfaces (void);


// this order matches a d3d fvf order which may therefore be assumed to be more optimal for hardware
typedef struct r_defaultquad_s
{
	float xyz[3];

	union
	{
		byte color[4];
		unsigned int rgba;
	};

	float st[2];
} r_defaultquad_t;

// for vertex buffer quads
typedef struct r_defaultquad2_s
{
	float xyz[3];

	union
	{
		byte color[4];
		unsigned int rgba;
	};
} r_defaultquad2_t;

extern r_defaultquad_t *r_default_quads;
extern r_defaultquad2_t *r_default_quads2;

extern int r_max_quads;
extern int r_num_quads;

extern unsigned short *r_quad_indexes;

#endif /* _GL_LOCAL_H_ */

