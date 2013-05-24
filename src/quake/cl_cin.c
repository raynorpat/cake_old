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
// cl_cin.c - cinematic functions

#include "quakedef.h"
#include "cdaudio.h"
#include "sound.h"
#include "keys.h"
#include "render.h"
#include "rc_image.h"

typedef struct
{
	byte	*data;
	int		count;
} cblock_t;

qbool cin_intro_hack = false;

/*
==================
SCR_StopCinematic
==================
*/
static void SCR_StopCinematic (void)
{
	cls.cinplayback = false;

	cl.cin.cinematictime = 0; // done
	if (cl.cin.pic)
	{
		free (cl.cin.pic);
		cl.cin.pic = NULL;
	}
	if (cl.cin.cinematicpalette_active)
	{
		R_SetPalette(NULL);
		cl.cin.cinematicpalette_active = false;
	}
	if (cl.cin.cinematic_file)
	{
		fclose (cl.cin.cinematic_file);
		cl.cin.cinematic_file = NULL;
	}
	if (cl.cin.hnodes1)
	{
		free (cl.cin.hnodes1);
		cl.cin.hnodes1 = NULL;
	}

	// HACK!
	if (cin_intro_hack)
	{
		// start demo loop and show main menu
		if (!com_serveractive)
		{
			Cbuf_AddText("startdemos demo1 demo2 demo3\n");
			Cbuf_AddText("togglemenu\n");
			Cbuf_Execute();
		}
	}
}

//==========================================================================

/*
==================
SmallestNode1
==================
*/
static int SmallestNode1 (int numhnodes)
{
	int		i;
	int		best, bestnode;

	best = 99999999;
	bestnode = -1;
	for (i=0 ; i<numhnodes ; i++)
	{
		if (cl.cin.h_used[i])
			continue;
		if (!cl.cin.h_count[i])
			continue;
		if (cl.cin.h_count[i] < best)
		{
			best = cl.cin.h_count[i];
			bestnode = i;
		}
	}

	if (bestnode == -1)
		return -1;

	cl.cin.h_used[bestnode] = true;
	return bestnode;
}


/*
==================
Huff1TableInit

Reads the 64k counts table and initializes the node trees
==================
*/
static void Huff1TableInit (void)
{
	int		prev;
	int		j;
	int		*node, *nodebase;
	byte	counts[256];
	int		numhnodes;

	cl.cin.hnodes1 = malloc (256*256*2*4);
	memset (cl.cin.hnodes1, 0, 256*256*2*4);

	for (prev=0 ; prev<256 ; prev++)
	{
		memset (cl.cin.h_count,0,sizeof(cl.cin.h_count));
		memset (cl.cin.h_used,0,sizeof(cl.cin.h_used));

		// read a row of counts
		fread (counts, 1, sizeof(counts), cl.cin.cinematic_file);
		for (j=0 ; j<256 ; j++)
			cl.cin.h_count[j] = counts[j];

		// build the nodes
		numhnodes = 256;
		nodebase = cl.cin.hnodes1 + prev*256*2;

		while (numhnodes != 511)
		{
			node = nodebase + (numhnodes-256)*2;

			// pick two lowest counts
			node[0] = SmallestNode1 (numhnodes);
			if (node[0] == -1)
				break;	// no more

			node[1] = SmallestNode1 (numhnodes);
			if (node[1] == -1)
				break;

			cl.cin.h_count[numhnodes] = cl.cin.h_count[node[0]] + cl.cin.h_count[node[1]];
			numhnodes++;
		}

		cl.cin.numhnodes1[prev] = numhnodes-1;
	}
}

/*
==================
Huff1Decompress
==================
*/
static cblock_t Huff1Decompress (cblock_t in)
{
	byte		*input;
	byte		*out_p;
	int			nodenum;
	int			count;
	cblock_t	out;
	int			inbyte;
	int			*hnodes, *hnodesbase;

	// get decompressed count
	count = in.data[0] + (in.data[1]<<8) + (in.data[2]<<16) + (in.data[3]<<24);
	input = in.data + 4;
	out_p = out.data = malloc (count);

	// read bits

	hnodesbase = cl.cin.hnodes1 - 256*2;	// nodes 0-255 aren't stored

	hnodes = hnodesbase;
	nodenum = cl.cin.numhnodes1[0];
	while (count)
	{
		inbyte = *input++;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cl.cin.numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cl.cin.numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cl.cin.numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cl.cin.numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cl.cin.numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cl.cin.numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cl.cin.numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cl.cin.numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
	}

	if (input - in.data != in.count && input - in.data != in.count+1)
	{
		Com_Printf ("Decompression overread by %i", (input - in.data) - in.count);
	}
	out.count = out_p - out.data;

	return out;
}

/*
==================
SCR_ReadNextFrame
==================
*/
static byte *SCR_ReadNextFrame (void)
{
	int		r;
	int		command;
	byte	samples[22050/14*4];
	byte	compressed[0x20000];
	int		size;
	byte	*pic;
	cblock_t	in, huf1;
	int		start, end, count;

	// read the next frame
	r = fread (&command, 4, 1, cl.cin.cinematic_file);
	if (r == 0)	// we'll give it one more chance
		r = fread (&command, 4, 1, cl.cin.cinematic_file);

	if (r != 1)
		return NULL;
	command = LittleLong(command);
	if (command == 2)
		return NULL; // last frame marker

	if (command == 1)
	{
		// read palette
		fread (cl.cin.cinematicpalette, 1, sizeof(cl.cin.cinematicpalette), cl.cin.cinematic_file);
		cl.cin.cinematicpalette_active = false;	// dubious....  exposes an edge case
	}

	// decompress the next frame
	fread (&size, 1, 4, cl.cin.cinematic_file);
	size = LittleLong(size);
	if (size > sizeof(compressed) || size < 1)
		Host_Error ("Bad compressed frame size");
	fread (compressed, 1, size, cl.cin.cinematic_file);

	// read sound
	start = cl.cin.cinematicframe*cl.cin.s_rate/14;
	end = (cl.cin.cinematicframe+1)*cl.cin.s_rate/14;
	count = end - start;

	fread (samples, 1, count*cl.cin.s_width*cl.cin.s_channels, cl.cin.cinematic_file);

	S_RawSamples (count, cl.cin.s_rate, cl.cin.s_width, cl.cin.s_channels, samples);

	in.data = compressed;
	in.count = size;

	huf1 = Huff1Decompress (in);

	pic = huf1.data;

	cl.cin.cinematicframe++;

	return pic;
}


/*
==================
SCR_RunCinematic
==================
*/
void SCR_RunCinematic (void)
{
	if (!cls.cinplayback)
		return;

	if (cl.cin.cinematictime <= 0)
	{
		cl.cin.cinematictime = cls.realtime;
		cl.cin.cinematicframe = -1;
	}

	while(cl.cin.cinematictime > 0)
	{
		int frame;
		
		if (cl.cin.cinematictime - 3 > cls.realtime * 1000)
			cl.cin.cinematictime = cls.realtime * 1000;

		frame = (cls.realtime * 1000 - cl.cin.cinematictime) * 14 / 1000;
		if (frame <= cl.cin.cinematicframe)
			return;

		if (frame > cl.cin.cinematicframe + 1)
		{
			Com_Printf ("Dropped frame: %i > %i\n", frame, cl.cin.cinematicframe + 1);
			cl.cin.cinematictime = cls.realtime * 1000 - cl.cin.cinematicframe * 1000 / 14;
		}

		if (cl.cin.pic)
			free (cl.cin.pic);
		cl.cin.pic = SCR_ReadNextFrame ();
		if (!cl.cin.pic)
		{
			SCR_StopCinematic ();
			return;
		}
	}
}

/*
==================
SCR_DrawCinematic
==================
*/
void SCR_DrawCinematic (void)
{
	if (cl.cin.cinematictime <= 0)
		return;

	if (!cl.cin.cinematicpalette_active)
	{
		R_SetPalette(cl.cin.cinematicpalette);
		cl.cin.cinematicpalette_active = true;
	}

	if (!cl.cin.pic)
		return;

	R_DrawStretchRaw (0, 0, vid.width, vid.height, cl.cin.width, cl.cin.height, cl.cin.pic);
}

/*
==================
SCR_PlayCinematic
==================
*/
static void SCR_PlayCinematic (char *name)
{
	int	width, height;

	// make sure CD isn't playing music
	CDAudio_Stop();

	cl.cin.cinematicframe = 0;

	FS_FOpenFile (name, &cl.cin.cinematic_file);
	if (!cl.cin.cinematic_file)
	{
//		Host_Error ("Cinematic %s not found.\n", name);
		cl.cin.cinematictime = 0; // done
		return;
	}

	SCR_EndLoadingPlaque ();

	fread (&width, 1, 4, cl.cin.cinematic_file);
	fread (&height, 1, 4, cl.cin.cinematic_file);
	cl.cin.width = LittleLong(width);
	cl.cin.height = LittleLong(height);

	fread (&cl.cin.s_rate, 1, 4, cl.cin.cinematic_file);
	cl.cin.s_rate = LittleLong(cl.cin.s_rate);
	fread (&cl.cin.s_width, 1, 4, cl.cin.cinematic_file);
	cl.cin.s_width = LittleLong(cl.cin.s_width);
	fread (&cl.cin.s_channels, 1, 4, cl.cin.cinematic_file);
	cl.cin.s_channels = LittleLong(cl.cin.s_channels);

	Huff1TableInit ();
	
	cls.cinplayback = true;
	cl.cin.cinematicframe = 0;
	cl.cin.pic = SCR_ReadNextFrame ();
	cl.cin.cinematictime = Sys_DoubleTime () * 1000 + 0.001;
}

void CL_PlayCin_f(void)
{
	char name[1024];

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("usage: playcin <cinname>\nplays cinematic named video/<cinname>.cin\n");
		return;
	}

	sprintf(name, "video/%s.cin", Cmd_Argv(1));

	// HACK: once the intro is done, load up demos
	if(!strcmp(Cmd_Argv(1), "idlog"))
		cin_intro_hack = true;

	SCR_PlayCinematic(name);
}

void CL_StopCin_f(void)
{
	SCR_StopCinematic();
}
