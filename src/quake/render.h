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
// render.h -- public interface to rendering functions
#ifndef _RENDER_H_
#define _RENDER_H_

#define	TOP_RANGE				16		// player uniform colors
#define	BOTTOM_RANGE			96

#define	MAX_VISEDICTS			256
#define MAX_PARTICLES			4096	// default max # of particles at one time

// entity->renderfx
#define RF_WEAPONMODEL			1
#define RF_PLAYERMODEL			2
#define RF_TRANSLUCENT			4
#define RF_LIMITLERP			8

//=============================================================================

typedef struct
{
	int		length;
	char	map[64];
} lightstyle_t;

#define	MAX_DLIGHTS		128

typedef struct
{
	vec3_t			origin;
	float			radius;
	float			minlight;
	int				rgb[3];
} dlight_t;

typedef struct efrag_s
{
	struct mleaf_s		*leaf;
	struct efrag_s		*leafnext;
	struct entity_s		*entity;
	struct efrag_s		*entnext;
} efrag_t;

// for lerping
#define LERP_MOVESTEP	(1<<0) // this is a MOVETYPE_STEP entity, enable movement lerp
#define LERP_RESETANIM	(1<<1) // disable anim lerping until next anim frame
#define LERP_RESETANIM2	(1<<2) // set this and previous flag to disable anim lerping for two anim frames
#define LERP_RESETMOVE	(1<<3) // disable movement lerping until next origin/angles change
#define LERP_FINISH		(1<<4) // use lerpfinish time from server update instead of assuming interval of 0.1

// stores current info about an alias model entity
typedef struct aliasstate_s
{
	struct gltexture_s *tx;
	struct gltexture_s *fb;

	float shadelight[4];
	float ambientlight[4];
	vec3_t lightspot;

	short pose1;
	short pose2;
	float blend;

	vec3_t origin;
	vec3_t angles;
} aliasstate_t;


typedef struct entity_s
{
	// need to store the entity matrix for bmodel transforms
	glmatrix				matrix;

	// entity number allocated
	int						entnum;

	aliasstate_t			aliasstate;

	vec3_t					origin;
	vec3_t					angles;
	struct model_s			*model;			// NULL = no model
	struct efrag_s			*efrag;			// linked list of efrags
	int						frame;
	int						colormap;
	int						skinnum;		// for alias models
	int						renderfx;		// RF_WEAPONMODEL, etc
	float					alpha;

	int						visframe;		// last frame this entity was found in an active leaf (only used for static objects)

	struct mnode_s			*topnode;		// for bmodels, first world node that splits bmodel, or NULL if not split

	int						oldframe;		// frame to lerp from
	float					backlerp;

	// animation lerping
	byte					lerpflags;
	float					lerpstart;
	float					lerptime;
	float					lerpfinish;
	short					previouspose;
	short					currentpose;
	float					movelerpstart;
	vec3_t					previousorigin;
	vec3_t					currentorigin;
	vec3_t					previousangles;
	vec3_t					currentangles;
} entity_t;


typedef enum
{
	pt_static, pt_grav, pt_slowgrav, pt_fire, pt_explode, pt_explode2, pt_blob, pt_blob2
} ptype_t;

typedef struct particle_s
{
	vec3_t		org;
	float		color;

	struct particle_s	*next;
	vec3_t		vel;
	float		ramp;
	float		die;
	ptype_t		type;
} particle_t;

// this is needed outside of r_part now...
// these aren't really "types" but just relate to spawn positions and sorting
typedef struct particle_type_s
{
	particle_t *particles;
	int numparticles;
	vec3_t spawnorg;
	struct particle_type_s *next;
} particle_type_t;


typedef struct translation_info_s {
	int			topcolor;
	int			bottomcolor;
	char		skinname[32];
} translation_info_t;


// eye position, enitiy list, etc - filled in before calling R_RenderView
typedef struct {
	vrect_t			vrect;			// subwindow in video for refresh

	vec3_t			vieworg;
	vec3_t			viewangles;
	float			fov_x, fov_y;

	float			time;
	qbool			allow_cheats;
	int				viewplayernum;	// don't draw own glow
	qbool			watervis;

	lightstyle_t	*lightstyles;

	int				numDlights;
	dlight_t		*dlights;

	int				numParticles;
	particle_t		*particles;

	translation_info_t *translations;  // [MAX_CLIENTS]
	char			baseskin[32];
} refdef2_t;

//====================================================

#include "rb_backend.h"

#include "rc_wad.h"
#include "rc_modules.h"

//
// refresh
//
extern refdef2_t	r_refdef2;

void R_Init (void);
void R_InitTextures (void);
void R_InitEfrags (void);
void R_RenderView (void);		// must set r_refdef first

void R_AddEfrags (entity_t *ent);
void R_RemoveEfrags (entity_t *ent);

void R_NewMap (struct model_s *worldmodel);

// memory pointed to by pcxdata is allocated using Hunk_TempAlloc
// never store this pointer for later use!
void R_RSShot (byte **pcxdata, int *pcxsize);

void R_SetPalette (unsigned char *palette);

// 2D drawing functions
void R_DrawChar (int x, int y, int num);
void R_DrawColoredChar (int x, int y, int num, unsigned int color);
void R_DrawString (int x, int y, const char *str);
void R_DrawPic (int x, int y, qpic_t *pic);
void R_DrawTransPicTranslate (int x, int y, qpic_t *pic, int top, int bottom);
void R_DrawFilledRect (int x, int y, int w, int h, int c, float alpha);
void R_DrawStretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data);
void R_DrawCrosshair (int crossx, int crossy, int color);
void R_DrawNetGraph (void);
void R_DrawConsoleBackground (float alpha);

void R_LoadingScreen (void);
void R_FadeScreen (void);

qpic_t *R_CachePic (char *path);
qpic_t *R_CacheWadPic (char *name);

#define GetPicWidth(pic) (pic->width)
#define GetPicHeight(pic) (pic->height)

// model flags
#define	MF_ROCKET	1			// leave a trail
#define	MF_GRENADE	2			// leave a trail
#define	MF_GIB		4			// leave a trail
#define	MF_ROTATE	8			// rotate (bonus items)
#define	MF_TRACER	16			// green split trail
#define	MF_ZOMGIB	32			// small blood trail
#define	MF_TRACER2	64			// orange split trail + rotate
#define	MF_TRACER3	128			// purple trail

void Mod_ClearAll (void);
void Mod_TouchModel (char *name);
struct model_s *Mod_ForName (char *name, qbool crash);
int R_ModelFlags (const struct model_s *model);
unsigned short R_ModelChecksum (const struct model_s *model);

#endif /* _RENDER_H_ */

