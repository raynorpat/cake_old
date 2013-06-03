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
// snd_dma.c -- main control for any streaming sound output device

#include "quakedef.h"
#include "sound.h"

void S_Play_f (void);
void S_PlayVol_f (void);
void S_SoundList_f (void);
void S_Update_ ();
void S_StopAllSounds (qbool clear);
void S_StopAllSounds_f (void);

// =======================================================================
// Internal sound data & structures
// =======================================================================

// only begin attenuating sound volumes when outside the FULLVOLUME range
#define		SOUND_FULLVOLUME	80

channel_t   channels[MAX_CHANNELS];
int			total_channels;

int			snd_blocked = 0;
qbool		snd_initialized = false;
qbool		sound_started = false;

dma_t		dma;

vec3_t		listener_origin;
vec3_t		listener_forward;
vec3_t		listener_right;
vec3_t		listener_up;

int			soundtime;		// sample PAIRS
int   		paintedtime; 	// sample PAIRS

// during registration it is possible to have more sounds
// than could actually be referenced during gameplay,
// because we don't want to free anything until we are
// sure we won't need it.
#define	MAX_SFX		(MAX_SOUNDS*2)
sfx_t		known_sfx[MAX_SFX];
int			num_sfx;

sfx_t		*ambient_sfx[NUM_AMBIENTS];

int			s_rawend;
portable_samplepair_t s_rawsamples[MAX_RAW_SAMPLES];


// ====================================================================
// User-setable variables
// ====================================================================

cvar_t bgmvolume = {"bgmvolume", "1", CVAR_ARCHIVE};
cvar_t s_initsound = {"s_initsound", "1"};
cvar_t s_volume = {"s_volume", "0.5", CVAR_ARCHIVE};
cvar_t s_nosound = {"s_nosound", "0"};
cvar_t s_precache = {"s_precache", "1"};
cvar_t s_khz = {"s_khz", "44", CVAR_ARCHIVE};
cvar_t s_ambientlevel = {"s_ambientlevel", "0.3"};
cvar_t s_ambientfade = {"s_ambientfade", "100"};
cvar_t s_show = {"s_show", "0"};
cvar_t s_mixahead = {"s_mixahead", "0.1", CVAR_ARCHIVE};

void S_SoundInfo_f (void)
{
	if (!sound_started)
	{
		Com_Printf ("sound system not started\n");
		return;
	}
	
    Com_Printf ("%5d stereo\n", dma.channels - 1);
    Com_Printf ("%5d samples\n", dma.samples);
    Com_Printf ("%5d samplepos\n", dma.samplepos);
    Com_Printf ("%5d samplebits\n", dma.samplebits);
	Com_Printf ("%5d submission_chunk\n", dma.submission_chunk);
    Com_Printf ("%5d speed\n", dma.speed);
    Com_Printf ("0x%x dma buffer\n", dma.buffer);
}


void S_Init(void)
{
	Com_DPrintf("\nSound Initialization\n");

	if (COM_CheckParm("-nosound") || COM_CheckParm("-safe"))
		return;

	Cvar_Register(&bgmvolume);
	Cvar_Register(&s_volume);
	Cvar_Register(&s_initsound);
	Cvar_Register(&s_nosound);
	Cvar_Register(&s_precache);
	Cvar_Register(&s_khz);
	Cvar_Register(&s_ambientlevel);
	Cvar_Register(&s_ambientfade);
	Cvar_Register(&s_show);
	Cvar_Register(&s_mixahead);

	// compatibility with old configs
	Cmd_AddLegacyCommand ("volume", "s_volume");
	Cmd_AddLegacyCommand ("nosound", "s_nosound");
	Cmd_AddLegacyCommand ("precache", "s_precache");
	Cmd_AddLegacyCommand ("ambient_level", "s_ambientlevel");
	Cmd_AddLegacyCommand ("ambient_fade", "s_ambientfade");
	Cmd_AddLegacyCommand ("snd_show", "s_show");
	Cmd_AddLegacyCommand ("_snd_mixahead", "s_mixahead");

	Cmd_AddCommand("play", S_Play_f);
	Cmd_AddCommand("playvol", S_PlayVol_f);
	Cmd_AddCommand("stopsound", S_StopAllSounds_f);
	Cmd_AddCommand("soundlist", S_SoundList_f);
	Cmd_AddCommand("soundinfo", S_SoundInfo_f);

	snd_initialized = true;

	num_sfx = 0;

	SND_InitScaletable ();

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;	// no statics
	memset(channels, 0, MAX_CHANNELS * sizeof(channel_t));
}


/*
================
S_Restart
================
*/
void S_Restart (void)
{
	int		i;

	S_Shutdown ();

	// flush all sounds
	for (i = 0; i < num_sfx; i++) {
		if (known_sfx[i].cache.data)
			Cache_Free (&known_sfx[i].cache);
	}

	S_Init ();
}


/*
================
S_Startup
================
*/
void S_Startup (void)
{
	Com_DPrintf ("\nSound Initialization\n");

	if (!s_initsound.value || COM_CheckParm("-nosound") || s_nosound.value) {
		Com_Printf ("sound initialization skipped\n");
		return;
	}
	
	if (!SNDDMA_Init())
	{
		sound_started = false;
		return;
	}
	sound_started = true;

	S_StopAllSounds (true);
}


// =======================================================================
// Shutdown sound engine
// =======================================================================

void S_Shutdown (void)
{
	if (!sound_started)
		return;

	sound_started = false;

	SNDDMA_Shutdown();
}


// =======================================================================
// Load a sound
// =======================================================================

/*
==================
S_FindName

==================
*/
sfx_t *S_FindName (char *name)
{
	int		i;
	sfx_t	*sfx;

	if (!name)
		Sys_Error ("S_FindName: NULL");

	if (strlen(name) >= MAX_QPATH)
		Sys_Error ("Sound name too long: %s", name);

// see if already loaded
	for (i=0 ; i < num_sfx ; i++)
	{
		if (!strcmp(known_sfx[i].name, name))
		{
			return &known_sfx[i];
		}
	}

	if (num_sfx == MAX_SFX)
		Sys_Error ("S_FindName: out of sfx_t");
	
	sfx = &known_sfx[i];
	strcpy (sfx->name, name);

	num_sfx++;
	
	return sfx;
}


/*
==================
S_TouchSound

==================
*/
void S_TouchSound (char *name)
{
	sfx_t	*sfx;
	
	sfx = S_FindName (name);
	Cache_Check (&sfx->cache);
}

/*
==================
S_PrecacheSound

==================
*/
sfx_t *S_PrecacheSound (char *name)
{
	sfx_t	*sfx;

	if (!snd_initialized)
		return NULL;

	sfx = S_FindName (name);
	
// cache it in
	if (!s_nosound.value || s_precache.value)
		S_LoadSound (sfx);
	
	return sfx;
}


//=============================================================================

/*
=================
SND_PickChannel
=================
*/
channel_t *SND_PickChannel (int entnum, int entchannel)
{
    int ch_idx;
    int first_to_die;
    int life_left;

// Check for replacement sound, or find the best one to replace
    first_to_die = -1;
    life_left = 0x7fffffff;
    for (ch_idx=NUM_AMBIENTS ; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS ; ch_idx++)
    {
		if (entchannel != 0		// channel 0 never overrides
		&& channels[ch_idx].entnum == entnum
		&& (channels[ch_idx].entchannel == entchannel || entchannel == -1) )
		{	// always override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (channels[ch_idx].entnum == cl.playernum+1 && entnum != cl.playernum+1 && channels[ch_idx].sfx)
			continue;

		if (channels[ch_idx].end - paintedtime < life_left)
		{
			life_left = channels[ch_idx].end - paintedtime;
			first_to_die = ch_idx;
		}
   }

	if (first_to_die == -1)
		return NULL;

	if (channels[first_to_die].sfx)
		channels[first_to_die].sfx = NULL;

    return &channels[first_to_die];    
}       

/*
=================
SND_Spatialize
=================
*/
void SND_Spatialize (channel_t *ch)
{
	vec_t		dot, dist;
    vec_t		lscale, rscale, scale;
    vec3_t		source_vec;

	// anything coming from the view entity will always be full volume
	if (ch->entnum == cl.playernum+1)
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
	}
	else
	{
		// calculate stereo seperation and distance attenuation
		VectorSubtract(ch->origin, listener_origin, source_vec);

		dist = VectorNormalize(source_vec);
		dist -= SOUND_FULLVOLUME;
		if (dist < 0)
			dist = 0;			// close enough to be at full volume
		dist *= ch->dist_mult;	// different attenuation levels

		dot = DotProduct(listener_right, source_vec);

		if (dma.channels == 1 || !ch->dist_mult)
		{
			// no attenuation = no spatialization
			rscale = 1.0;
			lscale = 1.0;
		}
		else
		{
			rscale = 0.5 * (1.0 + dot);
			lscale = 0.5 * (1.0 - dot);
		}

		// add in distance effect
		scale = (1.0 - dist) * rscale;
		ch->rightvol = (int) (ch->master_vol * scale);
		if (ch->rightvol < 0)
			ch->rightvol = 0;

		scale = (1.0 - dist) * lscale;
		ch->leftvol = (int) (ch->master_vol * scale);
		if (ch->leftvol < 0)
			ch->leftvol = 0;
	}
}           


// =======================================================================
// Start a sound effect
// =======================================================================

/*
====================
S_StartSound

Validates the parms and ques the sound up
if pos is NULL, the sound will be dynamically sourced from the entity
Entchannel 0 will never override a playing sound
====================
*/
int S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation)
{
	channel_t *target_chan, *check;
	sfxcache_t	*sc;
	int		vol;
	int		ch_idx;
	int		skip;

	if (!sound_started || !sfx || s_nosound.value)
		return -1;

	vol = fvol*255;

// pick a channel to play on
	target_chan = SND_PickChannel(entnum, entchannel);
	if (!target_chan)
		return -1;
		
// spatialize
	memset (target_chan, 0, sizeof(*target_chan));
	VectorCopy(origin, target_chan->origin);
	target_chan->dist_mult = attenuation * 0.0005;
	target_chan->master_vol = vol;
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	SND_Spatialize(target_chan);

	if (!target_chan->leftvol && !target_chan->rightvol)
		return -1;		// not audible at all

// new channel
	sc = S_LoadSound (sfx);
	if (!sc)
	{
		target_chan->sfx = NULL;
		return -1;		// couldn't load the sound's data
	}

	target_chan->sfx = sfx;
	target_chan->pos = 0.0;
    target_chan->end = paintedtime + sc->length;	

// if an identical sound has also been started this frame, offset the pos
// a bit to keep it from just making the first one louder
	check = &channels[NUM_AMBIENTS];
    for (ch_idx=NUM_AMBIENTS ; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS ; ch_idx++, check++)
    {
		if (check == target_chan)
			continue;
		if (check->sfx == sfx && !check->pos)
		{
			skip = 0.1 * sc->speed;
			if (skip > sc->length)
				skip = sc->length;
			if (skip > 0)
				skip = rand() % skip;
			target_chan->pos += skip;
			target_chan->end -= skip;
			break;
		}
	}

	return 0;
}

void S_StopSound (int entnum, int entchannel)
{
	int i;

	for (i=0 ; i<MAX_DYNAMIC_CHANNELS ; i++)
	{
		if (channels[i].entnum == entnum
			&& channels[i].entchannel == entchannel)
		{
			channels[i].end = 0;
			channels[i].sfx = NULL;
			return;
		}
	}
}

/*
==================
S_ClearBuffer
==================
*/
void S_ClearBuffer (void)
{
	int		clear;
		
	if (!sound_started)
		return;

	s_rawend = 0;

	if (dma.samplebits == 8)
		clear = 0x80;
	else
		clear = 0;

	SNDDMA_BeginPainting ();
	if (dma.buffer)
		memset(dma.buffer, clear, dma.samples * dma.samplebits/8);
	SNDDMA_Submit ();
}

/*
==================
S_StopAllSounds
==================
*/
void S_StopAllSounds (qbool clear)
{
	int		i;

	if (!sound_started)
		return;

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;	// no statics

	for (i=0 ; i<MAX_CHANNELS ; i++)
	{
		if (channels[i].sfx)
			channels[i].sfx = NULL;
	}

	memset(channels, 0, MAX_CHANNELS * sizeof(channel_t));

	if (clear)
		S_ClearBuffer ();
}

void S_StopAllSounds_f (void)
{
	S_StopAllSounds (true);
}

/*
=================
S_StaticSound
=================
*/
void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
	channel_t	*ss;
	sfxcache_t		*sc;

	if (!sfx || !sound_started)
		return;

	if (total_channels == MAX_CHANNELS)
	{
		Com_Printf ("total_channels == MAX_CHANNELS\n");
		return;
	}

	ss = &channels[total_channels];
	total_channels++;

	sc = S_LoadSound (sfx);
	if (!sc)
		return;

	if (sc->loopstart == -1)
	{
		Com_Printf ("Sound %s not looped\n", sfx->name);
		return;
	}
	
	ss->sfx = sfx;
	VectorCopy (origin, ss->origin);
	ss->master_vol = vol;
	ss->dist_mult = attenuation * 0.001;
    ss->end = paintedtime + sc->length;	
	
	SND_Spatialize (ss);
}


//=============================================================================

/*
===================
S_UpdateAmbientSounds
===================
*/
void S_UpdateAmbientSounds (void)
{
	struct cleaf_s *leaf;
	float		vol;
	int			ambient_channel;
	channel_t	*chan;

	if (cls.state != ca_active)
		return;

	leaf = CM_PointInLeaf (listener_origin);
	if (!CM_Leafnum(leaf) || !s_ambientlevel.value)
	{
		for (ambient_channel = 0 ; ambient_channel< NUM_AMBIENTS ; ambient_channel++)
			channels[ambient_channel].sfx = NULL;
		return;
	}

	for (ambient_channel = 0; ambient_channel < NUM_AMBIENTS; ambient_channel++)
	{
		chan = &channels[ambient_channel];	
		chan->sfx = ambient_sfx[ambient_channel];
	
		vol = s_ambientlevel.value * CM_LeafAmbientLevel(leaf, ambient_channel);
		if (vol < 8)
			vol = 0;

	// don't adjust volume too fast
		if (chan->master_vol < vol)
		{
			chan->master_vol += Q_rint(cls.frametime * s_ambientfade.value);
			if (chan->master_vol > vol)
				chan->master_vol = vol;
		}
		else if (chan->master_vol > vol)
		{
			chan->master_vol -= Q_rint(cls.frametime * s_ambientfade.value);
			if (chan->master_vol < vol)
				chan->master_vol = vol;
		}
		
		chan->leftvol = chan->rightvol = chan->master_vol;
	}
}

//=============================================================================

/*
============
S_RawSamples

Cinematic streaming and voice over network
============
*/
void S_RawSamples (int samples, int rate, int width, int channels, byte *data)
{
	int		i;
	int		src, dst;
	float	scale;

	if (!sound_started)
		return;

	if (s_rawend < paintedtime)
		s_rawend = paintedtime;
	scale = (float)rate / dma.speed;

//Com_Printf ("%i < %i < %i\n", soundtime, paintedtime, s_rawend);
	if (channels == 2 && width == 2)
	{
		if (scale == 1.0)
		{	// optimized case
			for (i=0 ; i<samples ; i++)
			{
				dst = s_rawend&(MAX_RAW_SAMPLES-1);
				s_rawend++;
				s_rawsamples[dst].left =
				    LittleShort(((short *)data)[i*2]) << 8;
				s_rawsamples[dst].right =
				    LittleShort(((short *)data)[i*2+1]) << 8;
			}
		}
		else
		{
			for (i=0 ; ; i++)
			{
				src = i*scale;
				if (src >= samples)
					break;
				dst = s_rawend&(MAX_RAW_SAMPLES-1);
				s_rawend++;
				s_rawsamples[dst].left =
				    LittleShort(((short *)data)[src*2]) << 8;
				s_rawsamples[dst].right =
				    LittleShort(((short *)data)[src*2+1]) << 8;
			}
		}
	}
	else if (channels == 1 && width == 2)
	{
		for (i=0 ; ; i++)
		{
			src = i*scale;
			if (src >= samples)
				break;
			dst = s_rawend&(MAX_RAW_SAMPLES-1);
			s_rawend++;
			s_rawsamples[dst].left =
			    LittleShort(((short *)data)[src]) << 8;
			s_rawsamples[dst].right =
			    LittleShort(((short *)data)[src]) << 8;
		}
	}
	else if (channels == 2 && width == 1)
	{
		for (i=0 ; ; i++)
		{
			src = i*scale;
			if (src >= samples)
				break;
			dst = s_rawend&(MAX_RAW_SAMPLES-1);
			s_rawend++;
			s_rawsamples[dst].left =
			    ((char *)data)[src*2] << 16;
			s_rawsamples[dst].right =
			    ((char *)data)[src*2+1] << 16;
		}
	}
	else if (channels == 1 && width == 1)
	{
		for (i=0 ; ; i++)
		{
			src = i*scale;
			if (src >= samples)
				break;
			dst = s_rawend&(MAX_RAW_SAMPLES-1);
			s_rawend++;
			s_rawsamples[dst].left =
			    (((byte *)data)[src]-128) << 16;
			s_rawsamples[dst].right = (((byte *)data)[src]-128) << 16;
		}
	}
}

//=============================================================================

/*
============
S_Update

Called once each time through the main loop
============
*/
void S_Update (vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
	int			i, j;
	int			total;
	channel_t	*ch;
	channel_t	*combine;

	if (!sound_started || (snd_blocked > 0))
		return;

	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);
	
// update general area ambient sound sources
	S_UpdateAmbientSounds ();

	combine = NULL;

// update spatialization for static and dynamic sounds	
	ch = channels+NUM_AMBIENTS;
	for (i=NUM_AMBIENTS ; i<total_channels; i++, ch++)
	{
		if (!ch->sfx)
			continue;
		SND_Spatialize(ch);         // respatialize channel
		if (!ch->leftvol && !ch->rightvol)
			continue;

	// try to combine static sounds with a previous channel of the same
	// sound effect so we don't mix five torches every frame
	
		if (i >= MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS)
		{
		// see if it can just use the last one
			if (combine && combine->sfx == ch->sfx)
			{
				combine->leftvol += ch->leftvol;
				combine->rightvol += ch->rightvol;
				ch->leftvol = ch->rightvol = 0;
				continue;
			}
		// search for one
			combine = channels+MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;
			for (j=MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS ; j<i; j++, combine++)
			{
				if (combine->sfx == ch->sfx)
					break;
			}
					
			if (j == total_channels)
			{
				combine = NULL;
			}
			else
			{
				if (combine != ch)
				{
					combine->leftvol += ch->leftvol;
					combine->rightvol += ch->rightvol;
					ch->leftvol = ch->rightvol = 0;
				}
				continue;
			}
		}		
	}

//
// debugging output
//
	if (s_show.value)
	{
		total = 0;
		ch = channels;
		for (i=0 ; i<total_channels; i++, ch++)
		{
			if (ch->sfx && (ch->leftvol || ch->rightvol) )
			{
				//Com_Printf ("%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name);
				total++;
			}
		}
		
		Com_Printf ("----(%i)----\n", total);
	}

// mix some sound
	S_Update_();
}

void GetSoundtime (void)
{
	int		samplepos;
	static	int		buffers;
	static	int		oldsamplepos;
	int		fullsamples;
	
	fullsamples = dma.samples / dma.channels;

// it is possible to miscount buffers if it has wrapped twice between
// calls to S_Update.  Oh well.
	samplepos = SNDDMA_GetDMAPos();

	if (samplepos < oldsamplepos)
	{
		buffers++;					// buffer wrapped
		
		if (paintedtime > 0x40000000)
		{
			// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds (true);
		}
	}

	oldsamplepos = samplepos;

	soundtime = buffers*fullsamples + samplepos/dma.channels;
}

void S_Update_ (void)
{
	unsigned        endtime;
	int				samps;
	
	if (!sound_started || (snd_blocked > 0))
		return;

	SNDDMA_BeginPainting ();

	if (!dma.buffer)
		return;

// Updates DMA time
	GetSoundtime();

// check to make sure that we haven't overshot
	if (paintedtime < soundtime)
	{
		//Com_Printf ("S_Update_ : overflow\n");
		paintedtime = soundtime;
	}

// mix ahead of current position
	endtime = soundtime + s_mixahead.value * dma.speed;

// mix to an even submission block size
	endtime = (endtime + dma.submission_chunk-1)
		& ~(dma.submission_chunk-1);
	samps = dma.samples >> (dma.channels-1);
	if (endtime - soundtime > samps)
		endtime = soundtime + samps;

	S_PaintChannels (endtime);

	SNDDMA_Submit ();
}

/*
===============================================================================

console functions

===============================================================================
*/

void S_Play_f (void)
{
	static int hash=345;
	int 	i;
	char name[256];
	sfx_t	*sfx;

	if (!sound_started || s_nosound.value)
		return;

	for (i=1; i < Cmd_Argc(); i++)
	{
		strcpy(name, Cmd_Argv(i));
		COM_DefaultExtension (name, ".wav");
		sfx = S_FindName(name);
		S_StartSound(hash++, 0, sfx, listener_origin, 1.0, ATTN_NONE);
	}
}

void S_PlayVol_f (void)
{
	static int hash=543;
	int i;
	float vol;
	char name[256];
	sfx_t	*sfx;

	if (!sound_started || s_nosound.value)
		return;
	
	for (i=1; i < Cmd_Argc(); i+=2)
	{
		strcpy(name, Cmd_Argv(i));
		COM_DefaultExtension (name, ".wav");
		sfx = S_FindName(name);
		vol = Q_atof(Cmd_Argv(i+1));
		S_StartSound(hash++, 0, sfx, listener_origin, vol, ATTN_NONE);
	}
}

void S_SoundList_f (void)
{
	int		i;
	sfx_t	*sfx;
	sfxcache_t	*sc;
	int		size, total;

	total = 0;
	for (sfx=known_sfx, i=0 ; i<num_sfx ; i++, sfx++)
	{
		sc = (sfxcache_t *) Cache_Check (&sfx->cache);
		if (!sc)
			continue;
		size = sc->length*sc->width*(sc->stereo+1);
		total += size;
		if (sc->loopstart >= 0)
			Com_Printf ("L");
		else
			Com_Printf (" ");
		Com_Printf ("(%2db) %6i : %s\n",sc->width*8,  size, sfx->name);
	}
	Com_Printf ("Total resident: %i\n", total);
}


void S_LocalSound (char *sound)
{
	sfx_t	*sfx;

	if (!sound_started || s_nosound.value)
		return;
		
	sfx = S_PrecacheSound (sound);
	if (!sfx)
	{
		Com_Printf ("S_LocalSound: can't cache %s\n", sound);
		return;
	}
	S_StartSound (cl.playernum+1, -1, sfx, vec3_origin, 1, ATTN_NONE);
}

