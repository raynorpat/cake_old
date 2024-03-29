/*
Copyright (C) 1999, 2000 contributors of the QuakeForge project

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

#define MAX_SERVER_LIST 512

typedef struct {
	char *server;
	char *description;
} server_entry_t;

extern server_entry_t slist[MAX_SERVER_LIST];

void SList_Init(void);
void SList_Shutdown(void);
void SList_Set(int i,char *addr,char *desc);
void SList_Reset_NoFree(int i);
void SList_Reset(int i);
void SList_Switch(int a,int b);
int SList_Len(void);
void SList_Load(void);
void SList_Save(qfile_t *f);