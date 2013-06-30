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

#include "quakedef.h"
#include "cl_slist.h"

static qbool slist_initialised = false;

server_entry_t	slist[MAX_SERVER_LIST];

void SList_Init(void) {
	memset(&slist, 0, sizeof(slist));
}

void SList_Shutdown(void) {  
	qfile_t *f;

	if (!slist_initialised)
		return;	

	if (!(f = FS_Open("servers.txt", "w", false, false)))
		return;
	SList_Save(f);
	FS_Close(f);
}

void SList_Set (int i, char *addr, char *desc) {
	if (i >= MAX_SERVER_LIST || i < 0)
		Sys_Error("SList_Switch: Bad index %d", i);

	if (slist[i].server)
		Q_free(slist[i].server);
	if (slist[i].description)
		Q_free(slist[i].description);

	slist[i].server = Q_strdup (addr);
	slist[i].description = Q_strdup (desc);
}

void SList_Reset_NoFree (int i) { 
	if (i >= MAX_SERVER_LIST || i < 0)
		Sys_Error("SList_Switch: Bad index %d", i);

	slist[i].description = slist[i].server = NULL;
}

void SList_Reset (int i) {
	if (i >= MAX_SERVER_LIST || i < 0)
		Sys_Error("SList_Switch: Bad index %d", i);

	if (slist[i].server) {
		Q_free(slist[i].server);
		slist[i].server = NULL;
	}

	if (slist[i].description) {
		Q_free(slist[i].description);
		slist[i].description = NULL;
	}
}

void SList_Switch (int a, int b) {
	server_entry_t temp;

	if (a >= MAX_SERVER_LIST || a < 0)
		Sys_Error("SList_Switch: Bad index %d", a);
	if (b >= MAX_SERVER_LIST || b < 0)
		Sys_Error("SList_Switch: Bad index %d", b);

	memcpy(&temp, &slist[a], sizeof(temp));
	memcpy(&slist[a], &slist[b], sizeof(temp));
	memcpy(&slist[b], &temp, sizeof(temp));
}

int SList_Len (void) {
	int count;

	for (count = 0; count < MAX_SERVER_LIST && slist[count].server; count++)
		;
	return count;
}

void SList_Load (void) {
	unsigned int len;
	int c, argc, count;
	char line[128], *desc, *addr;
	qfile_t *f;

	if (!(f = FS_Open ("servers.txt", "r", false, false)))
		return;

	count = len = 0;
	while ((c = FS_Getc(f))) {
		if (c == '\n' || c == '\r' || c == EOF) {
			if (c == '\r' && (c = FS_Getc(f)) != '\n' && c != EOF)
				FS_UnGetc(f, c);

			line[len] = 0;
			len = 0;
			Cmd_TokenizeString(line);

			if ((argc = Cmd_Argc()) >= 1) {
				addr = Cmd_Argv(0);
				desc = (argc >= 2) ? Cmd_MakeArgs(1) : "Unknown";
				SList_Set (count, addr, desc);
				if (++count == MAX_SERVER_LIST)
					break;
			}
			if (c == EOF)
				break;	// just in case an EOF follows a '\r'
		} else {
			if (len + 1 < sizeof(line))
				line[len++] = c;
		}
	}

	FS_Close (f);

	slist_initialised = true;
}

static char *CreateSpaces(int amount) {
	static char spaces[1024];
	int size;

	size = bound(1, amount, sizeof(spaces) - 1);
	memset(spaces, ' ', size);
	spaces[size] = 0;

	return spaces;
}

void SList_Save (qfile_t *f) {
	int i;
	char *spaces;

	for (i = 0; i < MAX_SERVER_LIST; i++) {
		if (!slist[i].server)
			break;
		spaces = CreateSpaces(32 - strlen(slist[i].server));
		FS_Printf(f, "%s%s%s\n", slist[i].server, spaces, slist[i].description);
	}
}
