extern int c_brush_polys, c_brush_passes, c_alias_polys;

extern float r_fovx, r_fovy; // rendering fov may be different becuase of r_waterwarp and r_stereo

void RB_InitBackend (void);

void RB_StartFrame (void);
void RB_EndFrame (void);

void RB_Finish (void);
void RB_Clear (void);

#define OFFSET_BMODEL	1
#define OFFSET_NONE		0
#define OFFSET_DECAL	-1
#define OFFSET_FOG		-2
#define OFFSET_SHOWTRIS -3
void RB_PolygonOffset (int);

typedef enum {
	CANVAS_INVALID = -1,
	CANVAS_NONE,
	CANVAS_DEFAULT,
	CANVAS_CONSOLE,
	CANVAS_MENU,
	CANVAS_SBAR,
	CANVAS_WARPIMAGE,
	CANVAS_CROSSHAIR,
	CANVAS_BOTTOMLEFT,
	CANVAS_BOTTOMRIGHT,
} canvastype;

void RB_SetDefaultCanvas (void);
void RB_SetCanvas (canvastype canvastype);
void RB_Set3DMatrix (void);
void RB_RotateMatrixForEntity (vec3_t origin, vec3_t angles);