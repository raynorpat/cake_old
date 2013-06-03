// cl_nqdemo.c

#include "quakedef.h"
#include "sound.h"
#include "cdaudio.h"
#include "cl_sbar.h"

void CL_FindModelNumbers (void);
void TP_NewMap (void);
void CL_ParseBaseline (entity_state_t *es);
void CL_ParseStatic (void);
void CL_ParseStaticSound (void);

#define	NQ_PROTOCOL_VERSION	15

#define	NQ_MAX_CLIENTS	16
#define NQ_SIGNONS		4

#define	NQ_MAX_EDICTS	600			// Must be <= MAX_CL_EDICTS!

// if the high bit of the servercmd is set, the low bits are fast update flags:
#define	NQ_U_MOREBITS	(1<<0)
#define	NQ_U_ORIGIN1	(1<<1)
#define	NQ_U_ORIGIN2	(1<<2)
#define	NQ_U_ORIGIN3	(1<<3)
#define	NQ_U_ANGLE2		(1<<4)
#define	NQ_U_NOLERP		(1<<5)		// don't interpolate movement
#define	NQ_U_FRAME		(1<<6)
#define NQ_U_SIGNAL		(1<<7)		// just differentiates from other updates
#define	NQ_U_ANGLE1		(1<<8)
#define	NQ_U_ANGLE3		(1<<9)
#define	NQ_U_MODEL		(1<<10)
#define	NQ_U_COLORMAP	(1<<11)
#define	NQ_U_SKIN		(1<<12)
#define	NQ_U_EFFECTS	(1<<13)
#define	NQ_U_LONGENTITY	(1<<14)

#define	SU_VIEWHEIGHT	(1<<0)
#define	SU_IDEALPITCH	(1<<1)
#define	SU_PUNCH1		(1<<2)
#define	SU_PUNCH2		(1<<3)
#define	SU_PUNCH3		(1<<4)
#define	SU_VELOCITY1	(1<<5)
#define	SU_VELOCITY2	(1<<6)
#define	SU_VELOCITY3	(1<<7)
//define	SU_AIMENT		(1<<8)  AVAILABLE BIT
#define	SU_ITEMS		(1<<9)
#define	SU_ONGROUND		(1<<10)		// no data follows, the bit is it
#define	SU_INWATER		(1<<11)		// no data follows, the bit is it
#define	SU_WEAPONFRAME	(1<<12)
#define	SU_ARMOR		(1<<13)
#define	SU_WEAPON		(1<<14)

// a sound with no channel is a local only sound
#define	NQ_SND_VOLUME		(1<<0)		// a byte
#define	NQ_SND_ATTENUATION	(1<<1)		// a byte
#define	NQ_SND_LOOPING		(1<<2)		// a long


//=========================================================================================

qbool	nq_drawpings;	// for sbar code

static qbool	nq_player_teleported;	// hacky

static vec3_t	nq_last_fixangle;

static int		nq_num_entities;
static int		nq_viewentity;
static int		nq_forcecdtrack;
int				nq_signon;
static int		nq_maxclients;
static float	nq_mtime[2];
static vec3_t	nq_mvelocity[2];
static vec3_t	nq_mviewangles[2];
static vec3_t	nq_mviewangles_temp;
static qbool	standard_quake = true;


static qbool CL_GetNQDemoMessage (void)
{
	int r, i;
	float f;

	// decide if it is time to grab the next message		
	if (cls.state == ca_active				// always grab until fully connected
		&& !(cl.paused & PAUSED_SERVER))	// or if the game was paused by server
	{
		if (cls.timedemo)
		{
			if (cls.framecount == cls.td_lastframe)
				return false;		// already read this frame's message
			cls.td_lastframe = cls.framecount;
			// if this is the second frame, grab the real td_starttime
			// so the bogus time on the first frame doesn't count
			if (cls.framecount == cls.td_startframe + 1)
				cls.td_starttime = cls.realtime;
		}
		else if (cl.time <= nq_mtime[0])
		{
			return false;		// don't need another message yet
		}
	}


	// get the next message
	fread (&net_message.cursize, 4, 1, cls.demofile);
	for (i=0 ; i<3 ; i++) {
		r = fread (&f, 4, 1, cls.demofile);
		nq_mviewangles_temp[i] = LittleFloat (f);
	}

	net_message.cursize = LittleLong (net_message.cursize);
	if (net_message.cursize > MAX_BIG_MSGLEN)
		Host_Error ("Demo message > MAX_BIG_MSGLEN");

	r = fread (net_message.data, net_message.cursize, 1, cls.demofile);
	if (r != 1) {
		Host_Error ("Unexpected end of demo");
	}

	return true;
}


static void NQD_BumpEntityCount (int num)
{
	if (num >= nq_num_entities)
		nq_num_entities = num + 1;
}


static void NQD_ParseClientdata (int bits)
{
	int		i, j;
	extern player_state_t view_message;

	if (bits & SU_VIEWHEIGHT)
		cl.viewheight = MSG_ReadChar ();
	else
		cl.viewheight = DEFAULT_VIEWHEIGHT;

	if (bits & SU_IDEALPITCH)
		MSG_ReadChar ();		// ignore
	
	VectorCopy (nq_mvelocity[0], nq_mvelocity[1]);
	for (i=0 ; i<3 ; i++)
	{
		if (bits & (SU_PUNCH1<<i) ) {
			if (i == 0)
				cl.punchangle = MSG_ReadChar ();
			else
				MSG_ReadChar();			// ignore
		}
		if (bits & (SU_VELOCITY1<<i) )
			nq_mvelocity[0][i] = MSG_ReadChar()*16;
		else
			nq_mvelocity[0][i] = 0;
	}

// [always sent]	if (bits & SU_ITEMS)
	i = MSG_ReadLong ();			// FIXME, check SU_ITEMS anyway? -- Tonik

	if (cl.stats[STAT_ITEMS] != i)
	{
		// set flash times
		for (j=0 ; j<32 ; j++)
			if ( (i & (1<<j)) && !(cl.stats[STAT_ITEMS] & (1<<j)))
				cl.item_gettime[j] = cl.time;
		cl.stats[STAT_ITEMS] = i;
	}
		
	cl.onground = (bits & SU_ONGROUND) != 0;
//	cl.inwater = (bits & SU_INWATER) != 0;

	if (bits & SU_WEAPONFRAME)
		view_message.weaponframe = MSG_ReadByte ();
	else
		view_message.weaponframe = 0;

	if (bits & SU_ARMOR)
		i = MSG_ReadByte ();
	else
		i = 0;
	if (cl.stats[STAT_ARMOR] != i)
		cl.stats[STAT_ARMOR] = i;

	if (bits & SU_WEAPON)
		i = MSG_ReadByte ();
	else
		i = 0;
	if (cl.stats[STAT_WEAPON] != i)
		cl.stats[STAT_WEAPON] = i;
	
	i = MSG_ReadShort ();
	if (cl.stats[STAT_HEALTH] != i)
		cl.stats[STAT_HEALTH] = i;

	i = MSG_ReadByte ();
	if (cl.stats[STAT_AMMO] != i)
		cl.stats[STAT_AMMO] = i;

	for (i=0 ; i<4 ; i++)
	{
		j = MSG_ReadByte ();
		if (cl.stats[STAT_SHELLS+i] != j)
			cl.stats[STAT_SHELLS+i] = j;
	}

	i = MSG_ReadByte ();

	if (standard_quake)
	{
		if (cl.stats[STAT_ACTIVEWEAPON] != i)
			cl.stats[STAT_ACTIVEWEAPON] = i;
	}
	else
	{
		if (cl.stats[STAT_ACTIVEWEAPON] != (1<<i))
			cl.stats[STAT_ACTIVEWEAPON] = (1<<i);
	}
}

/*
==================
NQD_ParseUpdatecolors
==================
*/
static void NQD_ParseUpdatecolors (void)
{
	int	i, colors;
	int top, bottom;

	i = MSG_ReadByte ();
	if (i >= nq_maxclients)
		Host_Error ("CL_ParseServerMessage: svc_updatecolors > NQ_MAX_CLIENTS");
	colors = MSG_ReadByte ();

	// fill in userinfo values
	top = min(colors & 15, 13);
	bottom = min((colors >> 4) & 15, 13);
	Info_SetValueForKey (cl.players[i].userinfo, "topcolor", va("%i", top), MAX_INFO_KEY);
	Info_SetValueForKey (cl.players[i].userinfo, "bottomcolor", va("%i", bottom), MAX_INFO_KEY);

	CL_NewTranslation (i);
}

			
/*
==================
NQD_ParsePrint
==================
*/
static void NQD_ParsePrint (void)
{
	extern cvar_t	cl_chatsound;

	char *s = MSG_ReadString();
	if (s[0] == 1) {	// chat
		if (cl_chatsound.value)
			S_LocalSound ("misc/talk.wav");
	}
	Com_Printf ("%s", s);
}


// JPG's ProQuake hacks
static int ReadPQByte (void) {
	int word;
	word = MSG_ReadByte() * 16;
	word += MSG_ReadByte() - 272;
	return word;
}
static int ReadPQShort (void) {
	int word;
	word = ReadPQByte() * 256;
	word += ReadPQByte();
	return word;
}

/*
==================
NQD_ParseStufftext
==================
*/
static void NQD_ParseStufftext (void)
{
	char	*s;
	byte	*p;

	if (msg_readcount + 7 <= net_message.cursize &&
		net_message.data[msg_readcount] == 1 && net_message.data[msg_readcount + 1] == 7)
	{
		int num, ping;
		MSG_ReadByte();
		MSG_ReadByte();
		while ((ping = ReadPQShort()) != 0)
		{
			num = ping / 4096;
			if ((unsigned int)num >= nq_maxclients)
				Host_Error ("Bad ProQuake message");
			cl.players[num].ping = ping & 4095;
			nq_drawpings = true;
		}
		// fall through to stufftext parsing (yes that's how it's intended by JPG)
	}

	s = MSG_ReadString ();
	Com_DPrintf ("stufftext: %s\n", s);

	for (p = (byte *)s; *p; p++) {
		if (*p > 32 && *p < 128)
			goto ok;
	}
	// ignore weird ProQuake stuff
	return;

ok:
	Cbuf_AddTextEx (&cbuf_svc, s);
}


/*
==================
NQD_ParseServerData
==================
*/
static void NQD_ParseServerData (void)
{
	char	*str;
	int		i;
	int		nummodels, numsounds;
	char	mapname[MAX_QPATH];

	Com_DPrintf ("Serverdata packet received.\n");
//
// wipe the client_state_t struct
//
	CL_ClearState ();

// parse protocol version number
	i = MSG_ReadLong ();
	if (i != NQ_PROTOCOL_VERSION)
		Host_Error ("Server returned version %i, not %i", i, NQ_PROTOCOL_VERSION);

// parse maxclients
	nq_maxclients = MSG_ReadByte ();
	if (nq_maxclients < 1 || nq_maxclients > NQ_MAX_CLIENTS)
		Host_Error ("Bad maxclients (%u) from server", nq_maxclients);

// parse gametype
	cl.gametype = MSG_ReadByte() ? GAME_DEATHMATCH : GAME_COOP;

// parse signon message
	str = MSG_ReadString ();
	strlcpy (cl.levelname, str, sizeof(cl.levelname));

// separate the printfs so the server message can have a color
	Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	Com_Printf ("%c%s\n", 2, str);

//
// first we go through and touch all of the precache data that still
// happens to be in the cache, so precaching something else doesn't
// needlessly purge it
//

// precache models
	for (nummodels=1 ; ; nummodels++)
	{
		str = MSG_ReadString ();
		if (!str[0])
			break;
		if (nummodels == MAX_MODELS)
			Host_Error ("Server sent too many model precaches");
		strlcpy (cl.model_name[nummodels], str, sizeof(cl.model_name[0]));
		Mod_TouchModel (str);
	}

// precache sounds
	for (numsounds=1 ; ; numsounds++)
	{
		str = MSG_ReadString ();
		if (!str[0])
			break;
		if (numsounds == MAX_SOUNDS)
			Host_Error ("Server sent too many sound precaches");
		strlcpy (cl.sound_name[numsounds], str, sizeof(cl.sound_name[0]));
		S_TouchSound (str);
	}

//
// now we try to load everything else until a cache allocation fails
//
	cl.clipmodels[1] = CM_LoadMap (cl.model_name[1], true, NULL, &cl.map_checksum2);

	for (i = 1; i < nummodels; i++)
	{
		cl.model_precache[i] = Mod_ForName (cl.model_name[i], false);
		if (cl.model_precache[i] == NULL)
			Host_Error ("Model %s not found", cl.model_name[i]);

		if (cl.model_name[i][0] == '*')
			cl.clipmodels[i] = CM_InlineModel(cl.model_name[i]);
	}

	for (i=1 ; i<numsounds ; i++) {
		cl.sound_precache[i] = S_PrecacheSound (cl.sound_name[i]);
	}


// local state
	if (!cl.model_precache[1])
		Host_Error ("NQD_ParseServerData: NULL worldmodel");

	COM_StripExtension (COM_SkipPath (cl.model_name[1]), mapname);
	Cvar_ForceSet (&host_mapname, mapname);

	CL_ClearParticles ();
	CL_FindModelNumbers ();
	R_NewMap (cl.model_precache[1]);

	TP_NewMap ();

	nq_signon = 0;
	nq_num_entities = 0;
	nq_drawpings = false;	// unless we have the ProQuake extension
	cl.servertime_works = true;
	r_refdef2.allow_cheats = true;	// why not
	cls.state = ca_onserver;
}

void CLNQ_SignonReply (void)
{
	extern cvar_t name, topcolor, bottomcolor;

	Com_DPrintf ("CL_SignonReply: %i\n", nq_signon);

	switch (nq_signon)
	{
	case 1:
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "prespawn");
		break;

	case 2:
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va("name \"%s\"\n", name.string));
	
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va("color %i %i\n", (int)topcolor.value, (int)bottomcolor.value));
	
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "spawn");
		break;

	case 3:
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "begin");
		break;

	case 4:
		//SCR_EndLoadingPlaque ();		// allow normal screen updates
		break;
	}
}


/*
==================
NQD_ParseStartSoundPacket
==================
*/
static void NQD_ParseStartSoundPacket(void)
{
    vec3_t  pos;
    int 	channel, ent;
    int 	sound_num;
    int 	volume;
    int 	field_mask;
    float 	attenuation;  
 	int		i;
	           
    field_mask = MSG_ReadByte(); 

    if (field_mask & NQ_SND_VOLUME)
		volume = MSG_ReadByte ();
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;
	
    if (field_mask & NQ_SND_ATTENUATION)
		attenuation = MSG_ReadByte () / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;
	
	channel = MSG_ReadShort ();
	sound_num = MSG_ReadByte ();

	ent = channel >> 3;
	channel &= 7;

	if (ent > NQ_MAX_EDICTS)
		Host_Error ("NQD_ParseStartSoundPacket: ent = %i", ent);
	
	for (i=0 ; i<3 ; i++)
		pos[i] = MSG_ReadCoord ();
 
    S_StartSound (ent, channel, cl.sound_precache[sound_num], pos, volume/255.0, attenuation);
}       


/*
==================
NQD_ParseUpdate

Parse an entity update message from the server
If an entities model or origin changes from frame to frame, it must be
relinked.  Other attributes can change without relinking.
==================
*/
static void NQD_ParseUpdate (int bits)
{
	int			i;
//	model_t		*model;
	int			modnum;
	qbool		forcelink;
	centity_t	*ent;
	entity_state_t	*state;
	int			num;

	if (nq_signon == NQ_SIGNONS - 1)
	{	// first update is the final signon stage
		nq_signon = NQ_SIGNONS;
		Con_ClearNotify ();
		//TP_ExecTrigger ("f_spawn");
		SCR_EndLoadingPlaque ();
		cls.state = ca_active;
	}

	if (bits & NQ_U_MOREBITS)
	{
		i = MSG_ReadByte ();
		bits |= (i<<8);
	}

	if (bits & NQ_U_LONGENTITY)	
		num = MSG_ReadShort ();
	else
		num = MSG_ReadByte ();

	if (num >= NQ_MAX_EDICTS)
		Host_Error ("NQD_ParseUpdate: ent > MAX_EDICTS");

	NQD_BumpEntityCount (num);

	ent = &cl_entities[num];
	ent->previous = ent->current;
	ent->current = ent->baseline;
	state = &ent->current;
	state->number = num;

	if (ent->lastframe != cl_entframecount-1)
		forcelink = true;	// no previous frame to lerp from
	else
		forcelink = false;

	ent->prevframe = ent->lastframe;
	ent->lastframe = cl_entframecount;
	
	if (bits & NQ_U_MODEL)
	{
		modnum = MSG_ReadByte ();
		if (modnum >= MAX_MODELS)
			Host_Error ("CL_ParseModel: bad modnum");
	}
	else
		modnum = ent->baseline.modelindex;
		
//	model = cl.model_precache[modnum];
	if (modnum != state->modelindex)
	{
		state->modelindex = modnum;
	// automatic animation (torches, etc) can be either all together
	// or randomized
		if (modnum)
		{
			/*if (model->synctype == ST_RAND)
				state->syncbase = (float)(rand()&0x7fff) / 0x7fff;
			else
				state->syncbase = 0.0;
				*/
		}
		else
			forcelink = true;	// hack to make null model players work
	}
	
	if (bits & NQ_U_FRAME)
		state->frame = MSG_ReadByte ();
	else
		state->frame = ent->baseline.frame;

	if (bits & NQ_U_COLORMAP)
		i = MSG_ReadByte();
	else
		i = ent->baseline.colormap;

	state->colormap = i;

	if (bits & NQ_U_SKIN)
		state->skinnum = MSG_ReadByte();
	else
		state->skinnum = ent->baseline.skinnum;

	if (bits & NQ_U_EFFECTS)
		state->effects = MSG_ReadByte();
	else
		state->effects = 0;

	if (bits & NQ_U_ORIGIN1)
		state->s_origin[0] = MSG_ReadShort ();
	else
		state->s_origin[0] = ent->baseline.s_origin[0];
	if (bits & NQ_U_ANGLE1)
		state->s_angles[0] = MSG_ReadChar ();
	else
		state->s_angles[0] = ent->baseline.s_angles[0];

	if (bits & NQ_U_ORIGIN2)
		state->s_origin[1] = MSG_ReadShort ();
	else
		state->s_origin[1] = ent->baseline.s_origin[1];
	if (bits & NQ_U_ANGLE2)
		state->s_angles[1] = MSG_ReadChar ();
	else
		state->s_angles[1] = ent->baseline.s_angles[1];

	if (bits & NQ_U_ORIGIN3)
		state->s_origin[2] = MSG_ReadShort ();
	else
		state->s_origin[2] = ent->baseline.s_origin[2];
	if (bits & NQ_U_ANGLE3)
		state->s_angles[2] = MSG_ReadChar ();
	else
		state->s_angles[2] = ent->baseline.s_angles[2];

	if ( bits & NQ_U_NOLERP )
		forcelink = true;

	if ( forcelink )
	{	// didn't have an update last message
		VectorCopy (state->s_origin, ent->previous.s_origin);
		MSG_UnpackOrigin (state->s_origin, ent->trail_origin);
		VectorCopy (state->s_angles, ent->previous.s_angles);
		//ent->forcelink = true;
	}
}


/*
===============
NQD_LerpPoint

Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
static float NQD_LerpPoint (void)
{
	float	f, frac;

	f = nq_mtime[0] - nq_mtime[1];
	
	if (!f || /* cl_nolerp.value || */ cls.timedemo) {
		cl.time = nq_mtime[0];
		return 1;
	}
		
	if (f > 0.1)
	{	// dropped packet, or start of demo
		nq_mtime[1] = nq_mtime[0] - 0.1;
		f = 0.1;
	}
	frac = (cl.time - nq_mtime[1]) / f;
	if (frac < 0)
	{
		if (frac < -0.01)
			cl.time = nq_mtime[1];
		frac = 0;
	}
	else if (frac > 1)
	{
		if (frac > 1.01)
			cl.time = nq_mtime[0];
		frac = 1;
	}
		
	return frac;
}



extern int	cl_playerindex; 
extern int	cl_rocketindex, cl_grenadeindex;

static void NQD_LerpPlayerinfo (float f)
{
	if (cl.intermission) {
		// just stay there
		return;
	}

	if (nq_player_teleported) {
		VectorCopy (nq_mvelocity[0], cl.simvel);
		VectorCopy (nq_mviewangles[0], cl.viewangles);
		if (cls.demoplayback)
			VectorCopy (nq_mviewangles[0], cl.simangles);
		return;
	}

	LerpVector (nq_mvelocity[1], nq_mvelocity[0], f, cl.simvel);
	if (cls.demoplayback) {
		LerpAngles (nq_mviewangles[1], nq_mviewangles[0], f, cl.simangles);
		VectorCopy (cl.simangles, cl.viewangles);
	}
}

void NQD_LinkEntities (void)
{
	entity_t			ent;
	centity_t			*cent;
	entity_state_t		*state;
	float				f;
	struct model_s		*model;
	int					modelflags;
	cdlight_t			*dl;
	vec3_t				cur_origin, old_origin;
	float				autorotate;
	int					i;
	int					num;

	f = NQD_LerpPoint ();

	NQD_LerpPlayerinfo (f);

	autorotate = anglemod (100*cl.time);

	memset (&ent, 0, sizeof(ent));

	for (num = 1; num < nq_num_entities; num++)
	{
		cent = &cl_entities[num];
		state = &cent->current;

		if (cent->lastframe != cl_entframecount)
			continue;		// not present in this frame

		MSG_UnpackOrigin (state->s_origin, cur_origin);

		if (state->effects & EF_BRIGHTFIELD)
			CL_EntityParticles (cur_origin);

		// spawn light flashes, even ones coming from invisible objects
		if (state->effects & EF_MUZZLEFLASH) {
			vec3_t		angles, forward;

			dl = CL_AllocDlight (-num);
			MSG_UnpackAngles (state->s_angles, angles);
			AngleVectors (angles, forward, NULL, NULL);
			VectorMA (cur_origin, 18, forward, dl->origin);
			dl->origin[2] += 16;
			dl->radius = 200 + (rand()&31);
			dl->minlight = 32;
			dl->die = cl.time + 0.1;

			R_ColorDLight (dl, 408, 242, 117);

			if (state->modelindex == cl_playerindex)
			{
				if (cl.stats[STAT_ACTIVEWEAPON] == IT_SUPER_LIGHTNING)
				{
					// switch the dlight colour
					R_ColorLightningLight (dl);
				}
				else if (cl.stats[STAT_ACTIVEWEAPON] == IT_LIGHTNING)
				{
					// switch the dlight colour
					R_ColorLightningLight (dl);
				}
			
				R_ColorDLight (dl, 255, 255, 255);
			}
		}
		if (state->effects & EF_BRIGHTLIGHT) {
			if (state->modelindex != cl_playerindex || r_powerupglow.value) {
				dl = CL_AllocDlight (-num);
				VectorCopy (cur_origin,  dl->origin);
				dl->origin[2] += 16;
				dl->radius = 400 + (rand() & 31);
				dl->die = cl.time + 0.001;
			}
		}
		if (state->effects & EF_DIMLIGHT) {
			if (state->modelindex != cl_playerindex || r_powerupglow.value) {
				dl = CL_AllocDlight (-num);
				VectorCopy (cur_origin,  dl->origin);
				dl->radius = 200 + (rand() & 31);
				dl->die = cl.time + 0.001;

				// powerup dynamic lights
				if (state->modelindex == cl_playerindex)
				{
					int AverageColour;
					int rgb[3];

					rgb[0] = 64;
					rgb[1] = 64;
					rgb[2] = 64;

					if (cl.stats[STAT_ITEMS] & IT_QUAD) rgb[2] = 255;
					if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) rgb[0] = 255;

					// re-balance the colours
					AverageColour = (rgb[0] + rgb[1] + rgb[2]) / 3;

					rgb[0] = rgb[0] * 255 / AverageColour;
					rgb[1] = rgb[1] * 255 / AverageColour;
					rgb[2] = rgb[2] * 255 / AverageColour;

					R_ColorDLight (dl, rgb[0], rgb[1], rgb[2]);
				}
			}
		}

		// if set to invisible, skip
		if (!state->modelindex)
			continue;

		cent->current = *state;

		ent.model = model = cl.model_precache[state->modelindex];
		if (!model)
			Host_Error ("CL_LinkPacketEntities: bad modelindex");

		if (cl_r2g.value && cl_grenadeindex != -1)
			if (state->modelindex == cl_rocketindex)
				ent.model = cl.model_precache[cl_grenadeindex];

		modelflags = R_ModelFlags (model);

		// rotate binary objects locally
		if (modelflags & MF_ROTATE)
		{
			ent.angles[0] = 0;
			ent.angles[1] = autorotate;
			ent.angles[2] = 0;
		}
		else
		{
			vec3_t	old, cur;

			MSG_UnpackAngles (cent->current.s_angles, old);
			MSG_UnpackAngles (cent->previous.s_angles, cur);
			LerpAngles (old, cur, f, ent.angles);
		}

if (num == nq_viewentity) {
extern float nq_speed;
float f;
nq_speed = 0;
	for (i = 0; i < 3; i++) {
		f = (cent->current.s_origin[i] - cent->previous.s_origin[i]) * 0.125;
		nq_speed += f * f;
	}
if (nq_speed) nq_speed = sqrt(nq_speed);
nq_speed /= nq_mtime[0] - nq_mtime[1];

}

		// calculate origin
		for (i = 0; i < 3; i++)
		{
			if (abs(cent->current.s_origin[i] - cent->previous.s_origin[i]) > 128 * 8) {
				// teleport or something, don't lerp
				VectorCopy (cur_origin, ent.origin);
				if (num == nq_viewentity)
					nq_player_teleported = true;
				break;
			}
			ent.origin[i] = cent->previous.s_origin[i] * 0.125 + 
				f * (cur_origin[i] - cent->previous.s_origin[i] * 0.125);
		}

		if (num == nq_viewentity) {
			VectorCopy (ent.origin, cent->trail_origin);	// FIXME?
			continue;			// player entity
		}

		if (cl_deadbodyfilter.value && state->modelindex == cl_playerindex
			&& ( (i=state->frame)==49 || i==60 || i==69 || i==84 || i==93 || i==102) )
			continue;

		if (cl_gibfilter.value && cl.modelinfos[state->modelindex] == mi_gib)
			continue;

		// set colormap
		if (state->colormap && state->colormap <= MAX_CLIENTS
			&& state->modelindex == cl_playerindex)
			ent.colormap = state->colormap;
		else
			ent.colormap = 0;

		// set skin
		ent.skinnum = state->skinnum;
		
		// set frame
		ent.frame = state->frame;

		// add automatic particle trails
		if ((modelflags & ~MF_ROTATE))
		{
			if (false /*cl_entframecount == 1 || cent->lastframe != cl_entframecount-1*/)
			{	// not in last message
				VectorCopy (ent.origin, old_origin);
			}
			else
			{
				VectorCopy (cent->trail_origin, old_origin);

				for (i=0 ; i<3 ; i++)
					if ( abs(old_origin[i] - ent.origin[i]) > 128)
					{	// no trail if too far
						VectorCopy (ent.origin, old_origin);
						break;
					}
			}

			if (modelflags & MF_ROCKET)
			{
				if (r_rockettrail.value) {
					if (r_rockettrail.value == 2)
						CL_GrenadeTrail (old_origin, ent.origin, cent->trail_origin);
					else
						CL_RocketTrail (old_origin, ent.origin, cent->trail_origin);
				} else
					VectorCopy (ent.origin, cent->trail_origin);


				if (r_rocketlight.value) {
					dl = CL_AllocDlight (state->number);
					VectorCopy (ent.origin, dl->origin);
					dl->radius = 200;
					dl->die = cl.time + 0.01;

					R_ColorDLight (dl, 408, 242, 117);
				}
			}
			else if (modelflags & MF_GRENADE && r_grenadetrail.value)
				CL_GrenadeTrail (old_origin, ent.origin, cent->trail_origin);
			else if (modelflags & MF_GIB)
				CL_BloodTrail (old_origin, ent.origin, cent->trail_origin);
			else if (modelflags & MF_ZOMGIB)
				CL_SlightBloodTrail (old_origin, ent.origin, cent->trail_origin);
			else if (modelflags & MF_TRACER) {
				CL_TracerTrail (old_origin, ent.origin, cent->trail_origin, 52);

				dl = CL_AllocDlight (state->number);
				VectorCopy (ent.origin, dl->origin);
				dl->radius = 200;
				dl->die = cl.time + 0.01;

				R_ColorWizLight (dl);
			} else if (modelflags & MF_TRACER2) {
				CL_TracerTrail (old_origin, ent.origin, cent->trail_origin, 230);

				dl = CL_AllocDlight (state->number);
				VectorCopy (ent.origin, dl->origin);
				dl->radius = 200;
				dl->die = cl.time + 0.01;

				R_ColorDLight (dl, 408, 242, 117);
			} else if (modelflags & MF_TRACER3) {
				CL_VoorTrail (old_origin, ent.origin, cent->trail_origin);

				dl = CL_AllocDlight (state->number);
				VectorCopy (ent.origin, dl->origin);
				dl->radius = 200;
				dl->die = cl.time + 0.01;

				R_ColorDLight (dl, 399, 141, 228);
			}
		}

		cent->lastframe = cl_entframecount;
		V_AddEntity (&ent);
	}

	if (nq_viewentity == 0)
		Host_Error ("viewentity == 0");
	VectorCopy (cl_entities[nq_viewentity].trail_origin, cl.simorg);
}





extern char *svc_strings[];
extern const int num_svc_strings;

#define SHOWNET(x) {if(cl_shownet.value==2)Com_Printf ("%3i:%s\n", msg_readcount-1, x);}

void CLNQ_ParseServerMessage (void)
{
	int		cmd;
	int		i;
	qbool	message_with_datagram;		// hack to fix glitches when receiving a packet
											// without a datagram

	nq_player_teleported = false;		// OMG, it's a hack!
	message_with_datagram = false;
	cl_entframecount++;

	if (cl_shownet.value == 1)
		Com_Printf ("%i ", net_message.cursize);
	else if (cl_shownet.value == 2)
		Com_Printf ("------------------\n");
	
	cl.onground = false;	// unless the server says otherwise	

//
// parse the message
//
	//MSG_BeginReading ();
	
	while (1)
	{
		if (msg_badread)
			Host_Error ("CL_ParseServerMessage: Bad server message");

		cmd = MSG_ReadByte ();

		if (cmd == -1)
		{
			SHOWNET("END OF MESSAGE");
			if (!message_with_datagram) {
				cl_entframecount--;
			}
			else
			{
				VectorCopy (nq_mviewangles[0], nq_mviewangles[1]);
				VectorCopy (nq_mviewangles_temp, nq_mviewangles[0]);
			}
			return;		// end of message
		}

	// if the high bit of the command byte is set, it is a fast update
		if (cmd & 128)
		{
			SHOWNET("fast update");
			NQD_ParseUpdate (cmd&127);
			continue;
		}

		if (cmd < num_svc_strings)
			SHOWNET(svc_strings[cmd]);
	
	// other commands
		switch (cmd)
		{
		default:
			Host_Error ("CL_ParseServerMessage: Illegible server message");
			break;

		case svc_nop:
			break;

		case nq_svc_time:
			nq_mtime[1] = nq_mtime[0];
			nq_mtime[0] = MSG_ReadFloat ();
			cl.servertime = nq_mtime[0];
			message_with_datagram = true;
			break;

		case nq_svc_clientdata:
			i = MSG_ReadShort ();
			NQD_ParseClientdata (i);
			break;

		case nq_svc_version:
			i = MSG_ReadLong ();
			if (i != NQ_PROTOCOL_VERSION)
				Host_Error ("CL_ParseServerMessage: Server is protocol %i instead of %i\n", i, NQ_PROTOCOL_VERSION);
			break;

		case svc_disconnect:
			Com_Printf ("\n======== End of demo ========\n\n");
			CL_NextDemo ();
			Host_EndGame ();
			Host_Abort ();
			break;

		case svc_print:
			NQD_ParsePrint ();
			break;
			
		case svc_centerprint:
			SCR_CenterPrint (MSG_ReadString ());
			break;

		case svc_stufftext:
			NQD_ParseStufftext ();
			break;

		case svc_damage:
			V_ParseDamage ();
			break;

		case svc_serverdata:
			NQD_ParseServerData ();
			break;

		case svc_setangle:
			for (i=0 ; i<3 ; i++)
				nq_last_fixangle[i] = cl.simangles[i] = cl.viewangles[i] = MSG_ReadAngle ();
			break;

		case svc_setview:
			nq_viewentity = MSG_ReadShort ();
			if (nq_viewentity <= nq_maxclients)
				cl.playernum = nq_viewentity - 1;
			else	{
				// just let cl.playernum stay where it was
			}
			break;

		case svc_lightstyle:
			i = MSG_ReadByte ();
			if (i >= MAX_LIGHTSTYLES)
				Sys_Error ("svc_lightstyle > MAX_LIGHTSTYLES");
			strlcpy (cl_lightstyle[i].map,  MSG_ReadString(), sizeof(cl_lightstyle[0].map));
			cl_lightstyle[i].length = strlen(cl_lightstyle[i].map);
			break;

		case svc_sound:
			NQD_ParseStartSoundPacket();
			break;

		case svc_stopsound:
			i = MSG_ReadShort();
			S_StopSound(i>>3, i&7);
			break;

		case nq_svc_updatename:
			i = MSG_ReadByte ();
			if (i >= nq_maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatename > NQ_MAX_CLIENTS");
			strlcpy (cl.players[i].name, MSG_ReadString(), sizeof(cl.players[i].name));
			break;

		case svc_updatefrags:
			i = MSG_ReadByte ();
			if (i >= nq_maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatefrags > NQ_MAX_CLIENTS");
			cl.players[i].frags = MSG_ReadShort();
			break;

		case nq_svc_updatecolors:
			NQD_ParseUpdatecolors ();
			break;
			
		case nq_svc_particle:
			CL_ParseParticleEffect ();
			break;

		case svc_spawnbaseline:
			i = MSG_ReadShort ();
			if (i >= NQ_MAX_EDICTS)
				Host_Error ("svc_spawnbaseline: ent > MAX_EDICTS");
			NQD_BumpEntityCount (i);
			CL_ParseBaseline (&cl_entities[i].baseline);
			break;
		case svc_spawnstatic:
			CL_ParseStatic ();
			break;
		case svc_temp_entity:
			CL_ParseTEnt ();
			break;

		case svc_setpause:
			if (MSG_ReadByte() != 0)
				cl.paused |= PAUSED_SERVER;
			else
				cl.paused &= ~PAUSED_SERVER;

			if (cl.paused)
				CDAudio_Pause ();
			else
				CDAudio_Resume ();
			break;

		case nq_svc_signonnum:
			i = MSG_ReadByte ();
			if (i <= nq_signon)
				Host_Error ("Received signon %i when at %i", i, nq_signon);
			nq_signon = i;
			CLNQ_SignonReply ();
			break;

		case svc_killedmonster:
			cl.stats[STAT_MONSTERS]++;
			break;

		case svc_foundsecret:
			cl.stats[STAT_SECRETS]++;
			break;

		case svc_updatestat:
			i = MSG_ReadByte ();
			if (i < 0 || i >= MAX_CL_STATS)
				Sys_Error ("svc_updatestat: %i is invalid", i);
			cl.stats[i] = MSG_ReadLong ();;
			break;

		case svc_spawnstaticsound:
			CL_ParseStaticSound ();
			break;

		case svc_cdtrack:
			cl.cdtrack = MSG_ReadByte ();
			MSG_ReadByte();		// loop track (unused)
			if (nq_forcecdtrack != -1)
				CDAudio_Play ((byte)nq_forcecdtrack, true);
			else
				CDAudio_Play ((byte)cl.cdtrack, true);
			break;

		case svc_intermission:
			cl.intermission = 1;
			cl.completed_time = cl.time;
			VectorCopy (nq_last_fixangle, cl.simangles);
			break;

		case svc_finale:
			cl.intermission = 2;
			cl.completed_time = cl.time;
			SCR_CenterPrint (MSG_ReadString ());
			VectorCopy (nq_last_fixangle, cl.simangles);
			break;

		case nq_svc_cutscene:
			cl.intermission = 3;
			cl.completed_time = cl.time;
			SCR_CenterPrint (MSG_ReadString ());
			VectorCopy (nq_last_fixangle, cl.simangles);
			break;

		case svc_sellscreen:
			break;
		}
	}

}


void NQD_ReadPackets (void)
{
	while (CL_GetNQDemoMessage()) {
		MSG_BeginReading ();
		CLNQ_ParseServerMessage();
	}
}


void NQD_StartPlayback ()
{
	int		c;
	qbool	neg = false;

	// parse forced cd track
	while ((c = getc(cls.demofile)) != '\n') {
		if (c == '-')
			neg = true;
		else
			nq_forcecdtrack = nq_forcecdtrack * 10 + (c - '0');
	}
	if (neg)
		nq_forcecdtrack = -nq_forcecdtrack;

	cl.spectator = false;
	nq_signon = 0;
	nq_mtime[0] = 0;
	nq_maxclients = 0;
}
