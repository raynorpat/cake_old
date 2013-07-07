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
// sv_edict.c -- entity dictionary

#include "server.h"
#include "sv_world.h"
#include "crc.h"

dprograms_t		*progs;
dfunction_t		*pr_functions;
char			*pr_strings;
ddef_t			*pr_fielddefs;
ddef_t			*pr_globaldefs;
dstatement_t	*pr_statements;
globalvars_t	*pr_global_struct;
float			*pr_globals;			// same as pr_global_struct
int				pr_edict_size;	// in bytes

struct pr_ext_enabled_s	pr_ext_enabled;

#define NQ_PROGHEADER_CRC 5927

#ifdef WITH_NQPROGS
qbool pr_nqprogs;
int pr_fieldoffsetpatch[106];
int pr_globaloffsetpatch[62];
static int pr_globaloffsetpatch_nq[62] = {0,0,0,0,0,666,-4,-4,8,8,
8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8, 
8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8, 8,8};
#endif

int		type_size[8] = {1,sizeof(void *)/4,1,3,1,1,sizeof(void *)/4,sizeof(void *)/4};

func_t SpectatorConnect, SpectatorThink, SpectatorDisconnect;
func_t BotConnect, BotDisconnect, BotPreThink, BotPostThink;
func_t GE_ClientCommand, GE_ConsoleCommand, GE_PausedTic, GE_ShouldPause;

int		fofs_maxspeed, fofs_gravity, fofs_items2;
int		fofs_forwardmove, fofs_sidemove, fofs_upmove;
#ifdef VWEP_TEST
int		fofs_vw_index;
#endif
int		fofs_buttonX[8-3];


ddef_t *ED_FieldAtOfs (int ofs);
qbool ED_ParseEpair (edict_t *ent, ddef_t *key, const char *s);


/*
=================
ED_ClearEdict

Sets everything to NULL and mark as used
=================
*/
void ED_ClearEdict (edict_t *e)
{
	memset (&e->v, 0, progs->entityfields * 4);
	e->lastruntime = 0;
	e->inuse = true;
}

/*
=================
ED_Alloc

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
edict_t *ED_Alloc (void)
{
	int			i;
	edict_t		*e;

	for ( i=MAX_CLIENTS+1 ; i<sv.num_edicts ; i++)
	{
		e = EDICT_NUM(i);
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (!e->inuse && ( e->freetime < 2 || sv.time - e->freetime > 0.5 ) )
		{
			ED_ClearEdict (e);
			return e;
		}
	}
	
	if (i == MAX_EDICTS)
	{
		Com_Printf ("WARNING: ED_Alloc: no free edicts\n");
		i--;	// step on whatever is the last edict
		e = EDICT_NUM(i);
		SV_UnlinkEdict(e);
	}
	else
		sv.num_edicts++;
	e = EDICT_NUM(i);
	ED_ClearEdict (e);

	return e;
}

/*
=================
ED_Free

Marks the edict as free
FIXME: walk all entities and NULL out references to this entity
=================
*/
void ED_Free (edict_t *ed)
{
	SV_UnlinkEdict (ed);		// unlink from world bsp

	ed->inuse = false;
	ed->v.model = 0;
	ed->v.takedamage = 0;
	ed->v.modelindex = 0;
	ed->v.colormap = 0;
	ed->v.skin = 0;
	ed->v.frame = 0;
	VectorClear (ed->v.origin);
	VectorClear (ed->v.angles);
	ed->v.nextthink = -1;
	ed->v.solid = 0;
	
	ed->freetime = sv.time;
}

//===========================================================================

/*
============
ED_GlobalAtOfs
============
*/
ddef_t *ED_GlobalAtOfs (int ofs)
{
	ddef_t		*def;
	int			i;
	
	for (i=0 ; i<progs->numglobaldefs ; i++)
	{
		def = &pr_globaldefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

/*
============
ED_FieldAtOfs
============
*/
ddef_t *ED_FieldAtOfs (int ofs)
{
	ddef_t		*def;
	int			i;
	
	for (i=0 ; i<progs->numfielddefs ; i++)
	{
		def = &pr_fielddefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

/*
============
ED_FindField
============
*/
ddef_t *ED_FindField (const char *name)
{
	ddef_t		*def;
	int			i;
	
	for (i=0 ; i<progs->numfielddefs ; i++)
	{
		def = &pr_fielddefs[i];
		if (!strcmp(PR_GetString(def->s_name),name) )
			return def;
	}
	return NULL;
}


/*
============
ED_FindGlobal
============
*/
ddef_t *ED_FindGlobal (char *name)
{
	ddef_t		*def;
	int			i;
	
	for (i=0 ; i<progs->numglobaldefs ; i++)
	{
		def = &pr_globaldefs[i];
		if (!strcmp(PR_GetString(def->s_name),name) )
			return def;
	}
	return NULL;
}


/*
============
ED_FindFunction
============
*/
dfunction_t *ED_FindFunction (const char *name)
{
	dfunction_t		*func;
	int				i;
	
	for (i=0 ; i<progs->numfunctions ; i++)
	{
		func = &pr_functions[i];
		if (!strcmp(PR_GetString(func->s_name),name) )
			return func;
	}
	return NULL;
}

func_t ED_FindFunctionOffset (char *name)
{
	dfunction_t *func;

	func = ED_FindFunction (name);
	return func ? (func_t)(func - pr_functions) : 0;
}

int ED_FindFieldOffset (char *field)
{
	ddef_t *d;
	d = ED_FindField(field);
	if (!d)
		return 0;
	return d->ofs*4;
}


/*
============
PR_ValueString

Returns a string describing *data in a type specific manner
=============
*/
char *PR_ValueString (etype_t type, eval_t *val)
{
	static char	line[1024];
	ddef_t		*def;
	dfunction_t	*f;
	int n;
	
	type &= ~DEF_SAVEGLOBAL;

	switch (type)
	{
	case ev_string:
		strlcpy (line, PR_GetString (val->string), sizeof (line));
		break;
	case ev_entity:	
		n = val->edict;
		Q_snprintf (line, sizeof (line), "entity %i", n);
		break;
	case ev_function:
		f = pr_functions + val->function;
		Q_snprintf (line, sizeof (line), "%s()", PR_GetString(f->s_name));
		break;
	case ev_field:
		def = ED_FieldAtOfs ( val->_int );
		Q_snprintf (line, sizeof (line), ".%s", PR_GetString(def->s_name));
		break;
	case ev_void:
		Q_snprintf (line, sizeof (line), "void");
		break;
	case ev_float:
		Q_snprintf (line, sizeof (line), "%10.4f", val->_float);
		break;
	case ev_vector:
		Q_snprintf (line, sizeof (line), "'%10.4f %10.4f %10.4f'", val->vector[0], val->vector[1], val->vector[2]);
		break;
	case ev_pointer:
		Q_snprintf (line, sizeof (line), "pointer");
		break;
	default:
		Q_snprintf (line, sizeof (line), "bad type %i", type);
		break;
	}
	
	return line;
}

/*
============
PR_UglyValueString

Returns a string describing *data in a type specific manner
Easier to parse than PR_ValueString
=============
*/
char *PR_UglyValueString (etype_t type, eval_t *val)
{
	static char line[4096];
	int i;
	char *s;
	ddef_t		*def;
	dfunction_t	*f;
	
	type &= ~DEF_SAVEGLOBAL;

	switch (type)
	{
	case ev_string:
		// Parse the string a bit to turn special characters
		// (like newline, specifically) into escape codes,
		// this fixes saving games from various mods
		s = PR_GetString (val->string);
		for (i = 0;i < (int)sizeof(line) - 2 && *s;)
		{
			if (*s == '\n')
			{
				line[i++] = '\\';
				line[i++] = 'n';
			}
			else if (*s == '\r')
			{
				line[i++] = '\\';
				line[i++] = 'r';
			}
			else
				line[i++] = *s;
			s++;
		}
		line[i] = '\0';
		break;
	case ev_entity:	
		Q_snprintf (line, sizeof (line), "%i", NUM_FOR_EDICT(PROG_TO_EDICT(val->edict)));
		break;
	case ev_function:
		f = pr_functions + val->function;
		strlcpy (line, PR_GetString (f->s_name), sizeof (line));
		break;
	case ev_field:
		def = ED_FieldAtOfs ( val->_int );
		Q_snprintf (line, sizeof (line), ".%s", PR_GetString(def->s_name));
		break;
	case ev_void:
		Q_snprintf (line, sizeof (line), "void");
		break;
	case ev_float:
		Q_snprintf (line, sizeof (line), "%f", val->_float);
		break;
	case ev_vector:
		Q_snprintf (line, sizeof (line), "%f %f %f", val->vector[0], val->vector[1], val->vector[2]);
		break;
	default:
		Q_snprintf (line, sizeof (line), "bad type %i", type);
		break;
	}
	
	return line;
}

/*
============
PR_GlobalString

Returns a string with a description and the contents of a global,
padded to 20 field width
============
*/
char *PR_GlobalString (int ofs)
{
	char	*s;
	int		i;
	ddef_t	*def;
	void	*val;
	static char	line[128];
	
	val = (void *)&pr_globals[ofs];
	def = ED_GlobalAtOfs(ofs);
	if (!def)
		Q_snprintf (line, sizeof (line), "%i(?)", ofs);
	else
	{
		s = PR_ValueString (def->type, val);
		Q_snprintf (line, sizeof (line), "%i(%s)%s", ofs, PR_GetString(def->s_name), s);
	}
	
	i = strlen(line);
	for ( ; i<20 ; i++)
		strlcat (line, " ", sizeof (line));
	strlcat (line, " ", sizeof (line));
		
	return line;
}

char *PR_GlobalStringNoContents (int ofs)
{
	int		i;
	ddef_t	*def;
	static char	line[128];
	
	def = ED_GlobalAtOfs(ofs);
	if (!def)
		Q_snprintf (line, sizeof (line), "%i(?)", ofs);
	else
		Q_snprintf (line, sizeof (line), "%i(%s)", ofs, PR_GetString(def->s_name));
	
	i = strlen(line);
	for ( ; i<20 ; i++)
		strlcat (line, " ", sizeof (line));
	strlcat (line, " ", sizeof (line));
		
	return line;
}


/*
=============
ED_Print

For debugging
=============
*/
void ED_Print (edict_t *ed)
{
	int		l;
	ddef_t	*d;
	int		*v;
	int		i, j;
	char	*name;
	int		type;

	if (!ed->inuse)
	{
		Com_Printf ("FREE\n");
		return;
	}
	
	for (i=1 ; i<progs->numfielddefs ; i++)
	{
		d = &pr_fielddefs[i];
		name = PR_GetString(d->s_name);
		if (name[strlen(name)-2] == '_')
			continue;	// skip _x, _y, _z vars
			
		v = (int *)((char *)&ed->v + d->ofs*4);

	// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;
		
		for (j=0 ; j<type_size[type] ; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;
	
		Com_Printf ("%s",name);
		l = strlen (name);
		while (l++ < 15)
			Com_Printf (" ");

		Com_Printf ("%s\n", PR_ValueString(d->type, (eval_t *)v));		
	}
}

/*
=============
ED_Write

For savegames
=============
*/
void ED_Write (qfile_t *f, edict_t *ed)
{
	ddef_t	*d;
	int		*v;
	int		i, j;
	char	*name;
	int		type;

	FS_Print (f, "{\n");

	if (!ed->inuse)
	{
		FS_Printf (f, "}\n");
		return;
	}
	
	for (i=1 ; i<progs->numfielddefs ; i++)
	{
		d = &pr_fielddefs[i];
		name = PR_GetString(d->s_name);
		if (name[strlen(name)-2] == '_')
			continue;	// skip _x, _y, _z vars
			
		v = (int *)((char *)&ed->v + d->ofs*4);

	// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;
		for (j=0 ; j<type_size[type] ; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;
	
		FS_Printf (f,"\"%s\" ",name);
		FS_Printf (f,"\"%s\"\n", PR_UglyValueString(d->type, (eval_t *)v));		
	}

	FS_Print (f, "}\n");
}

void ED_PrintNum (int ent)
{
	ED_Print (EDICT_NUM(ent));
}

/*
=============
ED_PrintEdicts_f

For debugging, prints all the entities in the current server
=============
*/
void ED_PrintEdicts_f (void)
{
	int		i;

	if (sv.state != ss_game)
		return;

	Com_Printf ("%i entities\n", sv.num_edicts);
	for (i=0 ; i<sv.num_edicts ; i++)
	{
		if (!EDICT_NUM(i)->inuse)
			continue;
		Com_Printf ("\nEDICT %i:\n",i);
		ED_PrintNum (i);
	}
}

/*
=============
ED_PrintEdict_f

For debugging, prints a single edict
=============
*/
void ED_PrintEdict_f (void)
{
	int		i;
	
	if (sv.state != ss_game)
		return;

	i = Q_atoi (Cmd_Argv(1));
	Com_Printf ("\n EDICT %i:\n",i);
	ED_PrintNum (i);
}

/*
=============
ED_EdictCount_f

For debugging
=============
*/
void ED_EdictCount_f (void)
{
	int		i;
	edict_t	*ent;
	int		active, models, solid, step;

	if (sv.state != ss_game)
		return;

	active = models = solid = step = 0;
	for (i=0 ; i<sv.num_edicts ; i++)
	{
		ent = EDICT_NUM(i);
		if (!ent->inuse)
			continue;
		active++;
		if (ent->v.solid)
			solid++;
		if (ent->v.model)
			models++;
		if (ent->v.movetype == MOVETYPE_STEP)
			step++;
	}

	Com_Printf ("num_edicts:%3i\n", sv.num_edicts);
	Com_Printf ("active    :%3i\n", active);
	Com_Printf ("view      :%3i\n", models);
	Com_Printf ("touch     :%3i\n", solid);
	Com_Printf ("step      :%3i\n", step);
}

/*
==============================================================================

					ARCHIVING GLOBALS

FIXME: need to tag constants, doesn't really work
==============================================================================
*/

/*
=============
ED_WriteGlobals
=============
*/
void ED_WriteGlobals (qfile_t *f)
{
	ddef_t		*def;
	int			i;
	char		*name;
	int			type;

	FS_Print (f,"{\n");
	for (i=0 ; i<progs->numglobaldefs ; i++)
	{
		def = &pr_globaldefs[i];
		type = def->type;
		if ( !(def->type & DEF_SAVEGLOBAL) )
			continue;
		type &= ~DEF_SAVEGLOBAL;

		if (type != ev_string && type != ev_float && type != ev_entity)
			continue;

		name = PR_GetString(def->s_name);
		FS_Printf (f,"\"%s\" ", name);
		FS_Printf (f,"\"%s\"\n", PR_UglyValueString(type, (eval_t *)&pr_globals[def->ofs]));		
	}
	FS_Print (f,"}\n");
}

/*
=============
ED_ParseGlobals
=============
*/
void ED_ParseGlobals (const char *data)
{
	char keyname[1024];
	ddef_t *key;

	while (1)
	{
		// parse key
		if (!COM_ParseToken(&data, false))
			Host_Error ("ED_ParseEntity: EOF without closing brace");
		if (com_token[0] == '}')
			break;

		strcpy (keyname, com_token);

		// parse value
		if (!COM_ParseToken(&data, false))
			Host_Error ("ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			Host_Error ("ED_ParseEntity: closing brace without data");

		key = ED_FindGlobal (keyname);
		if (!key)
		{
			Com_DPrintf("'%s' is not a global\n", keyname);
			continue;
		}

		if (!ED_ParseEpair(NULL, key, com_token))
			Host_Error ("ED_ParseGlobals: parse error");
	}
}

//============================================================================


/*
=============
ED_NewString
=============
*/
char *ED_NewString (const char *string)
{
	char	*new, *new_p;
	int		i,l;
	
	l = strlen(string) + 1;
	new = Hunk_Alloc (l);
	new_p = new;

	for (i=0 ; i< l ; i++)
	{
		if (string[i] == '\\' && i < l-1)
		{
			i++;
			if (string[i] == 'n')
				*new_p++ = '\n';
			else if (string[i] == 'r')
				*new_p++ = '\r';
			else
				*new_p++ = '\\';
		}
		else
			*new_p++ = string[i];
	}
	
	return new;
}


/*
=============
ED_ParseEval

Can parse either fields or globals
returns false if error
=============
*/
qbool ED_ParseEpair (edict_t *ent, ddef_t *key, const char *s)
{
	int i;
	ddef_t *def;
	eval_t *val;
	dfunction_t *func;

	if (ent)
		val = (eval_t *)((int *)&ent->v + key->ofs);
	else
		val = (eval_t *)((int *)pr_globals + key->ofs);

	switch (key->type & ~DEF_SAVEGLOBAL)
	{
	case ev_string:
		val->string = PR_SetString(ED_NewString(s));
		break;

	case ev_float:
		while (*s && *s <= ' ')
			s++;
		val->_float = atof(s);
		break;

	case ev_vector:
		for (i = 0;i < 3;i++)
		{
			while (*s && *s <= ' ')
				s++;
			if (*s)
				val->vector[i] = atof(s);
			else
				val->vector[i] = 0;
			while (*s > ' ')
				s++;
		}
		break;

	case ev_entity:
		while (*s && *s <= ' ')
			s++;
		i = atoi(s);
		val->edict = EDICT_TO_PROG(EDICT_NUM(i));
		break;
		
	case ev_field:
		def = ED_FindField(s);
		if (!def)
		{
			Com_DPrintf("ED_ParseEpair: Can't find field %s\n", s);
			return false;
		}
		val->_int = def->ofs;
		break;

	case ev_function:
		func = ED_FindFunction(s);
		if (!func)
		{
			Com_Printf("ED_ParseEpair: Can't find function %s\n", s);
			return false;
		}
		val->function = func - pr_functions;
		break;

	default:
		Com_Printf("ED_ParseEpair: Unknown key->type %i for key \"%s\"\n", key->type, PR_GetString(key->s_name));
		return false;
	}
	return true;
}

/*
====================
ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
Used for initial level load and for savegames.
====================
*/
const char *ED_ParseEdict (const char *data, edict_t *ent)
{
	ddef_t	*key;
	qbool	anglehack;
	qbool	init;
	char	keyname[256];
	int n;

	init = false;

// clear it
	if (ent != sv.edicts)	// hack
		memset (&ent->v, 0, progs->entityfields * 4);

// go through all the dictionary pairs
	while (1)
	{	
		// parse key
		if (!COM_ParseToken(&data, false))
			Host_Error ("ED_ParseEntity: EOF without closing brace");
		if (com_token[0] == '}')
			break;
		
		// anglehack is to allow QuakeEd to write single scalar angles
		// and allow them to be turned into vectors. (FIXME...)
		anglehack = !strcmp (com_token, "angle");
		if (anglehack)
			strlcpy (com_token, "angles", sizeof (com_token));

		// FIXME: change light to _light to get rid of this hack
		if (!strcmp(com_token, "light"))
			strlcpy (com_token, "light_lev", sizeof (com_token));	// hack for single light def

		strlcpy (keyname, com_token, sizeof (keyname));

		// another hack to fix keynames with trailing spaces
		n = strlen(keyname);
		while (n && keyname[n-1] == ' ')
		{
			keyname[n-1] = 0;
			n--;
		}
		
	// parse value	
		if (!COM_ParseToken(&data, false))
			Host_Error ("ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			Host_Error ("ED_ParseEntity: closing brace without data");

		init = true;	

// keynames with a leading underscore are used for utility comments,
// and are immediately discarded by quake
		if (keyname[0] == '_')
			continue;
		
		key = ED_FindField (keyname);
		if (!key)
		{
			Com_DPrintf("'%s' is not a field\n", keyname);
			continue;
		}

		if (anglehack)
		{
			char	temp[32];
			strlcpy (temp, com_token, sizeof (temp));
			Q_snprintf (com_token, sizeof (com_token), "0 %s 0", temp);
		}

		if (!ED_ParseEpair(ent, key, com_token))
			Host_Error ("ED_ParseEdict: parse error");
	}

	if (!init)
		ent->inuse = false;

	return data;
}


/*
================
ED_LoadFromFile

The entities are directly placed in the array, rather than allocated with
ED_Alloc, because otherwise an error loading the map would have entity
number references out of order.

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.

Used for both fresh maps and savegame loads.  A fresh map would also need
to call ED_CallSpawnFunctions () to let the objects initialize themselves.
================
*/
void ED_LoadFromFile (const char *data)
{	
	edict_t		*ent;
	int parsed, inhibited, spawned, died;
	dfunction_t	*func;
	
	ent = NULL;
	parsed = 0;
	inhibited = 0;
	spawned = 0;
	died = 0;
	pr_global_struct->time = sv.time;
	
// parse ents
	while (1)
	{
// parse the opening brace	
		if (!COM_ParseToken(&data, false))
			break;
		if (com_token[0] != '{')
			Host_Error ("ED_LoadFromFile: found %s when expecting {",com_token);

		if (!ent)
			ent = EDICT_NUM(0);
		else
			ent = ED_Alloc ();
		data = ED_ParseEdict (data, ent);
		parsed++;
		
// remove things from different skill levels or deathmatch
		if (deathmatch.value)
		{
			if (((int)ent->v.spawnflags & SPAWNFLAG_NOT_DEATHMATCH))
			{
				ED_Free (ent);
				inhibited++;
				continue;
			}
		}
		else if ((current_skill == 0 && ((int)ent->v.spawnflags & SPAWNFLAG_NOT_EASY))
				|| (current_skill == 1 && ((int)ent->v.spawnflags & SPAWNFLAG_NOT_MEDIUM))
				|| (current_skill >= 2 && ((int)ent->v.spawnflags & SPAWNFLAG_NOT_HARD)) )
		{
			ED_Free (ent);
			inhibited++;
			continue;
		}

//
// immediately call spawn function
//
		if (!ent->v.classname)
		{
			Com_Printf ("No classname for:\n");
			ED_Print (ent);
			ED_Free (ent);
			continue;
		}
		
	// look for the spawn function
		func = ED_FindFunction (PR_GetString(ent->v.classname));

		if (!func)
		{
			if (developer.integer) // don't confuse non-developers with errors
			{
				Com_Printf ("No spawn function for:\n");
				ED_Print (ent);
			}
			ED_Free (ent);
			continue;
		}

		pr_global_struct->self = EDICT_TO_PROG(ent);
		PR_ExecuteProgram (func - pr_functions);
		spawned++;
		SV_FlushSignon();
	}	

	Com_DPrintf("%i entities parsed, %i inhibited, %i spawned (%i removed self, %i stayed)\n", parsed, inhibited, spawned, died, spawned - died);
}


/*
===============
PR_LoadProgs
===============
*/
void PR_LoadProgs (void)
{
	int	i;
	char num[32];
	qbool check;
	static int lumpsize[6] = { sizeof(dstatement_t), sizeof(ddef_t), sizeof(ddef_t), sizeof(dfunction_t), 4, 4 };
	fs_offset_t	filesize = 0;
	char	*progsname;

	progs = NULL;

	// decide whether to load qwprogs.dat, progs.dat or spprogs.dat
	check = FS_FileExists ("progs.dat");
	if (check) {
		progs = (dprograms_t *)FS_LoadFile ("progs.dat", false, &filesize);
		progsname = "progs.dat";
		pr_nqprogs = true;
	} else {
		pr_nqprogs = false;
		check = FS_FileExists ("spprogs.dat");
		if (check) {
			progs = (dprograms_t *)FS_LoadFile ("spprogs.dat", false, &filesize);
			progsname = "spprogs.dat";
		} else {
			progs = (dprograms_t *) FS_LoadFile ("qwprogs.dat", false, &filesize);
			progsname = "qwprogs.dat";
		}
	}

	if (!progs)
		Host_Error ("PR_LoadProgs: couldn't load progs.dat");

	if (filesize < (int)sizeof(*progs))
		Host_Error("%s is corrupt", progsname);

	Com_DPrintf ("Using %s (%iK).\n", progsname, filesize / 1024);

// add prog crc to the serverinfo
	sprintf (num, "%i", CRC_Block ((byte *)progs, filesize));
	Info_SetValueForStarKey (svs.info, "*progs", num, MAX_SERVERINFO_STRING);

// byte swap the header
	for (i = 0; i < sizeof(*progs)/4; i++)
		((int *)progs)[i] = LittleLong ( ((int *)progs)[i] );		

	if (progs->version != PROG_VERSION)
		Host_Error ("progs.dat has wrong version number (%i should be %i)", progs->version, PROG_VERSION);
	if (progs->crc != (pr_nqprogs ? NQ_PROGHEADER_CRC : PROGHEADER_CRC))
	Host_Error ("progs.dat system vars have been modified, progdefs.h is out of date");

// check lump offsets and sizes
	for (i = 0; i < 6; i ++) {
		if (((int *)progs)[i*2 + 2] < sizeof(*progs)
			|| ((int *)progs)[i*2 + 2] + ((int *)progs)[i*2 + 3]*lumpsize[i] > filesize)
		Host_Error("progs.dat is corrupt");
	}

	pr_functions = (dfunction_t *)((byte *)progs + progs->ofs_functions);
	pr_strings = (char *)progs + progs->ofs_strings;
	pr_globaldefs = (ddef_t *)((byte *)progs + progs->ofs_globaldefs);
	pr_fielddefs = (ddef_t *)((byte *)progs + progs->ofs_fielddefs);
	pr_statements = (dstatement_t *)((byte *)progs + progs->ofs_statements);
	pr_global_struct = (globalvars_t *)((byte *)progs + progs->ofs_globals);

	pr_globals = (float *)pr_global_struct;

	PR_InitStrings ();
	
	pr_edict_size = progs->entityfields * 4 + sizeof (edict_t) - sizeof(entvars_t);
	
// byte swap the lumps
	for (i=0 ; i<progs->numstatements ; i++)
	{
		pr_statements[i].op = LittleShort(pr_statements[i].op);
		pr_statements[i].a = LittleShort(pr_statements[i].a);
		pr_statements[i].b = LittleShort(pr_statements[i].b);
		pr_statements[i].c = LittleShort(pr_statements[i].c);
	}

	for (i=0 ; i<progs->numfunctions; i++)
	{
		pr_functions[i].first_statement = LittleLong (pr_functions[i].first_statement);
		pr_functions[i].parm_start = LittleLong (pr_functions[i].parm_start);
		pr_functions[i].s_name = LittleLong (pr_functions[i].s_name);
		pr_functions[i].s_file = LittleLong (pr_functions[i].s_file);
		pr_functions[i].numparms = LittleLong (pr_functions[i].numparms);
		pr_functions[i].locals = LittleLong (pr_functions[i].locals);
	}	

	for (i=0 ; i<progs->numglobaldefs ; i++)
	{
		pr_globaldefs[i].type = LittleShort (pr_globaldefs[i].type);
		pr_globaldefs[i].ofs = LittleShort (pr_globaldefs[i].ofs);
		pr_globaldefs[i].s_name = LittleLong (pr_globaldefs[i].s_name);
	}

	for (i=0 ; i<progs->numfielddefs ; i++)
	{
		pr_fielddefs[i].type = LittleShort (pr_fielddefs[i].type);
		if (pr_fielddefs[i].type & DEF_SAVEGLOBAL)
			Host_Error ("PR_LoadProgs: pr_fielddefs[i].type & DEF_SAVEGLOBAL");
		pr_fielddefs[i].ofs = LittleShort (pr_fielddefs[i].ofs);
		pr_fielddefs[i].s_name = LittleLong (pr_fielddefs[i].s_name);
	}

	for (i=0 ; i<progs->numglobals ; i++)
		((int *)pr_globals)[i] = LittleLong (((int *)pr_globals)[i]);


#ifdef WITH_NQPROGS
	if (pr_nqprogs) {
		memcpy (&pr_globaloffsetpatch, &pr_globaloffsetpatch_nq, sizeof(pr_globaloffsetpatch));
		for (i = 0; i < 106; i++) {
			pr_fieldoffsetpatch[i] = (i < 8) ? i : (i < 25) ? i + 1 :
				(i < 28) ? i + (102 - 25) : (i < 73) ? i - 2 :
				(i < 74) ? i + (105 - 73) : (i < 105) ? i - 3 : /* (i == 105) */ 8;
		}

		for (i=0 ; i<progs->numfielddefs ; i++)
			pr_fielddefs[i].ofs = PR_FIELDOFS(pr_fielddefs[i].ofs);

	}
	else
	{
		memset (&pr_globaloffsetpatch, sizeof(pr_globaloffsetpatch), 0);

		for (i = 0; i < 106; i++)
			pr_fieldoffsetpatch[i] = i;
	}
#endif

	// find optional QC-exported functions
	SpectatorConnect = ED_FindFunctionOffset ("SpectatorConnect");
	SpectatorThink = ED_FindFunctionOffset ("SpectatorThink");
	SpectatorDisconnect = ED_FindFunctionOffset ("SpectatorDisconnect");
	BotConnect = ED_FindFunctionOffset ("BotConnect");
	BotDisconnect = ED_FindFunctionOffset ("BotDisconnect");
	BotPreThink = ED_FindFunctionOffset ("BotPreThink");
	BotPostThink = ED_FindFunctionOffset ("BotPostThink");
	GE_ClientCommand = ED_FindFunctionOffset ("GE_ClientCommand");
	GE_ConsoleCommand = ED_FindFunctionOffset ("GE_ConsoleCommand");
	GE_PausedTic = ED_FindFunctionOffset ("GE_PausedTic");
	GE_ShouldPause = ED_FindFunctionOffset ("GE_ShouldPause");

	// find optional QC-exported fields
	fofs_maxspeed = ED_FindFieldOffset ("maxspeed");
	fofs_gravity = ED_FindFieldOffset ("gravity");
	fofs_items2 = ED_FindFieldOffset ("items2");
	fofs_forwardmove = ED_FindFieldOffset ("forwardmove");
	fofs_sidemove = ED_FindFieldOffset ("sidemove");
	fofs_upmove = ED_FindFieldOffset ("upmove");
#ifdef VWEP_TEST
	fofs_vw_index = ED_FindFieldOffset ("vw_index");
#endif
	for (i = 3; i < 8; i++)
		fofs_buttonX[i-3] = ED_FindFieldOffset(va("button%i", i));

	// reset stuff like ZQ_CLIENTCOMMAND, progs must enable it explicitly
	memset (&pr_ext_enabled, sizeof(pr_ext_enabled), 0);
}


/*
===============
PR_Init
===============
*/
void PR_Init (void)
{
	PR_InitBuiltins ();

	Cmd_AddCommand ("edict", ED_PrintEdict_f);
	Cmd_AddCommand ("edicts", ED_PrintEdicts_f);
	Cmd_AddCommand ("edictcount", ED_EdictCount_f);
	Cmd_AddCommand ("profile", PR_Profile_f);
}



edict_t *EDICT_NUM(int n)
{
	if (n < 0 || n >= MAX_EDICTS)
		Host_Error ("EDICT_NUM: bad number %i", n);
	return (edict_t *)((byte *)sv.edicts+ (n)*pr_edict_size);
}

int NUM_FOR_EDICT(edict_t *e)
{
	int		b;
	
	b = (byte *)e - (byte *)sv.edicts;
	b = b / pr_edict_size;
	
	if (b < 0 || b >= sv.num_edicts)
		Host_Error ("NUM_FOR_EDICT: bad pointer");
	return b;
}
