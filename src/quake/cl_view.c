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
// cl_view.c -- player eye positioning

#include "gl_local.h"

/*

The view is allowed to move slightly from its true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

cvar_t	cl_rollspeed = {"cl_rollspeed", "200"};
cvar_t	cl_rollangle = {"cl_rollangle", "0"};
cvar_t	cl_bob = {"cl_bob","0"};
cvar_t	cl_bobcycle = {"cl_bobcycle","0.6"};
cvar_t	cl_bobup = {"cl_bobup","0.5"};
cvar_t	v_kicktime = {"v_kicktime", "0"};
cvar_t	v_kickroll = {"v_kickroll", "0.6"};
cvar_t	v_kickpitch = {"v_kickpitch", "0.6"};
cvar_t	v_kickback = {"v_kickback", "0"};	// recoil effect

cvar_t	cl_drawgun = {"r_drawviewmodel", "1"};

cvar_t	scr_crosshairscale = {"scr_crosshairscale", "2", CVAR_ARCHIVE};
cvar_t	crosshair = {"crosshair", "2", CVAR_ARCHIVE};
cvar_t	crosshaircolor = {"crosshaircolor", "11", CVAR_ARCHIVE};
cvar_t  cl_crossx = {"cl_crossx", "0", CVAR_ARCHIVE};
cvar_t  cl_crossy = {"cl_crossy", "0", CVAR_ARCHIVE};

cvar_t  v_contentblend = {"v_contentblend", "1"};
cvar_t	v_damagecshift = {"v_damagecshift", "1"};
cvar_t	v_quadcshift = {"v_quadcshift", "1"};
cvar_t	v_suitcshift = {"v_suitcshift", "1"};
cvar_t	v_ringcshift = {"v_ringcshift", "1"};
cvar_t	v_pentcshift = {"v_pentcshift", "1"};

cvar_t	v_bonusflash = {"cl_bonusflash", "1"};

float	v_dmg_time, v_dmg_roll, v_dmg_pitch;

player_state_t		view_message;

// idle swaying for intermission and TF
float	v_iyaw_cycle = 2;
float	v_iroll_cycle = 0.5;
float	v_ipitch_cycle = 1;
float	v_iyaw_level = 0.3;
float	v_iroll_level = 0.1;
float	v_ipitch_level = 0.3;
float	v_idlescale = 0;


void V_NewMap (void)
{
	v_iyaw_cycle = 2;
	v_iroll_cycle = 0.5;
	v_ipitch_cycle = 1;
	v_iyaw_level = 0.3;
	v_iroll_level = 0.1;
	v_ipitch_level = 0.3;
	v_idlescale = 0;

	v_dmg_time = 0;
	v_dmg_roll = 0;
	v_dmg_pitch = 0;

	V_ClearScene();
}

/*
===============
V_CalcRoll

===============
*/
float V_CalcRoll (vec3_t angles, vec3_t velocity)
{
	vec3_t	right;
	float	sign;
	float	side;
	
	AngleVectors (angles, NULL, right, NULL);
	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs(side);
	
	if (side < cl_rollspeed.value)
		side = side * cl_rollangle.value / cl_rollspeed.value;
	else
		side = cl_rollangle.value;

	if (side > 45)
		side = 45;

	return side*sign;
	
}


/*
===============
V_CalcBob
===============
*/
float V_CalcBob (void)
{
	static double bobtime;
	static float bob;
	float	cycle;
	
	if (cl.spectator)
		return 0;

	if (!cl.onground)
		return bob;		// just use old value

	if (!cl_bobcycle.value < 0.001)
		return 0;

	bobtime += cls.frametime;
	cycle = bobtime - (int)(bobtime/cl_bobcycle.value)*cl_bobcycle.value;
	cycle /= cl_bobcycle.value;

	if (cycle < cl_bobup.value)
		cycle = M_PI * cycle / cl_bobup.value;
	else
		cycle = M_PI + M_PI * (cycle - cl_bobup.value) / (1.0 - cl_bobup.value);

	// bob is proportional to simulated velocity in the xy plane
	// (don't count Z, or jumping messes it up)
	bob = sqrt(cl.simvel[0] * cl.simvel[0] + cl.simvel[1] * cl.simvel[1]) * cl_bob.value;
	bob = bob * 0.3 + bob * 0.7 * sin(cycle);

	if (bob > 4)
		bob = 4;
	else if (bob < -7)
		bob = -7;
	
	return bob;	
}


//=============================================================================


cvar_t	v_centermove = {"v_centermove", "0.15"};
cvar_t	v_centerspeed = {"v_centerspeed","500"};


void V_StartPitchDrift (void)
{
#if 1
	if (cl.laststop == cl.time)
	{
		return;		// something else is keeping it from drifting
	}
#endif
	if (cl.nodrift || !cl.pitchvel)
	{
		cl.pitchvel = v_centerspeed.value;
		cl.nodrift = false;
		cl.driftmove = 0;
	}
}

void V_StopPitchDrift (void)
{
	cl.laststop = cl.time;
	cl.nodrift = true;
	cl.pitchvel = 0;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards cl.idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.

Drifting is enabled when the center view key is hit, mlook is released and
lookspring is non 0, or when 
===============
*/
void V_DriftPitch (void)
{
	float		delta, move;

	if (!cl.onground || cls.demoplayback)
	{
		cl.driftmove = 0;
		cl.pitchvel = 0;
		return;
	}

// don't count small mouse motion
	if (cl.nodrift)
	{
		if ( fabs(cl.frames[(cls.netchan.outgoing_sequence-1)&UPDATE_MASK].cmd.forwardmove) < 200)
			cl.driftmove = 0;
		else
			cl.driftmove += cls.frametime;
	
		if ( cl.driftmove > v_centermove.value)
		{
			V_StartPitchDrift ();
		}
		return;
	}
	
	delta = 0 - cl.viewangles[PITCH];

	if (!delta)
	{
		cl.pitchvel = 0;
		return;
	}

	move = cls.frametime * cl.pitchvel;
	cl.pitchvel += cls.frametime * v_centerspeed.value;
	
//Com_Printf ("move: %f (%f)\n", move, cls.frametime);

	if (delta > 0)
	{
		if (move > delta)
		{
			cl.pitchvel = 0;
			move = delta;
		}
		cl.viewangles[PITCH] += move;
	}
	else if (delta < 0)
	{
		if (move > -delta)
		{
			cl.pitchvel = 0;
			move = -delta;
		}
		cl.viewangles[PITCH] -= move;
	}
}





/*
============================================================================== 
 
						PALETTE FLASHES 
 
============================================================================== 
*/ 

cshift_t    cshift_empty = { {130, 80, 50}, 0};
cshift_t    cshift_water = { {130, 80, 50}, 128};
cshift_t    cshift_slime = { {0, 25, 5}, 150};
cshift_t    cshift_lava = { {255, 80, 0}, 150};
cshift_t    cshift_bonus = { {215, 186, 60}, 50};

float		v_blend[4];		// rgba 0.0 - 1.0

/*
===============
V_ParseDamage
===============
*/
void V_ParseDamage (void)
{
	int		armor, blood;
	vec3_t	from;
	int		i;
	vec3_t	forward, right;
	float	side;
	float	count;
	
	armor = MSG_ReadByte ();
	blood = MSG_ReadByte ();
	for (i=0 ; i<3 ; i++)
		from[i] = MSG_ReadCoord ();

	count = blood*0.5 + armor*0.5;
	if (count < 10)
		count = 10;

	cl.faceanimtime = cl.time + 0.2;		// but sbar face into pain frame

	cl.cshifts[CSHIFT_DAMAGE].percent += 3*count;
	if (cl.cshifts[CSHIFT_DAMAGE].percent < 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;
	if (cl.cshifts[CSHIFT_DAMAGE].percent > 150)
		cl.cshifts[CSHIFT_DAMAGE].percent = 150;

	cl.cshifts[CSHIFT_DAMAGE].percent *= bound (0.0, v_damagecshift.value, 1.0);

	if (armor > blood)		
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 200;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 100;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 100;
	}
	else if (armor)
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 220;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 50;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 50;
	}
	else
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 255;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 0;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 0;
	}

	cl.cshifts[CSHIFT_DAMAGE].initialpct = cl.cshifts[CSHIFT_DAMAGE].percent;
	cl.cshifts[CSHIFT_DAMAGE].time = cl.time;

//
// calculate view angle kicks
//
	VectorSubtract (from, cl.simorg, from);
	VectorNormalize (from);
	
	AngleVectors (cl.simangles, forward, right, NULL);

	side = DotProduct (from, right);
	v_dmg_roll = count*side*v_kickroll.value;
	
	side = DotProduct (from, forward);
	v_dmg_pitch = count*side*v_kickpitch.value;

	v_dmg_time = v_kicktime.value;
}


/*
==================
V_BonusFlash_f

When you run over an item, the server sends this command
==================
*/
void V_BonusFlash_f (void)
{
	if (!v_bonusflash.value && cbuf_current == &cbuf_svc)
		return;

	cl.cshifts[CSHIFT_BONUS].destcolor[0] = 215;
	cl.cshifts[CSHIFT_BONUS].destcolor[1] = 186;
	cl.cshifts[CSHIFT_BONUS].destcolor[2] = 69;
	cl.cshifts[CSHIFT_BONUS].percent = 50;
	cl.cshifts[CSHIFT_BONUS].initialpct = 50;
	cl.cshifts[CSHIFT_BONUS].time = cl.time;
}

/*
=============
V_SetContentsColor

Underwater, lava, etc each has a color shift
=============
*/
void V_SetContentsColor (int contents)
{
	switch (contents)
	{
	case CONTENTS_EMPTY:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
		break;
	case CONTENTS_LAVA:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_lava;
		break;
	case CONTENTS_SOLID:
	case CONTENTS_SLIME:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_slime;
		break;
	default:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_water;
	}

	cl.cshifts[CSHIFT_CONTENTS].percent *= bound (0.0, v_contentblend.value, 1.0);
}

/*
=============
V_CalcPowerupCshift
=============
*/
void V_CalcPowerupCshift (void)
{
	if (cl.stats[STAT_ITEMS] & IT_QUAD)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
		cl.cshifts[CSHIFT_POWERUP].percent = 30 * bound (0, v_quadcshift.value, 1);
	}
	else if (cl.stats[STAT_ITEMS] & IT_SUIT)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 20 * bound (0, v_suitcshift.value, 1);
	}
	else if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 100;
		cl.cshifts[CSHIFT_POWERUP].percent = 100 * bound (0, v_ringcshift.value, 1);
	}
	else if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 30 * bound (0, v_pentcshift.value, 1);
	}
	else
		cl.cshifts[CSHIFT_POWERUP].percent = 0;
}

static void V_DropCShift (cshift_t *cs, float droprate)
{
	if (cs->time < 0)
	{
		cs->percent = 0;
	}
	else if ((cs->percent = cs->initialpct - (cl.time - cs->time) * droprate) <= 0)
	{
		cs->percent = 0;
		cs->time = -1;
	}
}

static void V_CalcBlend (void)
{
	float       a2, a3;
	float       r = 0, g = 0, b = 0, a = 0;
	int         i;

	for (i = 0; i < NUM_CSHIFTS; i++)
	{
		a2 = cl.cshifts[i].percent / 255.0;

		if (!a2)
			continue;

		a2 = min (a2, 1.0);
		r += (cl.cshifts[i].destcolor[0] - r) * a2;
		g += (cl.cshifts[i].destcolor[1] - g) * a2;
		b += (cl.cshifts[i].destcolor[2] - b) * a2;

		a3 = (1.0 - a) * (1.0 - a2);
		a = 1.0 - a3;
	}

	// LordHavoc: saturate color
	if (a)
	{
		a2 = 1.0 / a;
		r *= a2;
		g *= a2;
		b *= a2;
	}

	v_blend[0] = min (r, 255.0) / 255.0;
	v_blend[1] = min (g, 255.0) / 255.0;
	v_blend[2] = min (b, 255.0) / 255.0;
	v_blend[3] = bound (0.0, a, 1.0);
}


/*
=============
V_PrepBlend
=============
*/
void V_PrepBlend (void)
{
	if (cls.state != ca_active)
	{
		cl.cshifts[CSHIFT_CONTENTS].percent = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 0;
		cl.cshifts[CSHIFT_CUSTOM].percent = 0;
	}
	else
	{
		V_CalcPowerupCshift ();
	}

	// drop the damage value
	V_DropCShift (&cl.cshifts[CSHIFT_DAMAGE], 150);

	// drop the bonus value
	V_DropCShift (&cl.cshifts[CSHIFT_BONUS], 100);

	V_CalcBlend ();
}


/* 
============================================================================== 
 
						VIEW RENDERING 
 
============================================================================== 
*/ 

float angledelta (float a)
{
	a = anglemod(a);
	if (a > 180)
		a -= 360;
	return a;
}

/*
==============
V_BoundOffsets
==============
*/
void V_BoundOffsets (void)
{
// absolutely bound refresh relative to entity clipping hull
// so the view can never be inside a solid wall

	if (r_refdef2.vieworg[0] < cl.simorg[0] - 14)
		r_refdef2.vieworg[0] = cl.simorg[0] - 14;
	else if (r_refdef2.vieworg[0] > cl.simorg[0] + 14)
		r_refdef2.vieworg[0] = cl.simorg[0] + 14;
	if (r_refdef2.vieworg[1] < cl.simorg[1] - 14)
		r_refdef2.vieworg[1] = cl.simorg[1] - 14;
	else if (r_refdef2.vieworg[1] > cl.simorg[1] + 14)
		r_refdef2.vieworg[1] = cl.simorg[1] + 14;
	if (r_refdef2.vieworg[2] < cl.simorg[2] - 22)
		r_refdef2.vieworg[2] = cl.simorg[2] - 22;
	else if (r_refdef2.vieworg[2] > cl.simorg[2] + 30)
		r_refdef2.vieworg[2] = cl.simorg[2] + 30;
}

/*
==============
V_AddIdle

Idle swaying
==============
*/
void V_AddIdle (void)
{
	r_refdef2.viewangles[ROLL] += v_idlescale * sin(cl.time*v_iroll_cycle) * v_iroll_level;
	r_refdef2.viewangles[PITCH] += v_idlescale * sin(cl.time*v_ipitch_cycle) * v_ipitch_level;
	r_refdef2.viewangles[YAW] += v_idlescale * sin(cl.time*v_iyaw_cycle) * v_iyaw_level;
}


/*
==============
V_CalcViewRoll

Roll is induced by movement and damage
==============
*/
void V_CalcViewRoll (void)
{
	float	side;
	float	adjspeed;
	
	side = V_CalcRoll (cl.simangles, cl.simvel);
	adjspeed = 20 * bound (2, fabs(cl_rollangle.value), 45);
	if (side > cl.rollangle) {
		cl.rollangle += cls.frametime * adjspeed;
		if (cl.rollangle > side)
			cl.rollangle = side;
	}
	else if (side < cl.rollangle)
	{
		cl.rollangle -= cls.frametime * adjspeed;
		if (cl.rollangle < side)
			cl.rollangle = side;
	}
	r_refdef2.viewangles[ROLL] += cl.rollangle;

	if (v_dmg_time > 0 && v_kicktime.value > 0)
	{
		r_refdef2.viewangles[ROLL] += v_dmg_time/v_kicktime.value*v_dmg_roll;
		r_refdef2.viewangles[PITCH] += v_dmg_time/v_kicktime.value*v_dmg_pitch;
		v_dmg_time -= cls.frametime;
	}
}


/*
==================
V_AddViewWeapon
==================
*/
void V_AddViewWeapon (float bob)
{
	vec3_t		forward, up;
	entity_t	ent;
	extern cvar_t	scr_fov;
	// FIXME, move the statics to a structure like cl
	static int oldweapon, curframe, oldframe;
	static double start_lerp_time;

	if (!cl_drawgun.value || (cl_drawgun.value == 2 && scr_fov.value > 90)
		|| view_message.flags & (PF_GIB|PF_DEAD)
		|| !Cam_DrawViewModel())
		return;

	memset (&ent, 0, sizeof(ent));

	if ((unsigned int)cl.stats[STAT_WEAPON] >= MAX_MODELS)
		Host_Error ("STAT_WEAPON >= MAX_MODELS");
	ent.model = cl.model_precache[cl.stats[STAT_WEAPON]];
	if (!ent.model)
		return;
	ent.frame = view_message.weaponframe;
	ent.colormap = 0;
	ent.renderfx = RF_WEAPONMODEL;

	if (cl.stats[STAT_WEAPON] != oldweapon) {
		oldweapon = cl.stats[STAT_WEAPON];
		curframe = -1;
		start_lerp_time = -1;
	}
	if (ent.frame != curframe) {
		oldframe = curframe;
		curframe = ent.frame;
		start_lerp_time = cl.time;
	}
	ent.oldframe = oldframe;
	ent.backlerp = 1 - (cl.time - start_lerp_time)*10;
	ent.backlerp = bound (0, ent.backlerp, 1);

//	if (r_lerpmuzzlehack.value && cl.modelinfos[cl.stats[STAT_WEAPON]] != mi_no_lerp_hack)
//		ent.renderfx |= RF_LIMITLERP;

	if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY) {
		ent.renderfx |= RF_TRANSLUCENT;
		ent.alpha = 0.5;
	}

	ent.angles[YAW] = r_refdef2.viewangles[YAW];
	ent.angles[PITCH] = -r_refdef2.viewangles[PITCH];
	ent.angles[ROLL] = r_refdef2.viewangles[ROLL];

	AngleVectors (r_refdef2.viewangles, forward, NULL, up);
	
	VectorCopy (r_refdef2.vieworg, ent.origin);
	VectorMA (ent.origin, bob * 0.4, forward, ent.origin);

	// fudge position around to keep amount of weapon visible
	// roughly equal with different FOV
	if (scr_viewsize.value == 110)
		VectorMA (ent.origin, 1, up, ent.origin);
	else if (scr_viewsize.value == 100)
		VectorMA (ent.origin, 2, up, ent.origin);
	else if (scr_viewsize.value == 90)
		VectorMA (ent.origin, 1, up, ent.origin);
	else if (scr_viewsize.value == 80)
		VectorMA (ent.origin, 0.5, up, ent.origin);

	V_AddEntity (&ent);
}

/*
==================
V_CalcIntermissionRefdef

==================
*/
void V_CalcIntermissionRefdef (void)
{
	float		old;

	VectorCopy (cl.simorg, r_refdef2.vieworg);
	VectorCopy (cl.simangles, r_refdef2.viewangles);

// always idle in intermission
	old = v_idlescale;
	v_idlescale = 1;
	V_AddIdle ();
	v_idlescale = old;
}

/*
==================
V_CalcRefdef

==================
*/
void V_CalcRefdef (void)
{
	vec3_t		forward;
	float		bob;

	V_DriftPitch ();

	bob = V_CalcBob ();

	// set up the refresh position
	VectorCopy (cl.simorg, r_refdef2.vieworg);

	// never let it sit exactly on a node line, because a water plane can
	// dissapear when viewed with the eye exactly on it.
	// the server protocol only specifies to 1/8 pixel, so add 1/16 in each axis
	r_refdef2.vieworg[0] += 1.0/16;
	r_refdef2.vieworg[1] += 1.0/16;
	r_refdef2.vieworg[2] += 1.0/16;

	// add view height
	if (view_message.flags & PF_GIB)
		r_refdef2.vieworg[2] += 8;	// gib view height
	else if (view_message.flags & PF_DEAD)
		r_refdef2.vieworg[2] -= 16;	// corpse view height
	else
	{
		r_refdef2.vieworg[2] += cl.viewheight;	// normal view height

		r_refdef2.vieworg[2] += bob;

		// smooth out stair step ups
		r_refdef2.vieworg[2] += cl.crouch;
	}

	if (cl.landtime) {
		const float tt = 350;
		const float lt = 90;
		const float lh = 6;
		float _landtime = 350 - (cl.time - cl.landtime) * 1000;

		if (_landtime <= 0)
			cl.landtime = 0;
		else {
			if (_landtime >= (tt - lt))
				r_refdef2.vieworg[2] -= ((tt - _landtime) / lt) * lh;
			else
				r_refdef2.vieworg[2] -= _landtime / (tt - lt) * lh;
		}
	}

//
// set up refresh view angles
//
	VectorCopy (cl.simangles, r_refdef2.viewangles);
	V_CalcViewRoll ();
	V_AddIdle ();

	if (v_kickback.value)
	{
		if (cls.nqprotocol)
			r_refdef2.viewangles[PITCH] += cl.punchangle;
		else
		{
			// add weapon kick offset
			AngleVectors (r_refdef2.viewangles, forward, NULL, NULL);
			VectorMA (r_refdef2.vieworg, cl.punchangle, forward, r_refdef2.vieworg);

			// add weapon kick angle
			r_refdef2.viewangles[PITCH] += cl.punchangle * 0.5;
		}
	}

	if (view_message.flags & PF_DEAD)		// PF_GIB will also set PF_DEAD
		r_refdef2.viewangles[ROLL] = 80;	// dead view angle

	V_AddViewWeapon (bob);
}

/*
=============
DropPunchAngle
=============
*/
void DropPunchAngle (void)
{
	if (cls.nqprotocol)
		return;

	if (cl.ideal_punchangle < cl.punchangle)
	{
		if (cl.ideal_punchangle == -2)	// small kick
			cl.punchangle -= 20 * cls.frametime;
		else							// big kick
			cl.punchangle -= 40 * cls.frametime;

		if (cl.punchangle < cl.ideal_punchangle)
		{
			cl.punchangle = cl.ideal_punchangle;
			cl.ideal_punchangle = 0;
		}
	}
	else
	{
		cl.punchangle += 20 * cls.frametime;
		if (cl.punchangle > 0)
			cl.punchangle = 0;
	}
}

/*
==================
V_AddEntity
==================
*/
void V_AddEntity (entity_t *ent)
{
	if (cl_numvisedicts >= MAX_VISEDICTS)
		return;
	
	cl_visedicts[cl_numvisedicts++] = *ent;
	cl_visedicts[cl_numvisedicts].entnum = cl_numvisedicts;
}

/*
==================
V_AddDlight
==================
*/
void V_AddDlight(vec3_t origin, float radius, float minlight, int r, int g, int b)
{
	if (cl_numvisdlights >= MAX_DLIGHTS)
		return;

	VectorCopy (origin, cl_visdlights[cl_numvisdlights].origin);
	cl_visdlights[cl_numvisdlights].radius = radius;
	cl_visdlights[cl_numvisdlights].minlight = minlight;
	cl_visdlights[cl_numvisdlights].rgb[0] = r;
	cl_visdlights[cl_numvisdlights].rgb[1] = g;
	cl_visdlights[cl_numvisdlights].rgb[2] = b;
	cl_numvisdlights++;
}

/*
==================
V_AddParticle
==================
*/
void V_AddParticle (vec3_t origin, int color, float alpha)
{
/*
	extern int cl_numparticles;

	if (cl_numvisparticles >= cl_numparticles)
		return;
	
	cl_visparticles[cl_numvisparticles].color = color;
	VectorCopy (origin, cl_visparticles[cl_numvisparticles].org);
	cl_numvisparticles++;
*/
}

/*
==================
V_ClearScene
==================
*/
void V_ClearScene (void)
{
	cl_numvisedicts = 0;
//	cl_numvisparticles = 0;
	cl_numvisdlights = 0;
}


/*
==================
V_RenderView

The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
the entity origin, so any view position inside that will be valid
==================
*/
extern vrect_t scr_vrect;

void V_RenderView (void)
{
	int i;
	translation_info_t translations[MAX_CLIENTS];

//	if (cl.simangles[ROLL])
//		Sys_Error ("cl.simangles[ROLL]");	// DEBUG
cl.simangles[ROLL] = 0;	// FIXME @@@ 

	if (cls.state != ca_active)
		return;

	if (cl.validsequence)
		view_message = cl.frames[cl.validsequence & UPDATE_MASK].playerstate[Cam_PlayerNum()];

	DropPunchAngle ();
	if (cl.intermission)
	{
		// intermission / finale rendering
		V_CalcIntermissionRefdef ();	
	}
	else
	{
		V_CalcRefdef ();
	}
	
	r_refdef2.time = cl.time;
//	r_refdef2.allowCheats = false;
	r_refdef2.viewplayernum = Cam_PlayerNum();
	r_refdef2.watervis = (atoi(Info_ValueForKey(cl.serverinfo, "watervis")) != 0);

	r_refdef2.lightstyles = cl_lightstyle;

	r_refdef2.numDlights = cl_numvisdlights;
	r_refdef2.dlights = cl_visdlights;

	r_refdef2.numParticles = 0; //cl_numvisparticles;
	r_refdef2.particles = NULL;//cl_visparticles;

	for (i = 0; i < MAX_CLIENTS; i++) {
		translations[i].topcolor = cl.players[i].topcolor;
		translations[i].bottomcolor = cl.players[i].bottomcolor;
		strlcpy (translations[i].skinname, cl.players[i].skin, sizeof(translations[0].skinname));
	}
	r_refdef2.translations = translations;
	strlcpy (r_refdef2.baseskin, baseskin.string, sizeof(r_refdef2.baseskin));

	R_RenderView ();
}

//============================================================================

/*
=============
V_Init
=============
*/
void V_Init (void)
{
	Cmd_AddCommand ("bf", V_BonusFlash_f);
	Cmd_AddCommand ("centerview", V_StartPitchDrift);

	Cvar_Register (&v_centermove);
	Cvar_Register (&v_centerspeed);

	Cvar_Register (&crosshair);
	Cvar_Register (&crosshaircolor);
	Cvar_Register (&scr_crosshairscale);
	Cvar_Register (&cl_crossx);
	Cvar_Register (&cl_crossy);

	Cvar_Register (&cl_rollspeed);
	Cvar_Register (&cl_rollangle);
	Cvar_Register (&cl_bob);
	Cvar_Register (&cl_bobcycle);
	Cvar_Register (&cl_bobup);
	Cvar_Register (&v_kicktime);
	Cvar_Register (&v_kickroll);
	Cvar_Register (&v_kickpitch);
	Cvar_Register (&v_kickback);
	Cvar_Register (&cl_drawgun);

	Cvar_Register (&v_bonusflash);
	Cvar_Register (&v_contentblend);
	Cvar_Register (&v_damagecshift);
	Cvar_Register (&v_quadcshift);
	Cvar_Register (&v_suitcshift);
	Cvar_Register (&v_ringcshift);
	Cvar_Register (&v_pentcshift);
}
