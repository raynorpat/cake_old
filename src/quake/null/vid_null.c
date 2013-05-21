// vid_null.c -- null video driver to aid porting efforts

#include "quakedef.h"
#include "d_local.h"

viddef_t	vid;				// global video state

#define	BASEWIDTH	320
#define	BASEHEIGHT	200

byte	vid_buffer[BASEWIDTH*BASEHEIGHT];
short	zbuffer[BASEWIDTH*BASEHEIGHT];
byte	surfcache[256*1024];

unsigned	d_8to24table[256];


void	VID_SetCaption (char *text)
{
}


void	VID_SetPalette (unsigned char *palette)
{
}

void	VID_ShiftPalette (unsigned char *palette)
{
}

void	VID_Init (unsigned char *palette)
{
	vid.width = BASEWIDTH;
	vid.height = BASEHEIGHT;
	vid.aspect = 1.0;
	vid.numpages = 1;
	vid.colormap = host_colormap;
	vid.buffer = vid_buffer;
	vid.rowbytes = BASEWIDTH;
	
	d_pzbuffer = zbuffer;
	D_InitCaches (surfcache, sizeof(surfcache));
}

void	VID_Shutdown (void)
{
}

void	VID_Update (vrect_t *rects)
{
}

size_t VID_ListModes(vid_mode_t *modes, size_t maxcount)
{
	return 0;
}


