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
#include "input.h"
#include "keys.h"
#include "sound.h"
#include "version.h"
#include "cl_slist.h"

#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#else
#include <windows.h>
#endif

enum {m_none, m_main, m_singleplayer, m_load, m_save, m_multiplayer,
	m_setup, m_options, m_video, m_keys, m_help, m_credits, m_quit,
	m_gameoptions, m_slist, m_sedit, m_fps, m_demos, m_mods} m_state;

void M_Menu_Main_f (void);
	void M_Menu_SinglePlayer_f (void);
		void M_Menu_Load_f (void);
		void M_Menu_Save_f (void);
		void M_Menu_Help_f (void);
		void M_Menu_Credits_f (void);
	void M_Menu_MultiPlayer_f (void);
		void M_Menu_ServerList_f (void);
			void M_Menu_SEdit_f (void);
		void M_Menu_Demos_f (void);
		void M_Menu_Setup_f (void);
		void M_Menu_GameOptions_f (void);
	void M_Menu_Mods_f (void);
	void M_Menu_Options_f (void);
		void M_Menu_Keys_f (void);
		void M_Menu_Fps_f (void);
		void M_Menu_Video_f (void);
	void M_Menu_Quit_f (void);

void M_Main_Draw (void);
	void M_SinglePlayer_Draw (void);
		void M_Load_Draw (void);
		void M_Save_Draw (void);
		void M_Help_Draw (void);
		void M_Credits_Draw (void);
	void M_MultiPlayer_Draw (void);
		void M_ServerList_Draw (void);
			void M_SEdit_Draw (void);
		void M_Demos_Draw (void);
		void M_GameOptions_Draw (void);
		void M_Setup_Draw (void);
	void M_Mods_Draw (void);
	void M_Options_Draw (void);
		void M_Keys_Draw (void);
		void M_Fps_Draw (void);
		void M_Video_Draw (void);
	void M_Quit_Draw (void);

void M_Main_Key (int key);
	void M_SinglePlayer_Key (int key);
		void M_Load_Key (int key);
		void M_Save_Key (int key);
		void M_Help_Key (int key);
		void M_Credits_Key (int key);
	void M_MultiPlayer_Key (int key);
		void M_ServerList_Key (int key);
			void M_SEdit_Key (int key);
		void M_Demos_Key (int key);
		void M_GameOptions_Key (int key);
		void M_Setup_Key (int key);
	void M_Mods_Key (int key);
	void M_Options_Key (int key);
		void M_Keys_Key (int key);
		void M_Fps_Key (int key);
		void M_Video_Key (int key);
	void M_Quit_Key (int key);

qbool	m_entersound;		// play after drawing a frame, so caching won't disrupt the sound
int		m_topmenu;			// set if a submenu was entered via a menu_* command

//=============================================================================
/* Support Routines */

cvar_t	scr_menuscale = {"scr_menuscale", "2", CVAR_ARCHIVE};

void M_DrawChar (int cx, int line, int num)
{
	R_DrawChar (cx, line, num);
}

void M_Print (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawChar (cx, cy, (*str) + 128);
		str++;
		cx += 8;
	}
}

void M_PrintWhite (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawChar (cx, cy, *str);
		str++;
		cx += 8;
	}
}

static void M_ItemPrint(int cx, int cy, char *str, int unghosted)
{
	if (unghosted)
		M_Print (cx, cy, str);
	else
		M_PrintWhite (cx, cy, str);
}

void M_Centerprint (int cy, char *str)
{
	int cx;
	cx = vid.width / 2 - (strlen(str) * 4);

	while (*str)
	{
		R_DrawChar (cx, cy, (*str) + 128);
		str++;
		cx += 8;
	}
}

void M_CenterprintWhite (int cy, char *str)
{
	int cx;
	cx = vid.width / 2 - (strlen(str) * 4);

	while (*str)
	{
		R_DrawChar (cx, cy, *str);
		str++;
		cx += 8;
	}
}

void M_DrawPic (int x, int y, qpic_t *pic)
{
	R_DrawPic (x, y, pic);
}

void M_DrawTransPicTranslate (int x, int y, qpic_t *pic, int top, int bottom)
{
	R_DrawTransPicTranslate (x, y, pic, top, bottom);
}

void M_DrawTextBox (int x, int y, int width, int lines)
{
	Draw_TextBox (x, y, width, lines);
}

//=======================================================

#define BUTTON_HEIGHT	10
#define BUTTON_START	50
#define	BUTTON_MENU_X	100
#define LAYOUT_RED		116

/*
================
M_Main_Layout

JHL:ADD; Draws the main menu in desired manner
================
*/
void PrintRed (int cx, int cy, char *str)
{
	while (*str)
	{
		R_DrawChar (cx, cy, (*str) + 128);
		str++;
		cx += 8;
	}
}

void PrintWhite (int cx, int cy, char *str)
{
	while (*str)
	{
		R_DrawChar (cx, cy, *str);
		str++;
		cx += 8;
	}
}

void M_Main_ButtonList (char *buttons[], int cursor_location, int in_main)
{
	int	x, y,
		x_mod,
		x_length,
		i;

	x_length = 0;

	for ( i = 0; buttons[i] != 0; i++ )
	{
		x_length = x_length + (strlen(buttons[i])*8);
	}

	x_mod = (vid.width - x_length) / (i+1);
	y = vid.height / 14;
	x = 0;

	for ( i = 0; buttons[i] != 0; i++ )
	{
		// center on point origin
		x = x + x_mod;
		if (cursor_location == i)
		{
			PrintWhite (x, y, buttons[i]);
			if (in_main == true)
				R_DrawChar (x-10, y, 12+((int)(curtime * 4)&1));
		}
		else
			PrintRed (x, y, buttons[i]);
		x = x + (strlen(buttons[i])*8);
	}
}

void M_Main_Layout (int f_cursor, int f_inmenu)
{
	// the layout
	char	*names[] =
	{
		"Single",
		"Multiplayer",
		"Mods",
		"Options",
		"Quit",
		0
	};

	RB_SetCanvas (CANVAS_NONE);

	// top
	R_DrawFilledRect (0, 0, vid.width, vid.height / 8, 0, 1);
	R_DrawFilledRect (0, vid.height / 16, vid.width, 1, LAYOUT_RED, 1);
	
	// bottom
	R_DrawFilledRect (0,vid.height - 24, vid.width, vid.height, 0, 1);

	// bottom fade

	M_Main_ButtonList (names, f_cursor, f_inmenu);

	// HACK: In submenu, so do the background
	if (f_inmenu == false)
		R_FadeScreen ();

	RB_SetCanvas (CANVAS_MENU);
}


//=============================================================================

/*
================
M_ToggleMenu_f
================
*/
void M_ToggleMenu_f (void)
{
	m_entersound = true;

	if (key_dest == key_menu)
	{
		if (m_state != m_main)
		{
			M_Menu_Main_f ();
			return;
		}
		key_dest = key_game;
		m_state = m_none;
		return;
	}
	else
	{
		M_Menu_Main_f ();
	}
}

/*
================
M_EnterMenu
================
*/
void M_EnterMenu (int state)
{
	if (key_dest != key_menu) {
		m_topmenu = state;
		Con_ClearNotify ();
		// hide the console
		scr_conlines = 0;
		scr_con_current = 0;
	} else
		m_topmenu = m_none;

	key_dest = key_menu;
	m_state = state;
	m_entersound = true;
}

/*
================
M_LeaveMenu
================
*/
void M_LeaveMenu (int parent)
{
	if (m_topmenu == m_state) {
		m_state = m_none;
		key_dest = key_game;
	} else {
		m_state = parent;
		m_entersound = true;
	}
}

//=============================================================================
/* MAIN MENU */

int	m_main_cursor;
#define	MAIN_ITEMS	5

#define		M_M_SINGLE	0
#define		M_M_MULTI	1
#define		M_M_MODS	2
#define		M_M_OPTION	3
#define		M_M_QUIT	4

void M_Menu_Main_f (void)
{
	M_EnterMenu (m_main);
}

void M_Main_Draw (void)
{
	M_Main_Layout (m_main_cursor, true);
}

void M_Main_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		key_dest = key_game;
		m_state = m_none;
		break;

	case K_LEFTARROW:
	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		break;

	case K_RIGHTARROW:
	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		break;

	case K_ENTER:
		m_entersound = true;

		switch (m_main_cursor)
		{
		case M_M_SINGLE:
			M_Menu_SinglePlayer_f ();
			break;

		case M_M_MULTI:
			M_Menu_MultiPlayer_f ();
			break;

		case M_M_OPTION:
			M_Menu_Options_f ();
			break;

		case M_M_MODS:
			M_Menu_Mods_f ();
			break;

		case M_M_QUIT:
			M_Menu_Quit_f ();
			break;
		}
	}
}


//=============================================================================
/* OPTIONS MENU */

#define	OPTIONS_ITEMS	11

#define	SLIDER_RANGE	10

static int		options_cursor;

extern cvar_t	v_gamma;
extern cvar_t	v_contrast;

void M_Menu_Options_f (void)
{
	M_EnterMenu (m_options);
}

void M_AdjustSliders (int dir)
{
	S_LocalSound ("misc/menu3.wav");

	switch (options_cursor)
	{
	case 3:	// screen size
		scr_viewsize.value += dir * 10;
		if (scr_viewsize.value < 30)
			scr_viewsize.value = 30;
		if (scr_viewsize.value > 120)
			scr_viewsize.value = 120;
		Cvar_SetValue (&scr_viewsize, scr_viewsize.value);
		break;
	case 4:	// gamma
		v_gamma.value += dir * 0.0625;
		if (v_gamma.value < 0.5)
			v_gamma.value = 0.5;
		if (v_gamma.value > 3)
			v_gamma.value = 3;
		Cvar_SetValue (&v_gamma, v_gamma.value);
		break;
	case 5:	// contrast
		v_contrast.value += dir * 0.0625;
		if (v_contrast.value < 1)
			v_contrast.value = 1;
		if (v_contrast.value > 4)
			v_contrast.value = 4;
		Cvar_SetValue (&v_contrast, v_contrast.value);
		break;
	case 6:	// mouse speed
		sensitivity.value += dir * 0.5;
		if (sensitivity.value < 3)
			sensitivity.value = 3;
		if (sensitivity.value > 15)
			sensitivity.value = 15;
		Cvar_SetValue (&sensitivity, sensitivity.value);
		break;
	case 7:	// music volume
#ifdef _WIN32
		bgmvolume.value += dir * 1.0;
#else
		bgmvolume.value += dir * 0.1;
#endif
		if (bgmvolume.value < 0)
			bgmvolume.value = 0;
		if (bgmvolume.value > 1)
			bgmvolume.value = 1;
		Cvar_SetValue (&bgmvolume, bgmvolume.value);
		break;
	case 8:	// sfx volume
		s_volume.value += dir * 0.1;
		if (s_volume.value < 0)
			s_volume.value = 0;
		if (s_volume.value > 1)
			s_volume.value = 1;
		Cvar_SetValue (&s_volume, s_volume.value);
		break;
	}
}

void M_DrawSlider (int x, int y, float range)
{
	int	i;

	if (range < 0)
		range = 0;
	if (range > 1)
		range = 1;
	M_DrawChar (x-8, y, 128);
	for (i=0 ; i<SLIDER_RANGE ; i++)
		M_DrawChar (x + i*8, y, 129);
	M_DrawChar (x+i*8, y, 130);
	M_DrawChar (x + (SLIDER_RANGE-1)*8 * range, y, 131);
}

#define	SLIDER_RANGE	10

static void M_DrawRangeSlider (int x, int y, float num, float rangemin, float rangemax)
{
	char text[16];
	int i;
	float range;
	range = bound(0, (num - rangemin) / (rangemax - rangemin), 1);
	M_DrawChar (x-8, y, 128);
	for (i = 0;i < SLIDER_RANGE;i++)
		M_DrawChar (x + i*8, y, 129);
	M_DrawChar (x+i*8, y, 130);
	M_DrawChar (x + (SLIDER_RANGE-1)*8 * range, y, 131);
	if (fabs((int)num - num) < 0.01)
		Q_snprintf(text, sizeof(text), "%i", (int)num);
	else
		Q_snprintf(text, sizeof(text), "%.3f", num);
	M_Print(x + (SLIDER_RANGE+2) * 8, y, text);
}

void M_DrawCheckbox (int x, int y, int on)
{
#if 0
	if (on)
		M_DrawChar (x, y, 131);
	else
		M_DrawChar (x, y, 129);
#endif
	if (on)
		M_Print (x, y, "on");
	else
		M_Print (x, y, "off");
}

void M_Options_Draw (void)
{
	float		r;
	qpic_t	*p;

	M_Main_Layout (M_M_OPTION, false);

	p = R_CachePic ("gfx/p_option.lmp");
	M_DrawPic ((320 - GetPicWidth(p)) / 2, 4, p);

	M_PrintWhite (16, 32, "    Customize controls");
	M_PrintWhite (16, 40, "         Go to console");
	M_PrintWhite (16, 48, "     Reset to defaults");

	M_Print (16, 56, "           Screen size");
	r = (scr_viewsize.value - 30) / (120 - 30);
	M_DrawSlider (220, 56, r);

	M_Print (16, 64, "                 Gamma");
	r = v_gamma.value - 1.0;
	M_DrawSlider (220, 64, r);

	M_Print (16, 72, "              Contrast");
	r = v_contrast.value - 1.0;
	M_DrawSlider (220, 72, r);

	M_Print (16, 80, "           Mouse Speed");
	r = (sensitivity.value - 3)/(15 - 3);
	M_DrawSlider (220, 80, r);

	M_Print (16, 88, "       CD Music Volume");
	r = bgmvolume.value;
	M_DrawSlider (220, 88, r);

	M_Print (16, 96, "          Sound Volume");
	r = s_volume.value;
	M_DrawSlider (220, 96, r);

	M_PrintWhite (16, 104, "        Effect Options");

	M_PrintWhite (16, 112, "         Video Options");

	// cursor
	M_DrawChar (200, 32 + options_cursor*8, 12+((int)(curtime*4)&1));
}


void M_Options_Key (int k)
{
	switch (k)
	{
	case K_BACKSPACE:
		m_topmenu = m_none;	// intentional fallthrough
	case K_ESCAPE:
		M_LeaveMenu (m_main);
		break;

	case K_ENTER:
		m_entersound = true;
		switch (options_cursor)
		{
		case 0:
			M_Menu_Keys_f ();
			break;
		case 1:
			m_state = m_none;
			key_dest = key_console;
//			Con_ToggleConsole_f ();
			break;
		case 2:
			Cbuf_AddText ("exec default.cfg\n");
			break;
		case 9:
			M_Menu_Fps_f ();
			break;
		case 10:
			M_Menu_Video_f ();
			break;
		default:
			M_AdjustSliders (1);
			break;
		}
		return;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor--;
		if (options_cursor < 0)
			options_cursor = OPTIONS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor++;
		if (options_cursor >= OPTIONS_ITEMS)
			options_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		options_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		options_cursor = OPTIONS_ITEMS-1;
		break;

	case K_LEFTARROW:
		M_AdjustSliders (-1);
		break;

	case K_RIGHTARROW:
		M_AdjustSliders (1);
		break;
	}
}


//=============================================================================
/* KEYS MENU */

char *bindnames[][2] =
{
{"+attack", 		"attack"},
{"+use", 			"use"},
{"+jump", 			"jump"},
{"+forward", 		"move forward"},
{"+back", 			"move back"},
{"+moveleft", 		"move left"},
{"+moveright", 		"move right"},
{"+moveup",			"swim up"},
{"+movedown",		"swim down"},
{"impulse 12", 		"previous weapon"},
{"impulse 10", 		"next weapon"},
{"+left", 			"turn left"},
{"+right", 			"turn right"},
{"+lookup", 		"look up"},
{"+lookdown", 		"look down"},
};

#define	NUMCOMMANDS	(sizeof(bindnames)/sizeof(bindnames[0]))

int		keys_cursor;
int		bind_grab;

void M_Menu_Keys_f (void)
{
	M_EnterMenu (m_keys);
}


void M_FindKeysForCommand (char *command, int *twokeys)
{
	int		count;
	int		j;
	int		l;
	char	*b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen(command);
	count = 0;

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

void M_UnbindCommand (char *command)
{
	int		j;
	int		l;
	char	*b;

	l = strlen(command);

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
			Key_Unbind (j);
	}
}


void M_Keys_Draw (void)
{
	int		i, l;
	int		keys[2];
	char	*name;
	int		x, y;
	qpic_t	*p;

	M_Main_Layout (M_M_OPTION, false);

	p = R_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ( (320 - GetPicWidth(p))/2, 4, p);

	if (bind_grab)
		M_Print (12, 32, "Press a key or button for this action");
	else
		M_Print (18, 32, "Enter to change, del to clear");

	// search for known bindings
	for (i=0 ; i<NUMCOMMANDS ; i++)
	{
		y = 48 + 8*i;

		M_Print (16, y, bindnames[i][1]);

		l = strlen (bindnames[i][0]);

		M_FindKeysForCommand (bindnames[i][0], keys);

		if (keys[0] == -1)
		{
			M_Print (156, y, "???");
		}
		else
		{
			char str[64];

			name = Key_KeynumToString (keys[0], str, sizeof(str) - 1);
			M_Print (156, y, name);
			x = strlen(name) * 8;
			if (keys[1] != -1)
			{
				M_Print (156 + x + 8, y, "or");
				M_Print (156 + x + 32, y, Key_KeynumToString (keys[1], NULL, 0));
			}
		}
	}

	if (bind_grab)
		M_DrawChar (142, 48 + keys_cursor*8, '=');
	else
		M_DrawChar (142, 48 + keys_cursor*8, 12+((int)(curtime*4)&1));
}


void M_Keys_Key (int k)
{
	int		keys[2];

	if (bind_grab)
	{	// defining a key
		S_LocalSound ("misc/menu1.wav");
		if (k == K_ESCAPE)
		{
			bind_grab = false;
		}
		else if (k != '`')
		{
			Key_SetBinding (k, bindnames[keys_cursor][0]);
		}

		bind_grab = false;
		return;
	}

	switch (k)
	{
	case K_BACKSPACE:
		m_topmenu = m_none;	// intentional fallthrough
	case K_ESCAPE:
		M_LeaveMenu (m_options);
		break;

	case K_LEFTARROW:
	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor--;
		if (keys_cursor < 0)
			keys_cursor = NUMCOMMANDS-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor++;
		if (keys_cursor >= NUMCOMMANDS)
			keys_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor = NUMCOMMANDS - 1;
		break;

	case K_ENTER:		// go into bind mode
		M_FindKeysForCommand (bindnames[keys_cursor][0], keys);
		S_LocalSound ("misc/menu2.wav");
		if (keys[1] != -1)
			M_UnbindCommand (bindnames[keys_cursor][0]);
		bind_grab = true;
		break;

	case K_DEL:				// delete bindings
		S_LocalSound ("misc/menu2.wav");
		M_UnbindCommand (bindnames[keys_cursor][0]);
		break;
	}
}


//=============================================================================
/* FPS SETTINGS MENU */

#define	FPS_ITEMS	13

int		fps_cursor = 0;

extern cvar_t v_bonusflash;
extern cvar_t v_damagecshift;
extern cvar_t r_fastsky;

void M_Menu_Fps_f (void)
{
	M_EnterMenu (m_fps);
}

void M_Fps_Draw (void)
{
	qpic_t	*p;

	M_Main_Layout (M_M_OPTION, false);

	p = R_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ( (320 - GetPicWidth(p))/2, 4, p);

	M_Print (16, 32, "            Explosions");
	M_Print (220, 32, cl_explosion.value==0 ? "normal" :
		cl_explosion.value==1 ? "type 1" : cl_explosion.value==2 ? "type 2" :
		cl_explosion.value==3 ? "type 3" : "");

	M_Print (16, 40, "         Muzzleflashes");
	M_Print (220, 40, cl_muzzleflash.value==2 ? "own off" :
		cl_muzzleflash.value ? "on" : "off");

	M_Print (16, 48, "            Gib filter");
	M_DrawCheckbox (220, 48, cl_gibfilter.value);

	M_Print (16, 56, "    Dead bodies filter");
	M_DrawCheckbox (220, 56, cl_deadbodyfilter.value);

	M_Print (16, 64, "          Rocket model");
	M_Print (220, 64, cl_r2g.value ? "grenade" : "normal");

	M_Print (16, 72, "          Rocket trail");
	M_Print (220, 72, r_rockettrail.value==2 ? "grenade" :
		r_rockettrail.value ? "normal" : "off");

	M_Print (16, 80, "          Rocket light");
	M_DrawCheckbox (220, 80, r_rocketlight.value);

	M_Print (16, 88, "         Damage filter");
	M_DrawCheckbox (220, 88, v_damagecshift.value == 0);

	M_Print (16, 96, "        Pickup flashes");
	M_DrawCheckbox (220, 96, v_bonusflash.value);

	M_Print (16, 104, "         Powerup glow");
	M_Print (220, 104, r_powerupglow.value==2 ? "own off" :
		r_powerupglow.value ? "on" : "off");

	M_Print (16, 112, "             Fast sky");
	M_DrawCheckbox (220, 112, r_fastsky.value);

	M_PrintWhite (16, 120, "            Fast mode");

	M_PrintWhite (16, 128, "         High quality");

	// cursor
	M_DrawChar (200, 32 + fps_cursor*8, 12+((int)(curtime*4)&1));
}

void M_Fps_Key (int k)
{
	int i;

	switch (k)
	{
	case K_BACKSPACE:
		m_topmenu = m_none;	// intentional fallthrough
	case K_ESCAPE:
		M_LeaveMenu (m_options);
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		fps_cursor--;
		if (fps_cursor == 13)
			fps_cursor = 12;
		if (fps_cursor < 0)
			fps_cursor = FPS_ITEMS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		fps_cursor++;
		if (fps_cursor == 13)
			fps_cursor = 12;
		if (fps_cursor >= FPS_ITEMS)
			fps_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		fps_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		fps_cursor = FPS_ITEMS - 1;
		break;

	case K_RIGHTARROW:
	case K_ENTER:
		S_LocalSound ("misc/menu2.wav");
		switch (fps_cursor) {
		case 0:
			i = cl_explosion.value + 1;
			if (i > 3 || i < 0)
				i = 0;
			Cvar_SetValue (&cl_explosion, i);
			break;
		case 1:
			Cvar_SetValue (&cl_muzzleflash, cl_muzzleflash.value==2 ? 1 :
				cl_muzzleflash.value ? 0 : 2);
			break;
		case 2:
			Cvar_SetValue (&cl_gibfilter, !cl_gibfilter.value);
			break;
		case 3:
			Cvar_SetValue (&cl_deadbodyfilter, !cl_deadbodyfilter.value);
			break;
		case 4:
			Cvar_SetValue (&cl_r2g, !cl_r2g.value);
			break;
		case 5:
			i = r_rockettrail.value + 1;
			if (i < 0 || i > 2)
				i = 0;
			Cvar_SetValue (&r_rockettrail, i);
			break;
		case 6:
			Cvar_SetValue (&r_rocketlight, !r_rocketlight.value);
			break;
		case 7:
			Cvar_SetValue (&v_damagecshift, !v_damagecshift.value);
			break;
		case 8:
			Cvar_SetValue (&v_bonusflash, !v_bonusflash.value);
			break;
		case 9:
			i = r_powerupglow.value + 1;
				if (i < 0 || i > 2)
					i = 0;
			Cvar_SetValue (&r_powerupglow, i);
			break;
		case 10:
			Cvar_SetValue (&r_fastsky, !r_fastsky.value);
			break;

		// fast
		case 11:
			Cvar_SetValue (&cl_explosion, 3);
			Cvar_SetValue (&cl_muzzleflash, 2);
			Cvar_SetValue (&cl_gibfilter, 1);
			Cvar_SetValue (&cl_deadbodyfilter, 1);
			Cvar_SetValue (&r_rocketlight, 0);
			Cvar_SetValue (&r_powerupglow, 0);
			Cvar_SetValue (&r_fastsky, 1);
			break;

		// high quality
		case 12:
			Cvar_SetValue (&cl_explosion, 0);
			Cvar_SetValue (&cl_muzzleflash, 1);
			Cvar_SetValue (&cl_gibfilter, 0);
			Cvar_SetValue (&cl_deadbodyfilter, 0);
			Cvar_SetValue (&r_rocketlight, 1);
			Cvar_SetValue (&r_powerupglow, 2);
			Cvar_SetValue (&r_fastsky, 0);
		}
	}
}


//=============================================================================
/* VIDEO MENU */

typedef struct video_resolution_s
{
	const char *type;
	int width, height;
	int conwidth, conheight;
	double pixelheight; // pixel aspect
} video_resolution_t;

static video_resolution_t video_resolutions_hardcoded[] =
{
{"Standard 4x3"              ,  320, 240, 320, 240, 1     },
{"Standard 4x3"              ,  400, 300, 400, 300, 1     },
{"Standard 4x3"              ,  512, 384, 512, 384, 1     },
{"Standard 4x3"              ,  640, 480, 640, 480, 1     },
{"Standard 4x3"              ,  800, 600, 640, 480, 1     },
{"Standard 4x3"              , 1024, 768, 640, 480, 1     },
{"Standard 4x3"              , 1152, 864, 640, 480, 1     },
{"Standard 4x3"              , 1280, 960, 640, 480, 1     },
{"Standard 4x3"              , 1400,1050, 640, 480, 1     },
{"Standard 4x3"              , 1600,1200, 640, 480, 1     },
{"Standard 4x3"              , 1792,1344, 640, 480, 1     },
{"Standard 4x3"              , 1856,1392, 640, 480, 1     },
{"Standard 4x3"              , 1920,1440, 640, 480, 1     },
{"Standard 4x3"              , 2048,1536, 640, 480, 1     },
{"Short Pixel (CRT) 5x4"     ,  320, 256, 320, 256, 0.9375},
{"Short Pixel (CRT) 5x4"     ,  640, 512, 640, 512, 0.9375},
{"Short Pixel (CRT) 5x4"     , 1280,1024, 640, 512, 0.9375},
{"Tall Pixel (CRT) 8x5"      ,  320, 200, 320, 200, 1.2   },
{"Tall Pixel (CRT) 8x5"      ,  640, 400, 640, 400, 1.2   },
{"Tall Pixel (CRT) 8x5"      ,  840, 525, 640, 400, 1.2   },
{"Tall Pixel (CRT) 8x5"      ,  960, 600, 640, 400, 1.2   },
{"Tall Pixel (CRT) 8x5"      , 1680,1050, 640, 400, 1.2   },
{"Tall Pixel (CRT) 8x5"      , 1920,1200, 640, 400, 1.2   },
{"Square Pixel (LCD) 5x4"    ,  320, 256, 320, 256, 1     },
{"Square Pixel (LCD) 5x4"    ,  640, 512, 640, 512, 1     },
{"Square Pixel (LCD) 5x4"    , 1280,1024, 640, 512, 1     },
{"WideScreen 5x3"            ,  640, 384, 640, 384, 1     },
{"WideScreen 5x3"            , 1280, 768, 640, 384, 1     },
{"WideScreen 8x5"            ,  320, 200, 320, 200, 1     },
{"WideScreen 8x5"            ,  640, 400, 640, 400, 1     },
{"WideScreen 8x5"            ,  720, 450, 720, 450, 1     },
{"WideScreen 8x5"            ,  840, 525, 640, 400, 1     },
{"WideScreen 8x5"            ,  960, 600, 640, 400, 1     },
{"WideScreen 8x5"            , 1280, 800, 640, 400, 1     },
{"WideScreen 8x5"            , 1440, 900, 720, 450, 1     },
{"WideScreen 8x5"            , 1680,1050, 640, 400, 1     },
{"WideScreen 8x5"            , 1920,1200, 640, 400, 1     },
{"WideScreen 8x5"            , 2560,1600, 640, 400, 1     },
{"WideScreen 8x5"            , 3840,2400, 640, 400, 1     },
{"WideScreen 14x9"           ,  840, 540, 640, 400, 1     },
{"WideScreen 14x9"           , 1680,1080, 640, 400, 1     },
{"WideScreen 16x9"           ,  640, 360, 640, 360, 1     },
{"WideScreen 16x9"           ,  683, 384, 683, 384, 1     },
{"WideScreen 16x9"           ,  960, 540, 640, 360, 1     },
{"WideScreen 16x9"           , 1280, 720, 640, 360, 1     },
{"WideScreen 16x9"           , 1360, 768, 680, 384, 1     },
{"WideScreen 16x9"           , 1366, 768, 683, 384, 1     },
{"WideScreen 16x9"           , 1920,1080, 640, 360, 1     },
{"WideScreen 16x9"           , 2560,1440, 640, 360, 1     },
{"WideScreen 16x9"           , 3840,2160, 640, 360, 1     },
{"NTSC 3x2"                  ,  360, 240, 360, 240, 1.125 },
{"NTSC 3x2"                  ,  720, 480, 720, 480, 1.125 },
{"PAL 14x11"                 ,  360, 283, 360, 283, 0.9545},
{"PAL 14x11"                 ,  720, 566, 720, 566, 0.9545},
{"NES 8x7"                   ,  256, 224, 256, 224, 1.1667},
{"SNES 8x7"                  ,  512, 448, 512, 448, 1.1667},
{NULL, 0, 0, 0, 0, 0}
};
// this is the number of the default mode (640x480) in the list above

#define VIDEO_ITEMS 8
static int video_cursor = 0;
static int video_cursor_table[VIDEO_ITEMS] = {68, 88, 96, 104, 112, 120, 128, 136};
static int video_resolution;

video_resolution_t *video_resolutions;
int video_resolutions_count;

void M_Menu_Video_f (void)
{
	int i;

	M_EnterMenu (m_video);

	// Look for the closest match to the current resolution
	video_resolution = 0;
	for (i = 1;i < video_resolutions_count;i++)
	{
		// if the new mode would be a worse match in width, skip it
		if (fabs(video_resolutions[i].width - vid.width) > fabs(video_resolutions[video_resolution].width - vid.width))
			continue;
		// if it is equal in width, check height
		if (video_resolutions[i].width == vid.width && video_resolutions[video_resolution].width == vid.width)
		{
			// if the new mode would be a worse match in height, skip it
			if (fabs(video_resolutions[i].height - vid.height) > fabs(video_resolutions[video_resolution].height - vid.height))
				continue;
			// better match for width and height
			video_resolution = i;
		}
		else // better match for width
			video_resolution = i;
	}
}

void M_Video_Draw (void)
{
	qpic_t	*p;
	int t;

	M_Main_Layout (M_M_OPTION, false);

	p = R_CachePic("gfx/vidmodes.lmp");
	M_DrawPic((320-p->width)/2, 4, p);

	t = 0;

	// Current and Proposed Resolution
	M_Print(16, video_cursor_table[t] - 12, "    Current Resolution");
	if (vid_supportrefreshrate && vid.userefreshrate && vid.fullscreen)
		M_Print(220, video_cursor_table[t] - 12, va("%dx%d %dhz", vid.width, vid.height, vid.refreshrate));
	else
		M_Print(220, video_cursor_table[t] - 12, va("%dx%d", vid.width, vid.height));
	M_Print(16, video_cursor_table[t], "        New Resolution");
	M_Print(220, video_cursor_table[t], va("%dx%d", video_resolutions[video_resolution].width, video_resolutions[video_resolution].height));
	M_Print(96, video_cursor_table[t] + 8, va("Type: %s", video_resolutions[video_resolution].type));
	t++;

	// Antialiasing
	M_Print(16, video_cursor_table[t], "          Antialiasing");
	M_DrawRangeSlider(220, video_cursor_table[t], vid_samples.value, 1, 32);
	t++;

	// Refresh Rate
	M_ItemPrint(16, video_cursor_table[t], "      Use Refresh Rate", vid_supportrefreshrate);
	M_DrawCheckbox(220, video_cursor_table[t], vid_userefreshrate.value);
	t++;

	// Refresh Rate
	M_ItemPrint(16, video_cursor_table[t], "          Refresh Rate", vid_supportrefreshrate && vid_userefreshrate.value);
	M_DrawRangeSlider(220, video_cursor_table[t], vid_refreshrate.value, 60, 150);
	t++;

	// Fullscreen
	M_Print(16, video_cursor_table[t], "            Fullscreen");
	M_DrawCheckbox(220, video_cursor_table[t], vid_fullscreen.value);
	t++;

	// Vertical Sync
	M_ItemPrint(16, video_cursor_table[t], "         Vertical Sync", gl_videosyncavailable);
	M_DrawCheckbox(220, video_cursor_table[t], vid_vsync.value);
	t++;

	// Mouse
	M_Print(16, video_cursor_table[t], "         Use Mouse");
	M_DrawCheckbox(220, video_cursor_table[t], vid_mouse.value);
	t++;

	// "Apply" button
	M_Print(220, video_cursor_table[t], "Apply");
	t++;

	// Cursor
	M_DrawChar(200, video_cursor_table[video_cursor], 12+((int)(cl.time*4)&1));
}

void M_Menu_Video_AdjustSliders (int dir)
{
	int t;

	S_LocalSound ("misc/menu3.wav");

	t = 0;
	if (video_cursor == t++)
	{
		// Resolution
		int r;
		for(r = 0;r < video_resolutions_count;r++)
		{
			video_resolution += dir;
			if (video_resolution >= video_resolutions_count)
				video_resolution = 0;
			if (video_resolution < 0)
				video_resolution = video_resolutions_count - 1;
			if (video_resolutions[video_resolution].width >= vid_minwidth.value && video_resolutions[video_resolution].height >= vid_minheight.value)
				break;
		}
	}
	else if (video_cursor == t++)
		Cvar_SetValue (&vid_samples, bound(1, vid_samples.value * (dir > 0 ? 2 : 0.5), 32));
	else if (video_cursor == t++)
		Cvar_SetValue (&vid_userefreshrate, !vid_userefreshrate.value);
	else if (video_cursor == t++)
		Cvar_SetValue (&vid_refreshrate, bound(60, vid_refreshrate.value + dir, 150));
	else if (video_cursor == t++)
		Cvar_SetValue (&vid_fullscreen, !vid_fullscreen.value);
	else if (video_cursor == t++)
		Cvar_SetValue (&vid_vsync, !vid_vsync.value);
}

void M_Video_Key (int key)
{
	switch (key)
	{
		case K_ESCAPE:
			// vid_shared.c has a copy of the current video config. We restore it
			Cvar_SetValue(&vid_fullscreen, vid.fullscreen);
			Cvar_SetValue(&vid_samples, vid.samples);
			if (vid_supportrefreshrate)
				Cvar_SetValue(&vid_refreshrate, vid.refreshrate);
			Cvar_SetValue(&vid_userefreshrate, vid.userefreshrate);

			S_LocalSound ("misc/menu1.wav");
			M_Menu_Options_f ();
			break;

		case K_ENTER:
			m_entersound = true;
			switch (video_cursor)
			{
				case (VIDEO_ITEMS - 1):
					Cvar_SetValue (&vid_width, video_resolutions[video_resolution].width);
					Cvar_SetValue (&vid_height, video_resolutions[video_resolution].height);
					Cvar_SetValue (&vid_conwidth, video_resolutions[video_resolution].conwidth);
					Cvar_SetValue (&vid_conheight, video_resolutions[video_resolution].conheight);
					Cvar_SetValue (&vid_pixelheight, video_resolutions[video_resolution].pixelheight);
					Cbuf_AddText ("vid_restart\n");
					M_Menu_Options_f ();
					break;
				default:
					M_Menu_Video_AdjustSliders (1);
			}
			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			video_cursor--;
			if (video_cursor < 0)
				video_cursor = VIDEO_ITEMS-1;
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			video_cursor++;
			if (video_cursor >= VIDEO_ITEMS)
				video_cursor = 0;
			break;

		case K_LEFTARROW:
			M_Menu_Video_AdjustSliders (-1);
			break;

		case K_RIGHTARROW:
			M_Menu_Video_AdjustSliders (1);
			break;
	}
}

//=============================================================================
/* HELP MENU */

int		help_page;
#define	NUM_HELP_PAGES	6

#define HELP_MOVEMENT	0
#define HELP_SWIMMING	1
#define HELP_HAZARDS	2
#define HELP_BUTTONS	3
#define HELP_SECRETS	4
#define HELP_ORDERING	5

void M_Menu_Help_f (void)
{
	M_EnterMenu (m_help);
	help_page = 0;
}

void M_Help_Draw (void)
{
	qpic_t	*p;
	char	onscreen_help[64];
	int		y;

	M_Main_Layout (M_M_SINGLE, false);

	M_DrawTextBox (-4, -4, 39, 29);

	if (help_page == HELP_MOVEMENT)
	{
		p = R_CachePic ("gfx/menu_help.lmp");
		M_DrawPic ((320 - p->width) / 2, 8, p);

		y = 52;
		M_PrintWhite (16, y, "BASIC MOVEMENT"); y += 8;
		y += 8;
		M_Print (16, y,	    "  Use the arrow keys to turn and");  y += 8;
		M_Print (16, y,     "  move in the direction you're");  y += 8;
		M_Print (16, y,     "  facing. Holding the SHIFT key");  y += 8;
		M_Print (16, y,     "  down will speed your movement.");  y += 8;
		M_Print (16, y,     "  To reconfigure your keys and");  y += 8;
		M_Print (16, y,     "  mouse buttons, select the");  y += 8;
		M_Print (16, y,     "  CUSTOMIZE KEYS from OPTIONS.");  y += 8;
		M_Print (16, y,     "  menu.");  y += 8;
		y += 16;
		M_PrintWhite (16, y, "LOOKING AROUND"); y += 8;
		y += 8;
		M_Print (16, y,	    "  You can use the A and Z keys");  y += 8;
		M_Print (16, y,     "  to look up and down. A better");  y += 8;
		M_Print (16, y,     "  way to look around use the"); y += 8;
		M_Print (16, y,		"  mouse. It allows you to"); y += 8;
		M_Print (16, y,     "  observe your surroundings");  y += 8;
		M_Print (16, y,     "  smoothly.");  y += 8;
	}
	else if (help_page == HELP_SWIMMING)
	{
		p = R_CachePic ("gfx/menu_help.lmp");
		M_DrawPic ((320 - p->width) / 2, 8, p);

		y = 52;
		M_Print (16, y,	    "  It is very important to get");  y += 8;
		M_Print (16, y,	    "  used to looking around because");  y += 8;
		M_Print (16, y,	    "  many enemies come from above");  y += 8;
		M_Print (16, y,	    "  and if you want to swim it");  y += 8;
		M_Print (16, y,	    "  is a must.");  y += 8;
		y += 16;
		M_PrintWhite (16, y, "JUMPING AND SWIMMING"); y += 8;
		y += 8;
		M_Print (16, y,	    "  To jump up, press the SPACEBAR.");  y += 8;
		M_Print (16, y,	    "  When you are in water, pressing");  y += 8;
		M_Print (16, y,	    "  the SPACEBAR will make you");  y += 8;
		M_Print (16, y,	    "  swim directly up to the surface");  y += 8;
		M_Print (16, y,	    "  no matter which direction you");  y += 8;
		M_Print (16, y,	    "  are facing - this is a good");  y += 8;
		M_Print (16, y,	    "  panic button if you become");  y += 8;
		M_Print (16, y,	    "  disoriented in the water.");  y += 8;
	}
	else if (help_page == HELP_HAZARDS)
	{
		p = R_CachePic ("gfx/menu_help.lmp");
		M_DrawPic ((320 - p->width) / 2, 8, p);

		y = 52;
		M_Print (16, y,	    "  To swim in a certain direction,");  y += 8;
		M_Print (16, y,	    "  look into that direction and");  y += 8;
		M_Print (16, y,	    "  and press the UP arrow to move");  y += 8;
		M_Print (16, y,	    "  forward. DO NOT swim in green");  y += 8;
		M_Print (16, y,	    "  slime or lava, unless you have");  y += 8;
		M_Print (16, y,	    "  a Biosuit.");  y += 8;
		y += 8;
		p = R_CachePic ("gfx/menu_help_hazards.lmp");
		M_DrawPic ((320 - p->width) / 2, y, p); y += p->height+16;
		M_PrintWhite (16, y, "OPENING DOORS AND PUSHING BUTTONS"); y += 8;
		y += 8;
		M_Print (16, y,	    "  To open doors and push buttons,");  y += 8;
		M_Print (16, y,	    "  merely touch them by moving");  y += 8;
		M_Print (16, y,	    "  into them. If a door cannot be");  y += 8;
		M_Print (16, y,	    "  opened there is usually a");  y += 8;
		M_Print (16, y,	    "  message stating the reason.");  y += 8;
	}
	else if (help_page == HELP_BUTTONS)
	{
		p = R_CachePic ("gfx/menu_help.lmp");
		M_DrawPic ((320 - p->width) / 2, 8, p);

		y = 52;
		M_Print (16, y,	    "  If a door needs a key, get the");  y += 8;
		M_Print (16, y,	    "  key and come back and touch");  y += 8;
		M_Print (16, y,	    "  the door.");  y += 8;
		y += 8;
		p = R_CachePic ("gfx/menu_help_keys.lmp");
		M_DrawPic ((320 - p->width) / 2, y, p); y += p->height+16;
		M_Print (16, y,	    "  Most of the time, pressing a");  y += 8;
		M_Print (16, y,	    "  button will affect an object");  y += 8;
		M_Print (16, y,	    "  nearby. Some buttons can be");  y += 8;
		M_Print (16, y,	    "  pressed multiple times - You will");  y += 8;
		M_Print (16, y,	    "  see them press in, them come");  y += 8;
		M_Print (16, y,	    "  back out, ready for another");  y += 8;
		M_Print (16, y,	    "  press. Most buttons animate so");  y += 8;
		M_Print (16, y,	    "  they are easy to locate.");  y += 8;
		y += 8;
		p = R_CachePic ("gfx/menu_help_buttons.lmp");
		M_DrawPic ((320 - p->width) / 2, y, p);
	}
	else if (help_page == HELP_SECRETS)
	{
		p = R_CachePic ("gfx/menu_help.lmp");
		M_DrawPic ((320 - p->width) / 2, 8, p);

		y = 52;
		M_PrintWhite (16, y, "SHOOTING BUTTONS AND SECRET WALLS"); y += 8;
		y += 8;
		M_Print (16, y,	    "  Some buttons must be shot");  y += 8;
		M_Print (16, y,     "  (or hit with your axe) to use");  y += 8;
		M_Print (16, y,     "  them.");  y += 8;
		y += 8;
		p = R_CachePic ("gfx/menu_help_shoot.lmp");
		M_DrawPic ((320 - p->width) / 2, y, p); y += p->height+16;
		M_Print (16, y,	    "  To find secret areas, look for");  y += 8;
		M_Print (16, y,	    "  walls that have a texture on");  y += 8;
		M_Print (16, y,	    "  them that is different than");  y += 8;
		M_Print (16, y,	    "  the surrounding walls, then");  y += 8;
		M_Print (16, y,	    "  shoot it (or hit it with your");  y += 8;
		M_Print (16, y,	    "  axe)!");  y += 8;
	}
	else if (help_page == HELP_ORDERING)
	{
		p = R_CachePic ("gfx/menu_help_ordering.lmp");
		M_DrawPic ((320 - p->width) / 2, 8, p);

		y = 52;
		M_Print	(16,y, "All four episodes can be"); y += 8;
		M_Print	(16,y, "ordered by calling"); y += 8;
		y += 16;
		M_PrintWhite (16, y, "1-800-idGAMES"); y += 8;
		y += 16;
		M_Print	(16, y, "Quake's price is $45 plus"); y += 8;
		M_Print	(16, y, "$5 shipping and handling."); y += 8;
		y += 16;
		M_Print	(16, y, "If you live outside the U.S.A."); y += 8;
		M_Print	(16, y, "consult the ORDER.TXT file"); y += 8;
		M_Print	(16, y, "in the Quake directory for"); y += 8;
		M_Print	(16, y, "ordering information or"); y += 8;
		M_Print	(16, y, "visit your local software"); y += 8;
		M_Print	(16, y, "retailer."); y += 8;
	}

	RB_SetCanvas(CANVAS_NONE);

	// do the help message
	sprintf (onscreen_help, "Page %i/%i", help_page+1, NUM_HELP_PAGES);
	M_Centerprint (vid.height - 16, onscreen_help);
	M_CenterprintWhite (vid.height - 8, "Use the arrow keys to scroll the pages.");
}

void M_Help_Key (int key)
{
	switch (key)
	{
	case K_BACKSPACE:
		m_topmenu = m_none;	// intentional fallthrough
	case K_ESCAPE:
		M_LeaveMenu (m_main);
		break;

	case K_UPARROW:
	case K_RIGHTARROW:
		m_entersound = true;
		if (++help_page >= NUM_HELP_PAGES)
			help_page = 0;
		break;

	case K_DOWNARROW:
	case K_LEFTARROW:
		m_entersound = true;
		if (--help_page < 0)
			help_page = NUM_HELP_PAGES - 1;
		break;
	}
}

//=============================================================================
/* CREDITS MENU */

int		credits_page;
#define	NUM_CREDITS_PAGES	2

#define CREDITS_CAKE	0
#define CREDITS_ID		1

void M_Menu_Credits_f (void)
{
	M_EnterMenu (m_credits);
	credits_page = 0;
}

void M_Credits_Draw (void)
{
	char	onscreen_help[64];
	int		y;

	M_Main_Layout (M_M_SINGLE, false);

	M_DrawTextBox (-4, -4, 39, 29);

	if (credits_page == CREDITS_CAKE)
	{
		y = 16;
		M_PrintWhite (16, y, "Cake - Super QuakeWorld Engine Project");

		y = (int)(curtime * 10)%6;
		M_DrawPic (160, 26, R_CachePic( va("gfx/menudot%i.lmp", y+1 ) ) );

		y = 56;
		M_PrintWhite (16, y,  "Programming and Project Lead"); y += 8;
		M_Print (16, y,	      " Pat 'raynorpat' Raynor"); y += 8;
		y += 8;
		M_PrintWhite (16, y,  "Special Thanks"); y += 8;
		M_Print (16, y,       " Anton 'Tonik' Gavrilov     mh"); y += 8;
		M_Print (16, y,       " Spike                      LordHavoc"); y += 8;
		M_Print (16, y,       " Tim 'timbo' Angus          QuakeForge"); y += 8;
		y += 16;
		M_PrintWhite (16, y, "Cake is open source software; you can"); y += 8;
		M_PrintWhite (16, y, "redistribute it and/or modify it under"); y += 8;
		M_PrintWhite (16, y, "the terms of the GNU General Public"); y += 8;
		M_PrintWhite (16, y, "License. See the GNU General Public"); y += 8;
		M_PrintWhite (16, y, "License for more details.");
	}
	else if (credits_page == CREDITS_ID)
	{
		y = 16;
		M_PrintWhite (16, y, "QUAKE by id Software");

		y = 28;
		M_PrintWhite (16, y,  "Programming        Art"); y += 8;
		M_Print (16, y,	      " John Carmack       Adrian Carmack"); y += 8;
		M_Print (16, y,		  " Michael Abrash     Kevin Cloud"); y += 8;
		M_Print (16, y,       " John Cash          Paul Steed"); y += 8;
		M_Print (16, y,       " Dave 'Zoid' Kirsch"); y += 8;
		y += 8;
		M_PrintWhite (16, y,  "Design             Biz"); y += 8;
		M_Print (16, y,       " John Romero        Jay Wilbur"); y += 8;
		M_Print (16, y,       " Sandy Petersen     Mike Wilson"); y += 8;
		M_Print (16, y,       " American McGee     Donna Jackson"); y += 8;
		M_Print (16, y,       " Tim Willits        Todd Hollenshead"); y += 8;
		y += 8;
		M_PrintWhite (16, y,  "Support            Projects\n"); y += 8;
		M_Print (16, y,       " Barrett Alexander  Shawn Green\n"); y += 8;
		y += 8;
		M_PrintWhite (16, y,  "Sound Effects"); y += 8;
		M_Print (16, y,       " Trent Reznor and Nine Inch Nails"); y += 8;
		y += 16;
		M_PrintWhite (16, y, "Quake is a trademark of Id Software,"); y += 8;
		M_PrintWhite (16, y, "inc., (c)1996 Id Software, inc. All"); y += 8;
		M_PrintWhite (16, y, "rights reserved. NIN logo is a"); y += 8;
		M_PrintWhite (16, y, "registered trademark licensed to"); y += 8;
		M_PrintWhite (16, y, "Nothing Interactive, Inc. All rights"); y += 8;
		M_PrintWhite (16, y, "reserved.");
	}

	RB_SetCanvas(CANVAS_NONE);

	// do the credits message
	sprintf (onscreen_help, "Page %i/%i", credits_page+1, NUM_CREDITS_PAGES);
	M_Centerprint (vid.height - 16, onscreen_help);
	M_CenterprintWhite (vid.height - 8, "Use the arrow keys to scroll the pages.");
}

void M_Credits_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_SinglePlayer_f ();
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		m_entersound = true;
		if (++credits_page >= NUM_CREDITS_PAGES)
			credits_page = 0;
		break;

	case K_UPARROW:
	case K_LEFTARROW:
		m_entersound = true;
		if (--credits_page < 0)
			credits_page = NUM_CREDITS_PAGES - 1;
		break;
	}

}

//=============================================================================
/* QUIT MENU */

int		msgNumber;
int		m_quit_prevstate;
qbool	wasInMenus;

void M_Menu_Quit_f (void)
{
	if (m_state == m_quit)
		return;
	wasInMenus = (key_dest == key_menu);
	m_quit_prevstate = m_state;
	msgNumber = rand()&7;
	M_EnterMenu (m_quit);
}


void M_Quit_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case 'n':
	case 'N':
		if (wasInMenus)
		{
			m_state = m_quit_prevstate;
			m_entersound = true;
		}
		else
		{
			key_dest = key_game;
			m_state = m_none;
		}
		break;

	case K_ENTER:
	case 'Y':
	case 'y':
		key_dest = key_console;
		Host_Quit ();
		break;

	default:
		break;
	}
}


void M_Quit_Draw (void)
{
	static char *quitMessage [] = {
	/* .........1.........2.... */
	  "  Are you gonna quit    ",
	  "  this game just like   ",
	  "   everything else?     ",
	  "                        ",
 
	  " Milord, methinks that  ",
	  "   thou art a lowly     ",
	  " quitter. Is this true? ",
	  "                        ",

	  " Do I need to bust your ",
	  "  face open for trying  ",
	  "        to quit?        ",
	  "                        ",

	  " Man, I oughta smack you",
	  "   for trying to quit!  ",
	  "     Press Y to get     ",
	  "      smacked out.      ",
 
	  " Press Y to quit like a ",
	  "   big loser in life.   ",
	  "  Press N to stay proud ",
	  "    and successful!     ",
 
	  "   If you press Y to    ",
	  "  quit, I will summon   ",
	  "  Satan all over your   ",
	  "      hard drive!       ",
 
	  "  Um, Asmodeus dislikes ",
	  " his children trying to ",
	  " quit. Press Y to return",
	  "   to your Tinkertoys.  ",
 
	  "  If you quit now, I'll ",
	  "  throw a blanket-party ",
	  "   for you next time!   ",
	  "                        "
	};

	R_FadeScreen ();

	RB_SetCanvas (CANVAS_MENU);

	M_DrawTextBox (56, 76, 24, 4);
	M_Print (64, 84,  quitMessage[msgNumber*4+0]);
	M_Print (64, 92,  quitMessage[msgNumber*4+1]);
	M_Print (64, 100, quitMessage[msgNumber*4+2]);
	M_Print (64, 108, quitMessage[msgNumber*4+3]);
}

//=============================================================================
/* SINGLE PLAYER MENU */

#define	SINGLEPLAYER_ITEMS	5

#define	GAME_NEW		0
#define	GAME_LOAD		1
#define	GAME_SAVE		2
#define	GAME_HELP		3
#define	GAME_CREDITS	4

int game_cursor_table[] = {BUTTON_START,
	   					   BUTTON_START + BUTTON_HEIGHT,
						   BUTTON_START + BUTTON_HEIGHT*2,
						   BUTTON_START + BUTTON_HEIGHT*4,
						   BUTTON_START + BUTTON_HEIGHT*5};

int	m_singleplayer_cursor;
int	dim_load, dim_save;

extern	cvar_t	maxclients, teamplay, deathmatch, coop, skill, fraglimit, timelimit;

void M_Menu_SinglePlayer_f (void)
{
	M_EnterMenu (m_singleplayer);
}

void M_SinglePlayer_Draw (void)
{
	qpic_t	*p;
	char	*names[] =
	{
		"New game",
		"Load",
		"Save",
		"Help",
		"Credits",
		0
	};

	M_Main_Layout (M_M_SINGLE, false);

	p = R_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);

	dim_load = false;
	dim_save = false;

	//HACK: dim "save"/"load" if we can't choose 'em!
	if (cl.maxclients != 1 || deathmatch.value || coop.value)
	{
		dim_load = true;
		dim_save = true;
	}
	
	if (!com_serveractive || cl.intermission)
		dim_save = true;

	M_PrintWhite (BUTTON_MENU_X, game_cursor_table[GAME_NEW], names[0]);
	
	if (dim_load)
		M_Print (BUTTON_MENU_X, game_cursor_table[GAME_LOAD], names[1]);
	else
		M_PrintWhite (BUTTON_MENU_X, game_cursor_table[GAME_LOAD], names[1]);

	if (dim_save)
		M_Print (BUTTON_MENU_X, game_cursor_table[GAME_SAVE], names[2]);
	else
		M_PrintWhite (BUTTON_MENU_X, game_cursor_table[GAME_SAVE], names[2]);

	M_PrintWhite (BUTTON_MENU_X, game_cursor_table[GAME_HELP], names[3]);
	M_PrintWhite (BUTTON_MENU_X, game_cursor_table[GAME_CREDITS], names[4]);

	if ((m_singleplayer_cursor == 1 && dim_load == true) || (m_singleplayer_cursor == 2 && dim_save == true))
		m_singleplayer_cursor = 0;

	// cursor
	M_DrawChar (BUTTON_MENU_X - 10, game_cursor_table[m_singleplayer_cursor], 12+((int)(curtime * 4)&1));
}

static void StartNewGame (void)
{
	key_dest = key_game;

	Cvar_Set (&maxclients, "1");
	Cvar_Set (&teamplay, "0");
	Cvar_Set (&deathmatch, "0");
	Cvar_Set (&coop, "0");

	Host_EndGame ();
	Cbuf_AddText ("map start\n");
}

void M_SinglePlayer_Key (int key)
{
again:
	switch (key)
	{
	case K_BACKSPACE:
		m_topmenu = m_none;	// intentional fallthrough
	case K_ESCAPE:
		M_LeaveMenu (m_main);
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS)
			m_singleplayer_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_singleplayer_cursor < 0)
			m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
		break;

	case K_ENTER:
		switch (m_singleplayer_cursor)
		{
		case GAME_NEW:
			StartNewGame ();
			break;

		case GAME_LOAD:
			if (dim_load == true)
				m_entersound = false;
			M_Menu_Load_f ();
			break;

		case GAME_SAVE:
			if (dim_save == true)
				m_entersound = false;
			M_Menu_Save_f ();
			break;

		case GAME_HELP:
			M_Menu_Help_f ();
			break;

		case GAME_CREDITS:
			M_Menu_Credits_f ();
			break;
		}
	}
	if (m_singleplayer_cursor == GAME_LOAD && dim_load == true)
		goto again;
	if (m_singleplayer_cursor == GAME_SAVE && dim_save == true)
		goto again;
}


//=============================================================================
/* LOAD/SAVE MENU */

int		load_cursor;		// 0 < load_cursor < MAX_SAVEGAMES

#define	MAX_SAVEGAMES		12
char	m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH+1];
int		loadable[MAX_SAVEGAMES];

void M_ScanSaves (void)
{
	int		i, j, len;
	char	name[MAX_OSPATH];
	char	buf[SAVEGAME_COMMENT_LENGTH + 256];
	const char *t;
	qfile_t	*f;
	int		version;

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
	{
		strcpy (m_filenames[i], "--- UNUSED SLOT ---");
		loadable[i] = false;
		sprintf (name, "s%i.sav", i);
		f = FS_Open (name, "rb", false, false);
		if (!f)
			continue;
		// read enough to get the comment
		len = FS_Read(f, buf, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = 0;
		t = buf;
		// version
		COM_ParseTokenConsole(&t);
		version = atoi(com_token);
		// description
		COM_ParseTokenConsole(&t);
		strlcpy (m_filenames[i], com_token, sizeof (m_filenames[i]));

		// change _ back to space
		for (j=0 ; j<SAVEGAME_COMMENT_LENGTH ; j++)
			if (m_filenames[i][j] == '_')
				m_filenames[i][j] = ' ';
		loadable[i] = true;
		FS_Close (f);
	}
}

#ifndef CLIENTONLY
void M_Menu_Load_f (void)
{
	dim_load = false;
	if (cl.maxclients != 1 || deathmatch.value || coop.value)
		dim_load = true;
	
	if (dim_load == true)
		return;

	M_EnterMenu (m_load);
	M_ScanSaves ();
}


void M_Menu_Save_f (void)
{
	dim_save = false;
	if (cl.maxclients != 1 || deathmatch.value || coop.value)
		dim_save = true;
	
	if (!com_serveractive || cl.intermission)
		dim_save = true;

	if (dim_save == true)
		return;

	M_EnterMenu (m_save);
	M_ScanSaves ();
}
#endif


void M_Load_Draw (void)
{
	int		i;
	qpic_t	*p;

	M_Main_Layout (M_M_SINGLE, false);

	p = R_CachePic ("gfx/p_load.lmp");
	M_DrawPic ((320 - GetPicWidth(p)) / 2, 4, p);

	for (i = 0; i < MAX_SAVEGAMES; i++)
		M_Print (16, 32 + BUTTON_HEIGHT * i, m_filenames[i]);

	// line cursor
	M_DrawChar (8, 32 + load_cursor * BUTTON_HEIGHT, 12 + ((int)(curtime * 4)&1));
}


void M_Save_Draw (void)
{
	int		i;
	qpic_t	*p;

	M_Main_Layout (M_M_SINGLE, false);

	p = R_CachePic ("gfx/p_save.lmp");
	M_DrawPic ( (320 - GetPicWidth(p))/2, 4, p);

	for (i = 0; i < MAX_SAVEGAMES ; i++)
		M_Print (16, 32 + BUTTON_HEIGHT * i, m_filenames[i]);

	// line cursor
	M_DrawChar (8, 32 + load_cursor * BUTTON_HEIGHT, 12 + ((int)(curtime * 4)&1));
}


void M_Load_Key (int key)
{
	switch (key)
	{
	case K_BACKSPACE:
		m_topmenu = m_none;	// intentional fallthrough
	case K_ESCAPE:
		M_LeaveMenu (m_singleplayer);
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu2.wav");
		if (!loadable[load_cursor])
			return;
		m_state = m_none;
		key_dest = key_game;

		// SV_Loadgame_f can't bring up the loading plaque because too much
		// stack space has been used, so do it now
		SCR_BeginLoadingPlaque ();

		// issue the load command
		Cbuf_AddText (va ("load s%i\n", load_cursor) );
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;
	}
}

void M_Save_Key (int key)
{
	switch (key) {
	case K_BACKSPACE:
		m_topmenu = m_none;	// intentional fallthrough
	case K_ESCAPE:
		M_LeaveMenu (m_singleplayer);
		break;

	case K_ENTER:
		m_state = m_none;
		key_dest = key_game;
		Cbuf_AddText (va("save s%i\n", load_cursor));
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;
	}
}



//=============================================================================
/* MULTIPLAYER MENU */

int	m_multiplayer_cursor;

#define	MULTIPLAYER_ITEMS	4

void M_Menu_MultiPlayer_f (void)
{
	M_EnterMenu (m_multiplayer);
}

void M_MultiPlayer_Draw (void)
{
	qpic_t	*p;
	char	*names[] =
	{
		"Join game",
		"Demos",
		"Create game",
		"Player setup",
		0
	};

	M_Main_Layout (M_M_MULTI, false);

	p = R_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);

	M_DrawPic ((320 - p->width) / 2, 4, p);

	M_PrintWhite (BUTTON_MENU_X, BUTTON_START, names[0]);
	M_PrintWhite (BUTTON_MENU_X, BUTTON_START + BUTTON_HEIGHT, names[1]);
	M_PrintWhite (BUTTON_MENU_X, BUTTON_START + (BUTTON_HEIGHT * 2), names[2]);

	// cursor
	M_DrawChar (BUTTON_MENU_X - 10, BUTTON_START + (m_multiplayer_cursor * BUTTON_HEIGHT), 12 + ((int)(curtime * 4)&1));
}

void M_MultiPlayer_Key (int key)
{
	switch (key)
	{
	case K_BACKSPACE:
		m_topmenu = m_none;	// intentional fallthrough
	case K_ESCAPE:
		M_LeaveMenu (m_main);
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS)
			m_multiplayer_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_multiplayer_cursor < 0)
			m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		m_multiplayer_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
		break;

	case K_ENTER:
		m_entersound = true;
		switch (m_multiplayer_cursor)
		{
		case 0:
			M_Menu_ServerList_f ();
			break;

		case 1:
			M_Menu_GameOptions_f ();
			break;

		case 2:
			M_Menu_Demos_f ();
			break;

		case 3:
			M_Menu_Setup_f ();
			break;
		}
	}
}

//=============================================================================
/* DEMO MENU */

extern cvar_t sv_demoDir;

#ifdef _WIN32
#define	DEMO_TIME	FILETIME
#else
#define	DEMO_TIME	time_t
#endif

#define MAX_DEMO_NAME	MAX_OSPATH
#define MAX_DEMO_FILES	2048
#define DEMO_MAXLINES	17

typedef enum direntry_type_s {dt_file = 0, dt_dir, dt_up, dt_msg} direntry_type_t;
typedef enum demosort_type_s {ds_name = 0, ds_size, ds_time} demo_sort_t;

typedef struct direntry_s {
	direntry_type_t	type;
	char			*name;
	int				size;
	DEMO_TIME		time;
} direntry_t;

static direntry_t	demolist_data[MAX_DEMO_FILES];
static direntry_t	*demolist[MAX_DEMO_FILES];
static int			demolist_count;
static char			demo_currentdir[MAX_OSPATH] = {0};
static char			demo_prevdemo[MAX_DEMO_NAME] = {0};

static float		last_demo_time = 0;	

static int			demo_cursor = 0;
static int			demo_base = 0;

static demo_sort_t	demo_sorttype = ds_name;
static qbool		demo_reversesort = false;

int Demo_SortCompare(const void *p1, const void *p2) {
	int retval;
	int sign;
	direntry_t *d1, *d2;

	d1 = *((direntry_t **) p1);
	d2 = *((direntry_t **) p2);

	if ((retval = d2->type - d1->type) || d1->type > dt_dir)
		return retval;

	if (d1->type == dt_dir)
		return strcasecmp(d1->name, d2->name);
	
	sign = demo_reversesort ? -1 : 1;

	switch (demo_sorttype) {
		case ds_name:
			return sign * strcasecmp(d1->name, d2->name);
		case ds_size:
			return sign * (d1->size - d2->size);		
		case ds_time:
#ifdef _WIN32
			return -sign * CompareFileTime(&d1->time, &d2->time);
#else
			return -sign * (d1->time - d2->time);
#endif
		default:
			Sys_Error("Demo_SortCompare: unknown demo_sorttype (%d)", demo_sorttype);
			return 0;
	}
}

static void Demo_SortDemos(void) {
	int i;

	last_demo_time = 0;		

	for (i = 0; i < demolist_count; i++)
		demolist[i] = &demolist_data[i];

	qsort(demolist, demolist_count, sizeof(direntry_t *), Demo_SortCompare);
}

static void Demo_PositionCursor(void) {
	int i;

	last_demo_time = 0;		
	demo_base = demo_cursor = 0;	

	if (demo_prevdemo[0]) {
		for (i = 0; i < demolist_count; i++) {
			if (!strcmp (demolist[i]->name, demo_prevdemo)) {
				demo_cursor = i;
				if (demo_cursor >= DEMO_MAXLINES) {
					demo_base += demo_cursor - (DEMO_MAXLINES - 1);
					demo_cursor = DEMO_MAXLINES - 1;
				}
				break;
			}
		}
	}
	demo_prevdemo[0] = 0;
}

static void Demo_ReadDirectory(void) {
	int i, size;
	direntry_type_t type;
	DEMO_TIME time;
	char name[MAX_DEMO_NAME];
#ifdef _WIN32
	HANDLE h;
	WIN32_FIND_DATA fd;
#else	
	DIR *d;
	struct dirent *dstruct;
	struct stat fileinfo;
#endif

	demolist_count = demo_base = demo_cursor = 0;

	for (i = 0; i < MAX_DEMO_FILES; i++) {
		if (demolist_data[i].name) {
			free(demolist_data[i].name);
			demolist_data[i].name = NULL;
		}
	}

	if (demo_currentdir[0]) {	
		demolist_data[0].name = strdup ("..");
		demolist_data[0].type = dt_up;
		demolist_count = 1;
	}

#ifdef _WIN32
	h = FindFirstFile (va("%s/*.*", demo_currentdir), &fd);
	if (h == INVALID_HANDLE_VALUE) {
		demolist_data[demolist_count].name = strdup ("Error reading directory");
		demolist_data[demolist_count].type = dt_msg;
		demolist_count++;
		Demo_SortDemos();
		return;
	}
#else
	if (!(d = opendir(va("%s", demo_currentdir)))) {
		demolist_data[demolist_count].name = strdup ("Error reading directory");
		demolist_data[demolist_count].type = dt_msg;
		demolist_count++;
		Demo_SortDemos();
		return;
	}
	dstruct = readdir (d);
#endif

	do {
#ifdef _WIN32
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, ".."))
				continue;
			type = dt_dir;
			size = 0;
			memset(&time, 0, sizeof(time));
		} else {
			i = strlen(fd.cFileName);
			if (i < 5 ||
				(
					strcasecmp(fd.cFileName + i - 4, ".qwd") && 
					strcasecmp(fd.cFileName +i - 4, ".qwz") && 
					strcasecmp(fd.cFileName + i - 4, ".mvd")	
				)
			)
				continue;
			type = dt_file;
			size = fd.nFileSizeLow;
			time = fd.ftLastWriteTime;
		}

		Q_strncpyz (name, fd.cFileName, sizeof(name));
#else
		stat (va("%s/%s", demo_currentdir, dstruct->d_name), &fileinfo);

		if (S_ISDIR(fileinfo.st_mode)) {
			if (!strcmp(dstruct->d_name, ".") || !strcmp(dstruct->d_name, ".."))
				continue;
			type = dt_dir;
			time = size = 0;
		} else {
			i = strlen(dstruct->d_name);
			if (i < 5 ||
				(
					strcasecmp(dstruct->d_name + i - 4, ".qwd")
					&& strcasecmp(dstruct->d_name + i - 4, ".mvd")	
				)
			)
				continue;
			type = dt_file;
			size = fileinfo.st_size;
			time = fileinfo.st_mtime;
		}

		Q_strncpyz (name, dstruct->d_name, sizeof(name));
#endif

		if (demolist_count == MAX_DEMO_FILES)
			break;

		demolist_data[demolist_count].name = strdup(name);
		demolist_data[demolist_count].type = type;
		demolist_data[demolist_count].size = size;
		demolist_data[demolist_count].time = time;
		demolist_count++;

#ifdef _WIN32
	} while (FindNextFile(h, &fd));
	FindClose (h);
#else
	} while ((dstruct = readdir (d)));
	closedir (d);
#endif

	if (!demolist_count) {
		demolist_data[0].name = strdup("[ no files ]");
		demolist_data[0].type = dt_msg;
		demolist_count = 1;
	}

	Demo_SortDemos();
	Demo_PositionCursor();
}

void M_Menu_Demos_f (void) {
	static qbool demo_currentdir_init = false;

	M_EnterMenu(m_demos);
	
	if (!demo_currentdir_init) {
		demo_currentdir_init = true;
		strcpy(demo_currentdir, va("%s/%s", fs_gamedir, sv_demoDir.string));	
	}
	
	Demo_ReadDirectory();
}

static void Demo_FormatSize (char *t) {
	char *s;

	for (s = t; *s; s++) {
		if (*s >= '0' && *s <= '9')
			*s = *s - '0' + 18;
		else
			*s |= 128;
	}
}

#define DEMOLIST_NAME_WIDTH	29		

void M_Demos_Draw (void) {
	int i, y;
	direntry_t *d;
	char demoname[DEMOLIST_NAME_WIDTH], demosize[36 - DEMOLIST_NAME_WIDTH];

	static char last_demo_name[MAX_DEMO_NAME + 7];	
	static int last_demo_index = 0, last_demo_length = 0;	
	char demoname_scroll[38 + 1];
	int demoindex, scroll_index;
	float frac, time, elapsed;

	M_Main_Layout (M_M_MULTI, false);

	M_Print (140, 8, "DEMOS");
	Q_strncpyz(demoname_scroll, demo_currentdir[0] ? demo_currentdir : "/", sizeof(demoname_scroll));
	M_PrintWhite (16, 16, demoname_scroll);
	M_Print (8, 24, "\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1e\x1e\x1f");

	for (i = 0; i < demolist_count - demo_base && i < DEMO_MAXLINES; i++) {
		d = demolist[demo_base + i];
		y = 32 + 8 * i;
		Q_strncpyz (demoname, d->name, sizeof(demoname));

		switch (d->type) {
		case dt_file:
			M_Print (24, y, demoname);
			if (d->size > 99999 * 1024)
				Q_snprintf(demosize, sizeof(demosize), "%5iM", d->size >> 20);
			else
				Q_snprintf(demosize, sizeof(demosize), "%5iK", d->size >> 10);
			Demo_FormatSize(demosize);
			M_PrintWhite (24 + 8 * DEMOLIST_NAME_WIDTH, y, demosize);
			break;
		case dt_dir:
			M_PrintWhite (24, y, demoname);
			M_PrintWhite (24 + 8 * DEMOLIST_NAME_WIDTH, y, "folder");
			break;
		case dt_up:
			M_PrintWhite (24, y, demoname);
			M_PrintWhite (24 + 8 * DEMOLIST_NAME_WIDTH, y, "    up");
			break;
		case dt_msg:
			M_Print (24, y, demoname);
			break;
		default:
			Sys_Error("M_Demos_Draw: unknown d->type (%d)", d->type);
		}
	}

	M_DrawChar (8, 32 + demo_cursor * 8, 12 + ((int) (curtime * 4) & 1));

	demoindex = demo_base + demo_cursor;
	if (demolist[demoindex]->type == dt_file) {
		time = (float) Sys_DoubleTime();
		if (!last_demo_time || last_demo_index != demoindex) {
			last_demo_index = demoindex;
			last_demo_time = time;
			frac = scroll_index = 0;
			Q_snprintf(last_demo_name, sizeof(last_demo_name), "%s  ***  ", demolist[demoindex]->name);
			last_demo_length = strlen(last_demo_name);
		} else {			
			elapsed = 3.5 * max(time - last_demo_time - 0.75, 0);
			scroll_index = (int) elapsed;
			frac = bound(0, elapsed - scroll_index, 1);
			scroll_index = scroll_index % last_demo_length;
		}

		if (last_demo_length <= 38 + 7) {
			Q_strncpyz(demoname_scroll, demolist[demoindex]->name, sizeof(demoname_scroll));
			M_PrintWhite (160 - strlen(demoname_scroll) * 4, 40 + 8 * DEMO_MAXLINES, demoname_scroll);
		} else {
			for (i = 0; i < sizeof(demoname_scroll) - 1; i++)
				demoname_scroll[i] = last_demo_name[(scroll_index + i) % last_demo_length];
			demoname_scroll[sizeof(demoname_scroll) - 1] = 0;
			M_PrintWhite (12 -  (int) (8 * frac), 40 + 8 * DEMO_MAXLINES, demoname_scroll);
		}
	} else {
		last_demo_time = 0;
	}
}

void M_Demos_Key (int key) {
	char *p;
	demo_sort_t sort_target;

	switch (key) {
	case K_BACKSPACE:
		m_topmenu = m_none;	// intentional fallthrough
	case K_ESCAPE:
		Q_strncpyz(demo_prevdemo, demolist[demo_cursor + demo_base]->name, sizeof(demo_prevdemo));
		M_LeaveMenu (m_main);
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (demo_cursor > 0)
			demo_cursor--;
		else if (demo_base > 0)
			demo_base--;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (demo_cursor + demo_base < demolist_count - 1) {
			if (demo_cursor < DEMO_MAXLINES - 1)
				demo_cursor++;
			else
				demo_base++;
		}
		break;

	case K_HOME:
		S_LocalSound ("misc/menu1.wav");
		demo_cursor = 0;
		demo_base = 0;
		break;

	case K_END:
		S_LocalSound ("misc/menu1.wav");
		if (demolist_count > DEMO_MAXLINES) {
			demo_cursor = DEMO_MAXLINES - 1;
			demo_base = demolist_count - demo_cursor - 1;
		} else {
			demo_base = 0;
			demo_cursor = demolist_count - 1;
		}
		break;

	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		demo_cursor -= DEMO_MAXLINES - 1;
		if (demo_cursor < 0) {
			demo_base += demo_cursor;
			if (demo_base < 0)
				demo_base = 0;
			demo_cursor = 0;
		}
		break;

	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		demo_cursor += DEMO_MAXLINES - 1;
		if (demo_base + demo_cursor >= demolist_count)
			demo_cursor = demolist_count - demo_base - 1;
		if (demo_cursor >= DEMO_MAXLINES) {
			demo_base += demo_cursor - (DEMO_MAXLINES - 1);
			demo_cursor = DEMO_MAXLINES - 1;
			if (demo_base + demo_cursor >= demolist_count)
				demo_base = demolist_count - demo_cursor - 1;
		}
		break;

	case K_ENTER:
		if (!demolist_count || demolist[demo_base + demo_cursor]->type == dt_msg)
			break;

		if (demolist[demo_base + demo_cursor]->type != dt_file) {		
			if (demolist[demo_base + demo_cursor]->type == dt_up) {
				if ((p = strrchr(demo_currentdir, '/')) != NULL) {
					Q_strncpyz(demo_prevdemo, p + 1, sizeof(demo_prevdemo));
					*p = 0;
				}
			} else {	
				strncat(demo_currentdir, "/", sizeof(demo_currentdir) - strlen(demo_currentdir) - 1);
				strncat(demo_currentdir, demolist[demo_base + demo_cursor]->name, sizeof(demo_currentdir) - strlen(demo_currentdir) - 1);
			}
			demo_cursor = 0;
			Demo_ReadDirectory();
		} else {
			key_dest = key_game;
			m_state = m_none;
			if (keydown[K_CTRL])
				Cbuf_AddText (va("timedemo \".%s/%s\"\n", demo_currentdir, demolist[demo_cursor + demo_base]->name));
			else
				Cbuf_AddText (va("playdemo \".%s/%s\"\n", demo_currentdir, demolist[demo_cursor + demo_base]->name));
			Q_strncpyz(demo_prevdemo, demolist[demo_base + demo_cursor]->name, sizeof(demo_prevdemo));
		}
		break;

	case 'n':	
	case 's':	
	case 't':	
		if (!keydown[K_CTRL])
			break;

		sort_target = (key == 'n') ? ds_name : (key == 's') ? ds_size : ds_time;
		if (demo_sorttype == sort_target) {
			demo_reversesort = !demo_reversesort;
		} else {
			demo_sorttype = sort_target;
			demo_reversesort = false;
		}
		Q_strncpyz(demo_prevdemo, demolist[demo_cursor + demo_base]->name, sizeof(demo_prevdemo));
		Demo_SortDemos();
		Demo_PositionCursor();	
		break;

	case K_SPACE:
		Q_strncpyz(demo_prevdemo, demolist[demo_cursor + demo_base]->name, sizeof(demo_prevdemo));
		Demo_ReadDirectory();
		break;
	}
}

//=============================================================================
/* GAME OPTIONS MENU */

#ifndef CLIENTONLY

typedef struct
{
	char	*name;
	char	*description;
} level_t;

level_t		levels[] =
{
	{"start", "Entrance"},	// 0

	{"e1m1", "Slipgate Complex"},				// 1
	{"e1m2", "Castle of the Damned"},
	{"e1m3", "The Necropolis"},
	{"e1m4", "The Grisly Grotto"},
	{"e1m5", "Gloom Keep"},
	{"e1m6", "The Door To Chthon"},
	{"e1m7", "The House of Chthon"},
	{"e1m8", "Ziggurat Vertigo"},

	{"e2m1", "The Installation"},				// 9
	{"e2m2", "Ogre Citadel"},
	{"e2m3", "Crypt of Decay"},
	{"e2m4", "The Ebon Fortress"},
	{"e2m5", "The Wizard's Manse"},
	{"e2m6", "The Dismal Oubliette"},
	{"e2m7", "Underearth"},

	{"e3m1", "Termination Central"},			// 16
	{"e3m2", "The Vaults of Zin"},
	{"e3m3", "The Tomb of Terror"},
	{"e3m4", "Satan's Dark Delight"},
	{"e3m5", "Wind Tunnels"},
	{"e3m6", "Chambers of Torment"},
	{"e3m7", "The Haunted Halls"},

	{"e4m1", "The Sewage System"},				// 23
	{"e4m2", "The Tower of Despair"},
	{"e4m3", "The Elder God Shrine"},
	{"e4m4", "The Palace of Hate"},
	{"e4m5", "Hell's Atrium"},
	{"e4m6", "The Pain Maze"},
	{"e4m7", "Azure Agony"},
	{"e4m8", "The Nameless City"},

	{"end", "Shub-Niggurath's Pit"},			// 31

	{"dm1", "Place of Two Deaths"},				// 32
	{"dm2", "Claustrophobopolis"},
	{"dm3", "The Abandoned Base"},
	{"dm4", "The Bad Place"},
	{"dm5", "The Cistern"},
	{"dm6", "The Dark Zone"}
};

typedef struct
{
	char	*description;
	int		firstLevel;
	int		levels;
} episode_t;

episode_t	episodes[] =
{
	{"Welcome to Quake", 0, 1},
	{"Doomed Dimension", 1, 8},
	{"Realm of Black Magic", 9, 7},
	{"Netherworld", 16, 7},
	{"The Elder World", 23, 8},
	{"Final Level", 31, 1},
	{"Deathmatch Arena", 32, 6}
};

extern cvar_t maxclients, maxspectators;

int	startepisode;
int	startlevel;
int _maxclients, _maxspectators;
int _deathmatch, _teamplay, _skill, _coop;
int _fraglimit, _timelimit;

void M_Menu_GameOptions_f (void)
{
	M_EnterMenu (m_gameoptions);

	// 16 and 8 are not really limits --- just sane values
	// for these variables...
	_maxclients = min(16, (int)maxclients.value);
	if (_maxclients < 2) _maxclients = 8;
	_maxspectators = max(0, min((int)maxspectators.value, 8));

	_deathmatch = max (0, min((int)deathmatch.value, 5));
	_teamplay = max (0, min((int)teamplay.value, 2));
	_skill = max (0, min((int)skill.value, 3));
	_fraglimit = max (0, min((int)fraglimit.value, 100));
	_timelimit = max (0, min((int)timelimit.value, 60));
}

int gameoptions_cursor_table[] = {40, 56, 64, 72, 80, 96, 104, 120, 128};
#define	NUM_GAMEOPTIONS	9
int		gameoptions_cursor;

void M_GameOptions_Draw (void)
{
	qpic_t	*p;

	M_Main_Layout (M_M_MULTI, false);

	p = R_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - GetPicWidth(p)) / 2, 4, p);

	M_DrawTextBox (152, 32, 10, 1);
	M_Print (160, 40, "begin game");

	M_Print (0, 56, "        game type");
	if (!_deathmatch)
		M_Print (160, 56, "cooperative");
	else
		M_Print (160, 56, va("deathmatch %i", _deathmatch));

	M_Print (0, 64, "         teamplay");
	{
		char *msg;

		switch(_teamplay)
		{
			default: msg = "Off"; break;
			case 1: msg = "No Friendly Fire"; break;
			case 2: msg = "Friendly Fire"; break;
		}
		M_Print (160, 64, msg);
	}

	if (_deathmatch == 0)
	{
		M_Print (0, 72, "            skill");
		switch (_skill)
		{
		case 0:  M_Print (160, 72, "Easy"); break;
		case 1:  M_Print (160, 72, "Normal"); break;
		case 2:  M_Print (160, 72, "Hard"); break;
		default: M_Print (160, 72, "Nightmare");
		}
	}
	else
	{
		M_Print (0, 72, "        fraglimit");
		if (_fraglimit == 0)
			M_Print (160, 72, "none");
		else
			M_Print (160, 72, va("%i frags", _fraglimit));

		M_Print (0, 80, "        timelimit");
		if (_timelimit == 0)
			M_Print (160, 80, "none");
		else
			M_Print (160, 80, va("%i minutes", _timelimit));
	}
	M_Print (0, 96, "       maxclients");
	M_Print (160, 96, va("%i", _maxclients) );

	M_Print (0, 104, "       maxspect.");
	M_Print (160, 104, va("%i", _maxspectators) );

	M_Print (0, 120, "         Episode");
    M_Print (160, 120, episodes[startepisode].description);

	M_Print (0, 128, "           Level");
    M_Print (160, 128, levels[episodes[startepisode].firstLevel + startlevel].description);
	M_Print (160, 136, levels[episodes[startepisode].firstLevel + startlevel].name);

	// line cursor
	M_DrawChar (144, gameoptions_cursor_table[gameoptions_cursor], 12+((int)(curtime*4)&1));
}

void M_NetStart_Change (int dir)
{
	int count;
	extern cvar_t	registered;

	switch (gameoptions_cursor)
	{
	case 1:
		_deathmatch += dir;
		if (_deathmatch < 0) _deathmatch = 5;
		else if (_deathmatch > 5) _deathmatch = 0;
		break;

	case 2:
		_teamplay += dir;
		if (_teamplay < 0) _teamplay = 2;
		else if (_teamplay > 2) _teamplay = 0;
		break;

	case 3:
		if (_deathmatch == 0)
		{
			_skill += dir;
			if (_skill < 0) _skill = 3;
			else if (_skill > 3) _skill = 0;
		}
		else
		{
			_fraglimit += dir*10;
			if (_fraglimit < 0) _fraglimit = 100;
			else if (_fraglimit > 100) _fraglimit = 0;
		}
		break;

	case 4:
		_timelimit += dir*5;
		if (_timelimit < 0) _timelimit = 60;
		else if (_timelimit > 60) _timelimit = 0;
		break;

	case 5:
		_maxclients += dir;
		if (_maxclients > 16)
			_maxclients = 2;
		else if (_maxclients < 2)
			_maxclients = 16;
		break;

	case 6:
		_maxspectators += dir;
		if (_maxspectators > 8)
			_maxspectators = 0;
		else if (_maxspectators < 0)
			_maxspectators = 8;
		break;

	case 7:
		startepisode += dir;
		if (registered.value)
			count = 7;
		else
			count = 2;

		if (startepisode < 0)
			startepisode = count - 1;

		if (startepisode >= count)
			startepisode = 0;

		startlevel = 0;
		break;

	case 8:
		startlevel += dir;
		count = episodes[startepisode].levels;

		if (startlevel < 0)
			startlevel = count - 1;

		if (startlevel >= count)
			startlevel = 0;
		break;
	}
}

void M_GameOptions_Key (int key)
{
	switch (key)
	{
	case K_BACKSPACE:
		m_topmenu = m_none;	// intentional fallthrough
	case K_ESCAPE:
		M_LeaveMenu (m_multiplayer);
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor--;
		if (!_deathmatch && gameoptions_cursor == 4)
			gameoptions_cursor--;
		if (gameoptions_cursor < 0)
			gameoptions_cursor = NUM_GAMEOPTIONS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor++;
		if (!_deathmatch && gameoptions_cursor == 4)
			gameoptions_cursor++;
		if (gameoptions_cursor >= NUM_GAMEOPTIONS)
			gameoptions_cursor = 0;
		break;

	case K_HOME:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor = 0;
		break;

	case K_END:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor = NUM_GAMEOPTIONS-1;
		break;

	case K_LEFTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (-1);
		break;

	case K_RIGHTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (1);
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu2.wav");
//		if (gameoptions_cursor == 0)
		{
			key_dest = key_game;

			// Kill the server, unless we continue playing
			// deathmatch on another level
			if (!_deathmatch || !deathmatch.value)
				Cbuf_AddText ("disconnect\n");

			if (_deathmatch == 0)
			{
				_coop = 1;
				_timelimit = 0;
				_fraglimit = 0;
			}
			else
				_coop = 0;

			Cvar_Set (&deathmatch, va("%i", _deathmatch));
			Cvar_Set (&skill, va("%i", _skill));
			Cvar_Set (&coop, va("%i", _coop));
			Cvar_Set (&fraglimit, va("%i", _fraglimit));
			Cvar_Set (&timelimit, va("%i", _timelimit));
			Cvar_Set (&teamplay, va("%i", _teamplay));
			Cvar_Set (&maxclients, va("%i", _maxclients));
			Cvar_Set (&maxspectators, va("%i", _maxspectators));

			Cbuf_AddText ( va ("map %s\n", levels[episodes[startepisode].firstLevel + startlevel].name) );
			return;
		}
		break;
	}
}
#endif	// !CLIENTONLY

//=============================================================================
/* SERVERLIST MENU */

#define MENU_X 50
#define MENU_Y 30
#define STAT_X 50
#define STAT_Y 122

int m_multip_cursor=0;
int m_multip_mins;
int m_multip_maxs;
int m_multip_horiz;
int m_multip_state;

void M_Menu_ServerList_f (void)
{
	M_EnterMenu (m_slist);
	m_multip_mins = 0;
	m_multip_maxs = 10;
	m_multip_horiz = 0;
	m_multip_state = 0;
}

void M_ServerList_Draw (void)
{
	int serv;
	int line = 1;
	qpic_t *p;

	M_Main_Layout (M_M_MULTI, false);

	p = R_CachePic("gfx/p_multi.lmp");
	M_DrawPic((320-p->width)/2,4,p);

	if (!(slist[0].server)) {
		M_DrawTextBox(60,80,23,4);
		M_PrintWhite(110,12*8,"No server list");
		M_PrintWhite(140,13*8,"found.");
		return;
	}
	M_DrawTextBox(STAT_X,STAT_Y,23,4);
	M_DrawTextBox(STAT_X,STAT_Y+38,23,3);
	M_DrawTextBox(MENU_X,MENU_Y,23,(m_multip_maxs - m_multip_mins)+1);
	for (serv = m_multip_mins; serv <= m_multip_maxs; serv++) {
		if (slist[serv].server) {
			M_Print(MENU_X+18,line*8+MENU_Y,
				va("%1.21s",
				strlen(slist[serv].description) <= m_multip_horiz ? "" : slist[serv].description+m_multip_horiz));
			line++;
		}
	}
	M_PrintWhite(STAT_X+18,STAT_Y+16,"IP/Hostname:");
	M_Print(STAT_X+18,STAT_Y+24,slist[m_multip_cursor].server);
	M_DrawChar(MENU_X+8,(m_multip_cursor - m_multip_mins + 1) * 8+MENU_Y, 12+((int)(curtime*4)&1));
}

void M_ServerList_Key (int key)
{
	if (!(slist[0].server) && key != K_ESCAPE && key != K_INS)
		return;

	switch(key)
	{
	case K_BACKSPACE:
		m_topmenu = m_none;	// intentional fallthrough
	case K_ESCAPE:
		M_LeaveMenu (m_multiplayer);
		break;

	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		if (m_multip_cursor > 0)
		{
			if (keydown[K_CTRL])
			{
				SList_Switch(m_multip_cursor,m_multip_cursor-1);
				m_multip_cursor--;
			}
			else
				m_multip_cursor--;
		}
		break;

	case K_DOWNARROW:
		S_LocalSound("misc/menu1.wav");
		if (keydown[K_CTRL])
		{
			if (m_multip_cursor != SList_Len() - 1) {
				SList_Switch(m_multip_cursor,m_multip_cursor+1);
				m_multip_cursor++;
			}
		}
		else if (m_multip_cursor < (MAX_SERVER_LIST-1) && slist[m_multip_cursor+1].server) {
			m_multip_cursor++;
		}
		break;

	case K_HOME:
		S_LocalSound("misc/menu1.wav");
		m_multip_cursor = 0;
		break;

	case K_END:
		S_LocalSound("misc/menu1.wav");
		m_multip_cursor = SList_Len() - 1;
		break;
		
	case K_PGUP:
		S_LocalSound("misc/menu1.wav");
		m_multip_cursor -= (m_multip_maxs - m_multip_mins);
		if (m_multip_cursor < 0)
			m_multip_cursor = 0;
		break;

	case K_PGDN:
		S_LocalSound("misc/menu1.wav");
		m_multip_cursor += (m_multip_maxs - m_multip_mins);
		if (m_multip_cursor >= MAX_SERVER_LIST)
			m_multip_cursor = MAX_SERVER_LIST - 1;
		while (!(slist[m_multip_cursor].server))
			m_multip_cursor--;
		break;

	case K_RIGHTARROW:
		S_LocalSound("misc/menu1.wav");
		if (m_multip_horiz < 256)
			m_multip_horiz++;
		break;

	case K_LEFTARROW:
		S_LocalSound("misc/menu1.wav");
		if (m_multip_horiz > 0 )
			m_multip_horiz--;
		break;

	case K_ENTER:
		if (keydown[K_CTRL])
		{
			M_Menu_SEdit_f();
			break;
		}
		m_state = m_main;
		M_ToggleMenu_f();
		Cbuf_AddText(va("%s %s\n", keydown[K_SHIFT] ? "observe" : "join", slist[m_multip_cursor].server));
		break;

	case 'e':
	case 'E':
		M_Menu_SEdit_f();
		break;

	case K_INS:
		S_LocalSound("misc/menu2.wav");
		if (SList_Len() < (MAX_SERVER_LIST-1)) {
			memmove(&slist[m_multip_cursor+1],
				&slist[m_multip_cursor],
				(SList_Len() - m_multip_cursor)*sizeof(slist[0]));
			SList_Reset_NoFree(m_multip_cursor);
			SList_Set(m_multip_cursor,"127.0.0.1","<BLANK>");
		}
		break;

	case K_DEL:
		S_LocalSound("misc/menu2.wav");
		if (SList_Len() > 0) {
			free(slist[m_multip_cursor].server);
			free(slist[m_multip_cursor].description);
			if (SList_Len()-1 == m_multip_cursor) {
				SList_Reset_NoFree(m_multip_cursor);
				m_multip_cursor = !m_multip_cursor ? 0 : m_multip_cursor-1;
			} else {
				memmove(&slist[m_multip_cursor],
				&slist[m_multip_cursor+1],
				(SList_Len()-m_multip_cursor-1) * sizeof(slist[0]));
				SList_Reset_NoFree(SList_Len()-1);
			}
		}
		break;
	default:
		break;
	}
	if (m_multip_cursor < m_multip_mins) {
		m_multip_maxs -= (m_multip_mins - m_multip_cursor);
		m_multip_mins = m_multip_cursor;
	}
	if (m_multip_cursor > m_multip_maxs) {
		m_multip_mins += (m_multip_cursor - m_multip_maxs);
		m_multip_maxs = m_multip_cursor;
	}
}

#define SERV_X 60
#define SERV_Y 64
#define DESC_X 60
#define DESC_Y 40
#define SERV_L 22
#define DESC_L 22

char serv[256];
char desc[256];
int serv_max;
int serv_min;
int desc_max;
int desc_min;
int sedit_state;

void M_Menu_SEdit_f (void) {
	M_EnterMenu (m_sedit);
	sedit_state = 0;
	strlcpy (serv, slist[m_multip_cursor].server, sizeof(serv));
	strlcpy (desc, slist[m_multip_cursor].description, sizeof(desc));
	serv_max = strlen(serv) > SERV_L ? strlen(serv) : SERV_L;
	serv_min = serv_max - (SERV_L);
	desc_max = strlen(desc) > DESC_L ? strlen(desc) : DESC_L;
	desc_min = desc_max - (DESC_L);
}

void M_SEdit_Draw (void) {
	qpic_t *p;

	M_Main_Layout (M_M_MULTI, false);

	p = R_CachePic("gfx/p_multi.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);

	M_DrawTextBox(SERV_X,SERV_Y,23,1);
	M_DrawTextBox(DESC_X,DESC_Y,23,1);
	M_PrintWhite(SERV_X,SERV_Y-4,"Hostname/IP:");
	M_PrintWhite(DESC_X,DESC_Y-4,"Description:");
	M_Print(SERV_X+9,SERV_Y+8,va("%1.22s",serv+serv_min));
	M_Print(DESC_X+9,DESC_Y+8,va("%1.22s",desc+desc_min));
	if (sedit_state == 0)
		M_DrawChar(SERV_X+9+8*(strlen(serv)-serv_min), SERV_Y+8,10+((int)(curtime*4)&1));
	if (sedit_state == 1)
		M_DrawChar(DESC_X+9+8*(strlen(desc)-desc_min),	DESC_Y+8,10+((int)(curtime*4)&1));
}

void M_SEdit_Key (int key) {
	int	l;
	switch (key) {
		case K_ESCAPE:
			M_Menu_ServerList_f ();
			break;
		case K_ENTER:
			SList_Set(m_multip_cursor,serv,desc);
			M_Menu_ServerList_f ();
			break;
		case K_UPARROW:
			S_LocalSound("misc/menu1.wav");
			sedit_state = sedit_state == 0 ? 1 : 0;
			break;
		case K_DOWNARROW:
			S_LocalSound("misc/menu1.wav");
			sedit_state = sedit_state == 1 ? 0 : 1;
			break;
		case K_BACKSPACE:
			switch (sedit_state) {
				case 0:
					if ((l = strlen(serv)))
						serv[--l] = 0;
					if (strlen(serv)-6 < serv_min && serv_min) {
						serv_min--;
						serv_max--;
					}
					break;
				case 1:
					if ((l = strlen(desc)))
						desc[--l] = 0;
					if (strlen(desc)-6 < desc_min && desc_min) {
						desc_min--;
						desc_max--;
					}
					break;
				default:
					break;
			}
			break;
		default:
			if (key < 32 || key > 127)
				break;
			switch(sedit_state) {
				case 0:
					l = strlen(serv);
					if (l < 254) {
						serv[l+1] = 0;
						serv[l] = key;
					}
					if (strlen(serv) > serv_max) {
						serv_min++;
						serv_max++;
					}
					break;
				case 1:
					l = strlen(desc);
					if (l < 254) {
						desc[l+1] = 0;
						desc[l] = key;
					}
					if (strlen(desc) > desc_max) {
						desc_min++;
						desc_max++;
					}
					break;
			}
			break;
	}
}

//=============================================================================
/* SETUP MENU */

int		setup_cursor = 0;
int		setup_cursor_table[] = {40, 56, 80, 104, 140};

char	setup_name[16];
char	setup_team[16];
int		setup_oldtop;
int		setup_oldbottom;
int		setup_top;
int		setup_bottom;

extern cvar_t	name, team;
extern cvar_t	topcolor, bottomcolor;

#define	NUM_SETUP_CMDS	5

void M_Menu_Setup_f (void)
{
	M_EnterMenu (m_setup);
	strlcpy (setup_name, name.string, sizeof(setup_name));
	strlcpy (setup_team, team.string, sizeof(setup_team));
	setup_top = setup_oldtop = (int)topcolor.value;
	setup_bottom = setup_oldbottom = (int)bottomcolor.value;
}

void M_Setup_Draw (void)
{
	qpic_t	*p;

	M_Main_Layout (M_M_MULTI, false);

	p = R_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - GetPicWidth(p)) / 2, 4, p);

	M_Print (64, 40, "Your name");
	M_DrawTextBox (160, 32, 16, 1);
	M_PrintWhite (168, 40, setup_name);

	M_Print (64, 56, "Your team");
	M_DrawTextBox (160, 48, 16, 1);
	M_PrintWhite (168, 56, setup_team);

	M_Print (64, 80, "Shirt color");
	M_Print (64, 104, "Pants color");

	M_DrawTextBox (64, 140-8, 14, 1);
	M_Print (72, 140, "Accept Changes");

	p = R_CachePic ("gfx/bigbox.lmp");
	M_DrawPic (160, 64, p);
	p = R_CachePic ("gfx/menuplyr.lmp");
	M_DrawTransPicTranslate (172, 72, p, setup_top, setup_bottom);

	M_DrawChar (56, setup_cursor_table [setup_cursor], 12+((int)(curtime*4)&1));

	if (setup_cursor == 0)
		M_DrawChar (168 + 8*strlen(setup_name), setup_cursor_table [setup_cursor], 10+((int)(curtime*4)&1));

	if (setup_cursor == 1)
		M_DrawChar (168 + 8*strlen(setup_team), setup_cursor_table [setup_cursor], 10+((int)(curtime*4)&1));
}

void M_Setup_Key (int k)
{
	int		l;

	switch (k)
	{
	case K_ESCAPE:
		M_LeaveMenu (m_multiplayer);
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor--;
		if (setup_cursor < 0)
			setup_cursor = NUM_SETUP_CMDS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor++;
		if (setup_cursor >= NUM_SETUP_CMDS)
			setup_cursor = 0;
		break;

	case K_HOME:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor = 0;
		break;

	case K_END:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor = NUM_SETUP_CMDS-1;
		break;

	case K_LEFTARROW:
		if (setup_cursor < 2)
			return;
		S_LocalSound ("misc/menu3.wav");
		if (setup_cursor == 2)
			setup_top = setup_top - 1;
		if (setup_cursor == 3)
			setup_bottom = setup_bottom - 1;
		break;
	case K_RIGHTARROW:
		if (setup_cursor < 2)
			return;
		S_LocalSound ("misc/menu3.wav");
		if (setup_cursor == 2)
			setup_top = setup_top + 1;
		if (setup_cursor == 3)
			setup_bottom = setup_bottom + 1;
		break;

	case K_ENTER:
		Cvar_Set (&name, setup_name);
		Cvar_Set (&team, setup_team);
		Cvar_Set (&topcolor, va("%i", setup_top));
		Cvar_Set (&bottomcolor, va("%i", setup_bottom));
		m_entersound = true;
		M_Menu_MultiPlayer_f ();
		break;

	case K_BACKSPACE:
		if (setup_cursor == 0)
		{
			if (strlen(setup_name))
				setup_name[strlen(setup_name)-1] = 0;
		} else if (setup_cursor == 1)
		{
			if (strlen(setup_team))
				setup_team[strlen(setup_team)-1] = 0;
		} else {
			m_topmenu = m_none;
			M_LeaveMenu (m_multiplayer);
		}
		break;

	default:
		if (k < 32 || k > 127)
			break;
		if (setup_cursor == 0)
		{
			l = strlen(setup_name);
			if (l < 15)
			{
				setup_name[l+1] = 0;
				setup_name[l] = k;
			}
		}
		if (setup_cursor == 1)
		{
			l = strlen(setup_team);
			if (l < 15)
			{
				setup_team[l+1] = 0;
				setup_team[l] = k;
			}
		}
	}

	if (setup_top > 13)
		setup_top = 0;
	if (setup_top < 0)
		setup_top = 13;
	if (setup_bottom > 13)
		setup_bottom = 0;
	if (setup_bottom < 0)
		setup_bottom = 13;
}

//=============================================================================
/* MODS MENU */

// same limit of mod dirs as in fs.c
#define MODLIST_MAXDIRS 16
static int modlist_enabled [MODLIST_MAXDIRS];	// array of indexs to modlist
static int modlist_numenabled;					// number of enabled (or in process to be..) mods

typedef struct modlist_entry_s
{
	qbool loaded;		// used to determine whether this entry is loaded and running
	int enabled;		// index to array of modlist_enabled

	// name of the modification, this is (will...be) displayed on the menu entry
	char name[128];
	// directory where we will find it
	char dir[MAX_QPATH];
} modlist_entry_t;

static int modlist_cursor;
//static int modlist_viewcount;

#define MODLIST_TOTALSIZE		256
static int modlist_count = 0;
static modlist_entry_t modlist[MODLIST_TOTALSIZE];

void ModList_RebuildList(void)
{
	int i,j;
	stringlist_t list;

	stringlistinit(&list);
	if (fs_basedir[0])
		listdirectory(&list, fs_basedir);
	else
		listdirectory(&list, "./");
	stringlistsort(&list);
	modlist_count = 0;
	modlist_numenabled = fs_numgamedirs;
	for (i = 0;i < list.numstrings;i++)
	{
		if (modlist_count >= MODLIST_TOTALSIZE)
			break;

		// check all dirs to see if they "appear" to be mods
		// reject any dirs that are part of the base game
		// (such as "id1" and "hipnotic" when in GAME_HIPNOTIC mode)
		if (gamedirname1 && !strcasecmp(gamedirname1, list.strings[i]))
			continue;
		if (gamedirname2 && !strcasecmp(gamedirname2, list.strings[i]))
			continue;
		if (FS_CheckNastyPath (list.strings[i], true))
			continue;
		if (!FS_CheckGameDir(list.strings[i]))
			continue;

		strlcpy (modlist[modlist_count].dir, list.strings[i], sizeof(modlist[modlist_count].dir));
		// check currently loaded mods
		modlist[modlist_count].loaded = false;
		if (fs_numgamedirs)
			for (j = 0; j < fs_numgamedirs; j++)
				if (!strcasecmp(fs_gamedirs[j], modlist[modlist_count].dir))
				{
					modlist[modlist_count].loaded = true;
					modlist[modlist_count].enabled = j;
					modlist_enabled[j] = modlist_count;
					break;
				}
		modlist_count ++;
	}
	stringlistfreecontents(&list);
}

void ModList_Enable (void)
{
	int i;
	int numgamedirs;
	char gamedirs[MODLIST_MAXDIRS][MAX_QPATH];

	// copy our mod list into an array for FS_ChangeGameDirs
	numgamedirs = modlist_numenabled;
	for (i = 0; i < modlist_numenabled; i++)
		strlcpy (gamedirs[i], modlist[modlist_enabled[i]].dir, sizeof (gamedirs[i]));

	// this code snippet is from FS_ChangeGameDirs
	if (fs_numgamedirs == numgamedirs)
	{
		for (i = 0;i < numgamedirs;i++)
			if (strcasecmp(fs_gamedirs[i], gamedirs[i]))
				break;
		if (i == numgamedirs)
			return; // already using this set of gamedirs, do nothing
	}

	// this part is basically the same as the FS_GameDir_f function
	if ((cls.state == ca_connected && !cls.demoplayback) || com_serveractive)
	{
		// actually, changing during game would work fine, but would be stupid
		Com_Printf("Can not change gamedir while client is connected or server is running!\n");
		return;
	}

	FS_ChangeGameDirs (modlist_numenabled, gamedirs, true, true);
}

static void M_Menu_ModList_AdjustSliders (void)
{
	int i;

	S_LocalSound ("sound/misc/menu3.wav");

	// stop adding mods, we reach the limit
	if (!modlist[modlist_cursor].loaded && (modlist_numenabled == MODLIST_MAXDIRS))
		return;

	modlist[modlist_cursor].loaded = !modlist[modlist_cursor].loaded;
	if (modlist[modlist_cursor].loaded)
	{
		modlist[modlist_cursor].enabled = modlist_numenabled;

		// push the value on the enabled list
		modlist_enabled[modlist_numenabled++] = modlist_cursor;
	}
	else
	{
		// eliminate the value from the enabled list
		for (i = modlist[modlist_cursor].enabled; i < modlist_numenabled; i++)
		{
			modlist_enabled[i] = modlist_enabled[i + 1];
			modlist[modlist_enabled[i]].enabled--;
		}
		modlist_numenabled--;
	}
}

void M_Menu_Mods_f (void)
{
	M_EnterMenu (m_mods);

	modlist_cursor = 0;
	ModList_RebuildList();
}

void M_Mods_Draw (void)
{
	qpic_t *p;
	int n, y, visible, start, end;
	char *s_available = "Available Mods";
	char *s_enabled = "Enabled Mods";

	M_Main_Layout (M_M_MODS, false);

	p = R_CachePic ("gfx/p_option.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);

	M_PrintWhite(0 + 32, 32, s_available);
	M_PrintWhite(224, 32, s_enabled);

	// draw a list with all enabled mods
	for (y = 0; y < modlist_numenabled; y++)
		M_Print(224, 48 + y * 8, modlist[modlist_enabled[y]].dir);

	// scroll the list as the cursor moves
	y = 48;
	visible = (int)((640 - 16 - y) / 8 / 2);
	start = bound(0, modlist_cursor - (visible >> 1), modlist_count - visible);
	end = min(start + visible, modlist_count);
	if (end > start)
	{
		for (n = start;n < end;n++)
		{
			if (n == modlist_cursor)
				R_DrawChar (16, y, 12+((int)(curtime*4)&1));
			M_ItemPrint(80, y, modlist[n].dir, true);
			M_DrawCheckbox(48, y, modlist[n].loaded);
			y +=8;
		}
	}
	else
	{
		M_PrintWhite(80, y, "No Mods found");
	}
}

void M_Mods_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		ModList_Enable ();
		M_Menu_Main_f();
		break;

	case K_SPACE:
		S_LocalSound ("sound/misc/menu2.wav");
		ModList_RebuildList();
		break;

	case K_UPARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		modlist_cursor--;
		if (modlist_cursor < 0)
			modlist_cursor = modlist_count - 1;
		break;

	case K_LEFTARROW:
		M_Menu_ModList_AdjustSliders ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		modlist_cursor++;
		if (modlist_cursor >= modlist_count)
			modlist_cursor = 0;
		break;

	case K_RIGHTARROW:
		M_Menu_ModList_AdjustSliders ();
		break;

	case K_ENTER:
		S_LocalSound ("sound/misc/menu2.wav");
		ModList_Enable ();
		break;

	default:
		break;
	}
}


//=============================================================================
/* Menu Subsystem */

void M_Init (void)
{
	vid_mode_t res[1024];
	size_t res_count, i;

	res_count = VID_ListModes(res, sizeof(res) / sizeof(*res));
	res_count = VID_SortModes(res, res_count, false, false, true);
	if(res_count)
	{
		video_resolutions_count = res_count;
		video_resolutions = (video_resolution_t *) malloc(sizeof(*video_resolutions) * (video_resolutions_count + 1));
		memset(&video_resolutions[video_resolutions_count], 0, sizeof(video_resolutions[video_resolutions_count]));
		for(i = 0; i < res_count; ++i)
		{
			int n, d, t;
			video_resolutions[i].type = "Detected mode"; // FIXME make this more dynamic
			video_resolutions[i].width = res[i].width;
			video_resolutions[i].height = res[i].height;
			video_resolutions[i].pixelheight = res[i].pixelheight_num / (double) res[i].pixelheight_denom;
			n = res[i].pixelheight_denom * video_resolutions[i].width;
			d = res[i].pixelheight_num * video_resolutions[i].height;
			while(d)
			{
				t = n;
				n = d;
				d = t % d;
			}
			d = (res[i].pixelheight_num * video_resolutions[i].height) / n;
			n = (res[i].pixelheight_denom * video_resolutions[i].width) / n;
			switch(n * 0x10000 | d)
			{
				case 0x00040003:
					video_resolutions[i].conwidth = 640;
					video_resolutions[i].conheight = 480;
					video_resolutions[i].type = "Standard 4x3";
					break;
				case 0x00050004:
					video_resolutions[i].conwidth = 640;
					video_resolutions[i].conheight = 512;
					if(res[i].pixelheight_denom == res[i].pixelheight_num)
						video_resolutions[i].type = "Square Pixel (LCD) 5x4";
					else
						video_resolutions[i].type = "Short Pixel (CRT) 5x4";
					break;
				case 0x00080005:
					video_resolutions[i].conwidth = 640;
					video_resolutions[i].conheight = 400;
					if(res[i].pixelheight_denom == res[i].pixelheight_num)
						video_resolutions[i].type = "Widescreen 8x5";
					else
						video_resolutions[i].type = "Tall Pixel (CRT) 8x5";

					break;
				case 0x00050003:
					video_resolutions[i].conwidth = 640;
					video_resolutions[i].conheight = 384;
					video_resolutions[i].type = "Widescreen 5x3";
					break;
				case 0x000D0009:
					video_resolutions[i].conwidth = 640;
					video_resolutions[i].conheight = 400;
					video_resolutions[i].type = "Widescreen 14x9";
					break;
				case 0x00100009:
					video_resolutions[i].conwidth = 640;
					video_resolutions[i].conheight = 480;
					video_resolutions[i].type = "Widescreen 16x9";
					break;
				case 0x00030002:
					video_resolutions[i].conwidth = 720;
					video_resolutions[i].conheight = 480;
					video_resolutions[i].type = "NTSC 3x2";
					break;
				case 0x000D000B:
					video_resolutions[i].conwidth = 720;
					video_resolutions[i].conheight = 566;
					video_resolutions[i].type = "PAL 14x11";
					break;
				case 0x00080007:
					if(video_resolutions[i].width >= 512)
					{
						video_resolutions[i].conwidth = 512;
						video_resolutions[i].conheight = 448;
						video_resolutions[i].type = "SNES 8x7";
					}
					else
					{
						video_resolutions[i].conwidth = 256;
						video_resolutions[i].conheight = 224;
						video_resolutions[i].type = "NES 8x7";
					}
					break;
				default:
					video_resolutions[i].conwidth = 640;
					video_resolutions[i].conheight = 640 * d / n;
					video_resolutions[i].type = "Detected mode";
					break;
			}
			if(video_resolutions[i].conwidth > video_resolutions[i].width || video_resolutions[i].conheight > video_resolutions[i].height)
			{
				double f1, f2;
				f1 = video_resolutions[i].conwidth > video_resolutions[i].width;
				f2 = video_resolutions[i].conheight > video_resolutions[i].height;
				if(f1 > f2)
				{
					video_resolutions[i].conwidth = video_resolutions[i].width;
					video_resolutions[i].conheight = video_resolutions[i].conheight / f1;
				}
				else
				{
					video_resolutions[i].conwidth = video_resolutions[i].conwidth / f2;
					video_resolutions[i].conheight = video_resolutions[i].height;
				}
			}
		}
	}
	else
	{
		video_resolutions = video_resolutions_hardcoded;
		video_resolutions_count = sizeof(video_resolutions_hardcoded) / sizeof(*video_resolutions_hardcoded) - 1;
	}

	Cvar_Register (&scr_menuscale);

	Cmd_AddCommand ("togglemenu", M_ToggleMenu_f);

	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
#ifndef CLIENTONLY
	Cmd_AddCommand ("menu_singleplayer", M_Menu_SinglePlayer_f);
	Cmd_AddCommand ("menu_load", M_Menu_Load_f);
	Cmd_AddCommand ("menu_save", M_Menu_Save_f);
#endif
	Cmd_AddCommand ("menu_multiplayer", M_Menu_MultiPlayer_f);
	Cmd_AddCommand ("menu_slist", M_Menu_ServerList_f);
	Cmd_AddCommand ("menu_setup", M_Menu_Setup_f);
	Cmd_AddCommand ("menu_demos", M_Menu_Demos_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("menu_fps", M_Menu_Fps_f);
	Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	Cmd_AddCommand ("menu_mods", M_Menu_Mods_f);
	Cmd_AddCommand ("help", M_Menu_Help_f);
	Cmd_AddCommand ("menu_help", M_Menu_Help_f);
	Cmd_AddCommand ("credits", M_Menu_Credits_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);
}


void M_Draw (void)
{
	if (m_state == m_none || key_dest != key_menu)
		return;

	switch (m_state)
	{
	case m_none:
		break;

	case m_main:
		M_Main_Draw ();
		break;

	case m_singleplayer:
		M_SinglePlayer_Draw ();
		break;

	case m_load:
		M_Load_Draw ();
		break;

	case m_save:
		M_Save_Draw ();
		break;

	case m_multiplayer:
		M_MultiPlayer_Draw ();
		break;

	case m_setup:
		M_Setup_Draw ();
		break;

	case m_options:
		M_Options_Draw ();
		break;

	case m_keys:
		M_Keys_Draw ();
		break;

	case m_fps:
		M_Fps_Draw ();
		break;

	case m_video:
		M_Video_Draw ();
		break;

	case m_help:
		M_Help_Draw ();
		break;

	case m_credits:
		M_Credits_Draw ();
		break;

	case m_quit:
		M_Quit_Draw ();
		break;

#ifndef CLIENTONLY
	case m_gameoptions:
		M_GameOptions_Draw ();
		break;
#endif

	case m_slist:	 
		M_ServerList_Draw ();	 
		break;	 
 	 
	case m_sedit:	 
		M_SEdit_Draw ();	 
		break;

	case m_demos:
		M_Demos_Draw ();
		break;

	case m_mods:
		M_Mods_Draw ();
		break;
	}
	
	if (m_entersound)
	{
		S_LocalSound ("misc/menu2.wav");
		m_entersound = false;
	}
}


void M_Keydown (int key)
{
	switch (m_state)
	{
	case m_none:
		return;

	case m_main:
		M_Main_Key (key);
		return;

	case m_singleplayer:
		M_SinglePlayer_Key (key);
		return;

	case m_load:
		M_Load_Key (key);
		return;

	case m_save:
		M_Save_Key (key);
		return;

	case m_multiplayer:
		M_MultiPlayer_Key (key);
		return;

	case m_setup:
		M_Setup_Key (key);
		return;

	case m_options:
		M_Options_Key (key);
		return;

	case m_keys:
		M_Keys_Key (key);
		return;

	case m_fps:
		M_Fps_Key (key);
		return;

	case m_video:
		M_Video_Key (key);
		return;

	case m_help:
		M_Help_Key (key);
		return;

	case m_credits:
		M_Credits_Key (key);
		return;

	case m_quit:
		M_Quit_Key (key);
		return;

#ifndef CLIENTONLY
	case m_gameoptions:
		M_GameOptions_Key (key);
		return;
#endif

	case m_slist:	 
		M_ServerList_Key (key);	 
		return;	 
 	 
	case m_sedit:	 
		M_SEdit_Key (key);	 
		break;

	case m_demos:
		M_Demos_Key (key);
		break;

	case m_mods:
		M_Mods_Key (key);
		break;
	}
}


