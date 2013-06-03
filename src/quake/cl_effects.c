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

#define PARTICLE_BATCH_SIZE		2048
#define PARTICLE_EXTRA_SIZE		1024

// we don't expect these to ever be exceeded but we allow it if required
#define PARTICLE_TYPE_BATCH_SIZE	64
#define PARTICLE_TYPE_EXTRA_SIZE	32

int		ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
int		ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
int		ramp3[8] = {0x6d, 0x6b, 6, 5, 4, 3};

particle_t *free_particles;
particle_type_t *active_particle_types, *free_particle_types;

int r_numparticles = 0;


particle_t *R_NewParticle (particle_type_t *pt)
{
	particle_t	*p;
	int i;

	// with pointfiles we can have loads of particles, so set this as an absolute upper limit to prevent memory running out
	if (r_numparticles >= 65536)
		return NULL;
	else if (free_particles)
	{
		// just take from the free list
		p = free_particles;
		free_particles = p->next;

		p->next = pt->particles;
		pt->particles = p;
		pt->numparticles++;

		return p;
	}
	else
	{
		// alloc some more free particles
		free_particles = (particle_t *) Hunk_AllocName (PARTICLE_EXTRA_SIZE * sizeof (particle_t), "particles");

		// link them up
		for (i = 1; i < PARTICLE_EXTRA_SIZE; i++)
		{
			free_particles[i - 1].next = &free_particles[i];
			free_particles[i].next = NULL;
		}

		// finish the link
		r_numparticles += PARTICLE_EXTRA_SIZE;

		// call recursively to return the first new free particle
		return R_NewParticle (pt);
	}
}


particle_type_t *R_NewParticleType (vec3_t spawnorg)
{
	particle_type_t *pt;
	int i;

	if (free_particle_types)
	{
		// just take from the free list
		pt = free_particle_types;
		free_particle_types = pt->next;

		// no particles yet
		pt->particles = NULL;
		pt->numparticles = 0;

		// copy across origin
		VectorCopy (spawnorg, pt->spawnorg);

		// link it in
		pt->next = active_particle_types;
		active_particle_types = pt;

		// done
		return pt;
	}

	// alloc some more free particles
	free_particle_types = (particle_type_t *) Hunk_Alloc (PARTICLE_TYPE_EXTRA_SIZE * sizeof (particle_type_t));

	// link them up
	for (i = 1; i < PARTICLE_TYPE_EXTRA_SIZE; i++)
	{
		free_particle_types[i - 1].next = &free_particle_types[i];
		free_particle_types[i].next = NULL;
	}

	// call recursively to return the first new free particle type
	return R_NewParticleType (spawnorg);
}


/*
===============
CL_EntityParticles
===============
*/
#define NUMVERTEXNORMALS	162
extern float	r_avertexnormals[NUMVERTEXNORMALS][3];	// FIXME, links to renderer
vec3_t	avelocities[NUMVERTEXNORMALS];
float	beamlength = 16;
vec3_t	avelocity = {23, 7, 3};
float	partstep = 0.01;
float	timescale = 0.01;

void CL_EntityParticles (vec3_t org)
{
	int			count;
	int			i;
	particle_t	*p;
	particle_type_t *pt;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward;
	float		dist;

	dist = 64;
	count = 50;

	if (!avelocities[0][0])
	{
		for (i = 0; i < NUMVERTEXNORMALS; i++)
		{
			avelocities[i][0] = (rand() & 255) * 0.01;
			avelocities[i][1] = (rand() & 255) * 0.01;
			avelocities[i][2] = (rand() & 255) * 0.01;
		}
	}

	pt = R_NewParticleType (org);

	for (i=0 ; i<NUMVERTEXNORMALS ; i++)
	{
		angle = cl.time * avelocities[i][0];
		sy = sin(angle);
		cy = cos(angle);
		angle = cl.time * avelocities[i][1];
		sp = sin(angle);
		cp = cos(angle);
		angle = cl.time * avelocities[i][2];
		sr = sin(angle);
		cr = cos(angle);

		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;

		if (!(p = R_NewParticle (pt)))
			return;

		p->die = cl.time + 0.01;
		p->color = 0x6f;
		p->type = pt_explode;

		p->org[0] = org[0] + r_avertexnormals[i][0] * dist + forward[0] * beamlength;
		p->org[1] = org[1] + r_avertexnormals[i][1] * dist + forward[1] * beamlength;
		p->org[2] = org[2] + r_avertexnormals[i][2] * dist + forward[2] * beamlength;
	}
}

/*
===============
CL_ClearParticles
===============
*/
void CL_ClearParticles (void)
{
	int		i;

	free_particles = (particle_t *) Hunk_AllocName (PARTICLE_BATCH_SIZE * sizeof (particle_t), "particles");

	for (i = 1; i < PARTICLE_BATCH_SIZE; i++)
	{
		free_particles[i - 1].next = &free_particles[i];
		free_particles[i].next = NULL;
	}

	// particle type chains
	free_particle_types = (particle_type_t *) Hunk_Alloc (PARTICLE_TYPE_BATCH_SIZE * sizeof (particle_type_t));
	active_particle_types = NULL;

	for (i = 1; i < PARTICLE_TYPE_BATCH_SIZE; i++)
	{
		free_particle_types[i - 1].next = &free_particle_types[i];
		free_particle_types[i].next = NULL;
	}

	r_numparticles = PARTICLE_BATCH_SIZE;
}


/*
===============
CL_ReadPointFile_f
===============
*/
void CL_ReadPointFile_f (void)
{
	FILE	*f;
	vec3_t	org;
	int		r;
	int		c;
	particle_t	*p;
	particle_type_t *pt;
	char	name[MAX_OSPATH];

	if (!com_serveractive && !r_refdef2.allow_cheats)
		return;

	sprintf (name, "maps/%s.pts", host_mapname.string);

	FS_FOpenFile (name, &f);
	if (!f)
	{
		Com_Printf ("couldn't open %s\n", name);
		return;
	}
	
	Com_Printf ("Reading %s...\n", name);
	c = 0;

	pt = R_NewParticleType (vec3_origin);

	for ( ;; )
	{
		r = fscanf (f,"%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;

		c++;
		
		if (!(p = R_NewParticle (pt)))
		{
			Com_Printf ("Not enough free particles\n");
			break;
		}

		p->die = 99999;
		p->color = (-c)&15;
		p->type = pt_static;
		VectorClear (p->vel);
		VectorCopy (org, p->org);
	}

	fclose (f);
	Com_Printf ("%i points read\n", c);
}
	
/*
===============
CL_ParticleExplosion
===============
*/
void CL_ParticleExplosion (vec3_t org)
{
	int			i, j;
	particle_t	*p;
	particle_type_t *pt = R_NewParticleType (org);
	
	for (i=0 ; i<1024 ; i++)
	{
		if (!(p = R_NewParticle (pt)))
			return;

		p->die = cl.time + 5;
		p->color = ramp1[0];
		p->ramp = rand()&3;
		p->type = (i & 1) ? pt_explode : pt_explode2;

		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand()%32)-16);
			p->vel[j] = (rand()%512)-256;
		}
	}
}


/*
===============
CL_ParticleExplosion2

NetQuake
===============
*/
void CL_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength)
{
	int			i, j;
	particle_t	*p;
	int			colorMod = 0;
	particle_type_t *pt = R_NewParticleType (org);
	
	for (i = 0; i < 512; i++)
	{
		if (!(p = R_NewParticle (pt)))
			return;

		p->die = cl.time + 0.3;
		p->color = colorStart + (colorMod % colorLength);
		colorMod++;

		p->type = pt_blob;

		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand()%32)-16);
			p->vel[j] = (rand()%512)-256;
		}
	}
}

/*
===============
CL_BlobExplosion
===============
*/
void CL_BlobExplosion (vec3_t org)
{
	int			i, j;
	particle_t	*p;
	particle_type_t *pt = R_NewParticleType (org);
	
	for (i=0 ; i<1024 ; i++)
	{
		if (!(p = R_NewParticle (pt)))
			return;

		p->die = cl.time + 1 + (rand()&8)*0.05;

		if (i & 1)
		{
			p->type = pt_blob;
			p->color = 66 + rand()%6;
		}
		else
		{
			p->type = pt_blob2;
			p->color = 150 + rand()%6;
		}

		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand()%32)-16);
			p->vel[j] = (rand()%512)-256;
		}
	}
}

/*
===============
CL_RunParticleEffect
===============
*/
void CL_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	int			scale;

	if (count > 130)
		scale = 3;
	else if (count > 20)
		scale = 2;
	else
		scale = 1;

	CL_RunParticleEffect2 (org, dir, color, count, scale);
}

/*
===============
CL_RunParticleEffect2
===============
*/
void CL_RunParticleEffect2 (vec3_t org, vec3_t dir, int color, int count, int scale)
{
	int			i, j;
	particle_t	*p;
	particle_type_t *pt = R_NewParticleType (org);

	for (i=0 ; i<count ; i++)
	{
		if (!(p = R_NewParticle (pt)))
			return;

		p->die = cl.time + 0.1*(rand()%5);
		p->color = (color&~7) + (rand()&7);
		p->type = pt_slowgrav;

		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + scale*((rand()&15)-8);
			p->vel[j] = dir[j] * 15;
		}
	}
}

/*
===============
CL_LavaSplash
===============
*/
void CL_LavaSplash (vec3_t org)
{
	int			i, j, k;
	particle_t	*p;
	float		vel;
	vec3_t		dir;
	particle_type_t *pt = R_NewParticleType (org);

	for (i=-16 ; i<16 ; i++)
	{
		for (j=-16 ; j<16 ; j++)
		{
			for (k=0 ; k<1 ; k++)
			{
				if (!(p = R_NewParticle (pt)))
					return;
		
				p->die = cl.time + 2 + (rand()&31) * 0.02;
				p->color = 224 + (rand()&7);
				p->type = pt_slowgrav;

				dir[0] = j*8 + (rand()&7);
				dir[1] = i*8 + (rand()&7);
				dir[2] = 256;
				
				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + (rand()&63);

				VectorNormalize (dir);
				vel = 50 + (rand()&63);
				VectorScale (dir, vel, p->vel);
			}
		}
	}
}

/*
===============
CL_TeleportSplash
===============
*/
void CL_TeleportSplash (vec3_t org)
{
	int			i, j, k;
	particle_t	*p;
	float		vel;
	vec3_t		dir;
	particle_type_t *pt = R_NewParticleType (org);

	for (i=-16 ; i<16 ; i+=4)
	{
		for (j=-16 ; j<16 ; j+=4)
		{
			for (k=-24 ; k<32 ; k+=4)
			{
				if (!(p = R_NewParticle (pt)))
					return;
		
				p->die = cl.time + 0.2 + (rand()&7) * 0.02;
				p->color = 7 + (rand()&7);
				p->type = pt_slowgrav;

				dir[0] = j*8;
				dir[1] = i*8;
				dir[2] = k*8;
				
				p->org[0] = org[0] + i + (rand()&3);
				p->org[1] = org[1] + j + (rand()&3);
				p->org[2] = org[2] + k + (rand()&3);
				
				VectorNormalize (dir);
				vel = 50 + (rand()&63);
				VectorScale (dir, vel, p->vel);
			}
		}
	}
}

/*
===============
CL_SlightBloodTrail
===============
*/
void CL_SlightBloodTrail (vec3_t start, vec3_t end, vec3_t trail_origin)
{
	int		j, num_particles;
	vec3_t	vec, point;
	float	len;
	particle_t	*p;
	particle_type_t *pt = R_NewParticleType (start);

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	VectorScale (vec, 6, vec);
	VectorCopy (start, point);

	num_particles = len / 6;
	if (!num_particles)
		return;

	for (; num_particles; num_particles--)
	{
		if (!(p = R_NewParticle (pt)))
			return;

		p->die = cl.time + 2;
		p->type = pt_grav;
		p->color = 67 + (rand()&3);
		VectorClear (p->vel);
		for (j=0 ; j<3 ; j++)
			p->org[j] = point[j] + ((rand()%6)-3);

		VectorAdd (point, vec, point);
	}
	VectorCopy (point, trail_origin);
}

/*
===============
CL_BloodTrail
===============
*/
void CL_BloodTrail (vec3_t start, vec3_t end, vec3_t trail_origin)
{
	int		j, num_particles;
	vec3_t	vec, point;
	float	len;
	particle_t	*p;
	particle_type_t *pt = R_NewParticleType (start);

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	VectorScale (vec, 3, vec);
	VectorCopy (start, point);

	num_particles = len /3;
	if (!num_particles)
		return;

	for (; num_particles; num_particles--)
	{
		if (!(p = R_NewParticle (pt)))
			return;

		p->die = cl.time + 2;
		p->type = pt_grav;
		p->color = 67 + (rand()&3);
		VectorClear (p->vel);
		for (j=0 ; j<3 ; j++)
			p->org[j] = point[j] + ((rand()%6)-3);

		VectorAdd (point, vec, point);
	}
	VectorCopy (point, trail_origin);
}

/*
===============
CL_VoorTrail
===============
*/
void CL_VoorTrail (vec3_t start, vec3_t end, vec3_t trail_origin)
{
	int		j, num_particles;
	vec3_t	vec, point;
	float	len;
	particle_t	*p;
	particle_type_t *pt = R_NewParticleType (start);

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	VectorScale (vec, 3, vec);
	VectorCopy (start, point);

	num_particles = len /3;
	if (!num_particles)
		return;

	for (; num_particles; num_particles--)
	{
		if (!(p = R_NewParticle (pt)))
			return;

		p->die = cl.time + 2;
		p->color = 9*16 + 8 + (rand()&3);
		p->type = pt_static;
		p->die = cl.time + 0.3;
		VectorClear (p->vel);
		for (j=0 ; j<3 ; j++)
			p->org[j] = point[j] + ((rand()&15)-8);

		VectorAdd (point, vec, point);
	}
}

/*
===============
CL_GrenadeTrail
===============
*/
void CL_GrenadeTrail (vec3_t start, vec3_t end, vec3_t trail_origin)
{
	int		j, num_particles;
	vec3_t	vec, point;
	float	len;
	particle_t	*p;
	particle_type_t *pt = R_NewParticleType (start);

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	VectorScale (vec, 3, vec);
	VectorCopy (start, point);

	num_particles = len / 3;
	if (!num_particles)
		return;

	for (; num_particles; num_particles--)
	{
		if (!(p = R_NewParticle (pt)))
			return;

		p->die = cl.time + 2;
		p->ramp = (rand()&3) + 2;
		p->color = ramp3[(int)p->ramp];
		p->type = pt_fire;
		VectorClear (p->vel);
		for (j=0 ; j<3 ; j++)
			p->org[j] = point[j] + ((rand()%6)-3);

		VectorAdd (point, vec, point);
	}
	VectorCopy (point, trail_origin);
}

/*
===============
CL_RocketTrail
===============
*/
void CL_RocketTrail (vec3_t start, vec3_t end, vec3_t trail_origin)
{
	int		j, num_particles;
	vec3_t	vec, point;
	float	len;
	particle_t	*p;
	particle_type_t *pt = R_NewParticleType (start);

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	VectorScale (vec, 3, vec);
	VectorCopy (start, point);

	num_particles = len /3;
	if (!num_particles)
		return;

	for (; num_particles; num_particles--)
	{
		if (!(p = R_NewParticle (pt)))
			return;

		p->die = cl.time + 2;
		p->ramp = (rand()&3);
		p->color = ramp3[(int)p->ramp];
		p->type = pt_fire;
		VectorClear (p->vel);
		for (j=0 ; j<3 ; j++)
			p->org[j] = point[j] + ((rand()%6)-3);

		VectorAdd (point, vec, point);
	}
	VectorCopy (point, trail_origin);
}

/*
===============
CL_TracerTrail
===============
*/
void CL_TracerTrail (vec3_t start, vec3_t end, vec3_t trail_origin, int color)
{
	int		num_particles;
	vec3_t	vec, point;
	float	len;
	particle_t	*p;
	static int tracercount;
	particle_type_t *pt = R_NewParticleType (start);

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	VectorScale (vec, 3, vec);
	VectorCopy (start, point);

	num_particles = len /3;
	if (!num_particles)
		return;

	for (; num_particles; num_particles--)
	{
		if (!(p = R_NewParticle (pt)))
			return;
		
		p->die = cl.time + 0.5;
		p->type = pt_static;
		p->color = color + ((tracercount&4)<<1);

		tracercount++;

		VectorCopy (point, p->org);

		if (tracercount & 1)
		{
			p->vel[0] = 30*vec[1];
			p->vel[1] = 30*-vec[0];
		}
		else
		{
			p->vel[0] = 30*-vec[1];
			p->vel[1] = 30*vec[0];
		}

		p->vel[2] = 0;
		VectorAdd (point, vec, point);
	}
	VectorCopy (point, trail_origin);
}


/*
===============
CL_LinkParticles
===============
*/
void CL_LinkParticlesForType (particle_type_t *pt)
{
	particle_t		*p, *kill;
	int				i;
	float			time1, time2, time3, dvel, frametime, grav;
	extern	cvar_t	sv_gravity;

	// nothing to run
	if (!pt->numparticles) return;
	if (!pt->particles) return;

	frametime = cls.frametime;
	time1 = frametime * 5;
	time2 = frametime * 10;
	time3 = frametime * 15;
	grav = frametime * sv_gravity.value * 0.05;
	dvel = 4 * frametime;

	for ( ;; ) 
	{
		kill = pt->particles;

		if (kill && kill->die < cl.time)
		{
			pt->particles = kill->next;
			kill->next = free_particles;
			free_particles = kill;
			r_numparticles--;
			pt->numparticles--;
			continue;
		}
		break;
	}

	for (p = pt->particles; p; p = p->next)
	{
		for ( ;; )
		{
			kill = p->next;

			if (kill && kill->die < cl.time)
			{
				p->next = kill->next;
				kill->next = free_particles;
				free_particles = kill;
				r_numparticles--;
				pt->numparticles--;
				continue;
			}
			break;
		}

//		V_AddParticle (p->org, p->color, p->alpha);

		p->org[0] += p->vel[0] * frametime;
		p->org[1] += p->vel[1] * frametime;
		p->org[2] += p->vel[2] * frametime;

		switch (p->type)
		{
			case pt_static:
				break;
			case pt_fire:
				p->ramp += time1;

				if (p->ramp >= 6)
					p->die = -1;
				else
					p->color = ramp3[(int)p->ramp];

				p->vel[2] += grav;
				break;

			case pt_explode:
				p->ramp += time2;

				if (p->ramp >=8)
					p->die = -1;
				else
					p->color = ramp1[(int)p->ramp];

				for (i=0 ; i<3 ; i++)
					p->vel[i] += p->vel[i]*dvel;

				p->vel[2] -= grav;
				break;

			case pt_explode2:
				p->ramp += time3;

				if (p->ramp >=8)
					p->die = -1;
				else
					p->color = ramp2[(int)p->ramp];

				for (i=0 ; i<3 ; i++)
					p->vel[i] -= p->vel[i]*frametime;

				p->vel[2] -= grav;
				break;

			case pt_blob:
				for (i=0 ; i<3 ; i++)
					p->vel[i] += p->vel[i]*dvel;

				p->vel[2] -= grav;
				break;

			case pt_blob2:
				for (i=0 ; i<2 ; i++)
					p->vel[i] -= p->vel[i]*dvel;

				p->vel[2] -= grav;
				break;

			case pt_grav:
			case pt_slowgrav:
				p->vel[2] -= grav;
				break;
		}
	}
}

void CL_LinkParticles (void)
{
	particle_type_t	*pt, *kill;

	if (!active_particle_types) return;

	// remove from the head of the list
	for (;;)
	{
		kill = active_particle_types;

		if (kill && !kill->particles)
		{
			// return to the free list
			active_particle_types = kill->next;
			kill->next = free_particle_types;
			kill->numparticles = 0;
			free_particle_types = kill;

			continue;
		}

		break;
	}

	// no types currently active
	if (!active_particle_types)
		return;

	for (pt = active_particle_types; pt; pt = pt->next)
	{
		// remove from a mid-point in the list
		for (;;)
		{
			kill = pt->next;

			if (kill && !kill->particles)
			{
				pt->next = kill->next;
				kill->next = free_particle_types;
				kill->numparticles = 0;
				free_particle_types = kill;

				continue;
			}

			break;
		}

		CL_LinkParticlesForType (pt);
	}
}

//============================================================

static cdlight_t	cl_dlights[MAX_DLIGHTS];

// sent to the renderer
int				cl_numvisdlights;
dlight_t		cl_visdlights[MAX_DLIGHTS];

/*
===============
CL_FindDlight
===============
*/
cdlight_t *CL_FindDlight (int key)
{
	int		i;
	cdlight_t	*dl;

	// first look for an exact key match
	if (key)
	{
		dl = cl_dlights;
		for (i = 0; i < MAX_DLIGHTS; i++, dl++)
		{
			if (dl->key == key)
			{
				memset (dl, 0, sizeof(*dl));
				dl->key = key;
				return dl;
			}
		}
	}

	// then look for anything else
	dl = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, dl++)
	{
		if (dl->die < cl.time)
		{
			memset (dl, 0, sizeof(*dl));
			dl->key = key;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset (dl, 0, sizeof(*dl));
	dl->key = key;
	return dl;
}

/*
===============
CL_AllocDlight
===============
*/
cdlight_t *CL_AllocDlight (int key)
{
	// find a dlight to use
	cdlight_t *dl = CL_FindDlight (key);

	// set default colour for any we don't colour ourselves
	dl->color[0] = dl->color[1] = dl->color[2] = 255;

	// done
	return dl;
}


void R_ColorDLight (dlight_t *dl, int r, int g, int b)
{
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
}


void R_ColorWizLight (dlight_t *dl)
{
	R_ColorDLight (dl, 308, 351, 109);
}


void R_ColorLightningLight (dlight_t *dl)
{
	// old - R_ColorDLight (dl, 2, 225, 541);
	R_ColorDLight (dl, 65, 232, 470);
}


/*
===============
CL_LinkDlights
===============
*/
void CL_LinkDlights (void)
{
	int			i;
	cdlight_t	*dl;

	dl = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, dl++)
	{
		if (dl->die < cl.time || dl->radius <= 0)
			continue;

		V_AddDlight(dl->key, dl->origin, dl->radius, dl->minlight, dl->die, dl->color);

		dl->radius -= cls.frametime * dl->decay;

		if (dl->radius < 0)
			dl->radius = 0;
	}
}

/*
===============
CL_ClearDlights
===============
*/
void CL_ClearDlights (void)
{
	memset (cl_dlights, 0, sizeof(cl_dlights));
}
