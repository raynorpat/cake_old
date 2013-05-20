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
#ifndef _CL_SBAR_H_
#define _CL_SBAR_H_

#define	SBAR_HEIGHT 24

extern qbool	sb_drawinventory;
extern qbool	sb_drawmain;

void Sbar_Init (void);

// called every frame by screen
void Sbar_Draw (void);

void Sbar_IntermissionOverlay (void);
void Sbar_FinaleOverlay (void);

#endif /* _CL_SBAR_H_ */

