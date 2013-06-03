extern int rs_brushpolys, rs_aliaspolys, rs_drawelements;
extern int rs_dynamiclightmaps;

extern float r_fovx, r_fovy; // rendering fov may be different becuase of r_waterwarp and r_stereo

void RB_InitBackend (void);

void RB_StartFrame (void);
void RB_EndFrame (void);

void RB_Finish (void);
void RB_Clear (void);

typedef enum {
	CANVAS_INVALID = -1,
	CANVAS_NONE,
	CANVAS_DEFAULT,
	CANVAS_CONSOLE,
	CANVAS_MENU,
	CANVAS_SBAR,
	CANVAS_BOTTOMLEFT,
	CANVAS_BOTTOMRIGHT,
} canvastype;

void RB_Set2DMatrix (void);
void RB_SetCanvas (canvastype canvastype);
void RB_Set3DMatrix (void);