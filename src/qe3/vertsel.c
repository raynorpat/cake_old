
#include "qe3.h"

int	FindPoint (vec3_t point)
{
	int		i, j;

	for (i=0 ; i<g_qeglobals.d_numpoints ; i++)
	{
		for (j=0 ; j<3 ; j++)
			if (fabs(point[j] - g_qeglobals.d_points[i][j]) > 0.1)
				break;
		if (j == 3)
			return i;
	}

	VectorCopy (point, g_qeglobals.d_points[g_qeglobals.d_numpoints]);
	g_qeglobals.d_numpoints++;

	return g_qeglobals.d_numpoints-1;
}

int FindEdge (int p1, int p2, face_t *f)
{
	int		i;

	for (i=0 ; i<g_qeglobals.d_numedges ; i++)
		if (g_qeglobals.d_edges[i].p1 == p2 && g_qeglobals.d_edges[i].p2 == p1)
		{
			g_qeglobals.d_edges[i].f2 = f;
			return i;
		}

	g_qeglobals.d_edges[g_qeglobals.d_numedges].p1 = p1;
	g_qeglobals.d_edges[g_qeglobals.d_numedges].p2 = p2;
	g_qeglobals.d_edges[g_qeglobals.d_numedges].f1 = f;
	g_qeglobals.d_numedges++;

	return g_qeglobals.d_numedges-1;
}

void MakeFace (face_t *f)
{
	winding_t	*w;
	int			i;
	int			pnum[128];

	w = MakeFaceWinding (selected_brushes.next, f);
	if (!w)
		return;
	for (i=0 ; i<w->numpoints ; i++)
		pnum[i] = FindPoint (w->points[i]);
	for (i=0 ; i<w->numpoints ; i++)
		FindEdge (pnum[i], pnum[(i+1)%w->numpoints], f);

	free (w);
}

void SetupVertexSelection (void)
{
	face_t	*f;
	brush_t *b;

	g_qeglobals.d_numpoints = 0;
	g_qeglobals.d_numedges = 0;
	if (!QE_SingleBrush())
		return;
	b = selected_brushes.next;
	for (f=b->brush_faces ; f ; f=f->next)
		MakeFace (f);

	Sys_UpdateWindows (W_ALL);
}


void SelectFaceEdge (face_t *f, int p1, int p2)
{
	winding_t	*w;
	int			i, j, k;
	int			pnum[128];

	w = MakeFaceWinding (selected_brushes.next, f);
	if (!w)
		return;
	for (i=0 ; i<w->numpoints ; i++)
		pnum[i] = FindPoint (w->points[i]);
	for (i=0 ; i<w->numpoints ; i++)
		if (pnum[i] == p1 && pnum[(i+1)%w->numpoints] == p2)
		{
			VectorCopy (g_qeglobals.d_points[pnum[i]], f->planepts[0]);
			VectorCopy (g_qeglobals.d_points[pnum[(i+1)%w->numpoints]], f->planepts[1]);
			VectorCopy (g_qeglobals.d_points[pnum[(i+2)%w->numpoints]], f->planepts[2]);
			for (j=0 ; j<3 ; j++)
			{
				for (k=0 ; k<3 ; k++)
				{
					f->planepts[j][k] = floor(f->planepts[j][k]/g_qeglobals.d_gridsize+0.5)*g_qeglobals.d_gridsize;
				}
			}

			AddPlanept (f->planepts[0]);
			AddPlanept (f->planepts[1]);
			break;
		}

	if (i == w->numpoints)
		Sys_Printf ("Select face edge failed\n");
	free (w);
}

void SelectVertex (int p1)
{
	brush_t		*b;
	winding_t	*w;
	int			i, j, k;
	face_t		*f;

	b = selected_brushes.next;
	for (f=b->brush_faces ; f ; f=f->next)
	{
		w =  MakeFaceWinding (b, f);
		if (!w)
			continue;
		for (i=0 ; i<w->numpoints ; i++)
		{
			if (FindPoint (w->points[i]) == p1)
			{
				VectorCopy (w->points[(i+w->numpoints-1)%w->numpoints], f->planepts[0]);
				VectorCopy (w->points[i], f->planepts[1]);
				VectorCopy (w->points[(i+1)%w->numpoints], f->planepts[2]);
			for (j=0 ; j<3 ; j++)
			{
				for (k=0 ; k<3 ; k++)
				{
					f->planepts[j][k] = floor(f->planepts[j][k]/g_qeglobals.d_gridsize+0.5)*g_qeglobals.d_gridsize;
				}
			}

				AddPlanept (f->planepts[1]);
				break;
			}
		}
		free (w);
	}
}

void SelectEdgeByRay (vec3_t org, vec3_t dir)
{
	int		i, j, besti;
	float	d, bestd;
	vec3_t	mid, temp;
	pedge_t	*e;

	// find the edge closest to the ray
	besti = -1;
	bestd = 8;

	for (i=0 ; i<g_qeglobals.d_numedges ; i++)
	{
		for (j=0 ; j<3 ; j++)
			mid[j] = 0.5*(g_qeglobals.d_points[g_qeglobals.d_edges[i].p1][j] + g_qeglobals.d_points[g_qeglobals.d_edges[i].p2][j]);

		VectorSubtract (mid, org, temp);
		d = DotProduct (temp, dir);
		VectorMA (org, d, dir, temp);
		VectorSubtract (mid, temp, temp);
		d = VectorLength (temp);
		if (d < bestd)
		{
			bestd = d;
			besti = i;
		}
	}

	if (besti == -1)
	{
		Sys_Printf ("Click didn't hit an edge\n");
		return;
	}
	Sys_Printf ("Hit edge\n");

	// make the two faces that border the edge use the two edge points
	// as primary drag points
	g_qeglobals.d_num_move_points = 0;
	e = &g_qeglobals.d_edges[besti];
	SelectFaceEdge (e->f1, e->p1, e->p2);
	SelectFaceEdge (e->f2, e->p2, e->p1);
}

void SelectVertexByRay (vec3_t org, vec3_t dir)
{
	int		i, besti;
	float	d, bestd;
	vec3_t	temp;

	// find the point closest to the ray
	besti = -1;
	bestd = 8;

	for (i=0 ; i<g_qeglobals.d_numpoints ; i++)
	{
		VectorSubtract (g_qeglobals.d_points[i], org, temp);
		d = DotProduct (temp, dir);
		VectorMA (org, d, dir, temp);
		VectorSubtract (g_qeglobals.d_points[i], temp, temp);
		d = VectorLength (temp);
		if (d < bestd)
		{
			bestd = d;
			besti = i;
		}
	}

	if (besti == -1)
	{
		Sys_Printf ("Click didn't hit a vertex\n");
		return;
	}
	Sys_Printf ("Hit vertex\n");
	SelectVertex (besti);
}



