/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.h"
#include "pmove.h"
#include "teamplay.h"

void Cam_TryLock (void);
void CL_FinishTimeDemo (void);

// .qwz playback
static qbool	qwz_unpacking = false;

#ifdef _WIN32
static qbool	qwz_playback = false;
static HANDLE	hQizmoProcess = NULL;
static char tempqwd_name[256] = ""; // this file must be deleted
									// after playback is finished
void CheckQizmoCompletion (void);
static void StopQWZPlayback (void);
#endif

/*
==============================================================================

DEMO CODE

When a demo is playing back, all NET_SendMessages are skipped, and
NET_GetMessages are read from the demo file.

Whenever cl.time gets past the last received message, another message is
read from the demo file.
==============================================================================
*/

/*
==============
CL_StopPlayback

Called when a demo file runs out, or the user starts a game
==============
*/
void CL_StopPlayback (void)
{
	if (!cls.demoplayback)
		return;

	if (cls.demofile)
		FS_Close (cls.demofile);
	cls.demofile = NULL;
	cls.demoplayback = false;
	cls.nqprotocol = false;
#ifdef MVDPLAY
	cls.mvdplayback = false;
#endif

#ifdef _WIN32
	if (qwz_playback)
		StopQWZPlayback ();
#endif

	if (cls.timedemo)
		CL_FinishTimeDemo ();
}

/*
====================
CL_WriteDemoCmd

Writes the current user cmd
====================
*/
void CL_WriteDemoCmd (usercmd_t *pcmd)
{
	int		i;
	float	fl, t[3];
	byte	c;
	usercmd_t cmd;

//Com_Printf ("write: %ld bytes, %4.4f\n", msg->cursize, cls.realtime);

	fl = LittleFloat((float)cls.realtime);
	FS_Write (cls.demofile, &fl, sizeof(fl));

	c = dem_cmd;
	FS_Write (cls.demofile, &c, sizeof(c));

	// correct for byte order, bytes don't matter
	cmd = *pcmd;

	for (i = 0; i < 3; i++)
		cmd.angles[i] = LittleFloat(cmd.angles[i]);
	cmd.forwardmove = LittleShort(cmd.forwardmove);
	cmd.sidemove    = LittleShort(cmd.sidemove);
	cmd.upmove      = LittleShort(cmd.upmove);

	FS_Write(cls.demofile, &cmd, sizeof(cmd));

	t[0] = LittleFloat (cl.viewangles[0]);
	t[1] = LittleFloat (cl.viewangles[1]);
	t[2] = LittleFloat (cl.viewangles[2]);
	FS_Write (cls.demofile, t, 12);
}

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/
void CL_WriteDemoMessage (sizebuf_t *msg)
{
	int		len;
	float	fl;
	byte	c;

//Com_Printf ("write: %ld bytes, %4.4f\n", msg->cursize, cls.realtime);

	if (!cls.demorecording)
		return;

	fl = LittleFloat((float)cls.realtime);
	FS_Write (cls.demofile, &fl, sizeof(fl));

	c = dem_read;
	FS_Write (cls.demofile, &c, sizeof(c));

	len = LittleLong (msg->cursize);
	FS_Write (cls.demofile, &len, 4);
	FS_Write (cls.demofile, msg->data, msg->cursize);
}

/*
====================
CL_GetDemoMessage

FIXME...
====================
*/
#ifdef MVDPLAY
extern void CL_ParseClientdata ();
#endif
qbool CL_GetDemoMessage (void)
{
	int		r, i, j;
	float	demotime;
	byte	c;
	usercmd_t *pcmd;
#ifdef MVDPLAY
	byte	msec = 0;
#endif

	if (qwz_unpacking)
		return false;

	if (cl.paused & PAUSED_DEMO)
		return false;

readnext:
	// read the time from the packet
#ifdef MVDPLAY
	if (cls.mvdplayback) {
		FS_Read(cls.demofile, &msec, sizeof(msec));
		demotime = cls.mvd_newtime + msec * 0.001;
	}
	else
#endif
	{
		FS_Read(cls.demofile, &demotime, sizeof(demotime));
		demotime = LittleFloat(demotime);
    }

// decide if it is time to grab the next message		
	if (cls.timedemo) {
		if (cls.td_lastframe < 0)
			cls.td_lastframe = demotime;
		else if (demotime > cls.td_lastframe) {
			cls.td_lastframe = demotime;
			// rewind back to time
#ifdef MVDPLAY
			if (cls.mvdplayback) {
				FS_Seek(cls.demofile, FS_Tell(cls.demofile) - sizeof(msec), SEEK_SET);
			} else 
#endif
				FS_Seek(cls.demofile, FS_Tell(cls.demofile) - sizeof(demotime), SEEK_SET);
			return false;	// already read this frame's message
		}
		if (!cls.td_starttime && cls.state == ca_active) {
			cls.td_starttime = Sys_DoubleTime();
			cls.td_startframe = cls.framecount;
		}
		cls.demotime = demotime; // warp
	} else if (!(cl.paused & PAUSED_SERVER) && cls.state >= ca_active) {	// always grab until active
#ifdef MVDPLAY
		if (cls.mvdplayback)
		{
			if (msec/* a hack! */ && cls.demotime < cls.mvd_newtime) {
				FS_Seek(cls.demofile, FS_Tell(cls.demofile) - sizeof(msec), SEEK_SET);
				return false;
			}
		}
        else
#endif
		if (cls.demotime < demotime) {
            if (cls.demotime + 1.0 < demotime)
                cls.demotime = demotime - 1.0; // too far back
			// rewind back to time
			FS_Seek(cls.demofile, FS_Tell(cls.demofile) - sizeof(demotime), SEEK_SET);
			return false;		// don't need another message yet
		}
	} else
		cls.demotime = demotime; // we're warping


#ifdef MVDPLAY
	if (cls.mvdplayback)
	{
		if (msec)
		{
			cls.mvd_oldtime = cls.mvd_newtime;
			cls.mvd_newtime = demotime;
			cls.netchan.incoming_sequence++;
			cls.netchan.incoming_acknowledged++;

			CL_ParseClientdata();
		}
	}
#endif

	if (cls.state < ca_demostart)
		Host_Error ("CL_GetDemoMessage: cls.state < ca_demostart");
	
	// get the msg type
	r = FS_Read (cls.demofile, &c, sizeof(c));
	if (r == 0)
		Host_Error ("Unexpected end of demo");
	
#ifdef MVDPLAY
    switch (cls.mvdplayback ? c&7 : c) {
#else
	switch (c) {
#endif
	case dem_cmd :
		// user sent input
		i = cls.netchan.outgoing_sequence & UPDATE_MASK;
		pcmd = &cl.frames[i].cmd;
		r = FS_Read (cls.demofile, pcmd, sizeof(*pcmd));
		if (r == 0)
			Host_Error ("Unexpected end of demo");
		// byte order stuff
		for (j = 0; j < 3; j++)
			pcmd->angles[j] = LittleFloat(pcmd->angles[j]);
		pcmd->forwardmove = LittleShort(pcmd->forwardmove);
		pcmd->sidemove    = LittleShort(pcmd->sidemove);
		pcmd->upmove      = LittleShort(pcmd->upmove);
		cl.frames[i].senttime = demotime + (cls.realtime - cls.demotime);
		cl.frames[i].receivedtime = -1;		// we haven't gotten a reply yet
		cls.netchan.outgoing_sequence++;

		FS_Read (cls.demofile, cl.viewangles, 12);
		for (i = 0; i < 3; i++)
			cl.viewangles[i] = LittleFloat (cl.viewangles[i]);
		if (cl.spectator)
			Cam_TryLock ();
		goto readnext;

	case dem_read:
#ifdef MVDPLAY
readit:
#endif
		// get the next message
		FS_Read (cls.demofile, &net_message.cursize, 4);
		net_message.cursize = LittleLong (net_message.cursize);
		if (net_message.cursize > MAX_BIG_MSGLEN)
			Host_Error ("Demo message > MAX_BIG_MSGLEN");
		r = FS_Read (cls.demofile, net_message.data, net_message.cursize);
		if (r == 0)
			Host_Error ("Unexpected end of demo");

#ifdef MVDPLAY
		if (cls.mvdplayback) {
			switch(cls.mvd_lasttype) {
			case dem_multiple:
				if (!cam_locked || !(cls.mvd_lastto & (1 << cam_curtarget)))
					goto readnext;	
				break;
			case dem_single:
				if (!cam_locked || cls.mvd_lastto != cam_curtarget)
					goto readnext;
				break;
			}
		}
#endif

		return true;

	case dem_set:
		FS_Read (cls.demofile, &i, 4);
		cls.netchan.outgoing_sequence = LittleLong(i);
		FS_Read (cls.demofile, &i, 4);
		cls.netchan.incoming_sequence = LittleLong(i);
#ifdef MVDPLAY
		if (cls.mvdplayback)
			cls.netchan.incoming_acknowledged = cls.netchan.incoming_sequence;
#endif
		goto readnext;

#ifdef MVDPLAY
	case dem_multiple:
		r = FS_Read (cls.demofile, &i, 4);
		if (r == 0)
			Host_Error ("Unexpected end of demo");
		cls.mvd_lastto = LittleLong(i);
		cls.mvd_lasttype = dem_multiple;
		goto readit;

	case dem_single:
		cls.mvd_lastto = c>>3;
		cls.mvd_lasttype = dem_single;
		goto readit;

	case dem_stats:
		cls.mvd_lastto = c>>3;
		cls.mvd_lasttype = dem_stats;
		goto readit;

	case dem_all:
		cls.mvd_lastto = 0;
		cls.mvd_lasttype = dem_all;
		goto readit;
#endif

	default:
		Host_Error ("Corrupted demo 3 -> c='%d'", c);
		return false;
	}
}

/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void CL_Stop_f (void)
{
	if (!cls.demorecording)
	{
		Com_Printf ("Not recording a demo.\n");
		return;
	}

// write a disconnect message to the demo file
	SZ_Clear (&net_message);
	MSG_WriteLong (&net_message, -1);	// -1 sequence means out of band
	MSG_WriteByte (&net_message, svc_disconnect);
	MSG_WriteString (&net_message, "EndOfDemo");
	CL_WriteDemoMessage (&net_message);

// finish up
	FS_Close (cls.demofile);
	cls.demofile = NULL;
	cls.demorecording = false;
	Com_Printf ("Completed demo\n");
}


/*
====================
CL_WriteRecordDemoMessage
====================
*/
void CL_WriteRecordDemoMessage (sizebuf_t *msg, int seq)
{
	int		len;
	int		i;
	float	fl;
	byte	c;

//Com_Printf ("write: %ld bytes, %4.4f\n", msg->cursize, cls.realtime);

	if (!cls.demorecording)
		return;

	fl = LittleFloat((float)cls.realtime);
	FS_Write (cls.demofile, &fl, sizeof(fl));

	c = dem_read;
	FS_Write (cls.demofile, &c, sizeof(c));

	len = LittleLong (msg->cursize + 8);
	FS_Write (cls.demofile, &len, 4);

	i = LittleLong(seq);
	FS_Write (cls.demofile, &i, 4);
	FS_Write (cls.demofile, &i, 4);

	FS_Write (cls.demofile, msg->data, msg->cursize);
}


void CL_WriteSetDemoMessage (void)
{
	int		len;
	float	fl;
	byte	c;

//Com_Printf ("write: %ld bytes, %4.4f\n", msg->cursize, cls.realtime);

	if (!cls.demorecording)
		return;

	fl = LittleFloat((float)cls.realtime);
	FS_Write (cls.demofile, &fl, sizeof(fl));

	c = dem_set;
	FS_Write (cls.demofile, &c, sizeof(c));

	len = LittleLong(cls.netchan.outgoing_sequence);
	FS_Write (cls.demofile, &len, 4);
	len = LittleLong(cls.netchan.incoming_sequence);
	FS_Write (cls.demofile, &len, 4);
}


// FIXME: same as in sv_user.c. Move to common.c?
#ifdef VWEP_TEST
static char *TrimModelName (char *full)
{
	static char shortn[MAX_QPATH];
	int len;

	if (!strncmp(full, "progs/", 6) && !strchr(full + 6, '/'))
		strlcpy (shortn, full + 6, sizeof(shortn));		// strip progs/
	else
		strlcpy (shortn, full, sizeof(shortn));

	len = strlen(shortn);
	if (len > 4 && !strcmp(shortn + len - 4, ".mdl") && strchr(shortn, '.') == shortn + len - 4)
	{	// strip .mdl
		shortn[len - 4] = '\0';
	}

	return shortn;
}
#endif // VWEP_TEST


/*
====================
CL_Record

Called by CL_Record_f
====================
*/
static void CL_Record (void)
{
	sizebuf_t	buf;
	byte	buf_data[MAX_MSGLEN];
	int n, i, j;
	char *s;
	entity_t *ent;
	entity_state_t *es, blankes;
	player_info_t *player;
	int seq = 1;

#ifdef MVDPLAY
	if (cls.mvdplayback) {
		Com_Printf ("Can't record while playing MVD demo.\n");
		return;
	}
#endif
	cls.demorecording = true;

/*-------------------------------------------------*/

// serverdata
	// send the info about the new client to all connected clients
	SZ_Init (&buf, buf_data, sizeof(buf_data));

// send the serverdata
	MSG_WriteByte (&buf, svc_serverdata);
	MSG_WriteLong (&buf, PROTOCOL_VERSION);
	MSG_WriteLong (&buf, cl.servercount);

	MSG_WriteString (&buf, cls.gamedir);

	if (cl.spectator)
		MSG_WriteByte (&buf, cl.playernum | 128);
	else
		MSG_WriteByte (&buf, cl.playernum);

	// send full levelname
	MSG_WriteString (&buf, cl.levelname);

	// send the movevars
	MSG_WriteFloat(&buf, cl.movevars.gravity);
	MSG_WriteFloat(&buf, cl.movevars.stopspeed);
	MSG_WriteFloat(&buf, cl.movevars.maxspeed);
	MSG_WriteFloat(&buf, cl.movevars.spectatormaxspeed);
	MSG_WriteFloat(&buf, cl.movevars.accelerate);
	MSG_WriteFloat(&buf, cl.movevars.airaccelerate);
	MSG_WriteFloat(&buf, cl.movevars.wateraccelerate);
	MSG_WriteFloat(&buf, cl.movevars.friction);
	MSG_WriteFloat(&buf, cl.movevars.waterfriction);
	MSG_WriteFloat(&buf, cl.movevars.entgravity);

	// send music
	MSG_WriteByte (&buf, svc_cdtrack);
	MSG_WriteByte (&buf, 0); // none in demos

	// send server info string
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va("fullserverinfo \"%s\"\n", cl.serverinfo) );

	// flush packet
	CL_WriteRecordDemoMessage (&buf, seq++);
	SZ_Clear (&buf); 

// soundlist
	MSG_WriteByte (&buf, svc_soundlist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = cl.sound_name[n+1];
	while (*s) {
		MSG_WriteString (&buf, s);
		if (buf.cursize > MAX_MSGLEN/2) {
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf); 
			MSG_WriteByte (&buf, svc_soundlist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = cl.sound_name[n+1];
	}
	if (buf.cursize) {
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		CL_WriteRecordDemoMessage (&buf, seq++);
		SZ_Clear (&buf); 
	}

#ifdef VWEP_TEST
// vwep modellist
	if ((cl.z_ext & Z_EXT_VWEP) && cl.vw_model_name[0][0]) {
		// send VWep precaches
		// pray we don't overflow
		for (i = 0; i < MAX_VWEP_MODELS; i++) {
			s = cl.vw_model_name[i];
			if (!*s)
				continue;
			MSG_WriteByte (&buf, svc_serverinfo);
			MSG_WriteString (&buf, "#vw");
			MSG_WriteString (&buf, va("%i %s", i, TrimModelName(s)));
		}
		// send end-of-list messsage
		MSG_WriteByte (&buf, svc_serverinfo);
		MSG_WriteString (&buf, "#vw");
		MSG_WriteString (&buf, "");
	}
	// don't bother flushing, the vwep list is not that large (I hope)
#endif

// modellist
	MSG_WriteByte (&buf, svc_modellist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = cl.model_name[n+1];
	while (*s) {
		MSG_WriteString (&buf, s);
		if (buf.cursize > MAX_MSGLEN/2) {
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf); 
			MSG_WriteByte (&buf, svc_modellist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = cl.model_name[n+1];
	}
	if (buf.cursize) {
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		CL_WriteRecordDemoMessage (&buf, seq++);
		SZ_Clear (&buf); 
	}

// spawnstatic

	for (i = 0; i < cl.num_statics; i++) {
		ent = cl_static_entities + i;

		MSG_WriteByte (&buf, svc_spawnstatic);

		for (j = 1; j < MAX_MODELS; j++)
			if (ent->model == cl.model_precache[j])
				break;
		if (j == MAX_MODELS)
			MSG_WriteByte (&buf, 0);
		else
			MSG_WriteByte (&buf, j);

		MSG_WriteByte (&buf, ent->frame);
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, ent->skinnum);
		for (j=0 ; j<3 ; j++)
		{
			MSG_WriteCoord (&buf, ent->origin[j]);
			MSG_WriteAngle (&buf, ent->angles[j]);
		}

		if (buf.cursize > MAX_MSGLEN/2) {
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf); 
		}
	}

// spawnstaticsound

	for (i = 0; i < cl.num_static_sounds; i++) {
		static_sound_t *ss = &cl.static_sounds[i];
		MSG_WriteByte (&buf, svc_spawnstaticsound);
		for (j = 0; j < 3; j++)
			MSG_WriteCoord (&buf, ss->org[j]);
		MSG_WriteByte (&buf, ss->sound_num);
		MSG_WriteByte (&buf, ss->vol);
		MSG_WriteByte (&buf, ss->atten);

		if (buf.cursize > MAX_MSGLEN/2) {
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf); 
		}
	}

// baselines

	memset(&blankes, 0, sizeof(blankes));
	for (i = 0; i < MAX_CL_EDICTS; i++) {
		es = &cl_entities[i].baseline;

		if (memcmp(es, &blankes, sizeof(blankes))) {
			MSG_WriteByte (&buf,svc_spawnbaseline);		
			MSG_WriteShort (&buf, i);

			MSG_WriteByte (&buf, es->modelindex);
			MSG_WriteByte (&buf, es->frame);
			MSG_WriteByte (&buf, es->colormap);
			MSG_WriteByte (&buf, es->skinnum);
			for (j=0 ; j<3 ; j++)
			{
				MSG_WriteShort (&buf, es->s_origin[j]);
				MSG_WriteByte (&buf, es->s_angles[j]);
			}

			if (buf.cursize > MAX_MSGLEN/2) {
				CL_WriteRecordDemoMessage (&buf, seq++);
				SZ_Clear (&buf); 
			}
		}
	}

	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va("cmd spawn %i 0\n", cl.servercount) );

	if (buf.cursize) {
		CL_WriteRecordDemoMessage (&buf, seq++);
		SZ_Clear (&buf); 
	}

// send current status of all other players

	for (i = 0; i < MAX_CLIENTS; i++) {
		player = cl.players + i;

		MSG_WriteByte (&buf, svc_updatefrags);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, player->frags);
		
		MSG_WriteByte (&buf, svc_updateping);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, player->ping);
		
		MSG_WriteByte (&buf, svc_updatepl);
		MSG_WriteByte (&buf, i);
		MSG_WriteByte (&buf, player->pl);
		
		MSG_WriteByte (&buf, svc_updateentertime);
		MSG_WriteByte (&buf, i);
		MSG_WriteFloat (&buf, cls.realtime - player->entertime);

		MSG_WriteByte (&buf, svc_updateuserinfo);
		MSG_WriteByte (&buf, i);
		MSG_WriteLong (&buf, player->userid);
		MSG_WriteString (&buf, player->userinfo);

		if (buf.cursize > MAX_MSGLEN/2) {
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf); 
		}
	}
	
// send all current light styles
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		if (!cl_lightstyle[i].length)
			continue;		// don't send empty lightstyle strings
		MSG_WriteByte (&buf, svc_lightstyle);
		MSG_WriteByte (&buf, (char)i);
		MSG_WriteString (&buf, cl_lightstyle[i].map);
	}

	for (i = 0; i < MAX_CL_STATS; i++) {
		if (!cl.stats[i])
			continue;		// no need to send zero values
		if (cl.stats[i] >= 0 && cl.stats[i] <= 255) {
			MSG_WriteByte (&buf, svc_updatestat);
			MSG_WriteByte (&buf, i);
			MSG_WriteByte (&buf, cl.stats[i]);
		} else {
			MSG_WriteByte (&buf, svc_updatestatlong);
			MSG_WriteByte (&buf, i);
			MSG_WriteLong (&buf, cl.stats[i]);
		}
		if (buf.cursize > MAX_MSGLEN/2) {
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf); 
		}
	}

	// get the client to check and download skins
	// when that is completed, a begin command will be issued
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va("skins\n") );

	CL_WriteRecordDemoMessage (&buf, seq++);

	CL_WriteSetDemoMessage();

	// done
}

/*
====================
CL_Record_f

record <demoname>
====================
*/
void CL_Record_f (void)
{
	int		c;
	char	name[MAX_OSPATH];

	c = Cmd_Argc();
	if (c != 2)
	{
		Com_Printf ("record <demoname>\n");
		return;
	}

	if (cls.state != ca_active && cls.state != ca_disconnected) {
		Com_Printf ("Cannot record while connecting.\n");
		return;
	}

	if (cls.demorecording)
		CL_Stop_f();
  
	// open the demo file
	strlcpy (name, Cmd_Argv(1), sizeof (name));
	FS_DefaultExtension (name, ".qwd", sizeof (name));

	cls.demofile = FS_Open (name, "wb", false, false);
	if (!cls.demofile)
	{
		Com_Printf ("ERROR: couldn't open.\n");
		return;
	}

	Com_Printf ("recording to %s.\n", name);

	if (cls.state == ca_active)
		CL_Record ();
	else
		cls.demorecording = true;
}

//==================================================================
// .QWZ demos playback (via Qizmo)
//

#ifdef _WIN32
void CheckQizmoCompletion (void)
{
	DWORD ExitCode;

	if (!hQizmoProcess)
		return;

	if (!GetExitCodeProcess (hQizmoProcess, &ExitCode)) {
		Com_Printf ("WARINING: GetExitCodeProcess failed\n");
		hQizmoProcess = NULL;
		qwz_unpacking = false;
		qwz_playback = false;
		cls.demoplayback = cls.timedemo = false;
		StopQWZPlayback ();
		return;
	}
	
	if (ExitCode == STILL_ACTIVE)
		return;

	hQizmoProcess = NULL;

	if (!qwz_unpacking || !cls.demoplayback) {
		StopQWZPlayback ();
		return;
	}
	
	qwz_unpacking = false;
	
	cls.demofile = FS_Open (tempqwd_name, "rb", false, false);
	if (!cls.demofile) {
		Com_Printf ("Couldn't open %s\n", tempqwd_name);
		qwz_playback = false;
		cls.demoplayback = cls.timedemo = false;
		return;
	}

	// start playback
	cls.demoplayback = true;
	cls.state = ca_demostart;
	Netchan_Setup (NS_CLIENT, &cls.netchan, net_null, 0);
	cls.demotime = 0;
}

static void StopQWZPlayback (void)
{
	if (!hQizmoProcess && tempqwd_name[0]) {
		if (remove (tempqwd_name) != 0)
			Com_Printf ("Couldn't delete %s\n", tempqwd_name);
		tempqwd_name[0] = '\0';
	}
	qwz_playback = false;
}


void PlayQWZDemo (void)
{
	extern cvar_t	qizmo_dir;
	STARTUPINFO			si;
	PROCESS_INFORMATION	pi;
	char	*name;
	char	qwz_name[256];
	char	cmdline[512];
	char	*p;

	if (hQizmoProcess) {
		Com_Printf ("Cannot unpack -- Qizmo still running!\n");
		return;
	}
	
	name = Cmd_Argv(1);

	if (!strncmp(name, "../", 3) || !strncmp(name, "..\\", 3))
		strlcpy (qwz_name, va("%s/%s", fs_basedir, name+3), sizeof(qwz_name));
	else
		if (name[0] == '/' || name[0] == '\\')
			strlcpy (qwz_name, va("%s/%s", cls.gamedir, name+1), sizeof(qwz_name));
		else
			strlcpy (qwz_name, va("%s/%s", cls.gamedir, name), sizeof(qwz_name));

	// Qizmo needs an absolute file name
	_fullpath (qwz_name, qwz_name, sizeof(qwz_name)-1);
	qwz_name[sizeof(qwz_name)-1] = 0;

	// check if the file exists
	cls.demofile = FS_Open (qwz_name, "rb", false, false);
	if (!cls.demofile)
	{
		Com_Printf ("Couldn't open %s\n", name);
		return;
	}
	FS_Close (cls.demofile);
	
	strlcpy (tempqwd_name, qwz_name, sizeof(tempqwd_name)-4);
#if 0
	// the right way
	strcpy (tempqwd_name + strlen(tempqwd_name) - 4, ".qwd");
#else
	// the way Qizmo does it, sigh
	p = strstr (tempqwd_name, ".qwz");
	if (!p)
		p = strstr (tempqwd_name, ".QWZ");
	if (!p)
		p = tempqwd_name + strlen(tempqwd_name);
	strcpy (p, ".qwd");
#endif

	cls.demofile = FS_Open (tempqwd_name, "rb", false, false);
	if (cls.demofile) {
		// .qwd already exists, so just play it
		cls.demoplayback = true;
		cls.state = ca_demostart;
		Netchan_Setup (NS_CLIENT, &cls.netchan, net_null, 0);
		cls.demotime = 0;
		return;
	}
	
	Com_Printf ("Unpacking %s...\n", name);
	
	// start Qizmo to unpack the demo
	memset (&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.wShowWindow = SW_HIDE;
	si.dwFlags = STARTF_USESHOWWINDOW;
	
	strlcpy (cmdline, va("%s/%s/qizmo.exe -q -u -D \"%s\"", fs_basedir,
		qizmo_dir.string, qwz_name), sizeof(cmdline));
	
	if (!CreateProcess (NULL, cmdline, NULL, NULL,
		FALSE, 0/* | HIGH_PRIORITY_CLASS*/,
		NULL, va("%s/%s", fs_basedir, qizmo_dir.string), &si, &pi))
	{
		Com_Printf ("Couldn't execute %s/%s/qizmo.exe\n",
			fs_basedir, qizmo_dir.string);
		return;
	}
	
	hQizmoProcess = pi.hProcess;
	qwz_unpacking = true;
	qwz_playback = true;

	// demo playback doesn't actually start yet, we just set
	// cls.demoplayback so that CL_StopPlayback() will be called
	// if CL_Disconnect() is issued
	cls.demoplayback = true;
	cls.demofile = NULL;
	cls.state = ca_demostart;
	Netchan_Setup (NS_CLIENT, &cls.netchan, net_null, 0);
	cls.demotime = 0;
}
#endif

/*
====================
CL_PlayDemo_f

play [demoname]
====================
*/
void CL_PlayDemo_f (void)
{
	char	name[256];
	qbool	try_dem;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("playdemo <demoname> : plays a demo\n");
		return;
	}

//
// disconnect from server
//
	Host_EndGame ();
	
//
// open the demo file
//
	strlcpy (name, Cmd_Argv(1), sizeof(name)-4);

#ifdef _WIN32
	if (strlen(name) > 4 && !Q_stricmp(name + strlen(name) - 4, ".qwz")) {
		PlayQWZDemo ();
		return;
	}
#endif

	if (FS_FileExtension(name)[0] == 0) {
		FS_DefaultExtension (name, ".qwd", sizeof(name));
		try_dem = true;		// if we can't open the .qwd, try .dem also
	}
	else
		try_dem = false;

try_again:
	if (!strncmp(name, "../", 3) || !strncmp(name, "..\\", 3))
		cls.demofile = FS_Open (va("%s/%s", fs_basedir, name+3), "rb", false, false);
	else
		cls.demofile = FS_Open (name, "rb", false, false);

	if (!cls.demofile && try_dem) {
		FS_StripExtension (name, name, sizeof(name));
		strcat (name, ".dem");
		try_dem = false;
		goto try_again;
	}

	if (!cls.demofile) {
		Com_Printf ("ERROR: couldn't open \"%s\"\n", Cmd_Argv(1));
		cls.playdemos = 0;
		return;
	}

	Com_Printf ("Playing demo from %s.\n", name);

	cls.nqprotocol = !Q_stricmp(FS_FileExtension(name), "dem");
	if (cls.nqprotocol) {
		NQD_StartPlayback ();
	}

#ifdef MVDPLAY
	cls.mvdplayback = !Q_stricmp(FS_FileExtension(name), "mvd");
#endif

	cls.demoplayback = true;
	cls.state = ca_demostart;
	Netchan_Setup (NS_CLIENT, &cls.netchan, net_null, 0);
	cls.demotime = 0;

#ifdef MVDPLAY
	cls.mvd_newtime = cls.mvd_oldtime = 0;
	cls.mvd_findtarget = true;
	cls.mvd_lasttype = 0;
	cls.mvd_lastto = 0;
	MVD_ClearPredict();
#endif
}

/*
====================
CL_FinishTimeDemo
====================
*/
void CL_FinishTimeDemo (void)
{
	int		frames;
	float	time;
	
	cls.timedemo = false;
	
// the first frame didn't count
	frames = (cls.framecount - cls.td_startframe) - 1;
	time = Sys_DoubleTime() - cls.td_starttime;
	if (!time)
		time = 1;
	Com_Printf ("%i frames %5.1f seconds %5.1f fps\n", frames, time, frames/time);
}

/*
====================
CL_TimeDemo_f

timedemo [demoname]
====================
*/
void CL_TimeDemo_f (void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf ("timedemo <demoname> : gets demo speeds\n");
		return;
	}

	CL_PlayDemo_f ();
	
	if (cls.state != ca_demostart)
		return;

// cls.td_starttime will be grabbed at the second frame of the demo, so
// all the loading time doesn't get counted
	
	cls.timedemo = true;
	cls.td_starttime = 0;
	cls.td_startframe = cls.framecount;
	cls.td_lastframe = -1;		// get a new message this frame
}

/*
** Start next demo in loop
*/
void CL_NextDemo (void)
{
	if (!cls.playdemos)
		return;

	cls.demonum++;
	if (cls.demonum >= MAX_DEMOS || !cls.demos[cls.demonum][0])
	{
		if (cls.playdemos == 2)	{
			cls.demonum = 0;		// start again
		} else {
			// stop demo loop
			cls.playdemos = 0;
			return;
		}
	}

	SCR_BeginLoadingPlaque ();	// ok to call here?
	Cbuf_InsertText (va("playdemo \"%s\"\n", cls.demos[cls.demonum]));
}

/*
** Start demo loop
*/
void CL_StartDemos_f (void)
{
	int	i, num, c;

	c = Cmd_Argc();
	if (c < 2) {
		Com_Printf ("usage: startdemos [-loop|-noloop] demo1 demo2 ...\n");
		return;
	}

	cls.playdemos = 2; // deafult to looping

	num = 0;
	for (i = 1; i < c; i++) {
		if (!strcmp(Cmd_Argv(i), "-loop"))
			continue;
		if (!strcmp(Cmd_Argv(i), "-noloop")) {
			cls.playdemos = 1;
			continue;
		}
		strlcpy (cls.demos[num], Cmd_Argv(i), sizeof(cls.demos[0]));
		num++;
		if (num == MAX_DEMOS)
			break;
	}
	
	if (!num) {
		cls.playdemos = 0;
		return;
	}

	if (num < MAX_DEMOS)
		cls.demos[num][0] = 0;

	// start at first demo
	cls.demonum = 0;
	SCR_BeginLoadingPlaque ();
	Cbuf_InsertText (va("playdemo \"%s\"\n", cls.demos[0]));
}
