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

#include "quakedef.h"
#include "cdaudio.h"

// Prototypes of the system dependent functions
extern void CDAudio_SysEject (void);
extern void CDAudio_SysCloseDoor (void);
extern int CDAudio_SysGetAudioDiskInfo (void);
extern int CDAudio_SysPlay (byte track);
extern int CDAudio_SysStop (void);
extern int CDAudio_SysPause (void);
extern int CDAudio_SysResume (void);
extern int CDAudio_SysUpdate (void);
extern void CDAudio_SysInit (void);
extern int CDAudio_SysStartup (void);
extern void CDAudio_SysShutdown (void);

// used by menu to ghost CD audio slider
cvar_t	cdaudioinitialized = {"cdaudioinitialized","0",CVAR_ROM};

extern cvar_t bgmvolume;

static qbool wasPlaying = false;
static qbool initialized = false;
static qbool enabled = false;
static float cdvolume;
static byte remap[100];
static byte maxTrack;

// exported variables
qbool cdValid = false;
qbool cdPlaying = false;
qbool cdPlayLooping = false;
byte cdPlayTrack;


static void CDAudio_Eject (void)
{
	if (!enabled)
		return;

	CDAudio_SysEject();
}


static void CDAudio_CloseDoor (void)
{
	if (!enabled)
		return;

	CDAudio_SysCloseDoor();
}

static int CDAudio_GetAudioDiskInfo (void)
{
	int ret;

	cdValid = false;

	ret = CDAudio_SysGetAudioDiskInfo();
	if (ret < 1)
		return -1;

	cdValid = true;
	maxTrack = ret;

	return 0;
}


void CDAudio_Play (byte track, qbool looping)
{
	if (!enabled)
		return;

	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
			return;
	}

	track = remap[track];
	if (track < 1 || track > maxTrack)
	{
		Com_DPrintf("CDAudio: Bad track number %u.\n", track);
		return;
	}

	if (cdPlaying && cdPlayTrack == track)
		return;

	if (CDAudio_SysPlay(track) == -1)
		return;

	cdPlayLooping = looping;
	cdPlayTrack = track;
	cdPlaying = true;

	if (cdvolume == 0.0)
		CDAudio_Pause ();
}


void CDAudio_Stop (void)
{
	if (!enabled || !cdPlaying)
		return;

	if (CDAudio_SysStop() == -1)
		return;

	wasPlaying = false;
	cdPlaying = false;
}

void CDAudio_Pause (void)
{
	if (!enabled || !cdPlaying)
		return;

	if (CDAudio_SysPause() == -1)
		return;

	wasPlaying = cdPlaying;
	cdPlaying = false;
}


void CDAudio_Resume (void)
{
	if (!enabled || !cdValid || !wasPlaying)
		return;

	if (CDAudio_SysResume() == -1)
		return;
	cdPlaying = true;
}

static void CD_f (void)
{
	const char *command;
	int ret;
	int n;

	if (Cmd_Argc() < 2)
		return;

	command = Cmd_Argv (1);

	if (strcasecmp(command, "on") == 0)
	{
		enabled = true;
		return;
	}

	if (strcasecmp(command, "off") == 0)
	{
		if (cdPlaying)
			CDAudio_Stop();
		enabled = false;
		return;
	}

	if (strcasecmp(command, "reset") == 0)
	{
		enabled = true;
		if (cdPlaying)
			CDAudio_Stop();
		for (n = 0; n < 100; n++)
			remap[n] = n;
		CDAudio_GetAudioDiskInfo();
		return;
	}

	if (strcasecmp(command, "remap") == 0)
	{
		ret = Cmd_Argc() - 2;
		if (ret <= 0)
		{
			for (n = 1; n < 100; n++)
				if (remap[n] != n)
					Com_Printf("  %u -> %u\n", n, remap[n]);
			return;
		}
		for (n = 1; n <= ret; n++)
			remap[n] = atoi(Cmd_Argv (n+1));
		return;
	}

	if (strcasecmp(command, "close") == 0)
	{
		CDAudio_CloseDoor();
		return;
	}

	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
		{
			Com_Printf("No CD in player.\n");
			return;
		}
	}

	if (strcasecmp(command, "play") == 0)
	{
		CDAudio_Play((byte)atoi(Cmd_Argv (2)), false);
		return;
	}

	if (strcasecmp(command, "loop") == 0)
	{
		CDAudio_Play((byte)atoi(Cmd_Argv (2)), true);
		return;
	}

	if (strcasecmp(command, "stop") == 0)
	{
		CDAudio_Stop();
		return;
	}

	if (strcasecmp(command, "pause") == 0)
	{
		CDAudio_Pause();
		return;
	}

	if (strcasecmp(command, "resume") == 0)
	{
		CDAudio_Resume();
		return;
	}

	if (strcasecmp(command, "eject") == 0)
	{
		if (cdPlaying)
			CDAudio_Stop();
		CDAudio_Eject();
		cdValid = false;
		return;
	}

	if (strcasecmp(command, "info") == 0)
	{
		Com_Printf("%u tracks\n", maxTrack);
		if (cdPlaying)
			Com_Printf("Currently %s track %u\n", cdPlayLooping ? "looping" : "playing", cdPlayTrack);
		else if (wasPlaying)
			Com_Printf("Paused %s track %u\n", cdPlayLooping ? "looping" : "playing", cdPlayTrack);
		Com_Printf("Volume is %f\n", cdvolume);
		return;
	}
}

void CDAudio_Update (void)
{
	if (!enabled)
		return;

	if (bgmvolume.value != cdvolume)
	{
		if (cdvolume)
		{
			Cvar_SetValue (&bgmvolume, 0.0);
			cdvolume = bgmvolume.value;
			CDAudio_Pause ();
		}
		else
		{
			Cvar_SetValue (&bgmvolume, 1.0);
			cdvolume = bgmvolume.value;
			CDAudio_Resume ();
		}
	}

	CDAudio_SysUpdate();
}

int CDAudio_Init (void)
{
	int i;

	if (dedicated)
		return -1;

	if (COM_CheckParm("-nocdaudio") || COM_CheckParm("-safe"))
		return -1;

	CDAudio_SysInit();

	for (i = 0; i < 100; i++)
		remap[i] = i;

	Cvar_Register(&cdaudioinitialized);
	Cvar_SetValue(&cdaudioinitialized, true);
	enabled = true;

	Cmd_AddCommand("cd", CD_f);

	return 0;
}

int CDAudio_Startup (void)
{
	if (CDAudio_SysStartup() == -1)
		return -1;

	if (CDAudio_GetAudioDiskInfo())
	{
		Com_DPrintf("CDAudio_Init: No CD in player.\n");
		cdValid = false;
	}

	initialized = true;

	Com_DPrintf("CD Audio Initialized\n");

	return 0;
}

void CDAudio_Shutdown (void)
{
	if (!initialized)
		return;
	CDAudio_Stop();
	CDAudio_SysShutdown();
	initialized = false;
}
