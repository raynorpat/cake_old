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
#ifndef _CL_DRAW_H_
#define _CL_DRAW_H_

#define BIGLETTER_WIDTH				64
#define BIGLETTER_HEIGHT			64

#define BIGMENU_LEFT				72
#define BIGMENU_TOP					32
#define BIGMENU_ITEMS_SCALE			0.3
#define BIGMENU_TITLE_SCALE			0.45

#define	BIGMENU_LETTER_SPACING		-2
#define BIGMENU_VERTICAL_PADDING	2

void Draw_ColoredString (float x, float y, const char *text, int maxlen, float basered, float basegreen, float baseblue, float basealpha, int *outcolor, qbool ignorecolorcodes);

void Draw_TextBox (int x, int y, int width, int lines);
void Draw_BigString (int x, int y, char *str, float scale, int gap);
void Draw_Crosshair (void);

#endif /* _CL_DRAW_H_ */

