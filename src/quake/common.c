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
// common.c -- misc functions used in client and server

#include "common.h"
#include "crc.h"
#include "mdfour.h"

#define MAX_NUM_ARGVS	50

usercmd_t nullcmd; // guaranteed to be zero

static char	*largv[MAX_NUM_ARGVS + 1];

cvar_t	developer = {"developer","0"};
cvar_t	registered = {"registered","0"};

qbool	com_serveractive = false;

gamemode_t gamemode;
const char *gamename;
const char *gamedirname1;
const char *gamedirname2;
const char *gamescreenshotname;
const char *gameuserdirname;
char com_modname[MAX_OSPATH] = "";

//===========================================================================

// Game mods

typedef struct gamemode_info_s
{
	const char* prog_name;
	const char* cmdline;
	const char* gamename;
	const char* gamedirname1;
	const char* gamedirname2;
	const char* gamescreenshotname;
	const char* gameuserdirname;
} gamemode_info_t;

static const gamemode_info_t gamemode_info [] =
{// prog_name		cmdline			gamename		gamedirname		gamedirname2	gamescreenshotname	gameuserdirname

// GAME_NORMAL
// COMMANDLINEOPTION: Game: -quake runs the game Quake (default)
{ "",				"-quake",		"Quake",		"id1",		NULL,			"q",			"cake" },
// GAME_HIPNOTIC
// COMMANDLINEOPTION: Game: -hipnotic runs Quake mission pack 1: The Scourge of Armagon
{ "hipnotic",		"-hipnotic",	"Hipnotic",		"id1",		"hipnotic",		"q",			"cake" },
// GAME_ROGUE
// COMMANDLINEOPTION: Game: -rogue runs Quake mission pack 2: The Dissolution of Eternity
{ "rogue",			"-rogue",		"Rogue",		"id1",		"rogue",		"q",			"cake" },
};

void COM_InitGameType (void)
{
	char name [MAX_OSPATH];
	unsigned int i;

	FS_StripExtension (com_argv[0], name, sizeof (name));
	COM_ToLowerString (name, name, sizeof (name));

	// Check the binary name; default to GAME_NORMAL (0)
	gamemode = GAME_NORMAL;
	for (i = 1; i < sizeof (gamemode_info) / sizeof (gamemode_info[0]); i++)
		if (strstr (name, gamemode_info[i].prog_name))
		{
			gamemode = (gamemode_t)i;
			break;
		}

	// Look for a command-line option
	for (i = 0; i < sizeof (gamemode_info) / sizeof (gamemode_info[0]); i++)
		if (COM_CheckParm ((char *)gamemode_info[i].cmdline))
		{
			gamemode = (gamemode_t)i;
			break;
		}

	gamename = gamemode_info[gamemode].gamename;
	gamedirname1 = gamemode_info[gamemode].gamedirname1;
	gamedirname2 = gamemode_info[gamemode].gamedirname2;
	gamescreenshotname = gamemode_info[gamemode].gamescreenshotname;
	gameuserdirname = gamemode_info[gamemode].gameuserdirname;
}

//============================================================================

#define MAX_COM_TOKEN	1024

char	com_token[MAX_COM_TOKEN];
int		com_argc;
char	**com_argv;

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse (char *data)
{
	unsigned char c;
	int		len;

	len = 0;
	com_token[0] = 0;

	if (!data)
		return NULL;

// skip whitespace
skipwhite:
	while ( (c = *data) == ' ' || c == '\t' || c == '\r' || c == '\n')
		data++;

	if (c == 0)
		return NULL;			// end of file;

// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}


// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				if (!c)
					data--;
				return data;
			}
			if (len < MAX_COM_TOKEN-1)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

// parse a regular word
	do
	{
		if (len < MAX_COM_TOKEN-1)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c && c != ' ' && c != '\t' && c != '\n' && c != '\r');

	com_token[len] = 0;
	return data;
}

/*
==============
COM_ParseToken

Parse a token out of a string
==============
*/
int COM_ParseToken(const char **datapointer, int returnnewline)
{
	int len;
	const char *data = *datapointer;

	len = 0;
	com_token[0] = 0;

	if (!data)
	{
		*datapointer = NULL;
		return false;
	}

// skip whitespace
skipwhite:
	// line endings:
	// UNIX: \n
	// Mac: \r
	// Windows: \r\n
	for (;*data <= ' ' && ((*data != '\n' && *data != '\r') || !returnnewline);data++)
	{
		if (*data == 0)
		{
			// end of file
			*datapointer = NULL;
			return false;
		}
	}

	// handle Windows line ending
	if (data[0] == '\r' && data[1] == '\n')
		data++;

	if (data[0] == '/' && data[1] == '/')
	{
		// comment
		while (*data && *data != '\n' && *data != '\r')
			data++;
		goto skipwhite;
	}
	else if (data[0] == '/' && data[1] == '*')
	{
		// comment
		data++;
		while (*data && (data[0] != '*' || data[1] != '/'))
			data++;
		data += 2;
		goto skipwhite;
	}
	else if (*data == '\"')
	{
		// quoted string
		for (data++;*data != '\"';data++)
		{
			if (*data == '\\' && data[1] == '"' )
				data++;
			if (!*data || len >= (int)sizeof(com_token) - 1)
			{
				com_token[0] = 0;
				*datapointer = NULL;
				return false;
			}
			com_token[len++] = *data;
		}
		com_token[len] = 0;
		*datapointer = data+1;
		return true;
	}
	else if (*data == '\'')
	{
		// quoted string
		for (data++;*data != '\'';data++)
		{
			if (*data == '\\' && data[1] == '\'' )
				data++;
			if (!*data || len >= (int)sizeof(com_token) - 1)
			{
				com_token[0] = 0;
				*datapointer = NULL;
				return false;
			}
			com_token[len++] = *data;
		}
		com_token[len] = 0;
		*datapointer = data+1;
		return true;
	}
	else if (*data == '\r')
	{
		// translate Mac line ending to UNIX
		com_token[len++] = '\n';
		com_token[len] = 0;
		*datapointer = data;
		return true;
	}
	else if (*data == '\n' || *data == '{' || *data == '}' || *data == ')' || *data == '(' || *data == ']' || *data == '[' || *data == '\'' || *data == ':' || *data == ',' || *data == ';')
	{
		// single character
		com_token[len++] = *data++;
		com_token[len] = 0;
		*datapointer = data;
		return true;
	}
	else
	{
		// regular word
		for (;*data > ' ' && *data != '{' && *data != '}' && *data != ')' && *data != '(' && *data != ']' && *data != '[' && *data != '\'' && *data != ':' && *data != ',' && *data != ';' && *data != '\'' && *data != '"';data++)
		{
			if (len >= (int)sizeof(com_token) - 1)
			{
				com_token[0] = 0;
				*datapointer = NULL;
				return false;
			}
			com_token[len++] = *data;
		}
		com_token[len] = 0;
		*datapointer = data;
		return true;
	}
}

/*
==============
COM_ParseTokenConsole

Parse a token out of a string, behaving like the qwcl console
==============
*/
int COM_ParseTokenConsole(const char **datapointer)
{
	int len;
	const char *data = *datapointer;

	len = 0;
	com_token[0] = 0;

	if (!data)
	{
		*datapointer = NULL;
		return false;
	}

// skip whitespace
skipwhite:
	for (;*data <= ' ';data++)
	{
		if (*data == 0)
		{
			// end of file
			*datapointer = NULL;
			return false;
		}
	}

	if (*data == '/' && data[1] == '/')
	{
		// comment
		while (*data && *data != '\n' && *data != '\r')
			data++;
		goto skipwhite;
	}
	else if (*data == '\"')
	{
		// quoted string
		for (data++;*data != '\"';data++)
		{
			if (!*data || len >= (int)sizeof(com_token) - 1)
			{
				com_token[0] = 0;
				*datapointer = NULL;
				return false;
			}
			com_token[len++] = *data;
		}
		com_token[len] = 0;
		*datapointer = data+1;
		return true;
	}
	else
	{
		// regular word
		for (;*data > ' ';data++)
		{
			if (len >= (int)sizeof(com_token) - 1)
			{
				com_token[0] = 0;
				*datapointer = NULL;
				return false;
			}
			com_token[len++] = *data;
		}
		com_token[len] = 0;
		*datapointer = data;
		return true;
	}
}



/*
================
COM_AddParm

Adds the given string at the end of the current argument list
================
*/
void COM_AddParm (char *parm)
{
	if (com_argc >= MAX_NUM_ARGVS)
		return;
	largv[com_argc++] = parm;
}


/*
================
COM_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter appears, or 0 if not present
================
*/
int COM_CheckParm (char *parm)
{
	int		i;
	
	for (i=1 ; i<com_argc ; i++)
	{
		if (!strcmp(parm, com_argv[i]))
			return i;
	}
		
	return 0;
}

int COM_Argc (void)
{
	return com_argc;
}

char *COM_Argv (int arg)
{
	if (arg < 0 || arg >= com_argc)
		return "";
	return com_argv[arg];
}

void COM_ClearArgv (int arg)
{
	if (arg < 0 || arg >= com_argc)
		return;
	com_argv[arg] = "";
}

/*
================
COM_InitArgv
================
*/
void COM_InitArgv (int argc, char **argv)
{
	for (com_argc=0 ; (com_argc<MAX_NUM_ARGVS) && (com_argc < argc) ;
		 com_argc++)
	{
		if (argv[com_argc])
			largv[com_argc] = argv[com_argc];
		else
			largv[com_argc] = "";
	}

	largv[com_argc] = "";
	com_argv = largv;
}


/*
================
COM_Init
================
*/
void COM_Init (void)
{
	Cvar_Register (&developer);
	Cvar_Register (&registered);
}


/*
================
COM_Shutdown
================
*/
void COM_Shutdown (void)
{
}

//======================================

/*
============
va

Does a varargs printf into a temp buffer
2 buffers are used to allow subsequent va calls
============
*/
char *va (char *format, ...)
{
	va_list argptr;
	static char string[2][2048];
	static int idx = 0;
	
	idx = 1 - idx;

	va_start (argptr, format);
#ifdef _WIN32
	_vsnprintf (string[idx], sizeof(string[idx]) - 1, format, argptr);
	string[idx][sizeof(string[idx]) - 1] = '\0';
#else
	vsnprintf (string[idx], sizeof(string[idx]), format, argptr);
#endif // _WIN32
	va_end (argptr);

	return string[idx];
}

// snprintf and vsnprintf are NOT portable. Use their quake counterparts instead

#undef snprintf
#undef vsnprintf

#ifdef _WIN32
# define snprintf _snprintf
# define vsnprintf _vsnprintf
#endif

int Q_snprintf (char *buffer, size_t buffersize, const char *format, ...)
{
	va_list args;
	int result;

	va_start (args, format);
	result = vsnprintf (buffer, buffersize, format, args);
	va_end (args);

	return result;
}


int Q_vsnprintf (char *buffer, size_t buffersize, const char *format, va_list args)
{
	int result;

	result = vsnprintf (buffer, buffersize, format, args);
	if (result < 0 || (size_t)result >= buffersize)
	{
		buffer[buffersize - 1] = '\0';
		return -1;
	}

	return result;
}

//======================================

void COM_ToLowerString (const char *in, char *out, size_t size_out)
{
	if (size_out == 0)
		return;

	while (*in && size_out > 1)
	{
		if (*in >= 'A' && *in <= 'Z')
			*out++ = *in++ + 'a' - 'A';
		else
			*out++ = *in++;
		size_out--;
	}
	*out = '\0';
}

void COM_ToUpperString (const char *in, char *out, size_t size_out)
{
	if (size_out == 0)
		return;

	while (*in && size_out > 1)
	{
		if (*in >= 'a' && *in <= 'z')
			*out++ = *in++ + 'A' - 'a';
		else
			*out++ = *in++;
		size_out--;
	}
	*out = '\0';
}


/*
=====================================================================

  INFO STRINGS

=====================================================================
*/


/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
char *Info_ValueForKey (char *s, char *key)
{
	char	pkey[512];
	static	char value[4][512];	// use two buffers so compares
								// work without stomping on each other
	static	int	valueindex;
	char	*o;
	
	valueindex = (valueindex + 1) % 4;
	if (*s == '\\')
		s++;
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

void Info_RemoveKey (char *s, char *key)
{
	char	*start;
	char	pkey[512];
	char	value[512];
	char	*o;

	if (strstr (key, "\\"))
	{
		Com_Printf ("Can't use a key with a \\\n");
		return;
	}

	while (1)
	{
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
		{
			strcpy (start, s);	// remove this part
			return;
		}

		if (!*s)
			return;
	}

}

void Info_RemovePrefixedKeys (char *start, char prefix)
{
	char	*s;
	char	pkey[512];
	char	value[512];
	char	*o;

	s = start;

	while (1)
	{
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (pkey[0] == prefix)
		{
			Info_RemoveKey (start, pkey);
			s = start;
		}

		if (!*s)
			return;
	}

}


void Info_SetValueForStarKey (char *s, char *key, char *value, int maxsize)
{
	char	newstr[1024], *v;
	int		c;

	if (strstr (key, "\\") || strstr (value, "\\") )
	{
		Com_Printf ("Can't use keys or values with a \\\n");
		return;
	}

	if (strstr (key, "\"") || strstr (value, "\"") )
	{
		Com_Printf ("Can't use keys or values with a \"\n");
		return;
	}

	if (strlen(key) >= MAX_INFO_KEY || strlen(value) >= MAX_INFO_KEY)
	{
		Com_Printf ("Keys and values must be < %i characters.\n", MAX_INFO_KEY);
		return;
	}

	// this next line is kinda trippy
	if (*(v = Info_ValueForKey(s, key))) {
		// key exists, make sure we have enough room for new value, if we don't,
		// don't change it!
		if (strlen(value) - strlen(v) + strlen(s) >= maxsize) {
			Com_Printf ("Info string length exceeded\n");
			return;
		}
	}
	Info_RemoveKey (s, key);
	if (!value || !strlen(value))
		return;

	sprintf (newstr, "\\%s\\%s", key, value);

	if ((strlen(newstr) + strlen(s)) >= maxsize)
	{
		Com_Printf ("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = newstr;
	while (*v) {
		c = (unsigned char)*v++;
		if (c > 13)
			*s++ = c;
	}
	*s = 0;
}

void Info_SetValueForKey (char *s, char *key, char *value, int maxsize)
{
	if (key[0] == '*')
	{
		Com_Printf ("Can't set * keys\n");
		return;
	}

	Info_SetValueForStarKey (s, key, value, maxsize);
}

void Info_Print (char *s)
{
	char	key[512];
	char	value[512];
	char	*o;
	int		l;

	if (*s == '\\')
		s++;
	while (*s)
	{
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		if (l < 20)
		{
			memset (o, ' ', 20-l);
			key[20] = 0;
		}
		else
			*o = 0;
		Com_Printf ("%s", key);

		if (!*s)
		{
			Com_Printf ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		Com_Printf ("%s\n", value);
	}
}

//============================================================================

int matchpattern(const char *in, const char *pattern, int caseinsensitive)
{
	int c1, c2;
	while (*pattern)
	{
		switch (*pattern)
		{
		case 0:
			return 1; // end of pattern
		case '?': // match any single character
			if (*in == 0 || *in == '/' || *in == '\\' || *in == ':')
				return 0; // no match
			in++;
			pattern++;
			break;
		case '*': // match anything until following string
			if (!*in)
				return 1; // match
			pattern++;
			while (*in)
			{
				if (*in == '/' || *in == '\\' || *in == ':')
					break;
				// see if pattern matches at this offset
				if (matchpattern(in, pattern, caseinsensitive))
					return 1;
				// nope, advance to next offset
				in++;
			}
			break;
		default:
			if (*in != *pattern)
			{
				if (!caseinsensitive)
					return 0; // no match
				c1 = *in;
				if (c1 >= 'A' && c1 <= 'Z')
					c1 += 'a' - 'A';
				c2 = *pattern;
				if (c2 >= 'A' && c2 <= 'Z')
					c2 += 'a' - 'A';
				if (c1 != c2)
					return 0; // no match
			}
			in++;
			pattern++;
			break;
		}
	}
	if (*in)
		return 0; // reached end of pattern but not end of input
	return 1; // success
}

// a little strings system
void stringlistinit(stringlist_t *list)
{
	memset(list, 0, sizeof(*list));
}

void stringlistfreecontents(stringlist_t *list)
{
	int i;
	for (i = 0;i < list->numstrings;i++)
	{
		if (list->strings[i])
			free(list->strings[i]);
		list->strings[i] = NULL;
	}
	list->numstrings = 0;
	list->maxstrings = 0;
	if (list->strings)
		free(list->strings);
}

void stringlistappend(stringlist_t *list, char *text)
{
	size_t textlen;
	char **oldstrings;

	if (list->numstrings >= list->maxstrings)
	{
		oldstrings = list->strings;
		list->maxstrings += 4096;
		list->strings = malloc(list->maxstrings * sizeof(*list->strings));
		if (list->numstrings)
			memcpy(list->strings, oldstrings, list->numstrings * sizeof(*list->strings));
		if (oldstrings)
			free(oldstrings);
	}
	textlen = strlen(text) + 1;
	list->strings[list->numstrings] = malloc(textlen);
	memcpy(list->strings[list->numstrings], text, textlen);
	list->numstrings++;
}

void stringlistsort(stringlist_t *list)
{
	int i, j;
	char *temp;
	// this is a selection sort (finds the best entry for each slot)
	for (i = 0;i < list->numstrings - 1;i++)
	{
		for (j = i + 1;j < list->numstrings;j++)
		{
			if (strcmp(list->strings[i], list->strings[j]) > 0)
			{
				temp = list->strings[i];
				list->strings[i] = list->strings[j];
				list->strings[j] = temp;
			}
		}
	}
}

// operating system specific code
#ifdef WIN32
#include <io.h>
void listdirectory(stringlist_t *list, const char *path)
{
	int i;
	char pattern[4096], *c;
	struct _finddata_t n_file;
	long hFile;
	strlcpy (pattern, path, sizeof (pattern));
	strlcat (pattern, "*", sizeof (pattern));
	// ask for the directory listing handle
	hFile = _findfirst(pattern, &n_file);
	if(hFile == -1)
		return;
	// start a new chain with the the first name
	stringlistappend(list, n_file.name);
	// iterate through the directory
	while (_findnext(hFile, &n_file) == 0)
		stringlistappend(list, n_file.name);
	_findclose(hFile);

	// convert names to lowercase because windows does not care, but pattern matching code often does
	for (i = 0;i < list->numstrings;i++)
		for (c = list->strings[i];*c;c++)
			if (*c >= 'A' && *c <= 'Z')
				*c += 'a' - 'A';
}
#else
#include <dirent.h>
void listdirectory(stringlist_t *list, const char *path)
{
	DIR *dir;
	struct dirent *ent;
	dir = opendir(path);
	if (!dir)
		return;
	while ((ent = readdir(dir)))
		if (strcmp(ent->d_name, ".") && strcmp(ent->d_name, ".."))
			stringlistappend(list, ent->d_name);
	closedir(dir);
}
#endif

//============================================================================

static byte chktbl[1024] = {
0x78,0xd2,0x94,0xe3,0x41,0xec,0xd6,0xd5,0xcb,0xfc,0xdb,0x8a,0x4b,0xcc,0x85,0x01,
0x23,0xd2,0xe5,0xf2,0x29,0xa7,0x45,0x94,0x4a,0x62,0xe3,0xa5,0x6f,0x3f,0xe1,0x7a,
0x64,0xed,0x5c,0x99,0x29,0x87,0xa8,0x78,0x59,0x0d,0xaa,0x0f,0x25,0x0a,0x5c,0x58,
0xfb,0x00,0xa7,0xa8,0x8a,0x1d,0x86,0x80,0xc5,0x1f,0xd2,0x28,0x69,0x71,0x58,0xc3,
0x51,0x90,0xe1,0xf8,0x6a,0xf3,0x8f,0xb0,0x68,0xdf,0x95,0x40,0x5c,0xe4,0x24,0x6b,
0x29,0x19,0x71,0x3f,0x42,0x63,0x6c,0x48,0xe7,0xad,0xa8,0x4b,0x91,0x8f,0x42,0x36,
0x34,0xe7,0x32,0x55,0x59,0x2d,0x36,0x38,0x38,0x59,0x9b,0x08,0x16,0x4d,0x8d,0xf8,
0x0a,0xa4,0x52,0x01,0xbb,0x52,0xa9,0xfd,0x40,0x18,0x97,0x37,0xff,0xc9,0x82,0x27,
0xb2,0x64,0x60,0xce,0x00,0xd9,0x04,0xf0,0x9e,0x99,0xbd,0xce,0x8f,0x90,0x4a,0xdd,
0xe1,0xec,0x19,0x14,0xb1,0xfb,0xca,0x1e,0x98,0x0f,0xd4,0xcb,0x80,0xd6,0x05,0x63,
0xfd,0xa0,0x74,0xa6,0x86,0xf6,0x19,0x98,0x76,0x27,0x68,0xf7,0xe9,0x09,0x9a,0xf2,
0x2e,0x42,0xe1,0xbe,0x64,0x48,0x2a,0x74,0x30,0xbb,0x07,0xcc,0x1f,0xd4,0x91,0x9d,
0xac,0x55,0x53,0x25,0xb9,0x64,0xf7,0x58,0x4c,0x34,0x16,0xbc,0xf6,0x12,0x2b,0x65,
0x68,0x25,0x2e,0x29,0x1f,0xbb,0xb9,0xee,0x6d,0x0c,0x8e,0xbb,0xd2,0x5f,0x1d,0x8f,
0xc1,0x39,0xf9,0x8d,0xc0,0x39,0x75,0xcf,0x25,0x17,0xbe,0x96,0xaf,0x98,0x9f,0x5f,
0x65,0x15,0xc4,0x62,0xf8,0x55,0xfc,0xab,0x54,0xcf,0xdc,0x14,0x06,0xc8,0xfc,0x42,
0xd3,0xf0,0xad,0x10,0x08,0xcd,0xd4,0x11,0xbb,0xca,0x67,0xc6,0x48,0x5f,0x9d,0x59,
0xe3,0xe8,0x53,0x67,0x27,0x2d,0x34,0x9e,0x9e,0x24,0x29,0xdb,0x69,0x99,0x86,0xf9,
0x20,0xb5,0xbb,0x5b,0xb0,0xf9,0xc3,0x67,0xad,0x1c,0x9c,0xf7,0xcc,0xef,0xce,0x69,
0xe0,0x26,0x8f,0x79,0xbd,0xca,0x10,0x17,0xda,0xa9,0x88,0x57,0x9b,0x15,0x24,0xba,
0x84,0xd0,0xeb,0x4d,0x14,0xf5,0xfc,0xe6,0x51,0x6c,0x6f,0x64,0x6b,0x73,0xec,0x85,
0xf1,0x6f,0xe1,0x67,0x25,0x10,0x77,0x32,0x9e,0x85,0x6e,0x69,0xb1,0x83,0x00,0xe4,
0x13,0xa4,0x45,0x34,0x3b,0x40,0xff,0x41,0x82,0x89,0x79,0x57,0xfd,0xd2,0x8e,0xe8,
0xfc,0x1d,0x19,0x21,0x12,0x00,0xd7,0x66,0xe5,0xc7,0x10,0x1d,0xcb,0x75,0xe8,0xfa,
0xb6,0xee,0x7b,0x2f,0x1a,0x25,0x24,0xb9,0x9f,0x1d,0x78,0xfb,0x84,0xd0,0x17,0x05,
0x71,0xb3,0xc8,0x18,0xff,0x62,0xee,0xed,0x53,0xab,0x78,0xd3,0x65,0x2d,0xbb,0xc7,
0xc1,0xe7,0x70,0xa2,0x43,0x2c,0x7c,0xc7,0x16,0x04,0xd2,0x45,0xd5,0x6b,0x6c,0x7a,
0x5e,0xa1,0x50,0x2e,0x31,0x5b,0xcc,0xe8,0x65,0x8b,0x16,0x85,0xbf,0x82,0x83,0xfb,
0xde,0x9f,0x36,0x48,0x32,0x79,0xd6,0x9b,0xfb,0x52,0x45,0xbf,0x43,0xf7,0x0b,0x0b,
0x19,0x19,0x31,0xc3,0x85,0xec,0x1d,0x8c,0x20,0xf0,0x3a,0xfa,0x80,0x4d,0x2c,0x7d,
0xac,0x60,0x09,0xc0,0x40,0xee,0xb9,0xeb,0x13,0x5b,0xe8,0x2b,0xb1,0x20,0xf0,0xce,
0x4c,0xbd,0xc6,0x04,0x86,0x70,0xc6,0x33,0xc3,0x15,0x0f,0x65,0x19,0xfd,0xc2,0xd3,

// Only the first 512 bytes of the table are initialized, the rest
// is just zeros.
// This is an idiocy in QW but we can't change this, or checksums
// will not match.
};

/*
====================
COM_BlockSequenceCRCByte

For proxy protecting
====================
*/
byte COM_BlockSequenceCRCByte (byte *base, int length, int sequence)
{
	unsigned short crc;
	byte	*p;
	byte chkb[60 + 4];

	p = chktbl + ((unsigned int)sequence % (sizeof(chktbl) - 4));

	if (length > 60)
		length = 60;
	memcpy (chkb, base, length);

	chkb[length] = (sequence & 0xff) ^ p[0];
	chkb[length+1] = p[1];
	chkb[length+2] = ((sequence>>8) & 0xff) ^ p[2];
	chkb[length+3] = p[3];

	length += 4;

	crc = CRC_Block (chkb, length);

	crc &= 0xff;

	return crc;
}

//============================================================================

///////////////////////////////////////////////////////////////
//	MD4-based checksum utility functions
//
//	Copyright (C) 2000       Jeff Teunissen <d2deek@pmail.net>
//
//	Author: Jeff Teunissen	<d2deek@pmail.net>
//	Date: 01 Jan 2000

unsigned Com_BlockChecksum (void *buffer, int length)
{
	int				digest[4];
	unsigned 		val;

	mdfour ( (unsigned char *) digest, (unsigned char *) buffer, length );

	val = digest[0] ^ digest[1] ^ digest[2] ^ digest[3];

	return val;
}

void Com_BlockFullChecksum (void *buffer, int len, unsigned char *outbuf)
{
	mdfour ( outbuf, (unsigned char *) buffer, len );
}

//=====================================================================

/*
================
Com_Printf

Prints to all appropriate console targets
================
*/
void Com_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAX_INPUTLINE];

	va_start (argptr, fmt);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	Con_Print (msg);
}

/*
================
Com_DPrintf

A Com_Printf that only shows up if the "developer" cvar is set
================
*/
void Com_DPrintf (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAX_INPUTLINE];
		
	if (!developer.integer)
		return; // don't confuse non-developers with techie stuff...

	va_start (argptr, fmt);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);
	
	Con_Print (msg);
}
