/*
===============================================================================

LOAD / SAVE GAME

===============================================================================
*/

#include "server.h"
#include "sv_world.h"

// hmm...
extern int CL_Stat_Monsters(void), CL_Stat_TotalMonsters(void);
extern int CL_IntermissionRunning (void);

#define	SAVEGAME_COMMENT_LENGTH	39
#define	SAVEGAME_VERSION	6

/*
===============
SV_SaveGame_f
===============
*/
void SV_SaveGame_f (void)
{
	char	name[256];
	qfile_t	*f;
	int		i;
	char	comment[SAVEGAME_COMMENT_LENGTH+1];

	if (Cmd_Argc() != 2) {
		Com_Printf ("save <savename> : save a game\n");
		return;
	}

	if (strstr(Cmd_Argv(1), "..")) {
		Com_Printf ("Relative pathnames are not allowed.\n");
		return;
	}

	if (sv.state != ss_game) {
		Com_Printf ("Not playing a local game.\n");
		return;
	}

	if (CL_IntermissionRunning()) {
		Com_Printf ("Can't save in intermission.\n");
		return;
	}

	if (deathmatch.value != 0 || coop.value != 0 || maxclients.value != 1) {
		Com_Printf ("Can't save multiplayer games.\n");
		return;
	}

	for (i = 1; i < MAX_CLIENTS; i++) {
		if (svs.clients[i].state == cs_spawned)
		{
			Com_Printf ("Can't save multiplayer games.\n");
			return;
		}
	}	
	
	if (svs.clients[0].state != cs_spawned) {
		Com_Printf ("Can't save, client #0 not spawned.\n");
		return;
	}

	strlcpy (name, Cmd_Argv(1), sizeof (name));
	FS_DefaultExtension (name, ".sav", sizeof(name));
	
	Com_Printf ("Saving game to %s...\n", name);
	f = FS_Open (name, "wb", false, false);
	if (!f)
		return;
	
	FS_Printf (f, "%i\n", SAVEGAME_VERSION);
	
	memset(comment, 0, sizeof(comment));
	sprintf(comment, "%-21.21s kills:%3i/%3i", PR_GetString(sv.edicts->v.message), CL_Stat_Monsters(), CL_Stat_TotalMonsters());
	// convert space to _ to make stdio happy
	// convert control characters to _ as well
	for (i=0 ; i<SAVEGAME_COMMENT_LENGTH ; i++)
		if (comment[i] <= ' ')
			comment[i] = '_';
	comment[SAVEGAME_COMMENT_LENGTH] = '\0';

	FS_Printf (f, "%s\n", comment);
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		FS_Printf (f, "%f\n", svs.clients->spawn_parms[i]);
	FS_Printf (f, "%d\n", current_skill);
	FS_Printf (f, "%s\n", sv.mapname);
	FS_Printf (f, "%f\n", sv.time);

	// write the light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		if (sv.lightstyles[i])
			FS_Printf (f, "%s\n", sv.lightstyles[i]);
		else
			FS_Printf (f,"m\n");
	}

	ED_WriteGlobals (f);
	for (i=0 ; i<sv.num_edicts ; i++)
	{
		ED_Write (f, EDICT_NUM(i));
	}

	FS_Close (f);
	Com_Printf ("done.\n");
}


/*
===============
SV_LoadGame_f
===============
*/
void SV_LoadGame_f (void)
{
	char	name[MAX_OSPATH];
	char	mapname[MAX_QPATH];
	float	time;
	const char *start;
	const char *t;
	char	*text;
	edict_t	*ent;
	int		i;
	int		entnum;
	int		version;
	float	spawn_parms[NUM_SPAWN_PARMS];

	if (Cmd_Argc() != 2) {
		Com_Printf ("load <savename> : load a game\n");
		return;
	}

	strlcpy (name, Cmd_Argv(1), sizeof(name));
	FS_DefaultExtension (name, ".sav", sizeof(name));
	
// we can't call SCR_BeginLoadingPlaque, because too much stack space has
// been used.  The menu calls it before stuffing loadgame command
//	SCR_BeginLoadingPlaque ();

	Com_Printf ("Loading game from %s...\n", name);
	t = text = FS_LoadFile (name, false, NULL);
	if (!text)
	{
		Com_Printf("ERROR: couldn't open.\n");
		return;
	}

	// version
	COM_ParseTokenConsole(&t);
	version = atoi(com_token);
	if (version != SAVEGAME_VERSION)
	{
		free(text);
		Com_Printf ("Savegame is version %i, not %i\n", version, SAVEGAME_VERSION);
		return;
	}

	// description
	// this is a little hard to parse, as : is a separator in COM_ParseToken,
	// so use the console parser instead
	COM_ParseTokenConsole(&t);

	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
	{
		COM_ParseTokenConsole(&t);
		spawn_parms[i] = atof(com_token);
	}

	// skill
	COM_ParseTokenConsole(&t);
	// this silliness is so we can load 1.06 save files, which have float skill values
	current_skill = (int)(atof(com_token) + 0.5);
	Cvar_Set (&skill, va("%i", current_skill));

	// mapname
	COM_ParseTokenConsole(&t);
	strcpy (mapname, com_token);

	// time
	COM_ParseTokenConsole(&t);
	time = atof(com_token);

	Host_EndGame ();

	Cvar_SetValue (&deathmatch, 0);
	Cvar_SetValue (&coop, 0);
	Cvar_SetValue (&teamplay, 0);
	Cvar_SetValue (&maxclients, 1);

	CL_BeginLocalConnection ();

	SV_SpawnServer (mapname, false);
	if (sv.state != ss_game)
	{
		free(text);
		Com_Printf ("Couldn't load map\n");
		return;
	}
	Cvar_ForceSet (&sv_paused, "1");	// pause until all clients connect
	sv.loadgame = true;

	// load the light styles
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		COM_ParseTokenConsole(&t);
		sv.lightstyles[i] = Hunk_Alloc (strlen(com_token) + 1);
		strcpy (sv.lightstyles[i], com_token);
	}

	// load the edicts out of the savegame file
	entnum = -1;		// -1 is the globals
	for (;;)
	{
		start = t;
		while (COM_ParseTokenConsole(&t))
			if (!strcmp(com_token, "}"))
				break;

		if (!COM_ParseTokenConsole(&start))
		{
			// end of file
			break;
		}
		if (strcmp(com_token, "{"))
		{
			free(text);
			Host_Error ("First token isn't a brace");
		}
			
		if (entnum == -1)
		{
			// parse the global vars
			ED_ParseGlobals (start);
		}
		else
		{
			// parse an edict
			ent = EDICT_NUM(entnum);
			memset (&ent->v, 0, progs->entityfields * 4);
			ent->inuse = true;
			ED_ParseEdict (start, ent);
	
			// link it into the bsp tree
			if (ent->inuse)
				SV_LinkEdict (ent, false);
		}

		entnum++;
	}
	
	sv.num_edicts = entnum;
	sv.time = time;

//	FS_Close (f);

	// FIXME: this assumes the player is using client slot #0
	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		svs.clients->spawn_parms[i] = spawn_parms[i];
}
