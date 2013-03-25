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
// zone.c - memory management

#include "common.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h> // VirtualAlloc
#else
#include <sys/mman.h> // mmap, mprotect
#endif

//============================================================================

void *hunk_tempptr = NULL;

void Hunk_TempFree (void)
{
	if (hunk_tempptr)
	{
		free (hunk_tempptr);
		hunk_tempptr = NULL;
	}
}

void *Hunk_TempAlloc (int size)
{
	Hunk_TempFree ();
	hunk_tempptr = malloc (size);

	return hunk_tempptr;
}

//============================================================================

#define HUNK_SENTINAL 0x1df001ed

typedef struct
{
	int      sentinal;
	int      size;
	char   name[8];
} hunk_t;

typedef struct hunk_type_s
{
	byte *hunkbuffer;
	int maxmb;
	int lowmark;
	int used;
} hunk_type_t;

hunk_type_t high_hunk = {NULL, 32 * 1024 * 1024, 0, 0};
hunk_type_t low_hunk = {NULL, 128 * 1024 * 1024, 0, 0};

void *Hunk_TypeAlloc (hunk_type_t *ht, int size, char *name)
{
	hunk_t *hp;

	if (!ht->hunkbuffer)
	{
#ifdef _WIN32
		ht->hunkbuffer = VirtualAlloc (NULL, ht->maxmb, MEM_RESERVE, PAGE_NOACCESS);
#else
		ht->hunkbuffer = mmap(NULL, ht->maxmb, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
#endif
		ht->lowmark = 0;
		ht->used = 0;
	}

	size = sizeof (hunk_t) + ((size + 15) & ~15);

	while (ht->lowmark + size >= ht->used)
	{
#ifdef _WIN32
		VirtualAlloc (ht->hunkbuffer + ht->used, 65536, MEM_COMMIT, PAGE_READWRITE);
#else
		mprotect(0, ht->hunkbuffer + ht->used, PROT_READ | PROT_WRITE);
#endif
		ht->used += 65536;
   }

	hp = (hunk_t *) (ht->hunkbuffer + ht->lowmark);
	ht->lowmark += size;

	memcpy (hp->name, name, 8);
	hp->sentinal = HUNK_SENTINAL;
	hp->size = size;

	memset ((hp + 1), 0, size - sizeof (hunk_t));
	return (hp + 1);
}


void Hunk_TypeFreeToLowMark (hunk_type_t *ht, int mark)
{
	memset (ht->hunkbuffer + mark, 0, ht->used - mark);
	ht->lowmark = mark;
}


int Hunk_TypeLowMark (hunk_type_t *ht)
{
	return ht->lowmark;
}

void *Hunk_AllocName (int size, char *name) {return Hunk_TypeAlloc (&low_hunk, size, name);}
void *Hunk_Alloc (int size) {return Hunk_TypeAlloc (&low_hunk, size, "unused");}

int   Hunk_LowMark (void) {return Hunk_TypeLowMark (&low_hunk);}
void Hunk_FreeToLowMark (int mark) {Hunk_TypeFreeToLowMark (&low_hunk, mark);}

void *Hunk_HighAllocName (int size, char *name) {Hunk_TempFree (); return Hunk_TypeAlloc (&high_hunk, size, name);}
int   Hunk_HighMark (void) {Hunk_TempFree (); return Hunk_TypeLowMark (&high_hunk);}
void Hunk_FreeToHighMark (int mark) {Hunk_TempFree (); Hunk_TypeFreeToLowMark (&high_hunk, mark);}


/*
===============================================================================

CACHE MEMORY

===============================================================================
*/

typedef struct cache_system_s
{
   int						size;
   cache_user_t				*user;
   char						name[MAX_OSPATH];
   struct cache_system_s   *next;
   struct cache_system_s   *prev;
} cache_system_t;

cache_system_t *cache_head = NULL;
cache_system_t *cache_tail = NULL;

void *Cache_Check (cache_user_t *c)
{
	return c->data;
}

void Cache_Free (cache_user_t *c)
{
	cache_system_t *cs = ((cache_system_t *) c) - 1;

	if (cs->prev)
		cs->prev->next = cs->next;
	else
		cache_head = cs->next;

	if (cs->next)
		cs->next->prev = cs->prev;
	cache_tail = cs->prev;

	// prevent Cache_Check from blowing up
	cs->user->data = NULL;

	free (cs);
}

void Cache_Flush (void)
{
	cache_system_t *cs = NULL;

	// because we're throwing stuff away we don't need to worry about relinking list pointers
	for (cs = cache_head; cs;)
	{
		cache_system_t *tmp = cs->next;

		// prevent Cache_Check from blowing up
		cs->user->data = NULL;

		free (cs);
		cs = tmp;
	}

	cache_head = cache_tail = NULL;
}

void *Cache_Alloc (cache_user_t *c, int size, char *name)
{
	cache_system_t *cs = NULL;

	for (cs = cache_head; cs; cs = cs->next)
	{
		if (!strcmp (cs->name, name))
			return cs->user->data;
	}

	if (c->data)
		Sys_Error ("Cache_Alloc: allready allocated");
	if (size <= 0)
		Sys_Error ("Cache_Alloc: size %i", size);

	cs = (cache_system_t *) malloc (sizeof (cache_system_t) + size);
	cs->next = cache_head;

	if (!cache_head)
	{
		cache_head = cs;
		cs->prev = NULL;
	}
	else
	{
		cache_tail->next = cs;
		cs->prev = cache_tail;
	}

	cache_tail = cs;
	cs->next = NULL;

	strcpy (cs->name, name);
	cs->size = size;
	cs->user = c;

	c->data = (cs + 1);

	return c->data;
}
