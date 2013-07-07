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

// cl_draw.h - 2d drawing functions which don't belong to refresh

#include "quakedef.h"


/*
================
Draw_Alt_String
================
*/
void Draw_Alt_String (int x, int y, const char *str)
{
	while (*str) {
		R_DrawChar (x, y, (*str) | 0x80);
		str++;
		x += 8;
	}
}


/*
================
Draw_TextBox
================
*/
void Draw_TextBox (int x, int y, int width, int lines)
{
	qpic_t	*p;
	int		cx, cy;
	int		n;

	// draw left side
	cx = x;
	cy = y;
	p = R_CachePic ("gfx/box_tl.lmp");
	R_DrawPic (cx, cy, p);
	p = R_CachePic ("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		R_DrawPic (cx, cy, p);
	}
	p = R_CachePic ("gfx/box_bl.lmp");
	R_DrawPic (cx, cy+8, p);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		p = R_CachePic ("gfx/box_tm.lmp");
		R_DrawPic (cx, cy, p);
		p = R_CachePic ("gfx/box_mm.lmp");
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			if (n == 1)
				p = R_CachePic ("gfx/box_mm2.lmp");
			R_DrawPic (cx, cy, p);
		}
		p = R_CachePic ("gfx/box_bm.lmp");
		R_DrawPic (cx, cy+8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = R_CachePic ("gfx/box_tr.lmp");
	R_DrawPic (cx, cy, p);
	p = R_CachePic ("gfx/box_mr.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		R_DrawPic (cx, cy, p);
	}
	p = R_CachePic ("gfx/box_br.lmp");
	R_DrawPic (cx, cy+8, p);
}


/*
================
Draw_BigString
================
*/
static void Draw_GetBigfontSourceCoords(char c, int char_width, int char_height, int *sx, int *sy)
{
   if (c >= 'A' && c <= 'Z')
   {
      (*sx) = ((c - 'A') % 8) * char_width;
      (*sy) = ((c - 'A') / 8) * char_height;
   }
   else if (c >= 'a' && c <= 'z')
   {
      // Skip A-Z, hence + 26.
      (*sx) = ((c - 'a' + 26) % 8) * char_width;
      (*sy) = ((c - 'a' + 26) / 8) * char_height;
   }
   else if (c >= '0' && c <= '9')
   {
      // Skip A-Z and a-z.
      (*sx) = ((c - '0' + 26 * 2) % 8 ) * char_width;
      (*sy) = ((c - '0' + 26 * 2) / 8) * char_height;
   }
   else if (c == ':')
   {
      // Skip A-Z, a-z and 0-9.
      (*sx) = ((c - '0' + 26 * 2 + 10) % 8) * char_width;
      (*sy) = ((c - '0' + 26 * 2 + 10) / 8) * char_height;
   }
   else if (c == '/')
   {
      // Skip A-Z, a-z, 0-9 and :
      (*sx) = ((c - '0' + 26 * 2 + 11) % 8) * char_width;
      (*sy) = ((c - '0' + 26 * 2 + 11) / 8) * char_height;
   }
   else
   {
      (*sx) = -1;
      (*sy) = -1;
   }
}

void Draw_BigString (int x, int y, char *str, float scale, int gap)
{
   int num;

   if (y <= -8)
      return;         // totally off screen

   if (*str)
   {
      qpic_t *big_charset = R_CachePic("gfx/mcharset.tga");
      int char_width = (big_charset->width / 8);
      int char_height = (big_charset->height / 8);

      while (*str)  // stop rendering when out of characters
      {
         if ((num = *str++) != 32) // skip spaces
         {
            int sx = 0, sy = 0;
            char c = (char)(num & 0xFF);

            Draw_GetBigfontSourceCoords (c, char_width, char_height, &sx, &sy); // Get frow and fcol

            if (sx >= 0) // Negative values mean no char
               R_DrawSubPic (x, y, big_charset, sx, sy, char_width, char_height, scale);
         }

         x += (char_width * scale) + gap;
      }
   }
}


/*
================
Draw_Crosshair
================
*/
void Draw_Crosshair (void)
{
	extern cvar_t crosshaircolor, cl_crossx, cl_crossy;

	if (cl.spectator && cam_curtarget == CAM_NOTARGET)
		return;

	R_DrawCrosshair (cl_crossx.value, cl_crossy.value, crosshaircolor.value);
}
