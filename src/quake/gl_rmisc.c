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
// r_misc.c

#include "gl_local.h"
#include "rc_image.h"

#include <time.h>

extern gltexture_t *playertextures[MAX_CLIENTS];
int	skytexturenum = -1;

/*
===============
R_TranslatePlayerSkin

Translates a skin texture by the per-player color lookup
===============
*/
void R_TranslatePlayerSkin (int playernum)
{
	int			top, bottom;
	player_info_t *player;
	char		s[512];

	player = &cl.players[playernum];
	if (!player->name[0])
		return;

	strcpy(s, Info_ValueForKey(player->userinfo, "skin"));
	COM_StripExtension(s, s);
	if (player->skin && Q_stricmp(s, player->skin->name))
		player->skin = NULL;

	if (player->_topcolor != player->topcolor || player->_bottomcolor != player->bottomcolor || !player->skin) {
		player->_topcolor = player->topcolor;
		player->_bottomcolor = player->bottomcolor;

		top = player->topcolor;
		bottom = player->bottomcolor;
		top = (top < 0) ? 0 : ((top > 13) ? 13 : top);
		bottom = (bottom < 0) ? 0 : ((bottom > 13) ? 13 : bottom);
		top *= 16;
		bottom *= 16;

		// FIXME: if gl_nocolors is on, then turned off, the textures may be out of sync with the scoreboard colors.
		if (!gl_nocolors.value)
			if (playertextures[playernum])
				TexMgr_ReloadImage (playertextures[playernum], top, bottom);
	}
}

/*
===============
R_TranslateNewPlayerSkin

this is called when the skin or model actually changes, instead of just new colors
===============
*/
void R_TranslateNewPlayerSkin (int playernum)
{
	player_info_t	*player;
	char			name[256];
	int				width, height;
	byte			*skin;
	extern byte	player_8bit_texels[320*200];

	player = &cl.players[playernum];
	if (!player->name[0])
		return;

	// locate the original skin pixels
	if (!player->skin)
		Skin_Find(player);

	if ((skin = Skin_Cache(player->skin)) != NULL) {
		// skin data width
		width = 320;
		height = 200;
	} else {
		skin = player_8bit_texels;
		width = 296;
		height = 194;
	}

	// upload new image
	sprintf (name, "player_%i", playernum);
	playertextures[playernum] = TexMgr_LoadImage (NULL, name, width, height, SRC_INDEXED, skin, 0, NULL, TEXPREF_OVERWRITE);

	// now recolor it
	R_TranslatePlayerSkin (playernum);
}

/*
===============
R_NewMap
===============
*/
void R_NewMap (struct model_s *worldmodel)
{
	int		i;

	R_Modules_NewMap();

	r_worldmodel = worldmodel;

	memset (&r_worldentity, 0, sizeof(r_worldentity));
	r_worldentity.model = r_worldmodel;

// clear out efrags in case the level hasn't been reloaded
// FIXME: is this one short?
	for (i = 0; i < r_worldmodel->numleafs; i++)
		r_worldmodel->leafs[i].efrags = NULL;
		 	
	r_viewleaf = NULL;

	GL_BuildLightmaps ();

	// identify sky texture
	for (i = 0; i < r_worldmodel->numtextures; i++)
	{
		if (!r_worldmodel->textures[i])
			continue;
		if (!strncmp(r_worldmodel->textures[i]->name,"sky",3) )
			skytexturenum = i;
 		r_worldmodel->textures[i]->texturechain = NULL;
	}

	r_skyboxloaded = false;
}


void R_LoadSky_f ()
{
	if (Cmd_Argc() < 2) {
		Com_Printf ("loadsky <name> : load a custom skybox\n");
		return;
	}

	R_SetSky (Cmd_Argv(1));
}


/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/ 

typedef struct _TargaHeader {
	unsigned char   id_length, colormap_type, image_type;
	unsigned short  colormap_index, colormap_length;
	unsigned char   colormap_size;
	unsigned short  x_origin, y_origin, width, height;
	unsigned char   pixel_size, attributes;
} TargaHeader;


/* 
================== 
R_ScreenShot_f
================== 
*/  
void R_ScreenShot_f (void)
{
	byte	*buffer;
	char	pcxname[MAX_OSPATH]; 
	char	checkname[MAX_OSPATH];
	int		i, c, temp;
	FILE	*f;

	if (Cmd_Argc() == 2)
	{
		strlcpy (pcxname, Cmd_Argv(1), sizeof(pcxname));
		COM_ForceExtension (pcxname, ".tga");
	}
	else
	{
		// 
		// find a file name to save it to 
		// 
		strcpy(pcxname,"quake00.tga");
		
		for (i = 0; i <= 99; i++) 
		{ 
			pcxname[5] = i/10 + '0'; 
			pcxname[6] = i%10 + '0'; 
			sprintf (checkname, "%s/%s", cls.gamedir, pcxname);
			f = fopen (checkname, "rb");
			if (!f)
				break;  // file doesn't exist
			fclose (f);
		} 
		if (i==100) 
		{
			Com_Printf ("R_ScreenShot_f: Couldn't create a TGA file\n"); 
			return;
		}
	}		

	buffer = Q_malloc (vid.realwidth * vid.realheight * 3 + 18);
	memset (buffer, 0, 18);
	buffer[2] = 2;          // uncompressed type
	buffer[12] = vid.realwidth&255;
	buffer[13] = vid.realwidth>>8;
	buffer[14] = vid.realheight&255;
	buffer[15] = vid.realheight>>8;
	buffer[16] = 24;        // pixel size

	qglReadPixels (0, 0, vid.realwidth, vid.realheight, GL_RGB, GL_UNSIGNED_BYTE, buffer+18 ); 

	// swap rgb to bgr
	c = 18 + vid.realwidth * vid.realheight * 3;
	for (i=18 ; i<c ; i+=3)
	{
		temp = buffer[i];
		buffer[i] = buffer[i+2];
		buffer[i+2] = temp;
	}
	COM_WriteFile (va("%s/%s", cls.gamedirfile, pcxname), buffer, vid.realwidth*vid.realheight*3 + 18 );

	Q_free (buffer);
	Com_Printf ("Wrote %s\n", pcxname);
} 
 


/*
Find closest color in the palette for named color
*/
int MipColor(int r, int g, int b)
{
	int i;
	float dist;
	int best = 0;
	float bestdist;
	int r1, g1, b1;
	byte *pal = (byte *)d_8to24table;
	static int lr = -1, lg = -1, lb = -1;
	static int lastbest;

	if (r == lr && g == lg && b == lb)
		return lastbest;

	bestdist = 256*256*3;

	for (i = 0; i < 256; i++) {
		r1 = pal[i*3] - r;
		g1 = pal[i*3+1] - g;
		b1 = pal[i*3+2] - b;
		dist = r1*r1 + g1*g1 + b1*b1;
		if (dist < bestdist) {
			bestdist = dist;
			best = i;
		}
	}
	lr = r; lg = g; lb = b;
	lastbest = best;
	return best;
}

void R_DrawCharToSnap (int num, byte *dest, int width)
{
/*
	int		row, col;
	byte	*source;
	int		drawline;
	int		x;

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	drawline = 8;

	while (drawline--)
	{
		for (x=0 ; x<8 ; x++)
			if (source[x])
				dest[x] = source[x];
			else
				dest[x] = 98;
		source += 128;
		dest -= width;
	}
*/
}

void R_DrawStringToSnap (const char *s, byte *buf, int x, int y, int width)
{
	byte *dest;
	const unsigned char *p;

	dest = buf + ((y * width) + x);

	p = (const unsigned char *)s;
	while (*p) {
		R_DrawCharToSnap(*p++, dest, width);
		dest += 8;
	}
}


/* 
================== 
R_RSShot

Memory pointed to by pcxdata is allocated using Hunk_TempAlloc
Never store this pointer for later use!

On failure (not enough memory), *pcxdata will be set to NULL
================== 
*/  
void R_RSShot (byte **pcxdata, int *pcxsize)
{ 
	int     x, y;
	unsigned char	*src, *dest;
	unsigned char	*newbuf;
	int w, h;
	int dx, dy, dex, dey, nx;
	int r, b, g;
	int count;
	float fracw, frach;
	char st[80];
	time_t now;
	byte *pal = (byte *)d_8to24table;
	extern cvar_t name;

// 
// save the pcx file 
// 
	newbuf = Q_malloc (vid.realheight * vid.realwidth * 3);

	qglReadPixels (0, 0, vid.realwidth, vid.realheight, GL_RGB, GL_UNSIGNED_BYTE, newbuf);

	w = min (vid.realwidth, RSSHOT_WIDTH);
	h = min (vid.realheight, RSSHOT_HEIGHT);

	fracw = (float)vid.realwidth / (float)w;
	frach = (float)vid.realheight / (float)h;

	for (y = 0; y < h; y++) {
		dest = newbuf + (w*3 * y);

		for (x = 0; x < w; x++) {
			r = g = b = 0;

			dx = x * fracw;
			dex = (x + 1) * fracw;
			if (dex == dx) dex++; // at least one
			dy = y * frach;
			dey = (y + 1) * frach;
			if (dey == dy) dey++; // at least one

			count = 0;
			for (/* */; dy < dey; dy++) {
				src = newbuf + (vid.realwidth * 3 * dy) + dx * 3;
				for (nx = dx; nx < dex; nx++) {
					r += *src++;
					g += *src++;
					b += *src++;
					count++;
				}
			}
			r /= count;
			g /= count;
			b /= count;
			*dest++ = r;
			*dest++ = g;
			*dest++ = b;
		}
	}

	// convert to eight bit
	for (y = 0; y < h; y++) {
		src = newbuf + (w * 3 * y);
		dest = newbuf + (w * y);

		for (x = 0; x < w; x++) {
			*dest++ = MipColor(src[0], src[1], src[2]);
			src += 3;
		}
	}

	time(&now);
	strcpy(st, ctime(&now));
	st[strlen(st) - 1] = 0;
	R_DrawStringToSnap (st, newbuf, w - strlen(st)*8, h - 1, w);

	strlcpy (st, cls.servername, sizeof(st));
	R_DrawStringToSnap (st, newbuf, w - strlen(st)*8, h - 11, w);

	strlcpy (st, name.string, sizeof(st));
	R_DrawStringToSnap (st, newbuf, w - strlen(st)*8, h - 21, w);

	// +w*(h-1) and -w are because we have the data upside down in newbuf
	WritePCX (newbuf + w*(h-1), w, h, -w, pal, pcxdata, pcxsize);

	Q_free (newbuf);

	// return with pcxdata and pcxsize
} 
