#define MAX_ARRAY_VERTS			4096
#define MAX_ARRAY_TEX_COORDS	MAX_ARRAY_VERTS
#define MAX_ARRAY_INDEXES		MAX_ARRAY_VERTS*3
#define MAX_ARRAY_COLORS		MAX_ARRAY_VERTS

extern unsigned int	indexesArray[MAX_ARRAY_INDEXES];
extern vec4_t	vertexArray[MAX_ARRAY_VERTS];
extern vec3_t	normalsArray[MAX_ARRAY_VERTS];

extern vec2_t	inCoordsData[MAX_ARRAY_TEX_COORDS];
extern vec2_t	inLightmapCoordsData[MAX_ARRAY_TEX_COORDS];
extern vec4_t	inColorsData[MAX_ARRAY_COLORS];
extern int		numVerts, numIndexes, numCoords, numColors;

extern float	*currentVertex;
extern float	*currentNormal;
extern float	*currentCoords;
extern float	*currentLightmapCoords;
extern float	*currentColor;

extern unsigned int	r_numverts;
extern unsigned int	r_numtris;

extern int			c_brush_polys, c_brush_passes, c_alias_polys;

extern qbool r_blocked;

extern unsigned int quad_elems[6];

void RB_InitBackend (void);

void RB_StartFrame (void);
void RB_EndFrame (void);

void RB_Finish (void);

void RB_Set2DProjections (void);

void R_LockArrays (void);
void R_UnlockArrays (void);

void R_FlushArrays (void);
void R_FlushArraysMtex ( int tex1, int tex2 );

void R_ClearArrays (void);

void R_VertexTCBase ( int tc_gen, qbool mtex );

extern inline void R_PushElems ( unsigned int *elems, int numindexes )
{
	int i;

	for ( i = 0; i < numindexes; i++ ) {
		if( numIndexes >= MAX_ARRAY_INDEXES || r_blocked ) {
			r_blocked = true;
			return;
		}

		indexesArray[numIndexes] = numVerts + *elems++;
		numIndexes++;
	}
}

extern inline void R_PushVertex ( float *vertex )
{
	currentVertex[0] = vertex[0];
	currentVertex[1] = vertex[1];
	currentVertex[2] = vertex[2];

	numVerts++;
	r_numverts++;
	currentVertex += 4;
}

extern inline void R_PushNormal ( float *normal )
{
	currentNormal[0] = normal[0];
	currentNormal[1] = normal[1];
	currentNormal[2] = normal[2];
	currentNormal += 3;
}

extern inline void R_PushCoord ( float *tc )
{
	currentCoords[0] = tc[0];
	currentCoords[1] = tc[1];
	currentCoords += 2;

	numCoords++;
}

extern inline void R_PushLightmapCoord ( float *tc )
{
	currentLightmapCoords[0] = tc[0];
	currentLightmapCoords[1] = tc[1];
	currentLightmapCoords += 2;
}

inline void R_PushColor ( float *color )
{
	currentColor[0] = color[0];
	currentColor[1] = color[1];
	currentColor[2] = color[2];
	currentColor[3] = color[3];
	currentColor += 4;
}

extern inline void R_PushVertexes ( mvertex_t *verts, int numverts )
{
	int i;
	mvertex_t *vert;

	vert = verts;
	for ( i = 0; i < numverts; i++, vert++ )
	{
		if( numVerts >= MAX_ARRAY_VERTS || r_blocked ) {
			r_blocked = true;
			return;
		}

		R_PushVertex ( vert->position );
		R_PushNormal ( vert->normal );
		R_PushCoord ( vert->tex_st );
		R_PushLightmapCoord ( vert->lm_st );
		R_PushColor ( vert->color );
	}
}

//
// rb_backend_gl11.c
//
void RB_GL11_Init (void);

void RB_GL11_Set2DProjections (void);

//
// rb_backend_d3d.c
//
