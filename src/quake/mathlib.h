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
// mathlib.h
#ifndef _MATHLIB_H_
#define _MATHLIB_H_

#define	PITCH	0		// up / down
#define	YAW		1		// left / right
#define	ROLL	2		// fall over

typedef float vec_t;

typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];

typedef struct _glmatrix
{
    union
	{
        struct
		{
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
            float _41, _42, _43, _44;
        };

        float m4x4[4][4];
		float m16[16];
    };
} glmatrix;

typedef	int	fixed4_t;
typedef	int	fixed8_t;
typedef	int	fixed16_t;

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

#define M_PI_DIV_180 (M_PI / 180.0)

#define DEG2RAD( a ) ( a * M_PI ) / 180.0F

struct mplane_s;

extern vec3_t vec3_origin;

#define NANMASK (255<<23)
#define	IS_NAN(x) (((*(int *)&x)&NANMASK)==NANMASK)

#define Q_rint(x) ((x) > 0 ? (int)((x) + 0.5) : (int)((x) - 0.5))

#define Q_IsBitSet(data, bit)   (((data)[(bit) >> 3] & (1 << ((bit) & 7))) != 0)
#define Q_SetBit(data, bit)     ((data)[(bit) >> 3] |= (1 << ((bit) & 7)))
#define Q_ClearBit(data, bit)   ((data)[(bit) >> 3] &= ~(1 << ((bit) & 7)))

#define VectorNormalizeFast(_v)\
{\
	union { float f; int i; } _y, _number;\
	_number.f = DotProduct(_v, _v);\
	if (_number.f != 0.0)\
	{\
		_y.i = 0x5f3759df - (_number.i >> 1);\
		_y.f = _y.f * (1.5f - (_number.f * 0.5f * _y.f * _y.f));\
		VectorScale(_v, _y.f, _v);\
	}\
}

#define DotProduct(x,y)			(x[0]*y[0]+x[1]*y[1]+x[2]*y[2])

#define VectorSubtract(a,b,c)	((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define VectorAdd(a,b,c)		((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2])
#define VectorCopy(a,b)			((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define VectorClear(a)			((a)[0]=(a)[1]=(a)[2]=0)
#define VectorNegate(a,b)		((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2])
#define VectorSet(v, x, y, z)	((v)[0]=(x),(v)[1]=(y),(v)[2]=(z))
#define VectorAvg(a,b,c)		((c)[0]=((a)[0]+(b)[0])*0.5f,(c)[1]=((a)[1]+(b)[1])*0.5f, (c)[2]=((a)[2]+(b)[2])*0.5f)
#define Vector4Set(v, a, b, c, d)	((v)[0]=(a),(v)[1]=(b),(v)[2]=(c),(v)[3] = (d))
#define Vector4Copy(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])
#define Vector4Scale(in,scale,out)		((out)[0]=(in)[0]*scale,(out)[1]=(in)[1]*scale,(out)[2]=(in)[2]*scale,(out)[3]=(in)[3]*scale)
#define Vector4Add(a,b,c)		((c)[0]=(((a[0])+(b[0]))),(c)[1]=(((a[1])+(b[1]))),(c)[2]=(((a[2])+(b[2]))),(c)[3]=(((a[3])+(b[3])))) 
#define Vector4Avg(a,b,c)		((c)[0]=(((a[0])+(b[0]))*0.5f),(c)[1]=(((a[1])+(b[1]))*0.5f),(c)[2]=(((a[2])+(b[2]))*0.5f),(c)[3]=(((a[3])+(b[3]))*0.5f)) 

#define Vector2Copy(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1])
#define Vector2Avg(a,b,c)		((c)[0]=(((a[0])+(b[0]))*0.5f),(c)[1]=(((a[1])+(b[1]))*0.5f)) 

void VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc);

vec_t _DotProduct (vec3_t v1, vec3_t v2);
void _VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out);
void _VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out);
void _VectorCopy (vec3_t in, vec3_t out);

int VectorCompare (vec3_t v1, vec3_t v2);
vec_t VectorLength (vec3_t v);
void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross);
float VectorNormalize (vec3_t v);		// returns vector length
void VectorScale (vec3_t in, vec_t scale, vec3_t out);

#define LerpFloat(from, to, frac) ((from) + (frac)*((to) - (from)))
void LerpVector (const vec3_t from, const vec3_t to, float frac, vec3_t out);
void LerpAngles (const vec3_t from, const vec3_t to, float frac, vec3_t out);

int Q_log2(int val);

void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4]);

void FloorDivMod (double numer, double denom, int *quotient,
		int *rem);

void vectoangles(vec3_t vec, vec3_t ang);
void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
void MakeNormalVectors (vec3_t forward, vec3_t right, vec3_t up);
int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct mplane_s *plane);
float	anglemod(float a);


struct _glmatrix *GL_MultiplyMatrix (struct _glmatrix *out, struct _glmatrix *m1, struct _glmatrix *m2);
struct _glmatrix *GL_TranslateMatrix (struct _glmatrix *m, float x, float y, float z);
struct _glmatrix *GL_ScaleMatrix (struct _glmatrix *m, float x, float y, float z);
struct _glmatrix *GL_RotateMatrix (struct _glmatrix *m, float a, float x, float y, float z);
struct _glmatrix *GL_IdentityMatrix (struct _glmatrix *m);
struct _glmatrix *GL_LoadMatrix (struct _glmatrix *dst, struct _glmatrix *src);


#define BOX_ON_PLANE_SIDE(emins, emaxs, p)	\
	(((p)->type < 3)?						\
	(										\
		((p)->dist <= (emins)[(p)->type])?	\
			1								\
		:									\
		(									\
			((p)->dist >= (emaxs)[(p)->type])?\
				2							\
			:								\
				3							\
		)									\
	)										\
	:										\
		BoxOnPlaneSide( (emins), (emaxs), (p)))

#endif /* _MATHLIB_H_ */

