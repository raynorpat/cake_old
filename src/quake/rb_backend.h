extern int c_brush_polys, c_brush_passes, c_alias_polys;

void RB_InitBackend (void);

void RB_StartFrame (void);
void RB_EndFrame (void);

void RB_Finish (void);

void RB_Set2DProjections (void);

//
// rb_backend_gl11.c
//
void RB_GL11_Init (void);

void RB_GL11_Set2DProjections (void);

//
// rb_backend_d3d.c
//
