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

#ifndef __MODEL__
#define __MODEL__

#include "modelgen.h"
#include "spritegn.h"
#include "bspfile.h"

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/

typedef struct r_modelsurf_s
{
	struct msurface_s *surface;	// the surface; the same msurface_t may be included more than once (e.g. for instanced bmodels)
	struct texture_s *texture;	// the texture used by this modelsurf (after a run through R_TextureAnimation
	glmatrix *matrix;			// the matrix used to transform this modelsurf; this can be NULL
	int entnum;					// the entity number used with this modelsurf

	struct r_modelsurf_s *surfchain;
	struct r_modelsurf_s *lightchain;
} r_modelsurf_t;


//
// in memory representation
//
typedef struct
{
	vec3_t		position;
} mvertex_t;

typedef struct texture_s
{
	char		name[16];
	unsigned	width, height;
	struct gltexture_s	*gltexture; 	// pointer to gltexture
	struct gltexture_s	*fullbright; 	// fullbright mask texture
	int			anim_total;				// total tenths in sequence ( 0 = no)
	int			anim_min, anim_max;		// time for this frame min <=time< max
	struct texture_s *anim_next;		// in the animation sequence
	struct texture_s *alternate_anims;	// bmodels in frmae 1 use these
	unsigned	offsets[MIPLEVELS];		// four mip maps stored
} texture_t;


#define	SURF_PLANEBACK		2
#define	SURF_DRAWSKY		4
#define SURF_DRAWSPRITE		8
#define SURF_DRAWTURB		0x10
#define SURF_DRAWTILED		0x20
#define SURF_DRAWBACKGROUND	0x40
#define SURF_UNDERWATER		0x80
#define SURF_NOTEXTURE		0x100
#define SURF_DRAWFENCE		0x200
#define SURF_DRAWLAVA		0x400
#define SURF_DRAWSLIME		0x800
#define SURF_DRAWTELE		0x1000
#define SURF_DRAWWATER		0x2000

typedef struct
{
	unsigned short	v[2];
	unsigned int	cachededgeoffset;
} medge_t;

typedef struct
{
	float		vecs[2][4];
	texture_t	*texture;
	int			flags;
} mtexinfo_t;

typedef struct glvertex_s
{
	float v[3];
	float st1[2];
	float st2[2];
} glvertex_t;


// because we're using larger lightmap sizes we need larger sized variables here
typedef struct glRect_s
{
	int left, top, right, bottom;
} glRect_t;


typedef struct msurface_s
{
	int			surfnum;
	int			visframe;		// should be drawn when node is crossed
	float		mins[3];		// for frustum culling
	float		maxs[3];		// for frustum culling

	float		*matrix;
	float		midpoint[3];
	float		dist;

	qbool		intersect;		// true if the surface intersects the frustum

	mplane_t	*plane;
	int			flags;

	int			firstedge;		// look up in model->surfedges[], negative numbers
	int			numedges;		// are backwards edges

	glvertex_t	*glvertexes;
	int			numglvertexes;

	unsigned short *glindexes;
	int			numglindexes;

	float		subdividesize;

	// the model that uses this surface (for vertex regeneration)
	struct model_s		*model;

	int			texturemins[2];
	int			extents[2];

	// lightmaps
	int			smax;
	int			tmax;
	byte		*lightbase;

	// mh - for tracking overbright setting; if this is != gl_overbright we will need to rebuild the lightmap
	int			overbright;

	// update rectangle for lightmaps on this surf
	glRect_t	lightrect;

	mtexinfo_t	*texinfo;

	// explicit water alpha used by this surf
	float		wateralpha;

// lighting info
	int			dlightframe;
	int			dlightbits[4];

	// this may be switched to 0 if we're using the entity lightmap
	int			lightmaptexturenum;

	// this is the true lightmap texnum
	int			truelightmaptexturenum;

	byte		styles[MAX_SURFACE_STYLES];
	int			cached_light[MAX_SURFACE_STYLES];	// values currently used in lightmap
	qbool		cached_dlight;			// true if dynamic light in cache
	byte		*samples;		// [numstyles*surfsize]
} msurface_t;


#define INSIDE_FRUSTUM		1
#define OUTSIDE_FRUSTUM		2
#define INTERSECT_FRUSTUM	3

#define FULLY_INSIDE_FRUSTUM		0x01010101
#define FULLY_OUTSIDE_FRUSTUM		0x10101010
#define FULLY_INTERSECT_FRUSTUM		0x11111111


typedef struct mnode_s
{
// common with leaf
	int			contents;		// 0, to differentiate from leafs
	int			visframe;		// node needs to be traversed if current

	float		minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

	union
	{
		int			bops;
		byte		sides[4];
	};

// node specific
	mplane_t	*plane;
	struct mnode_s	*children[2];

	unsigned short		firstsurface;
	unsigned short		numsurfaces;
} mnode_t;

typedef struct mleaf_s
{
// common with node
	int			contents;		// wil be a negative contents number
	int			visframe;		// node needs to be traversed if current

	float		minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

	union
	{
		int			bops;
		byte		sides[4];
	};

// leaf specific
	byte		*compressed_vis;
	struct efrag_s	*efrags;

	msurface_t	**firstmarksurface;
	int			nummarksurfaces;
} mleaf_t;

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/


// FIXME: shorten these?
typedef struct mspriteframe_s
{
	int		width, height;
	float	up, down, left, right;
	struct gltexture_s *gl_texture;
} mspriteframe_t;

typedef struct
{
	int				numframes;
	float			*intervals;
	mspriteframe_t	*frames[1];
} mspritegroup_t;

typedef struct
{
	spriteframetype_t	type;
	mspriteframe_t		*frameptr;
} mspriteframedesc_t;

typedef struct
{
	int					type;
	int					maxwidth;
	int					maxheight;
	int					numframes;
	float				beamlength;		// remove?
	void				*cachespot;		// remove?
	mspriteframedesc_t	frames[1];
} msprite_t;


/*
==============================================================================

ALIAS MODELS

Alias models are position independent, so the cache manager can move them.
==============================================================================
*/

typedef struct
{
	int					firstpose;
	int					numposes;
	float				interval;

	float				mins[3];
	float				maxs[3];

	int					frame;
	char				name[16];
} maliasframedesc_t;

typedef struct
{
	trivertx_t			bboxmin;
	trivertx_t			bboxmax;
	int					frame;
} maliasgroupframedesc_t;

typedef struct
{
	int						numframes;
	int						intervals;
	maliasgroupframedesc_t	frames[1];
} maliasgroup_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct mtriangle_s {
	int					facesfront;
	int					vertindex[3];
} mtriangle_t;


#define	MAX_SKINS	32

// split out to keep vertex sizes down
typedef struct aliasmesh_s
{
	float st[2];
	float light;
	unsigned short vertindex;
} aliasmesh_t;

typedef struct aliashdr_s
{
	vec3_t		scale;
	vec3_t		scale_origin;

	float		boundingradius;
	synctype_t	synctype;
	int			flags;
	float		size;

	int         nummeshframes;
	int         numtris;
	int         numverts;

	int         vertsperframe;
	int         numframes;

	int         meshverts;
	intptr_t	aliasmesh;
	intptr_t    vertexes;
	int         framevertexsize;

	int         skinwidth;
	int         skinheight;
	int         numskins;

	intptr_t	indexes;
	int			numindexes;

	intptr_t	firstindex;
	intptr_t	firstvertex;

	struct gltexture_s	*gltextures[MAX_SKINS][4];
	struct gltexture_s	*fbtextures[MAX_SKINS][4];
	int         texels[MAX_SKINS];   // only for player skins
	maliasframedesc_t frames[1];
} aliashdr_t;

#define	MAXALIASVERTS	2048
#define	MAXALIASFRAMES	256
#define	MAXALIASTRIS	2048

extern	aliashdr_t	*pheader;
extern	stvert_t	stverts[MAXALIASVERTS];
extern	mtriangle_t	triangles[MAXALIASTRIS];
extern	trivertx_t	*poseverts[MAXALIASFRAMES];

//===================================================================

//
// Whole model
//

typedef enum {mod_brush, mod_sprite, mod_alias} modtype_t;

#define	MOD_NOLERP		256		// don't lerp when animating
#define	MOD_NOSHADOW	512		// don't cast a shadow
#define	MOD_FBRIGHTHACK	1024	// when fullbrights are disabled, use a hack to render this model brighter

// some models are special
typedef enum {MOD_NORMAL, MOD_PLAYER, MOD_EYES} modhint_t;

typedef struct model_s
{
	char		name[MAX_QPATH];
	qbool		needload;		// bmodels and sprites don't cache normally

	unsigned short	crc;

	modhint_t	modhint;

	modtype_t	type;
	int			numframes;
	synctype_t	synctype;

	int			flags;

	// volume occupied by the model graphics
	vec3_t		mins, maxs;
	vec3_t		ymins, ymaxs; // bounds for entities with nonzero yaw
	vec3_t		rmins, rmaxs; // bounds for entities with nonzero pitch or roll

	// brush model
	int			firstmodelsurface, nummodelsurfaces;

	int			numsubmodels;
	dmodel_t	*submodels;

	int			numplanes;
	mplane_t	*planes;

	int			numleafs;		// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	int			numvertexes;
	mvertex_t	*vertexes;

	int			numedges;
	medge_t		*edges;

	int			numnodes;
	int			firstnode;
	mnode_t		*nodes;

	int			numtexinfo;
	mtexinfo_t	*texinfo;

	int			numsurfaces;
	msurface_t	*surfaces;

	int			numsurfedges;
	int			*surfedges;

	int			nummarksurfaces;
	msurface_t	**marksurfaces;

	int			numtextures;
	texture_t	**textures;

	byte		*visdata;
	byte		*lightdata;
	char		*entities;

	// these are never overwritten unless the vertex needs regenerating
	// (e.g. transforms for surfaces, etc)
	glvertex_t	*glvertexes;
	int			numglvertexes;

	unsigned short *glindexes;
	int			numglindexes;

	// additional model data
	cache_user_t	cache;		// only access through Mod_Extradata
} model_t;

//============================================================================

void	Mod_Init (void);
void	Mod_ClearAll (void);
model_t *Mod_ForName (char *name, qbool crash);
void	*Mod_Extradata (model_t *mod);	// handles caching
void	Mod_TouchModel (char *name);

mleaf_t *Mod_PointInLeaf (float *p, model_t *model);
byte	*Mod_LeafPVS (mleaf_t *leaf, model_t *model);

qbool	Mod_CheckFullbrights (byte *pixels, int count);
void GL_RegenerateVertexes (model_t *mod, msurface_t *surf, glmatrix *matrix);

#endif	// __MODEL__

