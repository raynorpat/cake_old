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
// gl_mesh.c: triangle model functions

#include "gl_local.h"


unsigned int r_meshindexbuffer = 0;
unsigned int r_meshvertexbuffer = 0;

void GL_MakeAliasModelDisplayLists (stvert_t *stverts, dtriangle_t *triangles)
{
	int i, j;
	aliasmesh_t *hunkdesc;

	// there can never be more than this number of verts
	aliasmesh_t *desc = (aliasmesh_t *) malloc (sizeof (aliasmesh_t) * pheader->numverts);

	// there will always be this number of indexes
	unsigned short *indexes = (unsigned short *) Hunk_Alloc (sizeof (unsigned short) * pheader->numtris * 3);

	pheader->indexes = (intptr_t) indexes - (intptr_t) pheader;
	pheader->numindexes = 0;
	pheader->numverts = 0;

	for (i = 0; i < pheader->numtris; i++)
	{
		for (j = 0; j < 3; j++)
		{
			int v;

			// index into hdr->vertexes
			unsigned short vertindex = triangles[i].vertindex[j];

			// basic s/t coords
			int s = stverts[vertindex].s;
			int t = stverts[vertindex].t;

			// check for back side and adjust texcoord s
			if (!triangles[i].facesfront && stverts[vertindex].onseam)
				s += pheader->skinwidth / 2;

			// see does this vert already exist
			for (v = 0; v < pheader->numverts; v++)
			{
				// it could use the same xyz but have different s and t
				if (desc[v].vertindex == vertindex && (int) desc[v].st[0] == s && (int) desc[v].st[1] == t)
				{
					// exists; emit an index for it
					indexes[pheader->numindexes++] = v;

					// no need to check any more
					break;
				}
			}

			if (v == pheader->numverts)
			{
				// doesn't exist; emit a new vert and index
				indexes[pheader->numindexes++] = pheader->numverts;

				desc[pheader->numverts].vertindex = vertindex;
				desc[pheader->numverts].st[0] = s;
				desc[pheader->numverts++].st[1] = t;
			}
		}
	}

	// create a hunk buffer for the final mesh we'll actually use
	hunkdesc = (aliasmesh_t *) Hunk_Alloc (sizeof (aliasmesh_t) * pheader->numverts);
	pheader->aliasmesh = (intptr_t) hunkdesc - (intptr_t) pheader;

	// tidy up the verts by calculating final s and t and copying out to the hunk
	for (i = 0; i < pheader->numverts; i++)
	{
		hunkdesc[i].vertindex = desc[i].vertindex;
		hunkdesc[i].st[0] = ((float) desc[i].st[0] + 0.5f) / (float) pheader->skinwidth;
		hunkdesc[i].st[1] = ((float) desc[i].st[1] + 0.5f) / (float) pheader->skinheight;
	}

	// don't forget!!!
	free (desc);
}


void R_CreateMeshIndexBuffer (void)
{
}
