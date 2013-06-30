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
// gl_model.c -- model loading and caching

#include "gl_local.h"
#include "rc_image.h"
#include "rc_wad.h"
#include "crc.h"

fs_offset_t mod_filesize;
model_t	*loadmodel;
char	loadname[32];	// for hunk tags

void Mod_LoadSpriteModel (model_t *mod, void *buffer);
void Mod_LoadBrushModel (model_t *mod, void *buffer);
void Mod_LoadAliasModel (model_t *mod, void *buffer);
model_t *Mod_LoadModel (model_t *mod, qbool crash);

byte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN 2048
model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

texture_t	*r_notexture_mip; // moved here from r_main.c
texture_t	*r_notexture_mip2; // used for non-lightmapped surfs with a missing texture


void gl_models_start(void)
{
	memset (mod_novis, 0xff, sizeof(mod_novis));
}

void gl_models_shutdown(void)
{
}

void gl_models_newmap(void)
{
}

/*
===============
Mod_Init
===============
*/
void Mod_Init (void)
{
	// create notexture miptex
	r_notexture_mip = (texture_t *) Hunk_AllocName (sizeof(texture_t), "r_notexture_mip");
	strcpy (r_notexture_mip->name, "notexture");
	r_notexture_mip->height = r_notexture_mip->width = 32;

	r_notexture_mip2 = (texture_t *) Hunk_AllocName (sizeof(texture_t), "r_notexture_mip2");
	strcpy (r_notexture_mip2->name, "notexture2");
	r_notexture_mip2->height = r_notexture_mip2->width = 32;

	R_RegisterModule("GL_Models", gl_models_start, gl_models_shutdown, gl_models_newmap);
}

/*
===============
Mod_Extradata

Caches the data if needed
===============
*/
void *Mod_Extradata (model_t *mod)
{
	void	*r;

	r = Cache_Check (&mod->cache);
	if (r)
		return r;

	Mod_LoadModel (mod, true);

	if (!mod->cache.data)
		Sys_Error ("Mod_Extradata: caching failed");

	return mod->cache.data;
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t		*node;
	float		d;
	mplane_t	*plane;
	
	if (!model || !model->nodes)
		Sys_Error ("Mod_PointInLeaf: bad model");

	node = model->nodes;

	while (1)
	{
		if (node->contents < 0)
			return (mleaf_t *)node;

		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;

		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	return NULL;	// never reached
}


/*
===================
Mod_DecompressVis
===================
*/
byte *Mod_DecompressVis (byte *in, model_t *model)
{
	static byte	decompressed[MAX_MAP_LEAFS/8];
	int		c;
	byte	*out;
	int		row;

	row = (model->numleafs+7)>>3;	
	out = decompressed;

	if (!in)
	{
		// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);
	
	return decompressed;
}

byte *Mod_LeafPVS (mleaf_t *leaf, model_t *model)
{
	if (leaf == model->leafs)
		return mod_novis;

	return Mod_DecompressVis (leaf->compressed_vis, model);
}

/*
===================
Mod_ClearAll
===================
*/
void Mod_ClearAll (void)
{
	int		i;
	model_t	*mod;
	
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (mod->type != mod_alias)
		{
			mod->needload = true;
			TexMgr_FreeTexturesForOwner (mod);
		}
	}
}

/*
==================
Mod_FindName

==================
*/
model_t *Mod_FindName (char *name)
{
	int		i;
	model_t	*mod;
	
	if (!name[0])
		Sys_Error ("Mod_FindName: NULL name");
		
//
// search the currently loaded models
//
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
		if (!strcmp (mod->name, name) )
			break;
			
	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			Sys_Error ("mod_numknown == MAX_MOD_KNOWN");

		strcpy (mod->name, name);
		mod->needload = true;
		mod_numknown++;
	}

	return mod;
}

/*
==================
Mod_TouchModel

==================
*/
void Mod_TouchModel (char *name)
{
	model_t	*mod;
	
	mod = Mod_FindName (name);
	
	if (!mod->needload)
	{
		if (mod->type == mod_alias)
			Cache_Check (&mod->cache);
	}
}

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t *Mod_LoadModel (model_t *mod, qbool crash)
{
	unsigned *buf;

	if (!mod->needload)
	{
		if (mod->type == mod_alias)
		{
			if (Cache_Check (&mod->cache))
			{
				return mod;
			}
		}
		else
		{
			return mod; // not cached at all
		}
	}

// because the world is so huge, load it one piece at a time
// load the file
	buf = (unsigned *)FS_LoadFile (mod->name, false, &mod_filesize);
	if (!buf)
	{
		if (crash)
			Host_Error ("Mod_LoadModel: %s not found", mod->name);

		return NULL;
	}

// allocate a new model
	strcpy(loadname, mod->name);
//	COM_FileBase (mod->name, loadname);

	loadmodel = mod;

// fill it in
// call the apropriate loader
	mod->needload = false;
	
	switch (LittleLong(*(unsigned *)buf))
	{
	case IDPOLYHEADER:
		Mod_LoadAliasModel (mod, buf);
		break;
		
	case IDSPRITEHEADER:
		Mod_LoadSpriteModel (mod, buf);
		break;
	
	default:
		Mod_LoadBrushModel (mod, buf);
		break;
	}

	return mod;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName (char *name, qbool crash)
{
	model_t	*mod;
	
	mod = Mod_FindName (name);
	
	return Mod_LoadModel (mod, crash);
}


/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

byte	*mod_base;

/*
=================
Mod_CheckFullbrights
=================
*/
qbool Mod_CheckFullbrights (byte *pixels, int count)
{
	int i;

	for (i = 0; i < count; i++)
	{
		if (*pixels++ > 223)
			return true;
	}

	return false;
}


char bsptexturepaths[2][MAX_OSPATH];

void Mod_LoadBSPTexture (texture_t *tx, miptex_t *mt, byte *srcdata)
{
	// add an additional flag so that we can run the alpha edge fix on our fence textures
	int loadflag = TEXPREF_MIPMAP | TEXPREF_OVERWRITE;
	char		texturename[64];
	unsigned	offset;
	int			fwidth, fheight;
	char		filename[MAX_OSPATH], filename2[MAX_OSPATH], mapname[MAX_OSPATH];
	byte		*data;

	int hunkmark = Hunk_LowMark ();

	if (tx->name[0] == '{')
	{
#if 0
		// this is testing code so that i can verify fullbrights on fences work correctly without needing to modify a map
		int r1;
		byte *pix = srcdata;

		for (r1 = 0; r1 < 128; r1++)
		{
			int r2 = rand () % (tx->width * tx->height);
			pix[r2] = 251;
		}
#endif

//		loadflag |= TEXPREF_ALPHA;
	}

	if (!strncasecmp (tx->name, "sky", 3))
	{
		// sky texture //also note -- was Q_strncmp, changed to match qbsp
		Sky_LoadTexture (tx, srcdata);
	}
	else
	{
		// switch the name for liquids
		if (tx->name[0] == '*') tx->name[0] = '#';

		// external textures -- first look in "textures/mapname/" then look in "textures/"
		FS_StripExtension (loadmodel->name + 5, mapname, sizeof(mapname));
		sprintf (filename, "textures/%s/%s", mapname, tx->name);
		data = Image_LoadImage (filename, &fwidth, &fheight);

		if (!data)
		{
			sprintf (filename, "textures/%s", tx->name);
			data = Image_LoadImage (filename, &fwidth, &fheight);
		}

		// now load whatever we found
		if (data)
		{
			// load external image
			tx->gltexture = TexMgr_LoadImage (loadmodel, filename, fwidth, fheight, SRC_RGBA, data, filename, 0, loadflag);

			// now try to load glow/luma image from the same place
			Hunk_FreeToLowMark (hunkmark);

			// we're still in external texture land so we check #
			if (tx->name[0] != '#')
			{
				sprintf (filename2, "%s_glow", filename);
				data = Image_LoadImage (filename2, &fwidth, &fheight);

				if (!data)
				{
					sprintf (filename2, "%s_luma", filename);
					data = Image_LoadImage (filename2, &fwidth, &fheight);
				}

				if (data)
					tx->fullbright = TexMgr_LoadImage (loadmodel, filename2, fwidth, fheight, SRC_RGBA, data, filename, 0, loadflag);
			}
		}
		else
		{
			// use the texture from the bsp file
			// need to put the old name back here first so that vid_restart will reload it properly
			if (tx->name[0] == '#') tx->name[0] = '*';

			sprintf (texturename, "%s:%s", loadmodel->name, tx->name);
			offset = (unsigned) (mt + 1) - (unsigned) mod_base;

			// we're no longer in external texture land so we check *
			if (tx->name[0] != '*' && Mod_CheckFullbrights (srcdata, (tx->width * tx->height)))
			{
				tx->gltexture = TexMgr_LoadImage (loadmodel, texturename, tx->width, tx->height,
												  SRC_INDEXED, srcdata, loadmodel->name, offset, loadflag | TEXPREF_NOBRIGHT);
				sprintf (texturename, "%s:%s_glow", loadmodel->name, tx->name);
				tx->fullbright = TexMgr_LoadImage (loadmodel, texturename, tx->width, tx->height,
												   SRC_INDEXED, srcdata, loadmodel->name, offset, loadflag | TEXPREF_FULLBRIGHT);
			}
			else
			{
				tx->gltexture = TexMgr_LoadImage
				(
					loadmodel,
					texturename,
					tx->width,
					tx->height,
					SRC_INDEXED,
					srcdata,
					loadmodel->name,
					offset,
					loadflag
				);
			}
		}
	}

	// put the old name back on liquids (required for surface flags)
	if (tx->name[0] == '#') tx->name[0] = '*';

	Hunk_FreeToLowMark (hunkmark);
}


/*
=================
Mod_LoadTextures
=================
*/
void Mod_LoadTextures (lump_t *l)
{
	int		i, j, num, max, altmax;
	miptex_t	*mt;
	texture_t	*tx, *tx2;
	texture_t	*anims[10];
	texture_t	*altanims[10];
	dmiptexlump_t *m;
	int			nummiptex;

	if (!l->filelen)
	{
		Com_Printf ("Mod_LoadTextures: no textures in bsp file\n");
		nummiptex = 0;
		m = NULL;
	}
	else
	{
		m = (dmiptexlump_t *)(mod_base + l->fileofs);
		m->nummiptex = LittleLong (m->nummiptex);
		nummiptex = m->nummiptex;
	}
	
	loadmodel->numtextures = nummiptex + 2; // need 2 dummy texture chains for missing textures
	tx = Hunk_Alloc (loadmodel->numtextures * sizeof (texture_t));
	loadmodel->textures = (texture_t **) Hunk_Alloc (loadmodel->numtextures * sizeof (texture_t *));

	for (i = 0; i < nummiptex; i++, tx++)
	{
		m->dataofs[i] = LittleLong(m->dataofs[i]);
		if (m->dataofs[i] == -1)
			continue;

		mt = (miptex_t *)((byte *)m + m->dataofs[i]);
		mt->width = LittleLong (mt->width);
		mt->height = LittleLong (mt->height);

		for (j = 0; j < MIPLEVELS; j++)
			mt->offsets[j] = LittleLong (mt->offsets[j]);

		if ( (mt->width & 15) || (mt->height & 15) )
			Host_Error ("Texture %s is not 16 aligned", mt->name);

		loadmodel->textures[i] = tx;

		memcpy (tx->name, mt->name, sizeof(tx->name));
		tx->width = mt->width;
		tx->height = mt->height;

		for (j = 0; j < MIPLEVELS; j++)
			tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);

		tx->fullbright = NULL;

		Mod_LoadBSPTexture (tx, mt, (byte *) (mt + 1));
	}

	// last 2 slots in array should be filled with dummy textures
	loadmodel->textures[loadmodel->numtextures-2] = r_notexture_mip; // for lightmapped surfs
	loadmodel->textures[loadmodel->numtextures-1] = r_notexture_mip2; // for SURF_DRAWTILED surfs

	// sequence the animations
	for (i = 0; i < nummiptex; i++)
	{
		tx = loadmodel->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue;	// already sequenced

	// find the number of frames in the animation
		memset (anims, 0, sizeof(anims));
		memset (altanims, 0, sizeof(altanims));

		max = (int)(unsigned char)tx->name[1];
		altmax = 0;

		if ( islower(max) )
			max -= 'a' - 'A';

		if ( isdigit(max) )
		{
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		}
		else if (max >= 'A' && max <= 'J')
		{
			altmax = max - 'A';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		}
		else
			Host_Error ("Bad animating texture %s", tx->name);

		for (j = i+1; j < nummiptex; j++)
		{
			tx2 = loadmodel->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (strcmp (tx2->name+2, tx->name+2))
				continue;

			num = (int)(unsigned char)tx2->name[1];
			if ( islower(num) )
				num -= 'a' - 'A';

			if ( isdigit(num) )
			{
				num -= '0';
				anims[num] = tx2;
				if (num+1 > max)
					max = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num = num - 'A';
				altanims[num] = tx2;
				if (num+1 > altmax)
					altmax = num+1;
			}
			else
				Host_Error ("Bad animating texture %s", tx->name);
		}

#define	ANIM_CYCLE	2
	// link them all together
		for (j = 0; j < max; j++)
		{
			tx2 = anims[j];
			if (!tx2)
				Host_Error ("Missing frame %i of %s",j, tx->name);

			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = anims[ (j+1)%max ];

			if (altmax)
				tx2->alternate_anims = altanims[0];
		}

		for (j = 0; j < altmax; j++)
		{
			tx2 = altanims[j];
			if (!tx2)
				Host_Error ("Missing frame %i of %s",j, tx->name);

			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = altanims[ (j+1)%altmax ];

			if (max)
				tx2->alternate_anims = anims[0];
		}
	}
}

/*
=================
Mod_LoadLighting
=================
*/
void Mod_LoadLighting (lump_t *l)
{
	int i;
	byte *in, *out, *data;
	byte d;
	char litfilename[1024];
	fs_offset_t filesize;
	int hunkmark = Hunk_LowMark ();

	loadmodel->lightdata = NULL;
	
	// LordHavoc: check for a .lit file
	strcpy (litfilename, loadmodel->name);
	FS_StripExtension (litfilename, litfilename, sizeof(litfilename));
	strcat (litfilename, ".lit");
	data = (byte *) FS_LoadFile (litfilename, false, &filesize);

	// mh - lit file fix
	if (data && filesize == (l->filelen * 3) + 8)
	{
		if (data[0] == 'Q' && data[1] == 'L' && data[2] == 'I' && data[3] == 'T')
		{
			i = LittleLong (( (int *) data) [1]);

			if (i == 1)
			{
				Com_DPrintf ("%s loaded", litfilename);
				loadmodel->lightdata = data + 8;
				return;
			}
			else Com_Printf ("Unknown .lit file version (%d)\n", i);
		}
		else Com_Printf ("Corrupt .lit file (old version?), ignoring\n");
	}

	Hunk_FreeToLowMark (hunkmark);

	// LordHavoc: no .lit found, expand the white lighting data to color
	if (!l->filelen)
		return;

	loadmodel->lightdata = (byte *) Hunk_Alloc (l->filelen * 3);
	in = loadmodel->lightdata + l->filelen * 2; // place the file at the end, so it will not be overwritten until the very last write
	out = loadmodel->lightdata;
	memcpy (in, mod_base + l->fileofs, l->filelen);

	for (i = 0; i < l->filelen; i++)
	{
		d = *in++;
		*out++ = d;
		*out++ = d;
		*out++ = d;
	}
}


/*
=================
Mod_LoadVisibility
=================
*/
void Mod_LoadVisibility (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->visdata = NULL;
		return;
	}

	loadmodel->visdata = (byte *) Hunk_Alloc (l->filelen);
	memcpy (loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadEntities
=================
*/
void Mod_LoadEntities (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->entities = NULL;
		return;
	}

	loadmodel->entities = (char *) Hunk_Alloc (l->filelen);
	memcpy (loadmodel->entities, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes (lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	in = (dvertex_t *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mvertex_t *) Hunk_Alloc (count * sizeof (*out));

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);
	}
}

/*
=================
Mod_LoadEdges
=================
*/
void Mod_LoadEdges (lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	in = (dedge_t *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (medge_t *) Hunk_Alloc ((count + 1) * sizeof (*out));

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
	}
}

/*
=================
Mod_LoadTexinfo
=================
*/
void Mod_LoadTexinfo (lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int	i, j, count, miptex;
	int 	missing = 0;

	in = (texinfo_t *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mtexinfo_t *) Hunk_Alloc (count * sizeof (*out));

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j=0 ; j<4 ; j++)
		{
			out->vecs[0][j] = LittleFloat (in->vecs[0][j]);
			out->vecs[1][j] = LittleFloat (in->vecs[1][j]);
		}

		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);
	
		if (miptex >= loadmodel->numtextures-1 || !loadmodel->textures[miptex])
		{
			if (out->flags & TEX_SPECIAL)
				out->texture = loadmodel->textures[loadmodel->numtextures-1];
			else
				out->texture = loadmodel->textures[loadmodel->numtextures-2];

			out->flags |= TEX_MISSING;
			missing++;
		}
		else
		{
			out->texture = loadmodel->textures[miptex];
		}
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
void CalcSurfaceExtents (msurface_t *s)
{
	float	mins[2], maxs[2], val;
	int		i,j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;
	
	for (i = 0; i < s->numedges; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];

		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];
		
		for (j = 0; j < 2; j++)
		{
			val = v->position[0] * tex->vecs[j][0] + 
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];

			if (val < mins[j])
				mins[j] = val;

			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i = 0; i < 2; i++)
	{	
		bmins[i] = floor(mins[i]/16);
		bmaxs[i] = ceil(maxs[i]/16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;

		if (!(tex->flags & TEX_SPECIAL) && s->extents[i] > 8176)
			Host_Error ("Bad surface extents");
	}
}


/*
=================
Mod_CalcSurfaceBounds

calculate bounding box for per-surface frustum culling
=================
*/
void Mod_CalcSurfaceBounds (msurface_t *s)
{
	int			i, e;
	mvertex_t	*v;

	s->mins[0] = s->mins[1] = s->mins[2] = 9999999;
	s->maxs[0] = s->maxs[1] = s->maxs[2] = -9999999;

	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];

		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		if (s->mins[0] > v->position[0]) s->mins[0] = v->position[0];
		if (s->mins[1] > v->position[1]) s->mins[1] = v->position[1];
		if (s->mins[2] > v->position[2]) s->mins[2] = v->position[2];
		if (s->maxs[0] < v->position[0]) s->maxs[0] = v->position[0];
		if (s->maxs[1] < v->position[1]) s->maxs[1] = v->position[1];
		if (s->maxs[2] < v->position[2]) s->maxs[2] = v->position[2];
	}
}

/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces (lump_t *l)
{
	dface_t		*in;
	msurface_t 	*out;
	int			i, count, surfnum;
	int			planenum, side;
	extern		cvar_t gl_overbright;

	in = (dface_t *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (msurface_t *) Hunk_Alloc (count * sizeof (*out));

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	loadmodel->glvertexes = NULL;
	loadmodel->numglvertexes = 0;
	loadmodel->glindexes = NULL;
	loadmodel->numglindexes = 0;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++)
	{
		out->surfnum = surfnum;
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);		
		out->flags = 0;

		out->texinfo = loadmodel->texinfo + LittleShort (in->texinfo);

		// setup glvertexes and glindexes
		if (out->texinfo->texture->name[0] == '*' && !gl_support_shader_objects)
		{
			out->numglvertexes = 0;
			out->numglindexes = 0;
		}
		else
		{
			out->numglvertexes = out->numedges;
			out->numglindexes = (out->numglvertexes - 2) * 3;
		}

		// initially nothing here
		out->glvertexes = NULL;
		out->glindexes = NULL;

		// global counters so that we know how much we need to alloc
		loadmodel->numglvertexes += out->numglvertexes;
		loadmodel->numglindexes += out->numglindexes;

		// store out the model that uses this surf so that we can easily regenerate verts if required
		out->model = loadmodel;

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);

		if (side)
			out->flags |= SURF_PLANEBACK;			

		out->plane = loadmodel->planes + planenum;

		CalcSurfaceExtents (out);
		Mod_CalcSurfaceBounds (out); // for per-surface frustum culling

		// initial overbright setting
		out->overbright = (int) gl_overbright.value;
				
		// lighting info
		for (i = 0; i < MAX_SURFACE_STYLES; i++)
			out->styles[i] = in->styles[i];

		i = LittleLong(in->lightofs);

		if (i == -1)
			out->samples = NULL;
		else
			out->samples = loadmodel->lightdata + (i * 3);
		
		if (out->texinfo->texture->name[0] == '*')			// warp surface
		{
			out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);

			// detect special liquid types
			if (!strncmp (out->texinfo->texture->name, "*lava", 5))
				out->flags |= SURF_DRAWLAVA;
			else if (!strncmp (out->texinfo->texture->name, "*slime", 6))
				out->flags |= SURF_DRAWSLIME;
			else if (!strncmp (out->texinfo->texture->name, "*tele", 5))
				out->flags |= SURF_DRAWTELE;
			else out->flags |= SURF_DRAWWATER;

			if (!gl_support_shader_objects)
			{
				// original verts and indexes are nothing
				out->glindexes = NULL;
				out->glvertexes = NULL;
				R_SubdivideSurface (loadmodel, out);
			}
		}
		else if (out->texinfo->texture->name[0] == '{')
		{
			out->flags |= SURF_DRAWFENCE;
		}
		else if (!strncasecmp (out->texinfo->texture->name, "sky", 3))
		{
			// oops!!!
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
		}
	}

	// set up glvertexes and glindexes; these will be filled in when the polygon is generated
	loadmodel->glvertexes = (glvertex_t *) Hunk_Alloc (loadmodel->numglvertexes * sizeof (glvertex_t));
	loadmodel->glindexes = (unsigned short *) Hunk_Alloc (loadmodel->numglindexes * sizeof (unsigned short));
}


/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;

	if (node->contents < 0)
		return;

	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
void Mod_LoadNodes (lump_t *l)
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	in = (dnode_t *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mnode_t *) Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}
	
		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = (unsigned short)LittleShort (in->firstface);
		out->numsurfaces = (unsigned short)LittleShort (in->numfaces);
		
		for (j = 0; j < 2; j++)
		{
			p = (unsigned short)LittleShort (in->children[j]);

			if (p < count)
				out->children[j] = loadmodel->nodes + p;
			else
			{
				p = 65535 - p; // note this uses 65535 intentionally, -1 is leaf 0

				if (p < loadmodel->numleafs)
					out->children[j] = (mnode_t *)(loadmodel->leafs + p);
				else
				{
					Com_Printf("Mod_LoadNodes: invalid leaf index %i (file has only %i leafs)\n", p, loadmodel->numleafs);
					out->children[j] = (mnode_t *)(loadmodel->leafs); // map it to the solid leaf
				}
			}
		}
	}
	
	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

/*
=================
Mod_LoadLeafs
=================
*/
void Mod_LoadLeafs (lump_t *l)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count, p;

	in = (dleaf_t *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mleaf_t *) Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces + (unsigned short)LittleShort(in->firstmarksurface);
		out->nummarksurfaces = (unsigned short)LittleShort(in->nummarksurfaces);
		
		p = LittleLong(in->visofs);

		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->visdata + p;

		out->efrags = NULL;
	}	
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
void Mod_LoadMarksurfaces (lump_t *l)
{	
	int		i, j, count;
	short		*in;
	msurface_t **out;

	in = (short *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (msurface_t **) Hunk_Alloc (count * sizeof (*out));

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for (i = 0; i < count; i++)
	{
		j = (unsigned short)LittleShort(in[i]);

		if (j >= loadmodel->numsurfaces)
			Host_Error ("Mod_LoadMarksurfaces: bad surface number");

		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Mod_LoadSurfedges (lump_t *l)
{	
	int		i, count;
	int		*in, *out;
	
	in = (int *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (int *) Hunk_Alloc (count * sizeof (*out));

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for (i = 0; i < count; i++)
		out[i] = LittleLong (in[i]);
}


/*
=================
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes (lump_t *l)
{
	int			i, j;
	mplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;
	
	in = (dplane_t *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mplane_t *) Hunk_Alloc (count * 2 * sizeof (*out));
	
	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		bits = 0;

		for (j = 0; j < 3; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);

			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
	}
}

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int		i;
	vec3_t	corner;

	for (i = 0; i < 3; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return VectorLength (corner);
}

/*
=================
Mod_LoadSubmodels
=================
*/
void Mod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in;
	dmodel_t	*out;
	int			i, j, count;

	in = (dmodel_t *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (dmodel_t *) Hunk_Alloc (count * sizeof (*out));

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}

		for (j = 0; j < MAX_MAP_HULLS; j++)
			out->headnode[j] = LittleLong (in->headnode[j]);

		out->visleafs = LittleLong (in->visleafs);
		out->firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);
	}
}

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	int			i;
	dheader_t	*header;
	dmodel_t 	*bm;
	float		radius;
	
	loadmodel->type = mod_brush;
	
	header = (dheader_t *)buffer;

	i = LittleLong (header->version);

	if (i != BSPVERSION)
		Host_Error ("Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, i, BSPVERSION);

	// swap all the lumps
	mod_base = (byte *)header;

	for (i = 0; i < (int) sizeof(dheader_t) / 4; i++)
		((int *)header)[i] = LittleLong (((int *)header)[i]);

	// load into heap
	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges (&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadTextures (&header->lumps[LUMP_TEXTURES]);
	Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces (&header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces (&header->lumps[LUMP_MARKSURFACES]);
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes (&header->lumps[LUMP_NODES]);
	Mod_LoadEntities (&header->lumps[LUMP_ENTITIES]);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);

	mod->numframes = 2;		// regular and alternate animation

//
// set up the submodels (FIXME: this is confusing)
//
	for (i = 0; i < mod->numsubmodels; i++)
	{
		bm = &mod->submodels[i];

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;
		
		mod->firstnode = bm->headnode[0];
		if ((unsigned)mod->firstnode > loadmodel->numnodes)
			Host_Error ("Inline model %i has bad firstnode", i);

		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);

		radius = RadiusFromBounds (mod->mins, mod->maxs);
		mod->rmaxs[0] = mod->rmaxs[1] = mod->rmaxs[2] = mod->ymaxs[0] = mod->ymaxs[1] = mod->ymaxs[2] = radius;
		mod->rmins[0] = mod->rmins[1] = mod->rmins[2] = mod->ymins[0] = mod->ymins[1] = mod->ymins[2] = -radius;
		
		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels-1)
		{
			// duplicate the basic information
			char	name[10];

			sprintf (name, "*%i", i+1);
			loadmodel = Mod_FindName (name);
			*loadmodel = *mod;
			strcpy (loadmodel->name, name);
			mod = loadmodel;
		}
	}
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

byte		player_8bit_texels[320*200*2];

aliashdr_t *pheader;

// these need to be split out because they're slightly different for framegroups
// to do - add the yaw/pitch/roll stuff here
void Mod_InitAliasFrameBBox (maliasframedesc_t *frame)
{
	frame->mins[0] = frame->mins[1] = frame->mins[2] = 9999999;
	frame->maxs[0] = frame->maxs[1] = frame->maxs[2] = -9999999;
}


void Mod_FinalizeAliasFrameBBox (maliasframedesc_t *frame)
{
	int				i;
	
	for (i = 0; i < 3; i++)
	{
		frame->mins[i] = (frame->mins[i] * pheader->scale[i]) + pheader->scale_origin[i];
		frame->maxs[i] = (frame->maxs[i] * pheader->scale[i]) + pheader->scale_origin[i];

		if (frame->mins[i] > -16) frame->mins[i] = -16;
		if (frame->maxs[i] < 16) frame->maxs[i] = 16;
	}
}


void Mod_LoadFrameVerts (maliasframedesc_t *frame, trivertx_t *verts)
{
	int i, v;
	int mark = Hunk_LowMark ();
	trivertx_t *vertexes = (trivertx_t *) Hunk_Alloc (pheader->vertsperframe * sizeof (trivertx_t));

	// this should only be done once and stores an offset to the first set of verts for the frame
	if (!pheader->vertexes) pheader->vertexes = (intptr_t) vertexes - (intptr_t) pheader;

	for (i = 0; i < pheader->vertsperframe; i++, vertexes++, verts++)
	{
		vertexes->lightnormalindex = verts->lightnormalindex;

		for (v = 0; v < 3; v++)
		{
			if (verts->v[v] < frame->mins[v]) frame->mins[v] = verts->v[v];
			if (verts->v[v] > frame->maxs[v]) frame->maxs[v] = verts->v[v];

			if (verts->v[v] < loadmodel->mins[v]) loadmodel->mins[v] = verts->v[v];
			if (verts->v[v] > loadmodel->maxs[v]) loadmodel->maxs[v] = verts->v[v];

			vertexes->v[v] = verts->v[v];
		}
	}

	// hunk tags will mean that pheader->vertsperframe * sizeof (trivertx_t) is invalid for this
	// we only need to do this once but no harm in doing it every frame
	pheader->framevertexsize = Hunk_LowMark () - mark;

	pheader->nummeshframes++;
}


void *Mod_LoadAliasFrame (void *pin, maliasframedesc_t *frame)
{
	trivertx_t		*verts;
	daliasframe_t	*pdaliasframe;

	pdaliasframe = (daliasframe_t *) pin;

	strncpy (frame->name, pdaliasframe->name, 16);
	frame->firstpose = pheader->nummeshframes;
	frame->numposes = 1;

	verts = (trivertx_t *) (pdaliasframe + 1);

	// load the frame vertexes
	Mod_InitAliasFrameBBox (frame);
	Mod_LoadFrameVerts (frame, verts);
	Mod_FinalizeAliasFrameBBox (frame);
	verts += pheader->vertsperframe;

	return (void *) verts;
}


void *Mod_LoadAliasGroup (void * pin,  maliasframedesc_t *frame)
{
	daliasgroup_t		*pingroup;
	int					i, numframes;
	daliasinterval_t	*pin_intervals;
	void				*ptemp;
	
	pingroup = (daliasgroup_t *)pin;
	numframes = LittleLong (pingroup->numframes);
	frame->firstpose = pheader->nummeshframes;
	frame->numposes = numframes;

	pin_intervals = (daliasinterval_t *)(pingroup + 1);
	frame->interval = LittleFloat (pin_intervals->interval);
	pin_intervals += numframes;
	ptemp = (void *) pin_intervals;

	Mod_InitAliasFrameBBox (frame);

	for (i = 0; i < numframes; i++)
	{
		Mod_LoadFrameVerts (frame, (trivertx_t *) ((daliasframe_t *) ptemp + 1));
		ptemp = (trivertx_t *) ((daliasframe_t *) ptemp + 1) + pheader->vertsperframe;
	}

	Mod_FinalizeAliasFrameBBox (frame);

	return ptemp;
}



/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes - Ed
=================
*/

typedef struct
{
	short		x, y;
} floodfill_t;

extern unsigned d_8to24table[];

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

void Mod_FloodFillSkin( byte *skin, int skinwidth, int skinheight )
{
	byte				fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t			fifo[FLOODFILL_FIFO_SIZE];
	int					inpt = 0, outpt = 0;
	int					filledcolor = -1;
	int					i;

	if (filledcolor == -1)
	{
		filledcolor = 0;

		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
		{
			if (d_8to24table[i] == (255 << 0)) // alpha 1.0
			{
				filledcolor = i;
				break;
			}
		}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int			x = fifo[outpt].x, y = fifo[outpt].y;
		int			fdc = filledcolor;
		byte		*pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP( -1, -1, 0 );
		if (x < skinwidth - 1)	FLOODFILL_STEP( 1, 1, 0 );
		if (y > 0)				FLOODFILL_STEP( -skinwidth, 0, -1 );
		if (y < skinheight - 1)	FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
}

#define MDL_STANDARD	(TEXPREF_MIPMAP)
#define MDL_NOBRIGHT	(TEXPREF_MIPMAP | TEXPREF_NOBRIGHT)
#define MDL_FULLBRIGHT	(TEXPREF_MIPMAP | TEXPREF_FULLBRIGHT)

char *texpaths[] = {"textures/progs/", "textures/models/", "progs/", "models/", NULL};

void Mod_LoadSkinTexture (gltexture_t **tex, gltexture_t **fb, char *name, char *fbr_mask_name, char *extname, byte *data, int width, int height, unsigned int offset)
{
	int i;
	char extload[256] = {0};
	int extwidth = 0;
	int extheight = 0;
	byte *extdata;
	int hunkmark = Hunk_LowMark ();

	for (i = 0; ; i++)
	{
		// no more paths
		if (!texpaths[i]) break;

		sprintf (extload, "%s%s", texpaths[i], extname);
		extdata = Image_LoadImage (extload, &extwidth, &extheight);

		if (extdata)
		{
			tex[0] = TexMgr_LoadImage (loadmodel, extload, extwidth, extheight, SRC_RGBA, extdata, extload, 0, MDL_STANDARD);
			fb[0] = NULL;

			// now try to load glow/luma image from the same place
			Hunk_FreeToLowMark (hunkmark);

			sprintf (extload, "%s%s_glow", texpaths[i], extname);
			extdata = Image_LoadImage (extload, &extwidth, &extheight);

			if (!extdata)
			{
				sprintf (extload, "%s%s_luma", texpaths[i], extname);
				extdata = Image_LoadImage (extload, &extwidth, &extheight);
			}

			if (extdata)
			{
				fb[0] = TexMgr_LoadImage (loadmodel, extload, extwidth, extheight, SRC_RGBA, extdata, extload, 0, MDL_STANDARD);
				Hunk_FreeToLowMark (hunkmark);
			}

			return;
		}
	}

	// hurrah! for readable code
	if (Mod_CheckFullbrights (data, width * height))
	{
		tex[0] = TexMgr_LoadImage (loadmodel, name, width, height, SRC_INDEXED, data, loadmodel->name, offset, MDL_NOBRIGHT);
		fb[0] = TexMgr_LoadImage (loadmodel, fbr_mask_name, width, height, SRC_INDEXED, data, loadmodel->name, offset, MDL_FULLBRIGHT);
	}
	else
	{
		tex[0] = TexMgr_LoadImage (loadmodel, name, width, height, SRC_INDEXED, data, loadmodel->name, offset, MDL_STANDARD);
		fb[0] = NULL;
	}

	// just making sure...
	Hunk_FreeToLowMark (hunkmark);
}

/*
===============
Mod_LoadAllSkins
===============
*/
void *Mod_LoadAllSkins (int numskins, daliasskintype_t *pskintype)
{
	int						i, j, k, size, groupskins;
	char					name[256];
	byte					*skin, *texels;
	daliasskingroup_t		*pinskingroup;
	daliasskininterval_t	*pinskinintervals;
	char					fbr_mask_name[256];
	char					extname[256];
	unsigned				offset;
	
	skin = (byte *)(pskintype + 1);

	if (numskins < 1 || numskins > MAX_SKINS)
		Host_Error ("Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins);

	size = pheader->skinwidth * pheader->skinheight;

	for (i = 0; i < numskins; i++)
	{
		if (pskintype->type == ALIAS_SKIN_SINGLE)
		{
			Mod_FloodFillSkin (skin, pheader->skinwidth, pheader->skinheight);

			// save 8 bit texels for the player model to remap
			if (loadmodel->modhint == MOD_PLAYER)
			{
				if (size > sizeof(player_8bit_texels))
					Host_Error ("Player skin too large");
				memcpy (player_8bit_texels, (byte *)(pskintype + 1), size);
			}
			texels = (byte *) Hunk_AllocName (size, loadname);
			pheader->texels[i] = texels - (byte *) pheader;
			memcpy (texels, (byte *) (pskintype + 1), size);

			sprintf (name, "%s:frame%i", loadmodel->name, i);
			sprintf (extname, "%s_%i", loadmodel->name, i);
			sprintf (fbr_mask_name, "%s:frame%i_glow", loadmodel->name, i);
			offset = (unsigned)(pskintype + 1) - (unsigned)mod_base;

			Mod_LoadSkinTexture
			(
				&pheader->gltextures[i][0],
				&pheader->fbtextures[i][0],
				name,
				fbr_mask_name,
				&extname[6],
				(byte *) (pskintype + 1),
				pheader->skinwidth,
				pheader->skinheight,
				offset
			);
			
			pheader->gltextures[i][3] = pheader->gltextures[i][2] = pheader->gltextures[i][1] = pheader->gltextures[i][0];
			pheader->fbtextures[i][3] = pheader->fbtextures[i][2] = pheader->fbtextures[i][1] = pheader->fbtextures[i][0];

			pskintype = (daliasskintype_t *) ((byte *) (pskintype + 1) + size);
		}
		else
		{
			// animating skin group.  yuck.
			pskintype++;
			pinskingroup = (daliasskingroup_t *)pskintype;
			groupskins = LittleLong (pinskingroup->numskins);
			pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

			pskintype = (daliasskintype_t *)(pinskinintervals + groupskins);

			for (j = 0; j < groupskins; j++)
			{
				Mod_FloodFillSkin (skin, pheader->skinwidth, pheader->skinheight);

				if (j == 0)
				{
					texels = (byte *) Hunk_AllocName (size, loadname);
					pheader->texels[i] = texels - (byte *) pheader;
					memcpy (texels, (byte *) (pskintype), size);
				}

				sprintf (name, "%s:frame%i_%i", loadmodel->name, i, j);
				sprintf (extname, "%s_%i_%i", loadmodel->name, i, j);
				sprintf (fbr_mask_name, "%s:frame%i_%i_glow", loadmodel->name, i, j);
				offset = (unsigned)(pskintype) - (unsigned)mod_base;

				Mod_LoadSkinTexture
				(
					&pheader->gltextures[i][j & 3],
					&pheader->fbtextures[i][j & 3],
					name,
					fbr_mask_name,
					extname,
					(byte *) (pskintype),
					pheader->skinwidth,
					pheader->skinheight,
					offset
				);

				pskintype = (daliasskintype_t *) ((byte *) (pskintype) + size);
			}

			k = j;

			for (/* */; j < 4; j++)
				pheader->gltextures[i][j&3] =
					pheader->gltextures[i][j - k];
		}
	}

	return (void *)pskintype;
}


/*
=================
Mod_SetExtraFlags

set up extra flags that aren't in the mdl
=================
*/
void Mod_SetExtraFlags (model_t *mod)
{
	extern cvar_t r_nolerp_list;
	char *s;
	int i;

	if (!mod || !mod->name || mod->type != mod_alias)
		return;

	mod->flags &= 0xFF; // only preserve first byte

	// nolerp flag
	for (s = r_nolerp_list.string; *s; s += i + 1, i = 0)
	{
		// search forwards to the next comma or end of string
		for (i = 0; s[i] != ',' && s[i] != 0; i++) ;

		// compare it to the model name
		if (!strncmp (mod->name, s, i))
		{
			mod->flags |= MOD_NOLERP;
			break;
		}
	}

	// noshadow flag (TODO: make this a cvar list)
	if (!strcmp (mod->name, "progs/flame2.mdl") ||
			!strcmp (mod->name, "progs/flame.mdl") ||
			!strcmp (mod->name, "progs/bolt1.mdl") ||
			!strcmp (mod->name, "progs/bolt2.mdl") ||
			!strcmp (mod->name, "progs/bolt3.mdl") ||
			!strcmp (mod->name, "progs/laser.mdl"))
		mod->flags |= MOD_NOSHADOW;

	// fullbright hack (TODO: make this a cvar list)
	if (!strcmp (mod->name, "progs/flame2.mdl") ||
			!strcmp (mod->name, "progs/flame.mdl") ||
			!strcmp (mod->name, "progs/boss.mdl"))
		mod->flags |= MOD_FBRIGHTHACK;
}

/*
=================
Mod_LoadAliasModel
=================
*/
void Mod_LoadAliasModel (model_t *mod, void *buffer)
{
	int					i;
	mdl_t				*pinmodel;
	stvert_t			*pinstverts;
	dtriangle_t			*pintriangles;
	int					version;
	daliasframetype_t	*pframetype;
	daliasskintype_t	*pskintype;
	int					start, end, total;

	// some models are special
	if(!strcmp(mod->name, "progs/player.mdl"))
		mod->modhint = MOD_PLAYER;
	else if(!strcmp(mod->name, "progs/eyes.mdl"))
		mod->modhint = MOD_EYES;
	else
		mod->modhint = MOD_NORMAL;

	if (mod->modhint == MOD_PLAYER || mod->modhint == MOD_EYES)
		mod->crc = CRC_Block (buffer, mod_filesize);
	
	start = Hunk_LowMark ();

	pinmodel = (mdl_t *)buffer;
	mod_base = (byte *)buffer;

	version = LittleLong (pinmodel->version);

	if (version != ALIAS_VERSION) {
		Hunk_FreeToLowMark (start);
		Host_Error ("%s has wrong version number (%i should be %i)\n", mod->name, version, ALIAS_VERSION);
		return;
	}

	// allocate space for a working header
	pheader = Hunk_AllocName (sizeof (aliashdr_t) + LittleLong (pinmodel->numframes) * sizeof (maliasframedesc_t), loadname);
	mod->flags = LittleLong (pinmodel->flags);
	mod->type = mod_alias;

	// endian-adjust and copy the data, starting with the alias model header
	pheader->boundingradius = LittleFloat (pinmodel->boundingradius);
	pheader->numskins = LittleLong (pinmodel->numskins);
	pheader->skinwidth = LittleLong (pinmodel->skinwidth);
	pheader->skinheight = LittleLong (pinmodel->skinheight);
	pheader->vertsperframe = LittleLong (pinmodel->numverts);
	pheader->numtris = LittleLong (pinmodel->numtris);
	pheader->numframes = LittleLong (pinmodel->numframes);
	pheader->numverts = pheader->numtris * 3;

	// validate the setup
	if (pheader->numframes < 1) Host_Error ("Mod_LoadAliasModel: Invalid # of frames: %d\n", pheader->numframes);

	if (pheader->numtris <= 0) Host_Error ("model %s has no triangles", mod->name);

	if (pheader->vertsperframe <= 0) Host_Error ("model %s has no vertices", mod->name);

	pheader->size = LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = (synctype_t) LittleLong (pinmodel->synctype);
	mod->numframes = pheader->numframes;

	for (i = 0; i < 3; i++)
	{
		pheader->scale[i] = LittleFloat (pinmodel->scale[i]);
		pheader->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
	}

	// load the skins
	pskintype = (daliasskintype_t *)&pinmodel[1];
	pskintype = Mod_LoadAllSkins (pheader->numskins, pskintype);

	// load base s and t vertices
	pinstverts = (stvert_t *)pskintype;

	for (i = 0; i < pheader->vertsperframe; i++)
	{
		pinstverts[i].onseam = LittleLong (pinstverts[i].onseam);
		pinstverts[i].s = LittleLong (pinstverts[i].s);
		pinstverts[i].t = LittleLong (pinstverts[i].t);
	}

	// load triangle lists
	pintriangles = (dtriangle_t *) &pinstverts[pheader->vertsperframe];

	for (i = 0; i < pheader->numtris; i++)
	{
		pintriangles[i].facesfront = LittleLong (pintriangles[i].facesfront);

		pintriangles[i].vertindex[0] = LittleLong (pintriangles[i].vertindex[0]);
		pintriangles[i].vertindex[1] = LittleLong (pintriangles[i].vertindex[1]);
		pintriangles[i].vertindex[2] = LittleLong (pintriangles[i].vertindex[2]);
	}

	// load the frames
	pheader->nummeshframes = 0;
	pheader->vertexes = 0;
	pheader->framevertexsize = 0;
	pframetype = (daliasframetype_t *)&pintriangles[pheader->numtris];

	loadmodel->mins[0] = loadmodel->mins[1] = loadmodel->mins[2] = 99999999;
	loadmodel->maxs[0] = loadmodel->maxs[1] = loadmodel->maxs[2] = -99999999;

	for (i = 0; i < pheader->numframes; i++)
	{
		aliasframetype_t frametype = LittleLong (pframetype->type);

		if (frametype == ALIAS_SINGLE)
			pframetype = (daliasframetype_t *) Mod_LoadAliasFrame (pframetype + 1, &pheader->frames[i]);
		else pframetype = (daliasframetype_t *) Mod_LoadAliasGroup (pframetype + 1, &pheader->frames[i]);
		}

	for (i = 0; i < 3; i++)
	{
		loadmodel->mins[i] = (loadmodel->mins[i] * pheader->scale[i]) + pheader->scale_origin[i];
		loadmodel->maxs[i] = (loadmodel->maxs[i] * pheader->scale[i]) + pheader->scale_origin[i];

		if (loadmodel->mins[i] > -16) loadmodel->mins[i] = -16;
		if (loadmodel->maxs[i] < 16) loadmodel->maxs[i] = 16;
	}

	// build the draw lists
	GL_MakeAliasModelDisplayLists (pinstverts, pintriangles);

	Mod_SetExtraFlags (mod);

	// move the complete, relocatable alias model to the cache
	end = Hunk_LowMark ();
	total = end - start;
	Cache_Alloc (&mod->cache, total, loadname);

	if (!mod->cache.data)
		return;

	memcpy (mod->cache.data, pheader, total);
	Hunk_FreeToLowMark (start);
}


//=============================================================================

/*
=================
Mod_LoadSpriteFrame
=================
*/
void *Mod_LoadSpriteFrame (void * pin, mspriteframe_t **ppframe, int framenum)
{
	dspriteframe_t		*pinframe;
	mspriteframe_t		*pspriteframe;
	int					width, height, size, origin[2];
	char				name[64];
	unsigned			offset;

	pinframe = (dspriteframe_t *)pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;

	pspriteframe = (mspriteframe_t *) Hunk_Alloc (sizeof (mspriteframe_t));

	memset (pspriteframe, 0, sizeof (mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	sprintf (name, "%s:frame%i", loadmodel->name, framenum);
	offset = (unsigned)(pinframe+1) - (unsigned)mod_base;
	pspriteframe->gl_texture = TexMgr_LoadImage (loadmodel, name, width, height, SRC_INDEXED, (byte *)(pinframe + 1), loadmodel->name, offset, TEXPREF_NOPICMIP);

	return (void *)((byte *)pinframe + sizeof (dspriteframe_t) + size);
}


/*
=================
Mod_LoadSpriteGroup
=================
*/
void *Mod_LoadSpriteGroup (void * pin, mspriteframe_t **ppframe, int framenum)
{
	dspritegroup_t		*pingroup;
	mspritegroup_t		*pspritegroup;
	int					i, numframes;
	dspriteinterval_t	*pin_intervals;
	float				*poutintervals;
	void				*ptemp;

	pingroup = (dspritegroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	pspritegroup = (mspritegroup_t *) Hunk_Alloc (sizeof (mspritegroup_t) + (numframes - 1) * sizeof (pspritegroup->frames[0]));

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;

	pin_intervals = (dspriteinterval_t *)(pingroup + 1);

	poutintervals = (float *) Hunk_Alloc (numframes * sizeof (float));

	pspritegroup->intervals = poutintervals;

	for (i = 0; i < numframes; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);

		if (*poutintervals <= 0.0)
			Host_Error ("Mod_LoadSpriteGroup: interval<=0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (dspriteinterval_t *)pin_intervals;

	for (i = 0; i < numframes; i++)
	{
		ptemp = Mod_LoadSpriteFrame (ptemp, &pspritegroup->frames[i], framenum * 100 + i);
	}

	return ptemp;
}


/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	int					i;
	int					version;
	dsprite_t			*pin;
	msprite_t			*psprite;
	int					numframes;
	int					size;
	dspriteframetype_t	*pframetype;
	
	pin = (dsprite_t *)buffer;
	mod_base = (byte *)buffer;

	version = LittleLong (pin->version);

	if (version != SPRITE_VERSION) {
		Host_Error ("%s has wrong version number "
				 "(%i should be %i)", mod->name, version, SPRITE_VERSION);
		return;
	}

	numframes = LittleLong (pin->numframes);

	size = sizeof (msprite_t) +	(numframes - 1) * sizeof (psprite->frames);

	psprite = (msprite_t *) Hunk_Alloc (size);

	mod->cache.data = psprite;

	psprite->type = LittleLong (pin->type);
	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = (synctype_t) LittleLong (pin->synctype);
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth/2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth/2;
	mod->mins[2] = -psprite->maxheight/2;
	mod->maxs[2] = psprite->maxheight/2;

//
// load the frames
//
	if (numframes < 1)
		Host_Error ("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *)(pin + 1);

	for (i = 0; i < numframes; i++)
	{
		spriteframetype_t	frametype;

		frametype = (spriteframetype_t) LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE)
		{
			pframetype = (dspriteframetype_t *)
					Mod_LoadSpriteFrame (pframetype + 1, &psprite->frames[i].frameptr, i);
		}
		else
		{
			pframetype = (dspriteframetype_t *)
					Mod_LoadSpriteGroup (pframetype + 1, &psprite->frames[i].frameptr, i);
		}
	}

	mod->type = mod_sprite;
}

//=============================================================================

/*
================
Mod_Print
================
*/
void Mod_Print (void)
{
	int		i;
	model_t	*mod;

	Com_Printf ("Cached models:\n");
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		Com_Printf ("%8p : %s\n",mod->cache.data, mod->name);
	}
}


int R_ModelFlags (const struct model_s *model)
{
	return model->flags;
}

unsigned short R_ModelChecksum (const struct model_s *model)
{
	return model->crc;
}

