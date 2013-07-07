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
// sbar.c -- status bar code

#include "quakedef.h"
#include "keys.h"
#include "cl_sbar.h"

qbool		sb_drawinventory;
qbool		sb_drawmain;

#define STAT_MINUS 10					// num frame for '-' stats digit
static qpic_t	*sb_nums[2][11];
static qpic_t	*sb_colon, *sb_slash;
static qpic_t	*sb_scorebar;

static qpic_t	*sb_weapons[7][8];		// 0 is active, 1 is owned, 2-5 are flashes
static qpic_t	*sb_ammo[4];
static qpic_t	*sb_sigil[4];
static qpic_t	*sb_armor[3];
static qpic_t	*sb_items[32];

static qpic_t	*sb_faces[5][2];		// 0 is dead, 1-4 are alive
										// 0 is static, 1 is temporary animation
static qpic_t	*sb_face_invis;
static qpic_t	*sb_face_quad;
static qpic_t	*sb_face_invuln;
static qpic_t	*sb_face_invis_invuln;

static qpic_t	*sb_disc;				// invulnerability

// hipnotic defines
#define HIT_PROXIMITY_GUN_BIT 16
#define HIT_MJOLNIR_BIT       7
#define HIT_LASER_CANNON_BIT  23
#define HIT_PROXIMITY_GUN   (1<<HIT_PROXIMITY_GUN_BIT)
#define HIT_MJOLNIR         (1<<HIT_MJOLNIR_BIT)
#define HIT_LASER_CANNON    (1<<HIT_LASER_CANNON_BIT)
#define HIT_WETSUIT         (1<<25)
#define HIT_EMPATHY_SHIELDS (1<<26)

static qpic_t	*hsb_weapons[7][5];   // 0 is active, 1 is owned, 2-5 are flashes
static qpic_t	*hsb_items[2];
static int	hipweapons[4] = {HIT_LASER_CANNON_BIT,HIT_MJOLNIR_BIT,4,HIT_PROXIMITY_GUN_BIT};

static qbool	sb_showscores;
static qbool	sb_showteamscores;

static void Sbar_DeathmatchOverlay (int start);
static void Sbar_TeamOverlay (int start);

cvar_t	scr_sbarscale = {"scr_sbarscale", "2", CVAR_ARCHIVE};

/*
===============
Sbar_ShowTeamScores

Tab key down
===============
*/
static void Sbar_ShowTeamScores (void)
{
	if (sb_showteamscores)
		return;

	sb_showteamscores = true;
}

/*
===============
Sbar_DontShowTeamScores

Tab key up
===============
*/
static void Sbar_DontShowTeamScores (void)
{
	sb_showteamscores = false;
}

/*
===============
Sbar_ShowScores

Tab key down
===============
*/
static void Sbar_ShowScores (void)
{
	if (sb_showscores)
		return;

	sb_showscores = true;
}

/*
===============
Sbar_DontShowScores

Tab key up
===============
*/
static void Sbar_DontShowScores (void)
{
	sb_showscores = false;
}

static void Sbar_RegisterHipnoticPics (void)
{
	int i;

	hsb_weapons[0][0] = R_CacheWadPic ("inv_laser");
	hsb_weapons[0][1] = R_CacheWadPic ("inv_mjolnir");
	hsb_weapons[0][2] = R_CacheWadPic ("inv_gren_prox");
	hsb_weapons[0][3] = R_CacheWadPic ("inv_prox_gren");
	hsb_weapons[0][4] = R_CacheWadPic ("inv_prox");

	hsb_weapons[1][0] = R_CacheWadPic ("inv2_laser");
	hsb_weapons[1][1] = R_CacheWadPic ("inv2_mjolnir");
	hsb_weapons[1][2] = R_CacheWadPic ("inv2_gren_prox");
	hsb_weapons[1][3] = R_CacheWadPic ("inv2_prox_gren");
	hsb_weapons[1][4] = R_CacheWadPic ("inv2_prox");

	for (i = 0; i < 5; i++)
	{
		hsb_weapons[2+i][0] = R_CacheWadPic (va("inva%i_laser",i+1));
		hsb_weapons[2+i][1] = R_CacheWadPic (va("inva%i_mjolnir",i+1));
		hsb_weapons[2+i][2] = R_CacheWadPic (va("inva%i_gren_prox",i+1));
		hsb_weapons[2+i][3] = R_CacheWadPic (va("inva%i_prox_gren",i+1));
		hsb_weapons[2+i][4] = R_CacheWadPic (va("inva%i_prox",i+1));
	}

	hsb_items[0] = R_CacheWadPic ("sb_wsuit");
	hsb_items[1] = R_CacheWadPic ("sb_eshld");
}

static void sbar_start (void)
{
	int		i;

	for (i = 0; i < 10; i++)
	{
		sb_nums[0][i] = R_CacheWadPic (va("num_%i",i));
		sb_nums[1][i] = R_CacheWadPic (va("anum_%i",i));
	}

	sb_nums[0][10] = R_CacheWadPic ("num_minus");
	sb_nums[1][10] = R_CacheWadPic ("anum_minus");

	sb_colon = R_CacheWadPic ("num_colon");
	sb_slash = R_CacheWadPic ("num_slash");

	sb_weapons[0][0] = R_CacheWadPic ("inv_shotgun");
	sb_weapons[0][1] = R_CacheWadPic ("inv_sshotgun");
	sb_weapons[0][2] = R_CacheWadPic ("inv_nailgun");
	sb_weapons[0][3] = R_CacheWadPic ("inv_snailgun");
	sb_weapons[0][4] = R_CacheWadPic ("inv_rlaunch");
	sb_weapons[0][5] = R_CacheWadPic ("inv_srlaunch");
	sb_weapons[0][6] = R_CacheWadPic ("inv_lightng");
	
	sb_weapons[1][0] = R_CacheWadPic ("inv2_shotgun");
	sb_weapons[1][1] = R_CacheWadPic ("inv2_sshotgun");
	sb_weapons[1][2] = R_CacheWadPic ("inv2_nailgun");
	sb_weapons[1][3] = R_CacheWadPic ("inv2_snailgun");
	sb_weapons[1][4] = R_CacheWadPic ("inv2_rlaunch");
	sb_weapons[1][5] = R_CacheWadPic ("inv2_srlaunch");
	sb_weapons[1][6] = R_CacheWadPic ("inv2_lightng");
	
	for (i = 0; i < 5; i++)
	{
		sb_weapons[2+i][0] = R_CacheWadPic (va("inva%i_shotgun",i+1));
		sb_weapons[2+i][1] = R_CacheWadPic (va("inva%i_sshotgun",i+1));
		sb_weapons[2+i][2] = R_CacheWadPic (va("inva%i_nailgun",i+1));
		sb_weapons[2+i][3] = R_CacheWadPic (va("inva%i_snailgun",i+1));
		sb_weapons[2+i][4] = R_CacheWadPic (va("inva%i_rlaunch",i+1));
		sb_weapons[2+i][5] = R_CacheWadPic (va("inva%i_srlaunch",i+1));
		sb_weapons[2+i][6] = R_CacheWadPic (va("inva%i_lightng",i+1));
	}

	sb_ammo[0] = R_CacheWadPic ("sb_shells");
	sb_ammo[1] = R_CacheWadPic ("sb_nails");
	sb_ammo[2] = R_CacheWadPic ("sb_rocket");
	sb_ammo[3] = R_CacheWadPic ("sb_cells");

	sb_armor[0] = R_CacheWadPic ("sb_armor1");
	sb_armor[1] = R_CacheWadPic ("sb_armor2");
	sb_armor[2] = R_CacheWadPic ("sb_armor3");

	sb_items[0] = R_CacheWadPic ("sb_key1");
	sb_items[1] = R_CacheWadPic ("sb_key2");
	sb_items[2] = R_CacheWadPic ("sb_invis");
	sb_items[3] = R_CacheWadPic ("sb_invuln");
	sb_items[4] = R_CacheWadPic ("sb_suit");
	sb_items[5] = R_CacheWadPic ("sb_quad");

	sb_sigil[0] = R_CacheWadPic ("sb_sigil1");
	sb_sigil[1] = R_CacheWadPic ("sb_sigil2");
	sb_sigil[2] = R_CacheWadPic ("sb_sigil3");
	sb_sigil[3] = R_CacheWadPic ("sb_sigil4");

	sb_faces[4][0] = R_CacheWadPic ("face1");
	sb_faces[4][1] = R_CacheWadPic ("face_p1");
	sb_faces[3][0] = R_CacheWadPic ("face2");
	sb_faces[3][1] = R_CacheWadPic ("face_p2");
	sb_faces[2][0] = R_CacheWadPic ("face3");
	sb_faces[2][1] = R_CacheWadPic ("face_p3");
	sb_faces[1][0] = R_CacheWadPic ("face4");
	sb_faces[1][1] = R_CacheWadPic ("face_p4");
	sb_faces[0][0] = R_CacheWadPic ("face5");
	sb_faces[0][1] = R_CacheWadPic ("face_p5");

	sb_face_invis = R_CacheWadPic ("face_invis");
	sb_face_invuln = R_CacheWadPic ("face_invul2");
	sb_face_invis_invuln = R_CacheWadPic ("face_inv2");
	sb_face_quad = R_CacheWadPic ("face_quad");

	sb_disc = R_CacheWadPic ("disc");

	sb_scorebar = R_CacheWadPic ("scorebar");

	if (gamemode == GAME_HIPNOTIC)
		Sbar_RegisterHipnoticPics ();
}

static void sbar_shutdown(void)
{
}

static void sbar_newmap(void)
{
}

/*
===============
Sbar_Init
===============
*/
void Sbar_Init (void)
{
	Cvar_Register (&scr_sbarscale);

	Cmd_AddCommand ("+showscores", Sbar_ShowScores);
	Cmd_AddCommand ("-showscores", Sbar_DontShowScores);
		
	Cmd_AddCommand ("+showteamscores", Sbar_ShowTeamScores);
	Cmd_AddCommand ("-showteamscores", Sbar_DontShowTeamScores);

	R_RegisterModule("sbar", sbar_start, sbar_shutdown, sbar_newmap);
}

//=============================================================================
// drawing routines are relative to the status bar location

/*
=============
Sbar_DrawPic
=============
*/
static void Sbar_DrawPic (int x, int y, qpic_t *pic)
{
	R_DrawPic (x, y + 24, pic);
}

/*
================
Sbar_DrawChar

Draws one solid graphics character
================
*/
static void Sbar_DrawChar (int x, int y, int num)
{
	R_DrawChar (x, y + 24, num);
}

/*
================
Sbar_DrawString
================
*/
static void Sbar_DrawString (int x, int y, char *str)
{
	R_DrawString (x, y + 24, str);
}

/*
=============
Sbar_itoa
=============
*/
static int Sbar_itoa (int num, char *buf)
{
	char	*str;
	int		pow10;
	int		dig;
	
	str = buf;
	
	if (num < 0)
	{
		*str++ = '-';
		num = -num;
	}
	
	for (pow10 = 10 ; num >= pow10 ; pow10 *= 10)
	;
	
	do
	{
		pow10 /= 10;
		dig = num/pow10;
		*str++ = '0'+dig;
		num -= dig*pow10;
	} while (pow10 != 1);
	
	*str = 0;
	
	return str-buf;
}

/*
=============
Sbar_DrawNum
=============
*/
static void Sbar_DrawNum (int x, int y, int num, int digits, int color)
{
	char			str[12];
	char			*ptr;
	int				l, frame;

	l = Sbar_itoa (num, str);
	ptr = str;
	if (l > digits)
		ptr += (l-digits);
	if (l < digits)
		x += (digits-l)*24;

	while (*ptr)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		Sbar_DrawPic (x,y,sb_nums[color][frame]);
		x += 24;
		ptr++;
	}
}

/*
================
Sbar_DrawTitle
================
*/
static void Sbar_DrawTitle (int y, char *str)
{
	int thiswidth = strlen(str) * BIGMENU_TITLE_SCALE * BIGLETTER_WIDTH;

//	M_DrawTextBox ((320 - thiswidth) / 2, y - 8, thiswidth/4, 4);

	// title text
	Draw_BigString((320 - thiswidth) / 2, y, str, BIGMENU_TITLE_SCALE, BIGMENU_LETTER_SPACING);
}

//=============================================================================

int		fragsort[MAX_CLIENTS];
int		scoreboardlines;
typedef struct {
	char team[MAX_INFO_KEY];
	int frags;
	int players;
	int plow, phigh, ptotal;
} team_t;
team_t teams[MAX_CLIENTS];
int teamsort[MAX_CLIENTS];
int scoreboardteams;

/*
===============
Sbar_SortFrags
===============
*/
static void Sbar_SortFrags (qbool includespec)
{
	int		i, j, k;
		
// sort by frags
	scoreboardlines = 0;
	for (i=0 ; i<MAX_CLIENTS ; i++) {
		if (cl.players[i].name[0] && (!cl.players[i].spectator || includespec)) {
			fragsort[scoreboardlines] = i;
			scoreboardlines++;
		}
	}
		
	for (i=0 ; i<scoreboardlines ; i++) {
		for (j=0 ; j<scoreboardlines-1-i ; j++) {
			if (cl.players[fragsort[j]].spectator != cl.players[fragsort[j+1]].spectator ?
			cl.players[fragsort[j]].spectator > cl.players[fragsort[j+1]].spectator :
			cl.players[fragsort[j]].frags < cl.players[fragsort[j+1]].frags)
			{
				k = fragsort[j];
				fragsort[j] = fragsort[j+1];
				fragsort[j+1] = k;
			}
		}
	}
}

static void Sbar_SortTeams (void)
{
	int				i, j, k;
	player_info_t	*s;
	char t[16+1];

	scoreboardteams = 0;

	if (!cl.teamplay)
		return;

// sort the teams
	memset(teams, 0, sizeof(teams));
	for (i = 0; i < MAX_CLIENTS; i++)
		teams[i].plow = 999;

	for (i = 0; i < MAX_CLIENTS; i++) {
		s = &cl.players[i];
		if (!s->name[0])
			continue;
		if (s->spectator)
			continue;

		// find his team in the list
		strcpy (t, s->team);	// safe
		if (!t[0])
			continue; // not on team
		for (j = 0; j < scoreboardteams; j++) {
			if (!strcmp(teams[j].team, t)) {
				teams[j].frags += s->frags;
				teams[j].players++;
				goto addpinginfo;
			}
		}
		if (j == scoreboardteams) { // must add him
			j = scoreboardteams++;
			strcpy(teams[j].team, t);
			teams[j].frags = s->frags;
			teams[j].players = 1;
addpinginfo:
			if (teams[j].plow > s->ping)
				teams[j].plow = s->ping;
			if (teams[j].phigh < s->ping)
				teams[j].phigh = s->ping;
			teams[j].ptotal += s->ping;
		}
	}

	// sort
	for (i = 0; i < scoreboardteams; i++) {
		teamsort[i] = i;
	}

	// good 'ol bubble sort
	for (i = 0; i < scoreboardteams - 1; i++) {
		for (j = i + 1; j < scoreboardteams; j++) {
			if (teams[teamsort[i]].frags < teams[teamsort[j]].frags) {
				k = teamsort[i];
				teamsort[i] = teamsort[j];
				teamsort[j] = k;
			}
		}
	}
}

static int Sbar_ColorForMap (int m)
{
	m = (m < 0) ? 0 : ((m > 13) ? 13 : m);

	m *= 16;
	return m < 128 ? m + 8 : m + 8;
}


/*
===============
Sbar_SoloScoreboard
===============
*/
static void Sbar_SoloScoreboard (void)
{
	char	str[80];
	double	_time;
	int		minutes, seconds, tens, units, l;

	if (!cl.gametype == GAME_COOP)
		return;

	Sbar_DrawPic (0, 0, sb_scorebar);

	sprintf(str, "Monsters:%3i /%3i", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
	Sbar_DrawString (8, 4, str);

	sprintf(str, "Secrets :%3i /%3i", cl.stats[STAT_SECRETS], cl.stats[STAT_TOTALSECRETS]);
	Sbar_DrawString (8, 12, str);

	// time
	if (cl.servertime_works) {
		_time = cl.servertime;	// good, we know real time spent on level
	} else {
		// well.. we must show something, right?
		_time = cls.realtime;
		if (cl.gametype == GAME_COOP)
			_time -= cl.players[cl.playernum].entertime;
	}
	minutes = _time / 60;
	seconds = _time - 60*minutes;
	tens = seconds / 10;
	units = seconds - 10*tens;
	sprintf (str,"Time :%3i:%i%i", minutes, tens, units);
	Sbar_DrawString (184, 4, str);

	// draw level name
	l = strlen (cl.levelname);
	if (l < 22 && !strstr(cl.levelname, "\n"))
		Sbar_DrawString (232 - l*4, 12, cl.levelname);
}

//=============================================================================

/*
===============
Sbar_DrawInventory
===============
*/
static void Sbar_DrawInventory (void)
{	
	int		i;
	char	num[6];
	float	time;
	int		flashon;

	// weapons
	for (i=0 ; i<7 ; i++) {
		if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN<<i) ) {
			time = cl.item_gettime[i];
			flashon = (int)((cl.time - time)*10);
			if (flashon < 0)
				flashon = 0;
			if (flashon >= 10) {
				if ( cl.stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN<<i)  )
					flashon = 1;
				else
					flashon = 0;
			} else {
				flashon = (flashon%5) + 2;
			}

			Sbar_DrawPic (i*24, -16, sb_weapons[flashon][i]);
		}
	}

	// hipnotic weapons
    if (gamemode == GAME_HIPNOTIC) {
		int grenadeflashing = 0;
		for (i = 0; i < 4; i++) {
			if (cl.stats[STAT_ITEMS] & (1<<hipweapons[i]) ) {
				time = cl.item_gettime[hipweapons[i]];
				flashon = (int)((cl.time - time)*10);
				if (flashon >= 10) {
					if (cl.stats[STAT_ACTIVEWEAPON] == (1<<hipweapons[i]))
						flashon = 1;
					else
						flashon = 0;
				} else {
					flashon = (flashon%5) + 2;
				}
				
				// check grenade launcher
				if (i == 2) {
					if (cl.stats[STAT_ITEMS] & HIT_PROXIMITY_GUN) {
						if (flashon) {
							grenadeflashing = 1;
							Sbar_DrawPic (96, -16, hsb_weapons[flashon][2]);
						}
					}
				} else if (i == 3) {
					if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN<<4)) {
						if (flashon && !grenadeflashing) {
							Sbar_DrawPic (96, -16, hsb_weapons[flashon][3]);
						}
						else if (!grenadeflashing) {
							Sbar_DrawPic (96, -16, hsb_weapons[0][3]);
						}
					} else {
						Sbar_DrawPic (96, -16, hsb_weapons[flashon][4]);
					}
				} else {
					Sbar_DrawPic (176 + (i*24), -16, hsb_weapons[flashon][i]);
				}
			}
		}
    }

	// ammo counts
	for (i=0 ; i<4 ; i++) {
		sprintf (num, "%3i",cl.stats[STAT_SHELLS+i] );
		if (num[0] != ' ')
			Sbar_DrawChar ( (6*i+1)*8 - 2, -24, 18 + num[0] - '0');
		if (num[1] != ' ')
			Sbar_DrawChar ( (6*i+2)*8 - 2, -24, 18 + num[1] - '0');
		if (num[2] != ' ')
			Sbar_DrawChar ( (6*i+3)*8 - 2, -24, 18 + num[2] - '0');
	}
	
	flashon = 0;
	// items
	for (i=0 ; i<6 ; i++) {
		if (cl.stats[STAT_ITEMS] & (1<<(17+i))) {
			time = cl.item_gettime[17+i];
			if (!(time && time > cl.time - 2 && flashon)) {
				if ( !(gamemode == GAME_HIPNOTIC && i < 2) )
					Sbar_DrawPic (192 + i*16, -16, sb_items[i]);		
			}
		}
	}

	// hipnotic items
	if (gamemode == GAME_HIPNOTIC) {
		for (i=0 ; i<2 ; i++) {
			if (cl.stats[STAT_ITEMS] & (1<<(24+i))) {
				time = cl.item_gettime[24+i];
				if (!(time && time > cl.time - 2 && flashon))
					Sbar_DrawPic (288 + i*16, -16, hsb_items[i]);
			}
		}
	}

	// sigils
	for (i=0 ; i<4 ; i++) {
		if (cl.stats[STAT_ITEMS] & (1<<(28+i)))	{
			time = cl.item_gettime[28+i];
			if (!(time &&	time > cl.time - 2 && flashon))
				Sbar_DrawPic (320-32 + i*8, -16, sb_sigil[i]);
		}
	}
}

//=============================================================================

/*
===============
Sbar_DrawFace
===============
*/
static void Sbar_DrawFace (void)
{
	int		f, anim;

	if ( (cl.stats[STAT_ITEMS] & (IT_INVISIBILITY | IT_INVULNERABILITY) ) == (IT_INVISIBILITY | IT_INVULNERABILITY) )
	{
		Sbar_DrawPic (112, 0, sb_face_invis_invuln);
		return;
	}
	if (cl.stats[STAT_ITEMS] & IT_QUAD) 
	{
		Sbar_DrawPic (112, 0, sb_face_quad );
		return;
	}
	if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY) 
	{
		Sbar_DrawPic (112, 0, sb_face_invis );
		return;
	}
	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) 
	{
		Sbar_DrawPic (112, 0, sb_face_invuln);
		return;
	}

	f = cl.stats[STAT_HEALTH] / 20;
	f = bound (0, f, 4);
	
	if (cl.time <= cl.faceanimtime)
		anim = 1;
	else
		anim = 0;
	Sbar_DrawPic (112, 0, sb_faces[f][anim]);
}

/*
=============
Sbar_DrawNormal
=============
*/
static void Sbar_DrawNormal (void)
{
	// armor
	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		Sbar_DrawNum (24, 0, 666, 3, 1);
		Sbar_DrawPic (0, 0, sb_disc);
	} else {
		if (cl.stats[STAT_ARMOR]) {
			if (cl.stats[STAT_ITEMS] & IT_ARMOR3)
				Sbar_DrawPic (0, 0, sb_armor[2]);
			else if (cl.stats[STAT_ITEMS] & IT_ARMOR2)
				Sbar_DrawPic (0, 0, sb_armor[1]);
			else if (cl.stats[STAT_ITEMS] & IT_ARMOR1)
				Sbar_DrawPic (0, 0, sb_armor[0]);

			Sbar_DrawNum (24, 0, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);
		}
	}
	
	// face
	Sbar_DrawFace ();
	
	// health
	Sbar_DrawNum (136, 0, cl.stats[STAT_HEALTH], 3, cl.stats[STAT_HEALTH] <= 25);

	// ammo
	if (cl.stats[STAT_AMMO]) {
		if (cl.stats[STAT_ITEMS] & IT_SHELLS)
			Sbar_DrawPic (224, 0, sb_ammo[0]);
		else if (cl.stats[STAT_ITEMS] & IT_NAILS)
			Sbar_DrawPic (224, 0, sb_ammo[1]);
		else if (cl.stats[STAT_ITEMS] & IT_ROCKETS)
			Sbar_DrawPic (224, 0, sb_ammo[2]);
		else if (cl.stats[STAT_ITEMS] & IT_CELLS)
			Sbar_DrawPic (224, 0, sb_ammo[3]);
	
		Sbar_DrawNum (248, 0, cl.stats[STAT_AMMO], 3, cl.stats[STAT_AMMO] <= 10);
	}

	// keys (hipnotic only)
	if (gamemode == GAME_HIPNOTIC) {
		if (cl.stats[STAT_ITEMS] & IT_KEY1)
            Sbar_DrawPic (209, 3, sb_items[0]);
		if (cl.stats[STAT_ITEMS] & IT_KEY2)
            Sbar_DrawPic (209, 12, sb_items[1]);
	}
}


/*
=============
Sbar_SpectatorScoreboard
=============
*/
static void Sbar_SpectatorScoreboard (void)
{
	char	st[512];

	if (!cam_track) {
		Sbar_DrawPic (0, 0, sb_scorebar);
		Sbar_DrawString (160-7*8,4, "SPECTATOR MODE");
		if (Cam_TargetCount())
			Sbar_DrawString(160-14*8+4, 12, "Press [ATTACK] for AutoCamera");
	} else {
		if (sb_showscores || cl.stats[STAT_HEALTH] <= 0)
			Sbar_SoloScoreboard ();
		else
			Sbar_DrawNormal ();

		sprintf (st, "Tracking %-.13s", cl.players[cam_target].name);	// sprintf ok
		if (!cam_locked)
			strcat (st, " (waiting)");
		else if (!cls.demoplayback && Cam_TargetCount() > 1)
			strcat (st, ", [JUMP] for next");
		Sbar_DrawString(0, -8, st);
	}
}

/*
===============
Sbar_Draw

Before calling, make sure sb_drawinventory and sb_drawmain are set to something reasonable.
If sb_drawinventory is set, then sb_drawmain should be set also.
===============
*/
void Sbar_Draw (void)
{
	qbool	inventory_area_drawn = false;

	if (scr_con_current == vid.height)
		return;		// console is full screen
	if (key_dest == key_menu)
		return;		// menu is up
	if (!sb_drawmain && !sb_drawinventory)
		return;		// nothing to do

	RB_SetCanvas (CANVAS_SBAR);
	
	// inventory
	if (sb_drawinventory) {
		if (!cl.spectator || cam_track) {
			Sbar_DrawInventory ();
			inventory_area_drawn = true;
		}
	}	

	// main area
	if (sb_drawmain) {
		if (cl.spectator)
			Sbar_SpectatorScoreboard ();
		else if (sb_showscores || cl.stats[STAT_HEALTH] <= 0)
			Sbar_SoloScoreboard ();
		else
			Sbar_DrawNormal ();
	}

	// main screen deathmatch rankings
	if (cl.stats[STAT_HEALTH] <= 0 && !cl.spectator) {
		// if we're dead show team scores in team games
		if (cl.teamplay && !sb_showscores)
			Sbar_TeamOverlay (0);
		else
			Sbar_DeathmatchOverlay (0);
	} else if (sb_showscores) {
		Sbar_DeathmatchOverlay (0);
	} else if (sb_showteamscores) {
		Sbar_TeamOverlay (0);
	}
}

//=============================================================================

/*
==================
Sbar_IntermissionNumber
==================
*/
static void Sbar_IntermissionNumber (int x, int y, int num, int digits, int color)
{
	char			str[12];
	char			*ptr;
	int				l, frame;

	l = Sbar_itoa (num, str);
	ptr = str;
	if (l > digits)
		ptr += (l-digits);
	if (l < digits)
		x += (digits-l)*24;

	while (*ptr)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		R_DrawPic (x,y,sb_nums[color][frame]);
		x += 24;
		ptr++;
	}
}

/*
==================
Sbar_TeamOverlay
==================
*/
static void Sbar_TeamOverlay (int start)
{
	int				i, k, l;
	int				x, y;
	char			num[12];
	char			team[4+1];
	team_t *tm;
	int plow, phigh, pavg;

	if (cl.gametype == GAME_COOP && cl.maxclients == 1)
		return;

	if (!cl.teamplay) {
		Sbar_DeathmatchOverlay (start);
		return;
	}

	RB_SetCanvas (CANVAS_MENU);

	if (!start) {
		Sbar_DrawTitle(0, "Ranking");
		y = 24;
	} else {
		y = start;
	}

	x = 36;
	R_DrawString (x, y, "low/avg/high team total players");
	y += 8;
//	R_DrawString (x, y, "------------ ---- ----- -------");
	R_DrawString (x, y, "\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f \x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1e\x1e\x1f");
	y += 8;

	// sort the teams
	Sbar_SortTeams();

	// draw the text
	l = scoreboardlines;

	for (i=0 ; i < scoreboardteams && y <= vid.height-10 ; i++)
	{
		k = teamsort[i];
		tm = teams + k;

		// draw pings
		plow = tm->plow;
		if (plow < 0 || plow > 999)
			plow = 999;
		phigh = tm->phigh;
		if (phigh < 0 || phigh > 999)
			phigh = 999;
		if (!tm->players)
			pavg = 999;
		else
			pavg = tm->ptotal / tm->players;
		if (pavg < 0 || pavg > 999)
			pavg = 999;

		sprintf (num, "%3i/%3i/%3i", plow, pavg, phigh);
		R_DrawString ( x, y, num);

		// draw team
		strlcpy (team, tm->team, sizeof(team));
		R_DrawString (x + 104, y, team);

		// draw total
		sprintf (num, "%5i", tm->frags);
		R_DrawString (x + 104 + 40, y, num);
		
		// draw players
		sprintf (num, "%5i", tm->players);
		R_DrawString (x + 104 + 88, y, num);
		
		if (!strcmp(cl.players[cl.playernum].team, tm->team)) {
			R_DrawChar ( x + 104 - 8, y, 16);
			R_DrawChar ( x + 104 + 32, y, 17);
		}
		
		y += 8;
	}
	y += 8;

	Sbar_DeathmatchOverlay(y);
}

/*
==================
Sbar_DeathmatchOverlay

ping time frags name
==================
*/
static void Sbar_DeathmatchOverlay (int start)
{
	int				i, k, l;
	int				top, bottom;
	int				x, y, f;
	char			num[12];
	player_info_t	*s;
	int				total;
	int				minutes;
	int				p;
	int				skip;
	qbool			largegame;
	extern qbool	nq_drawpings;

	if (cl.gametype == GAME_COOP && (cl.maxclients == 1 || cls.nqprotocol))
		return;

	RB_SetCanvas (CANVAS_MENU);

	// request new ping times every two second
	if (cls.realtime - cl.last_ping_request > 2)
	{
		cl.last_ping_request = cls.realtime;
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "pings");
	}
	
	if (!start) {
		y = (cls.nqprotocol && !nq_drawpings) ? 8 : 0;
		Sbar_DrawTitle(y, "Ranking");
	}

	// scores	
	Sbar_SortFrags (true);

	// draw the text
	l = scoreboardlines;

	if (start)
		y = start;
	else
		y = 24;

	if (cls.nqprotocol)
	{
		x = 80 - 104;
		if (nq_drawpings) {
			x += 16;
			R_DrawString ( x + 104 - 40, y, "ping frags name");
//			R_DrawString ( x + 104 - 40, y, "---- ----- ----------------");
			R_DrawString ( x + 104 - 40, y + 8, "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");
		}
		y += 16;
	}
	else if (cl.teamplay)
	{
		x = 4;
//                            0    40 64   104   152  192 
		R_DrawString ( x , y, "ping pl time frags team name");
		y += 8;
//		R_DrawString ( x , y, "---- -- ---- ----- ---- ----------------");
		R_DrawString ( x , y, "\x1d\x1e\x1e\x1f \x1d\x1f \x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f \x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");
		y += 8;
	}
	else
	{
		x = 16;
//                            0    40 64   104   152
		R_DrawString ( x , y, "ping pl time frags name");
		y += 8;
//		R_DrawString ( x , y, "---- -- ---- ----- ----------------");
		R_DrawString ( x , y, "\x1d\x1e\x1e\x1f \x1d\x1f \x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");
		y += 8;
	}

	// squish the lines if out of space
	if (y + l * 10 <= vid.height) {
		largegame = false;
		skip = 10;
	} else if (y + l * 9 <= vid.height) {
		largegame = true;
		skip = 9;
	} else {
		largegame = true;
		skip = 8;
	}

	for (i = 0; i < l && y <= vid.height - 8; i++)
	{
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name[0])
			continue;

		if (!cls.nqprotocol || nq_drawpings)
		{
			// draw ping
			p = s->ping;
			if (p < 0 || p > 999)
				p = 999;
			sprintf (num, "%4i", p);
			R_DrawString (cls.nqprotocol ? x + 104 - 40 : x, y, num);
		}

		if (!cls.nqprotocol)
		{
			if (cl.protocol >= 28)
			{
				// draw pl
				p = s->pl;
				sprintf (num, "%3i", p);
				if (p > 25)
					Draw_Alt_String ( x+32, y, num);
				else
					R_DrawString ( x+32, y, num);
			}

			if (s->spectator)
			{
				R_DrawString (x+68, y, "spectator");
				// draw name
				if (cl.teamplay)
					R_DrawString (x+152+40, y, s->name);
				else
					R_DrawString (x+152, y, s->name);
				y += skip;
				continue;
			}

			// draw time
			if (cl.intermission)
				total = cl.completed_time - s->entertime;
			else
				total = cls.realtime - s->entertime;
			minutes = (int)total/60;
			sprintf (num, "%4i", minutes);
			R_DrawString ( x+64 , y, num);
		}

		// draw background
		top = s->topcolor;
		bottom = s->bottomcolor;
		top = Sbar_ColorForMap (top);
		bottom = Sbar_ColorForMap (bottom);
	
		if (largegame)
			R_DrawFilledRect ( x+104, y+1, 40, 3, top, 0.6f);
		else
			R_DrawFilledRect ( x+104, y, 40, 4, top, 0.6f);
		R_DrawFilledRect ( x+104, y+4, 40, 4, bottom, 0.6f);

		// draw number
		f = s->frags;
		sprintf (num, "%3i",f);
		
		R_DrawChar ( x+112 , y, num[0]);
		R_DrawChar ( x+120 , y, num[1]);
		R_DrawChar ( x+128 , y, num[2]);

		if (k == cl.playernum)
		{
			R_DrawChar ( x + 104, y, 16);
			R_DrawChar ( x + 136, y, 17);
		}
		
		// team
		if (cl.teamplay)
		{
			char team[4+1];
			strlcpy (team, s->team, sizeof(team));
			R_DrawString (x+152, y, team);
		}

		// draw name
		if (cls.nqprotocol && !nq_drawpings)
			R_DrawString (x+104+64, y, s->name);
		else if (cl.teamplay)
			R_DrawString (x+152+40, y, s->name);
		else
			R_DrawString (x+152, y, s->name);
		
		y += skip;
	}

	RB_SetCanvas (CANVAS_SBAR);
}

/*
==================
Sbar_IntermissionOverlay
==================
*/
void Sbar_IntermissionOverlay (void)
{
	int		dig;
	int		num;
	int		time;
	int		thiswidth;

	if (cl.gametype == GAME_DEATHMATCH)
	{
		if (cl.teamplay && !sb_showscores)
			Sbar_TeamOverlay (0);
		else
			Sbar_DeathmatchOverlay (0);
		return;
	}

	RB_SetCanvas (CANVAS_MENU);

	// title text
	thiswidth = strlen("Complete") * BIGMENU_TITLE_SCALE * BIGLETTER_WIDTH;
	Draw_BigString(64, 24, "Complete", BIGMENU_TITLE_SCALE, BIGMENU_LETTER_SPACING);

	// in coop, pressing TAB shows player frags instead of totals
	if ((sb_showscores || sb_showteamscores) && cl.maxclients > 1 && atoi(Info_ValueForKey(cl.serverinfo, "coop")))
	{
		Sbar_TeamOverlay (48);
		return;
	}

	// time
	thiswidth = strlen("Time") * BIGMENU_ITEMS_SCALE * BIGLETTER_WIDTH;
	Draw_BigString(0, 64, "Time", BIGMENU_ITEMS_SCALE, BIGMENU_LETTER_SPACING);
	if (cl.servertime_works) {
		// we know the exact time
		time = cl.solo_completed_time;
	} else {
		// use an estimate
		time = cl.completed_time - cl.players[cl.playernum].entertime;
	}
	dig = time / 60;
	Sbar_IntermissionNumber (152, 64, dig, 3, 0);
	num = time - dig*60;
	R_DrawPic (224, 64, sb_colon);
	R_DrawPic (240, 64, sb_nums[0][num/10]);
	R_DrawPic (264, 64, sb_nums[0][num%10]);

	// secrets
	thiswidth = strlen("Secrets") * BIGMENU_ITEMS_SCALE * BIGLETTER_WIDTH;
	Draw_BigString(0, 104, "Secrets", BIGMENU_ITEMS_SCALE, BIGMENU_LETTER_SPACING);
	Sbar_IntermissionNumber (152, 104, cl.stats[STAT_SECRETS], 3, 0);
	R_DrawPic (224, 104, sb_slash);
	Sbar_IntermissionNumber (240, 104, cl.stats[STAT_TOTALSECRETS], 3, 0);

	// monsters
	thiswidth = strlen("Monsters") * BIGMENU_ITEMS_SCALE * BIGLETTER_WIDTH;
	Draw_BigString(0, 144, "Monsters", BIGMENU_ITEMS_SCALE, BIGMENU_LETTER_SPACING);
	Sbar_IntermissionNumber (152, 144, cl.stats[STAT_MONSTERS], 3, 0);
	R_DrawPic (224, 144, sb_slash);
	Sbar_IntermissionNumber (240, 144, cl.stats[STAT_TOTALMONSTERS], 3, 0);
}


/*
==================
Sbar_FinaleOverlay
==================
*/
void Sbar_FinaleOverlay (void)
{
	RB_SetCanvas (CANVAS_MENU);

	Sbar_DrawTitle(16, "Finale");
}

