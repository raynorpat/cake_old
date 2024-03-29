/*
	client.qc

	client functions

	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/

// prototypes
void W_WeaponFrame ();
void W_SetCurrentAmmo ();
void player_pain ();
void player_stand1 ();
void play_teleport (entity ent);
void spawn_tfog (vector org);
void spawn_tdeath (vector org, entity death_owner);
void ExitIntermission ();
string ObituaryForMonster (string attacker_class);

float   modelindex_eyes, modelindex_player;

/*
=============================================================================

				LEVEL CHANGING / INTERMISSION

=============================================================================
*/

string nextmap;

float	ignore_changeparms;

float   intermission_running;
float   intermission_exittime;
entity	intermission_spot;

/*QUAKED info_intermission (1 0.5 0.5) (-16 -16 -16) (16 16 16)
This is the camera point for the intermission.
Use mangle instead of angle, so you can set pitch or roll as well as yaw.  'pitch roll yaw'
*/
void info_intermission ()
{
	self.angles = self.mangle;      // so C can get at it
}



void SetChangeParms ()
{
	if (ignore_changeparms)
		return;

	if (self.health <= 0)
	{
		SetNewParms ();
		return;
	}
 
// remove items
	self.items = self.items - (self.items & 
	(IT_KEY1 | IT_KEY2 | IT_INVISIBILITY | IT_INVULNERABILITY | IT_SUIT | IT_QUAD) );
	
// cap super health
	if (self.health > 100)
		self.health = 100;
	if (self.health < 50)
		self.health = 50;
	parm1 = self.items;
	parm2 = self.health;
	parm3 = self.armorvalue;
	if (self.ammo_shells < 25)
		parm4 = 25;
	else
		parm4 = self.ammo_shells;
	parm5 = self.ammo_nails;
	parm6 = self.ammo_rockets;
	parm7 = self.ammo_cells;
	parm8 = self.weapon;
	parm9 = self.armortype * 100;
}

void SetNewParms ()
{
	parm1 = IT_SHOTGUN | IT_AXE;
	parm2 = 100;
	parm3 = 0;
	parm4 = 25;
	parm5 = 0;
	parm6 = 0;
	parm7 = 0;
	parm8 = 1;
	parm9 = 0;
}

void DecodeLevelParms ()
{
	if (serverflags)
	{
		if (world.model == "maps/start.bsp")
			SetNewParms ();         // take away all stuff on starting new episode
	}
	
	self.items = parm1;
	self.health = parm2;
	self.armorvalue = parm3;
	self.ammo_shells = parm4;
	self.ammo_nails = parm5;
	self.ammo_rockets = parm6;
	self.ammo_cells = parm7;
	self.weapon = parm8;
	self.armortype = parm9 * 0.01;
}

/*
============
FindIntermission

Returns the entity to view from
============
*/
entity FindIntermission ()
{
	entity	spot;
	float	cyc;

// look for info_intermission first
	spot = find (world, classname, "info_intermission");
	if (spot)
	{       // pick a random one
		cyc = random() * 4;
		while (cyc > 1)
		{
			spot = find (spot, classname, "info_intermission");
			if (!spot)
				spot = find (spot, classname, "info_intermission");
			cyc = cyc - 1;
		}
		return spot;
	}

// then look for the start position
	spot = find (world, classname, "info_player_start");
	if (!spot)	
		objerror ("FindIntermission: no spot");

	return spot;
}


void GotoNextMap ()
{
	string	newmap;

//ZOID: 12-13-96, samelevel is overloaded, only 1 works for same level

	if (!deathmatch)
		changelevel (nextmap);
	else if (cvar("samelevel") == 1)	// if samelevel is set, stay on same level
		changelevel (mapname);
	else {
		// configurable map lists, see if the current map exists as a
		// serverinfo/localinfo var
		newmap = infokey(world, mapname);
		if (newmap != "")
			changelevel (newmap);
		else
			changelevel (nextmap);
	}
}



/*
============
IntermissionThink

When the player presses attack or jump, change to the next level
============
*/
void IntermissionThink ()
{
	if (time < intermission_exittime)
		return;

	if (!self.button0 && !self.button1 && !self.button2)
		return;
	
	ExitIntermission ();
}

/*
============
execute_changelevel

The global "nextmap" has been set previously.
Take the players to the intermission spot
============
*/
void execute_changelevel ()
{
	intermission_running = 1;
	
// enforce a wait time before allowing changelevel
	intermission_exittime = time + 5;

	intermission_spot = FindIntermission ();

// play intermission music
	WriteByte (MSG_ALL, SVC_CDTRACK);
	WriteByte (MSG_ALL, 3);

	WriteByte (MSG_ALL, SVC_INTERMISSION);
	WriteCoord (MSG_ALL, intermission_spot.origin_x);
	WriteCoord (MSG_ALL, intermission_spot.origin_y);
	WriteCoord (MSG_ALL, intermission_spot.origin_z);
	WriteAngle (MSG_ALL, intermission_spot.mangle_x);
	WriteAngle (MSG_ALL, intermission_spot.mangle_y);
	WriteAngle (MSG_ALL, intermission_spot.mangle_z);
	
	other = find (world, classname, "player");
	while (other != world)
	{
		other.takedamage = DAMAGE_NO;
		other.flags = other.flags | FL_NOTARGET;
		other.solid = SOLID_NOT;
		other.movetype = MOVETYPE_NONE;
		other.modelindex = 0;
		other.effects = 0;
		other.items = other.items - (other.items & (IT_INVISIBILITY | IT_INVULNERABILITY | IT_SUIT | IT_QUAD));
		other.super_damage_finished = 0;
		other.radsuit_finished = 0;
		other.invisible_finished = 0;
		other.invincible_finished = 0;

		setorigin (other, intermission_spot.origin);
		other.velocity = VEC_ORIGIN;
		other = find (other, classname, "player");
	}       
}


/*
============
SendIntermissionToClient

If a client connects during intermission, send svc_intermission
to him personally
============
*/
void SendIntermissionToClient ()
{
	msg_entity = self;

// play intermission music
	WriteByte (MSG_ONE, SVC_CDTRACK);
	WriteByte (MSG_ONE, 3);

	WriteByte (MSG_ONE, SVC_INTERMISSION);
	WriteCoord (MSG_ONE, intermission_spot.origin_x);
	WriteCoord (MSG_ONE, intermission_spot.origin_y);
	WriteCoord (MSG_ONE, intermission_spot.origin_z);
	WriteAngle (MSG_ONE, intermission_spot.mangle_x);
	WriteAngle (MSG_ONE, intermission_spot.mangle_y);
	WriteAngle (MSG_ONE, intermission_spot.mangle_z);
	
	setorigin (self, intermission_spot.origin);
}


void changelevel_touch ()
{
	if (other.classname != "player")
		return;

	if (deathmatch || coop)
	{
		// if "noexit" is set, blow up the player trying to leave
		// ZOID, 12-13-96, noexit isn't supported in QW.  Overload samelevel
		//      if ((cvar("noexit") == 1) || ((cvar("noexit") == 2) && (mapname != "start")))
		if ((cvar("samelevel") == 2) || ((cvar("samelevel") == 3) && (mapname != "start")))
		{
			T_Damage (other, self, self, 50000);
			return;
		}

		bprint (PRINT_HIGH, other.netname, " exited the level\n");
	}
	
	nextmap = self.map;

	SUB_UseTargets ();

	if ( (self.spawnflags & 1) && !deathmatch )
	{	// NO_INTERMISSION
		GotoNextMap();
		return;
	}

	self.touch = SUB_Null;

// we can't move people right now, because touch functions are called
// in the middle of C movement code, so set a think time to do it
	self.think = execute_changelevel;
	self.nextthink = time + 0.1;
}

/*QUAKED trigger_changelevel (0.5 0.5 0.5) ? NO_INTERMISSION
When the player touches this, he gets sent to the map listed in the "map" variable.  Unless the NO_INTERMISSION flag is set, the view will go to the info_intermission spot and display stats.
*/
void trigger_changelevel ()
{
	if (!self.map)
		objerror ("changelevel trigger doesn't have map");
	
	InitTrigger ();
	self.touch = changelevel_touch;
}


/*
=============================================================================

				PLAYER GAME EDGE FUNCTIONS

=============================================================================
*/

void set_suicide_frame ();

// called by ClientKill and DeadThink
void respawn ()
{
	if (coop)
	{
		// make a copy of the dead body for appearances sake
		CopyToBodyQue (self);
		// get the spawn parms as they were at level start
		setspawnparms (self);
		// respawn
		PutClientInServer ();
	}
	else if (deathmatch)
	{
		// make a copy of the dead body for appearances sake
		CopyToBodyQue (self);
		// set default spawn parms
		SetNewParms ();
		// respawn
		PutClientInServer ();
	}
	else
	{
		// let the stats stay as they were when the player
		// entered the level
		ignore_changeparms = true;
		// restart the entire server
		changelevel (mapname);
	}
}


/*
============
ClientKill

Player entered the suicide command
============
*/
void ClientKill ()
{
	if (intermission_running)
		return;

	if (time < self.suicide_time)
	{
		sprint (self, PRINT_HIGH, "Only one suicide in 5 seconds\n");
		return;
	}

	self.suicide_time = time + 5;

	bprint (PRINT_MEDIUM, self.netname, " suicides\n");

#ifdef AGRIP
        agh_player_dietidy();
#endif
        
	set_suicide_frame ();
	self.modelindex = modelindex_player;
	logfrag (self, self);
	self.frags = self.frags - 2;    // extra penalty
	respawn ();
}


/*
============
SelectSpawnPoint

Returns the entity to spawn at
============
*/
entity SelectSpawnPoint ()
{
	entity	spot, thing;
	float	numspots, totalspots;
	float	pcount;
	entity	spots;

	numspots = 0;
	totalspots = 0;

// testinfo_player_start is only found in regioned levels
	spot = find (world, classname, "testplayerstart");
	if (spot)
		return spot;

	if (!deathmatch)
	{
		if (coop)
		{
			lastspawn = find(lastspawn, classname, "info_player_coop");
			if (lastspawn == world)
				lastspawn = find (lastspawn, classname, "info_player_start");
			if (lastspawn != world)
				return lastspawn;
		}

		if (serverflags)
		{	// return with a rune to start
			spot = find (world, classname, "info_player_start2");
			if (spot)
				return spot;
		}

		spot = find (world, classname, "info_player_start");
		if (!spot)
			error ("PutClientInServer: no info_player_start on level");
		return spot;
	}
		
// choose a info_player_deathmatch point
// ok, find all spots that don't have players nearby

	spots = world;
	spot = find (world, classname, "info_player_deathmatch");       
	while (spot)
	{
		totalspots = totalspots + 1;

		thing=findradius(spot.origin, 84);
		pcount=0;               
		while (thing)
		{
			if (thing.classname == "player")
				pcount=pcount + 1;                      
			thing=thing.chain;      
		}
		if (pcount == 0) {
			spot.goalentity = spots;
			spots = spot;
			numspots = numspots + 1;
		}

		// Get the next spot in the chain
		spot = find (spot, classname, "info_player_deathmatch");                
	}
	totalspots=totalspots - 1;
	if (!numspots) {
		// ack, they are all full, just pick one at random
//		bprint (PRINT_HIGH, "Ackk! All spots are full. Selecting random spawn spot\n");
		totalspots = rint((random() * totalspots));
		spot = find (world, classname, "info_player_deathmatch");       
		while (totalspots > 0) {
			totalspots = totalspots - 1;
			spot = find (spot, classname, "info_player_deathmatch");
		}
		return spot;
	}
		
// We now have the number of spots available on the map in numspots

	// Generate a random number between 1 and numspots

	numspots = numspots - 1;
	
	numspots = rint((random() * numspots ) );

	spot = spots;
	while (numspots > 0) {
		spot = spot.goalentity;
		numspots = numspots - 1;
	}
	return spot;

}


void DecodeLevelParms ();
void PlayerDie ();

/*
===========
PutClientInServer

called each time a player enters a new level
============
*/
void PutClientInServer ()
{
	entity	spot;

	self.classname = "player";
	self.health = 100;
	self.takedamage = DAMAGE_AIM;
	self.solid = SOLID_SLIDEBOX;
	self.movetype = MOVETYPE_WALK;
	self.show_hostile = 0;
	self.max_health = 100;
	self.flags = FL_CLIENT;
	self.air_finished = time + 12;
	self.dmg = 2;                   // initial water damage
	self.super_damage_finished = 0;
	self.radsuit_finished = 0;
	self.invisible_finished = 0;
	self.invincible_finished = 0;
	self.effects = 0;
	self.invincible_time = 0;

	DecodeLevelParms ();
	
	W_SetCurrentAmmo ();

	self.attack_finished = time;
	self.th_pain = player_pain;
	self.th_die = PlayerDie;
	
	self.deadflag = DEAD_NO;
// paustime is set by teleporters to keep the player from moving a while
	self.pausetime = 0;
	
	spot = SelectSpawnPoint ();

	self.origin = spot.origin + '0 0 1';
	self.angles = spot.angles;
	self.fixangle = true;           // turn this way immediately
	self.jump_flag = 0;
	self.swim_flag = 0;

#ifdef AGRIP
	// Stop the player from spawning at a bad angle...
	if (self.angles_y != -90 && self.angles_y != 90
		&& self.angles_y != 0 &&  self.angles_y != 180
							&& self.angles_y != -180)
		self.angles_y = -90;
#endif

// oh, this is a hack!
	setmodel (self, "progs/eyes.mdl");
	modelindex_eyes = self.modelindex;

	setmodel (self, "progs/player.mdl");
	modelindex_player = self.modelindex;

// set size and link into world
	setsize (self, VEC_HULL_MIN, VEC_HULL_MAX);
	
	self.view_ofs = '0 0 22';

// Mod - Xian (May.20.97)
// Bug where player would have velocity from their last kill

	self.velocity = VEC_ORIGIN;

	player_stand1 ();

	if (deathmatch || coop)
	{	
		makevectors(self.angles);
		play_teleport (self);
		spawn_tfog (self.origin + v_forward*20);
	}

	spawn_tdeath (self.origin, self);

	if (deathmatch == 4)
	{
		self.ammo_shells = 0;
		if (stof(infokey(world, "axe")) == 0)
		{
			self.ammo_nails = 255;
			self.ammo_shells = 255;
			self.ammo_rockets = 255;
			self.ammo_cells = 255;
			self.items = self.items | IT_NAILGUN;
			self.items = self.items | IT_SUPER_NAILGUN;
			self.items = self.items | IT_SUPER_SHOTGUN;
			self.items = self.items | IT_ROCKET_LAUNCHER;
//		self.items = self.items | IT_GRENADE_LAUNCHER;
			self.items = self.items | IT_LIGHTNING;
		}
		self.items = self.items - (self.items & (IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) + IT_ARMOR3;
		self.armorvalue = 200;
		self.armortype = 0.8;
		self.health = 250;
		self.items = self.items | IT_INVULNERABILITY;
		self.invincible_time = 1;
		self.invincible_finished = time + 3;
	}

	if (deathmatch == 5)
	{
		self.ammo_nails = 80;
		self.ammo_shells = 30;
		self.ammo_rockets = 10;
		self.ammo_cells = 30;
		self.items = self.items | IT_NAILGUN;
		self.items = self.items | IT_SUPER_NAILGUN;
		self.items = self.items | IT_SUPER_SHOTGUN;
		self.items = self.items | IT_ROCKET_LAUNCHER;
		self.items = self.items | IT_GRENADE_LAUNCHER;
		self.items = self.items | IT_LIGHTNING;
		self.items = self.items - (self.items & (IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) + IT_ARMOR3;
		self.armorvalue = 200;
		self.armortype = 0.8;
		self.health = 200;
		self.items = self.items | IT_INVULNERABILITY;
		self.invincible_time = 1;
		self.invincible_finished = time + 3;
	}

#ifdef AGRIP
	agh_client_startdevs();
#endif
}


/*
=============================================================================

				QUAKED FUNCTIONS

=============================================================================
*/


/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 24)
The normal starting point for a level.
*/
void info_player_start ()
{
}


/*QUAKED info_player_start2 (1 0 0) (-16 -16 -24) (16 16 24)
Only used on start map for the return point from an episode.
*/
void info_player_start2 ()
{
}

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 24)
potential spawning position for deathmatch games
*/
void info_player_deathmatch ()
{
}

/*QUAKED info_player_coop (1 0 1) (-16 -16 -24) (16 16 24)
potential spawning position for coop games
*/
void info_player_coop ()
{
}

/*
===============================================================================

RULES

===============================================================================
*/

/*
go to the next level for deathmatch
*/
void NextLevel ()
{
	entity	o;

	if (nextmap != "")
		return; // already done

	if (mapname == "start")
	{
		if (!cvar("registered"))
		{
			mapname = "e1m1";
		}
		else if (!(serverflags & 1))
		{
			mapname = "e1m1";
			serverflags = serverflags | 1;
		}
		else if (!(serverflags & 2))
		{
			mapname = "e2m1";
			serverflags = serverflags | 2;
		}
		else if (!(serverflags & 4))
		{
			mapname = "e3m1";
			serverflags = serverflags | 4;
		}
		else if (!(serverflags & 8))
		{
			mapname = "e4m1";
			serverflags = serverflags - 7;
		}
 
		o = spawn();
		o.map = mapname;
	}
	else
	{
		// find a trigger changelevel
		o = find(world, classname, "trigger_changelevel");
		if (!o || mapname == "start")
		{       // go back to same map if no trigger_changelevel
			o = spawn();
			o.map = mapname;
		}
	}

	nextmap = o.map;

	if (o.nextthink < time)
	{
		o.think = execute_changelevel;
		o.nextthink = time + 0.1;
	}
}

/*
============
CheckRules

Exit deathmatch games upon conditions
============
*/
void CheckRules ()
{       
	if (deathmatch && fraglimit && self.frags >= fraglimit)
		NextLevel ();
}

//============================================================================

void PlayerDeathThink ()
{
	float	forward;

	if ((self.flags & FL_ONGROUND))
	{
		forward = vlen (self.velocity);
		forward = forward - 20;
		if (forward <= 0)
			self.velocity = VEC_ORIGIN;
		else    
			self.velocity = forward * normalize(self.velocity);
	}

// wait for all buttons released
	if (self.deadflag == DEAD_DEAD)
	{
		if (self.button2 || self.button1 || self.button0)
			return;
		self.deadflag = DEAD_RESPAWNABLE;
		return;
	}

// wait for any button down
	if (!self.button2 && !self.button1 && !self.button0)
		return;

	self.button0 = 0;
	self.button1 = 0;
	self.button2 = 0;
	respawn();
}


void PlayerJump ()
{
	if (self.flags & FL_WATERJUMP)
		return;
	
	if (self.waterlevel >= 2)
	{
// play swiming sound
		if (self.swim_flag < time)
		{
			self.swim_flag = time + 1;
			if (random() < 0.5)
				sound (self, CHAN_BODY, "misc/water1.wav", 1, ATTN_NORM);
			else
				sound (self, CHAN_BODY, "misc/water2.wav", 1, ATTN_NORM);
		}

		return;
	}

	if (!(self.flags & FL_ONGROUND))
		return;

	if ( !(self.flags & FL_JUMPRELEASED) )
		return;         // don't pogo stick

	self.flags = self.flags - (self.flags & FL_JUMPRELEASED);       
	self.button2 = 0;

// player jumping sound
	sound (self, CHAN_BODY, "player/plyrjmp8.wav", 1, ATTN_NORM);
}


/*
===========
WaterMove

============
*/
.float  dmgtime;

void WaterMove ()
{
//dprint (ftos(self.waterlevel));
	if (self.movetype == MOVETYPE_NOCLIP)
		return;
	if (self.health < 0)
		return;

	if (self.waterlevel != 3)
	{
		if (self.air_finished < time)
			sound (self, CHAN_VOICE, "player/gasp2.wav", 1, ATTN_NORM);
		else if (self.air_finished < time + 9)
			sound (self, CHAN_VOICE, "player/gasp1.wav", 1, ATTN_NORM);
		self.air_finished = time + 12;
		self.dmg = 2;
	}
	else if (self.air_finished < time)
	{       // drown!
		if (self.pain_finished < time)
		{
			self.dmg = self.dmg + 2;
			if (self.dmg > 15)
				self.dmg = 10;
			T_Damage (self, world, world, self.dmg);
			self.pain_finished = time + 1;
		}
	}
	
	if (!self.waterlevel)
	{
		if (self.flags & FL_INWATER)
		{       
			// play leave water sound
			sound (self, CHAN_BODY, "misc/outwater.wav", 1, ATTN_NORM);
			self.flags = self.flags - FL_INWATER;
		}
		return;
	}

	if (self.watertype == CONTENT_LAVA)
	{       // do damage
		if (self.dmgtime < time)
		{
			if (self.radsuit_finished > time)
				self.dmgtime = time + 1;
			else
				self.dmgtime = time + 0.2;

			T_Damage (self, world, world, 10*self.waterlevel);
		}
	}
	else if (self.watertype == CONTENT_SLIME)
	{       // do damage
		if (self.dmgtime < time && self.radsuit_finished < time)
		{
			self.dmgtime = time + 1;
			T_Damage (self, world, world, 4*self.waterlevel);
		}
	}
	
	if ( !(self.flags & FL_INWATER) )
	{       

// player enter water sound

		if (self.watertype == CONTENT_LAVA)
			sound (self, CHAN_BODY, "player/inlava.wav", 1, ATTN_NORM);
		if (self.watertype == CONTENT_WATER)
			sound (self, CHAN_BODY, "player/inh2o.wav", 1, ATTN_NORM);
		if (self.watertype == CONTENT_SLIME)
			sound (self, CHAN_BODY, "player/slimbrn2.wav", 1, ATTN_NORM);

		self.flags = self.flags + FL_INWATER;
		self.dmgtime = 0;
	}       
}

/*
================
PlayerPreThink

Called every frame before physics are run
================
*/
void PlayerPreThink ()
{
	if (intermission_running)
	{
		IntermissionThink ();   // otherwise a button could be missed between
		return;                                 // the think tics
	}

	makevectors (self.v_angle);             // is this still used

	self.deathtype = "";

	CheckRules ();
	WaterMove ();

	if (self.deadflag >= DEAD_DEAD)
	{
		PlayerDeathThink ();
		return;
	}
	
	if (self.deadflag == DEAD_DYING)
		return; // dying, so do nothing

	if (self.button2)
	{
		PlayerJump ();
	}
	else
		self.flags = self.flags | FL_JUMPRELEASED;

// teleporters can force a non-moving pause time        
	if (time < self.pausetime)
		self.velocity = VEC_ORIGIN;

	if(time > self.attack_finished && self.currentammo == 0 && self.weapon != IT_AXE)
	{
		self.weapon = W_BestWeapon ();
		W_SetCurrentAmmo ();
	}
}
	
/*
================
CheckPowerups

Check for turning off powerups
================
*/
void CheckPowerups ()
{
	if (self.health <= 0)
		return;

// invisibility
	if (self.invisible_finished)
	{
// sound and screen flash when items starts to run out
		if (self.invisible_sound < time)
		{
			sound (self, CHAN_AUTO, "items/inv3.wav", 0.5, ATTN_IDLE);
			self.invisible_sound = time + ((random() * 3) + 1);
		}


		if (self.invisible_finished < time + 3)
		{
			if (self.invisible_time == 1)
			{
				sprint (self, PRINT_HIGH, "Ring of Shadows magic is fading\n");
				stuffcmd (self, "bf\n");
				sound (self, CHAN_AUTO, "items/inv2.wav", 1, ATTN_NORM);
				self.invisible_time = time + 1;
			}
			
			if (self.invisible_time < time)
			{
				self.invisible_time = time + 1;
				stuffcmd (self, "bf\n");
			}
		}

		if (self.invisible_finished < time)
		{       // just stopped
			self.items = self.items - IT_INVISIBILITY;
			self.invisible_finished = 0;
			self.invisible_time = 0;
		}
		
	// use the eyes
		self.frame = 0;
		self.modelindex = modelindex_eyes;
	}
	else
		self.modelindex = modelindex_player;    // don't use eyes

// invincibility
	if (self.invincible_finished)
	{
// sound and screen flash when items starts to run out
		if (self.invincible_finished < time + 3)
		{
			if (self.invincible_time == 1)
			{
				sprint (self, PRINT_HIGH, "Protection is almost burned out\n");
				stuffcmd (self, "bf\n");
				sound (self, CHAN_AUTO, "items/protect2.wav", 1, ATTN_NORM);
				self.invincible_time = time + 1;
			}
			
			if (self.invincible_time < time)
			{
				self.invincible_time = time + 1;
				stuffcmd (self, "bf\n");
			}
		}
		
		if (self.invincible_finished < time)
		{       // just stopped
			self.items = self.items - IT_INVULNERABILITY;
			self.invincible_time = 0;
			self.invincible_finished = 0;
		}
		if (self.invincible_finished > time)
		{
			self.effects = self.effects | EF_DIMLIGHT;
			self.effects = self.effects | EF_RED;
		}
		else
		{
			self.effects = self.effects - (self.effects & EF_DIMLIGHT);
			self.effects = self.effects - (self.effects & EF_RED);
		}
	}

// super damage
	if (self.super_damage_finished)
	{

// sound and screen flash when items starts to run out

		if (self.super_damage_finished < time + 3)
		{
			if (self.super_time == 1)
			{
				if (deathmatch == 4)
					sprint (self, PRINT_HIGH, "OctaPower is wearing off\n");
				else
					sprint (self, PRINT_HIGH, "Quad Damage is wearing off\n");
				stuffcmd (self, "bf\n");
				sound (self, CHAN_AUTO, "items/damage2.wav", 1, ATTN_NORM);
				self.super_time = time + 1;
			}         
			
			if (self.super_time < time)
			{
				self.super_time = time + 1;
				stuffcmd (self, "bf\n");
			}
		}

		if (self.super_damage_finished < time)
		{       // just stopped
			self.items = self.items - IT_QUAD;
			if (deathmatch == 4)
			{
				self.ammo_cells = 255;
				self.armorvalue = 1;
				self.armortype = 0.8;
				self.health = 100;
			}
			self.super_damage_finished = 0;
			self.super_time = 0;
		}
		if (self.super_damage_finished > time)
		{
			self.effects = self.effects | EF_DIMLIGHT;
			self.effects = self.effects | EF_BLUE;
		}
		else
		{
			self.effects = self.effects - (self.effects & EF_DIMLIGHT);
			self.effects = self.effects - (self.effects & EF_BLUE);
		}
	}       

// suit 
	if (self.radsuit_finished)
	{
		self.air_finished = time + 12;          // don't drown

// sound and screen flash when items starts to run out
		if (self.radsuit_finished < time + 3)
		{
			if (self.rad_time == 1)
			{
				sprint (self, PRINT_HIGH, "Air supply in Biosuit expiring\n");
				stuffcmd (self, "bf\n");
				sound (self, CHAN_AUTO, "items/suit2.wav", 1, ATTN_NORM);
				self.rad_time = time + 1;
			}
			
			if (self.rad_time < time)
			{
				self.rad_time = time + 1;
				stuffcmd (self, "bf\n");
			}
		}

		if (self.radsuit_finished < time)
		{       // just stopped
			self.items = self.items - IT_SUIT;
			self.rad_time = 0;
			self.radsuit_finished = 0;
		}
	}       

}


/*
================
PlayerPostThink

Called every frame after physics are run
================
*/
void PlayerPostThink ()
{
#ifdef VWEP_TEST
    // update vwep frame
    if(stof(infokey(world, "vwep")) && vw_available > 0)
    {
        if (self.health <= 0)
        {
            self.vw_index = 0;
            self.vw_frame = 0;
        }
        else
            self.vw_frame = self.frame;
    }
#endif

	if (intermission_running)
	{
		setorigin (self, intermission_spot.origin);
		self.velocity = VEC_ORIGIN;	// don't stray off the intermission spot too far
		return;
	}

	if (self.deadflag)
		return;

// check to see if player landed and play landing sound 
	if ((self.jump_flag < -300) && (self.flags & FL_ONGROUND) )
	{
		if (self.watertype == CONTENT_WATER)
			sound (self, CHAN_BODY, "player/h2ojump.wav", 1, ATTN_NORM);
		else if (self.jump_flag < -650)
		{
			self.deathtype = "falling";
			T_Damage (self, world, world, 5); 
			sound (self, CHAN_VOICE, "player/land2.wav", 1, ATTN_NORM);

			if (self.groundentity.takedamage == DAMAGE_AIM)
			{
				// we landed on someone's head, hurt him
				self.groundentity.deathtype = "stomp";
				T_Damage (self.groundentity, self, self, 10);
			}
		}
		else
			sound (self, CHAN_VOICE, "player/land.wav", 1, ATTN_NORM);
	}

	self.jump_flag = self.velocity_z;

	CheckPowerups ();

	W_WeaponFrame ();

#ifdef AGRIP
	snap_misc_blockedtest();
#endif
}


/*
===========
ClientConnect

called when a player connects to a server
============
*/
void ClientConnect ()
{
#ifdef FRIKBOT
	ClientConnected();
    // FIXME: make these stuffcmds use IMPULSE_ADDBOT constants
    stuffcmd(self, "alias addbot \"impulse 100\"\n");
    stuffcmd(self, "alias removebot \"impulse 101\"\n");
#endif

	if (deathmatch || coop)
	{
		bprint (PRINT_HIGH, self.netname, " entered the game\n");
	}

	// if the guy started connecting during intermission and
	// thus missed the svc_intermission, we'd better let him know
	if (intermission_running)
		SendIntermissionToClient ();
}


/*
===========
ClientDisconnect

called when a player disconnects from a server
============
*/
void ClientDisconnect ()
{
	string	s;

#ifdef FRIKBOT
	ClientDisconnected(); // FrikBot
#endif
#ifdef AGRIP
	agh_player_dietidy(); // tidy up before leaving
#endif

	// let everyone else know
	s = ftos(self.frags);
	bprint (PRINT_HIGH, self.netname, " left the game with ", s, " frags\n");
	sound (self, CHAN_BODY, "player/tornoff2.wav", 1, ATTN_NONE);
	set_suicide_frame ();

	self.classname = "";	// don't turn up in find()
}

/*
===========
ClientObituary

called when a player dies
============
*/
void ClientObituary (entity targ, entity attacker)
{
	float	rnum;
	string	deathstring, deathstring2;
	string	attackerteam, targteam;

	if (targ.classname != "player")
		return;

	rnum = random();
	//ZOID 12-13-96: self.team doesn't work in QW.  Use keys
	attackerteam = infokey(attacker, "team");
	targteam = infokey(targ, "team");

	if (deathmatch > 3)	
	{
		if (targ.deathtype == "selfwater")
		{
			bprint (PRINT_MEDIUM, targ.netname, " electrocutes himself.\n ");
			targ.frags = targ.frags - 1;
			return;
		}
	}

	if (attacker.classname == "teledeath")
	{
		bprint (PRINT_MEDIUM, targ.netname, " was telefragged by ",
					attacker.owner.netname, "\n");
		logfrag (attacker.owner, targ);

		attacker.owner.frags = attacker.owner.frags + 1;
		return;
	}

	if (attacker.classname == "teledeath2")
	{
		bprint (PRINT_MEDIUM, "Satan's power deflects ", targ.netname, "'s telefrag\n");
		targ.frags = targ.frags - 1;
		logfrag (targ, targ);
		return;
	}

	// double 666 telefrag (can happen often in deathmatch 4)
	if (attacker.classname == "teledeath3") 
	{
		bprint (PRINT_MEDIUM, targ.netname, " was telefragged by ",
					attacker.owner.netname, "'s Satan's power\n");
		targ.frags = targ.frags - 1;
		logfrag (targ, targ);
		return;
	}


	if (targ.deathtype == "squish")
	{
		if (teamplay && targteam == attackerteam && attackerteam != "" && targ != attacker)
		{
			logfrag (attacker, attacker);
			attacker.frags = attacker.frags - 1; 
			bprint (PRINT_MEDIUM, attacker.netname, " squished a teammate\n");
			return;
		}
		else if (attacker.classname == "player" && attacker != targ)
		{
			bprint (PRINT_MEDIUM, attacker.netname, " squishes ",
						targ.netname, "\n");
			logfrag (attacker, targ);
			attacker.frags = attacker.frags + 1;
			return;
		}
		else
		{
			logfrag (targ, targ);
			targ.frags = targ.frags - 1;            // killed self
			bprint (PRINT_MEDIUM, targ.netname, " was squished\n");
			return;
		}
	}

	if (attacker.classname == "player")
	{
		if (targ == attacker)
		{
			// killed self
			logfrag (attacker, attacker);
			attacker.frags = attacker.frags - 1;
			if (targ.deathtype == "grenade")
				bprint (PRINT_MEDIUM, targ.netname, " tries to put the pin back in\n");
			else if (targ.deathtype == "rocket")
				bprint (PRINT_MEDIUM, targ.netname, " becomes bored with life\n");
			else if (targ.weapon == 64 && targ.waterlevel > 1)
			{
				if (targ.watertype == CONTENT_SLIME)
					bprint (PRINT_MEDIUM, targ.netname, " discharges into the slime\n");
				else if (targ.watertype == CONTENT_LAVA)
					bprint (PRINT_MEDIUM, targ.netname, " discharges into the lava\n");
				else
					bprint (PRINT_MEDIUM, targ.netname, " discharges into the water.\n");
			}
			else
				bprint (PRINT_MEDIUM, targ.netname, " becomes bored with life\n");
			return;
		}
		else if ( ((teamplay == 2) && (targteam == attackerteam) &&
			(attackerteam != "")) || coop )
		{
			if (coop)
				rnum = rnum * 0.5;	// only use the first two messages
			if (rnum < 0.25)
				deathstring = " checks his glasses\n";
			else if (rnum < 0.50)
				deathstring = " loses another friend\n";
			else if (rnum < 0.75)
				deathstring = " gets a frag for the other team\n";
			else
				deathstring = " mows down a teammate\n";
			bprint (PRINT_MEDIUM, attacker.netname, deathstring);
			attacker.frags = attacker.frags - 1;
			//ZOID 12-13-96:  killing a teammate logs as suicide
			logfrag (attacker, attacker);
			return;
		}
		else
		{
			logfrag (attacker, targ);
			attacker.frags = attacker.frags + 1;

			rnum = attacker.weapon;
			if (targ.deathtype == "nail")
			{
				deathstring = " was nailed by ";
				deathstring2 = "\n";
			}
			else if (targ.deathtype == "supernail")
			{
				deathstring = " was punctured by ";
				deathstring2 = "\n";
			}
			else if (targ.deathtype == "grenade")
			{
				deathstring = " eats ";
				deathstring2 = "'s pineapple\n";
				if (targ.health < -40)
				{
					deathstring = " was gibbed by ";
					deathstring2 = "'s grenade\n";
				}
			}
			else if (targ.deathtype == "rocket")
			{
				if (attacker.super_damage_finished > 0 && targ.health < -40)
				{
					rnum = random();
					if (rnum < 0.3)
						deathstring = " was brutalized by ";
					else if (rnum < 0.6)
						deathstring = " was smeared by ";
					else
					{
						bprint (PRINT_MEDIUM, attacker.netname, " rips ",
							targ.netname, " a new one\n");
						return;
					}
					deathstring2 = "'s quad rocket\n";
				}
				else
				{
					deathstring = " rides ";
					deathstring2 = "'s rocket\n";
					if (targ.health < -40)
					{
						deathstring = " was gibbed by ";
						deathstring2 = "'s rocket\n" ;
					}
				}
			}
			else if (targ.deathtype == "stomp")
			{
				deathstring = " was stomped by ";
				deathstring2 = "\n";
			}
			else if (rnum == IT_AXE)
			{
				deathstring = " was ax-murdered by ";
				deathstring2 = "\n";
			}
			else if (rnum == IT_SHOTGUN)
			{
				deathstring = " chewed on ";
				deathstring2 = "'s boomstick\n";
			}
			else if (rnum == IT_SUPER_SHOTGUN)
			{
				deathstring = " ate 2 loads of ";
				deathstring2 = "'s buckshot\n";
			}
			else if (rnum == IT_LIGHTNING)
			{
				deathstring = " accepts ";
				if (attacker.waterlevel > 1)
					deathstring2 = "'s discharge\n";
				else
					deathstring2 = "'s shaft\n";
			}
			bprint (PRINT_MEDIUM, targ.netname, deathstring,
									attacker.netname, deathstring2);
		}
		return;
	}
	else
	{
		logfrag (targ, targ);
		targ.frags = targ.frags - 1;            // killed self
		rnum = targ.watertype;

		if (rnum == -3)
		{
			if (random() < 0.5)
				deathstring = " sleeps with the fishes\n";
			else
				deathstring = " sucks it down\n";
		}
		else if (rnum == -4)
		{
			if (random() < 0.5)
				deathstring = " gulped a load of slime\n";
			else
				deathstring = " can't exist on slime alone\n";
		}
		else if (rnum == -5)
		{
			if (targ.health < -15)
				deathstring = " burst into flames\n";
			else if (random() < 0.5)
				deathstring = " turned into hot slag\n";
			else
				deathstring = " visits the Volcano God\n";
		}
		else if (attacker.classname == "explo_box")
		{
			deathstring = " blew up\n";
		}
		else if (targ.deathtype == "falling")
		{
			deathstring = " fell to his death\n";
		}
		else if (targ.deathtype == "nail" || targ.deathtype == "supernail")
		{
			deathstring = " was spiked\n";
		}
		else if (targ.deathtype == "laser")
		{
			deathstring = " was zapped\n";
		}
		else if (attacker.classname == "fireball")
		{
			deathstring = " ate a lavaball\n";
		}
		else if (attacker.classname == "trigger_changelevel")
		{
			deathstring = " tried to leave\n";
		}
		else if (attacker.flags & FL_MONSTER)
		{
			deathstring = ObituaryForMonster(attacker.classname);
		}
		else
		{
			deathstring = " died\n";
		}

		bprint (PRINT_MEDIUM, targ.netname, deathstring);
	}
}
