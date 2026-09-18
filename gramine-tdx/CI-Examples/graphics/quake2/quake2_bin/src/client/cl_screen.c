/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * This file implements the 2D stuff. For example the HUD and the
 * networkgraph.
 *
 * =======================================================================
 */

#include "header/client.h"
#include <limits.h>

float scr_con_current; /* aproaches scr_conlines at scr_conspeed */
float scr_conlines; /* 0.0 to 1.0 lines of console to display */

qboolean scr_initialized; /* ready to draw */

int scr_draw_loading;

vrect_t scr_vrect; /* position of render window on screen */

cvar_t *scr_viewsize;
cvar_t *scr_conspeed;
cvar_t *scr_centertime;
cvar_t *scr_showturtle;
cvar_t *scr_showpause;

cvar_t *scr_netgraph;
cvar_t *scr_timegraph;
cvar_t *scr_debuggraph;
cvar_t *scr_graphheight;
cvar_t *scr_graphscale;
cvar_t *scr_graphshift;
cvar_t *scr_drawall;

cvar_t *r_hudscale; /* named for consistency with R1Q2 */
cvar_t *r_consolescale;
cvar_t *r_menuscale;
long long cumulative_render_time = 0;
long long cumulative_display_time = 0;
long long num_frames = 0;
typedef struct
{
	int x1, y1, x2, y2;
} dirty_t;

dirty_t scr_dirty, scr_old_dirty[2];

char crosshair_pic[MAX_QPATH];
int crosshair_width, crosshair_height;

extern cvar_t *cl_showfps, *vid_maxfps, *cl_maxfps;
extern cvar_t *crosshair_scale;

void SCR_TimeRefresh_f(void);
void SCR_Loading_f(void);

/*
 * A new packet was just parsed
 */
void
CL_AddNetgraph(void)
{
	int i;
	int in;
	int ping;

	/* if using the debuggraph for something
	   else, don't add the net lines */
	if (scr_debuggraph->value || scr_timegraph->value)
	{
		return;
	}

	for (i = 0; i < cls.netchan.dropped; i++)
	{
		SCR_DebugGraph(30, 0x40);
	}

	for (i = 0; i < cl.surpressCount; i++)
	{
		SCR_DebugGraph(30, 0xdf);
	}

	/* see what the latency was on this packet */
	in = cls.netchan.incoming_acknowledged & (CMD_BACKUP - 1);
	ping = cls.realtime - cl.cmd_time[in];
	ping /= 30;

	if (ping > 30)
	{
		ping = 30;
	}

	SCR_DebugGraph((float)ping, 0xd0);
}

typedef struct
{
	float value;
	int color;
} graphsamp_t;

static int current;
static graphsamp_t values[2024];

void
SCR_DebugGraph(float value, int color)
{
	values[current & 2023].value = value;
	values[current & 2023].color = color;
	current++;
}

void
SCR_DrawDebugGraph(void)
{
	int a, x, y, w, i, h;
	float v;
	int color;

	/* draw the graph */
	w = scr_vrect.width;

	x = scr_vrect.x;
	y = scr_vrect.y + scr_vrect.height;
	Draw_Fill(x, y - scr_graphheight->value,
			w, scr_graphheight->value, 8);

	for (a = 0; a < w; a++)
	{
		i = (current - 1 - a + 1024) & 1023;
		v = values[i].value;
		color = values[i].color;
		v = v * scr_graphscale->value + scr_graphshift->value;

		if (v < 0)
		{
			v += scr_graphheight->value *
				 (1 + (int)(-v / scr_graphheight->value));
		}

		h = (int)v % (int)scr_graphheight->value;
		Draw_Fill(x + w - 1 - a, y - h, 1, h, color);
	}
}

char scr_centerstring[1024];
float scr_centertime_start; /* for slow victory printing */
float scr_centertime_off;
int scr_center_lines;
int scr_erase_center;

/*
 * Called for important messages that should stay
 * in the center of the screen for a few moments
 */
void
SCR_CenterPrint(char *str)
{
	char *s;
	char line[64];
	int i, j, l;

	Q_strlcpy(scr_centerstring, str, sizeof(scr_centerstring));
	scr_centertime_off = scr_centertime->value;
	scr_centertime_start = cl.time;

	/* count the number of lines for centering */
	scr_center_lines = 1;
	s = str;

	while (*s)
	{
		if (*s == '\n')
		{
			scr_center_lines++;
		}

		s++;
	}

	/* echo it to the console */
	/* Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n"); */

	s = str;

	do
	{
		/* scan the width of the line */
		for (l = 0; l < 40; l++)
		{
			if ((s[l] == '\n') || !s[l])
			{
				break;
			}
		}

		for (i = 0; i < (40 - l) / 2; i++)
		{
			line[i] = ' ';
		}

		for (j = 0; j < l; j++)
		{
			line[i++] = s[j];
		}

		line[i] = '\n';
		line[i + 1] = 0;

		/* Com_Printf("%s", line); */

		while (*s && *s != '\n')
		{
			s++;
		}

		if (!*s)
		{
			break;
		}

		s++; /* skip the \n */
	}
	while (1);

	/* Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n"); */
	Con_ClearNotify();
}

void
SCR_DrawCenterString(void)
{
	char *start;
	int l;
	int j;
	int x, y;
	int remaining;
	float scale;
    const int char_unscaled_width  = 8;
    const int char_unscaled_height = 8;

	/* the finale prints the characters one at a time */
	remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;
	scale = SCR_GetConsoleScale();

	if (scr_center_lines <= 4)
	{
		y = (viddef.height * 0.35) / scale;
	}

	else
	{
		y = 48 / scale;
	}

	do
	{
		/* scan the width of the line */
		for (l = 0; l < 40; l++)
		{
			if ((start[l] == '\n') || !start[l])
			{
				break;
			}
		}

		x = ((viddef.width / scale) - (l * char_unscaled_width)) / 2;
		SCR_AddDirtyPoint(x, y);

		for (j = 0; j < l; j++, x += char_unscaled_width)
		{
			Draw_CharScaled(x * scale, y * scale, start[j], scale);

			if (!remaining--)
			{
				return;
			}
		}

		SCR_AddDirtyPoint(x, y + char_unscaled_height);

		y += char_unscaled_height;

		while (*start && *start != '\n')
		{
			start++;
		}

		if (!*start)
		{
			break;
		}

		start++; /* skip the \n */
	}
	while (1);
}

void
SCR_CheckDrawCenterString(void)
{
	scr_centertime_off -= cls.rframetime;

	if (scr_centertime_off <= 0)
	{
		return;
	}

	SCR_DrawCenterString();
}

/*
 * Sets scr_vrect, the coordinates of the rendered window
 */
static void
SCR_CalcVrect(void)
{
	int size;

	/* bound viewsize */
	if (scr_viewsize->value < 40)
	{
		Cvar_Set("viewsize", "40");
	}

	if (scr_viewsize->value > 100)
	{
		Cvar_Set("viewsize", "100");
	}

	size = scr_viewsize->value;

	scr_vrect.width = viddef.width * size / 100;
	scr_vrect.height = viddef.height * size / 100;

	scr_vrect.x = (viddef.width - scr_vrect.width) / 2;
	scr_vrect.y = (viddef.height - scr_vrect.height) / 2;
}

/*
 * Keybinding command
 */
void
SCR_SizeUp_f(void)
{
	Cvar_SetValue("viewsize", (float)scr_viewsize->value + 10);
}

/*
 *Keybinding command
 */
void
SCR_SizeDown_f(void)
{
	Cvar_SetValue("viewsize", (float)scr_viewsize->value - 10);
}

/*
 * Set a specific sky and rotation speed
 */
void
SCR_Sky_f(void)
{
	float rotate;
	vec3_t axis;

	if (Cmd_Argc() < 2)
	{
		Com_Printf("Usage: sky <basename> <rotate> <axis x y z>\n");
		return;
	}

	if (Cmd_Argc() > 2)
	{
		rotate = (float)strtod(Cmd_Argv(2), (char **)NULL);
	}

	else
	{
		rotate = 0;
	}

	if (Cmd_Argc() == 6)
	{
		axis[0] = (float)strtod(Cmd_Argv(3), (char **)NULL);
		axis[1] = (float)strtod(Cmd_Argv(4), (char **)NULL);
		axis[2] = (float)strtod(Cmd_Argv(5), (char **)NULL);
	}
	else
	{
		axis[0] = 0;
		axis[1] = 0;
		axis[2] = 1;
	}

	R_SetSky(Cmd_Argv(1), rotate, axis);
}

void
SCR_Init(void)
{
	scr_viewsize = Cvar_Get("viewsize", "100", CVAR_ARCHIVE);
	scr_conspeed = Cvar_Get("scr_conspeed", "3", 0);
	scr_centertime = Cvar_Get("scr_centertime", "2.5", 0);
	scr_showturtle = Cvar_Get("scr_showturtle", "0", 0);
	scr_showpause = Cvar_Get("scr_showpause", "1", 0);
	scr_netgraph = Cvar_Get("netgraph", "0", 0);
	scr_timegraph = Cvar_Get("timegraph", "0", 0);
	scr_debuggraph = Cvar_Get("debuggraph", "0", 0);
	scr_graphheight = Cvar_Get("graphheight", "32", 0);
	scr_graphscale = Cvar_Get("graphscale", "1", 0);
	scr_graphshift = Cvar_Get("graphshift", "0", 0);
	scr_drawall = Cvar_Get("scr_drawall", "0", 0);
	r_hudscale = Cvar_Get("r_hudscale", "-1", CVAR_ARCHIVE);
	r_consolescale = Cvar_Get("r_consolescale", "-1", CVAR_ARCHIVE);
	r_menuscale = Cvar_Get("r_menuscale", "-1", CVAR_ARCHIVE);

	/* register our commands */
	Cmd_AddCommand("timerefresh", SCR_TimeRefresh_f);
	Cmd_AddCommand("loading", SCR_Loading_f);
	Cmd_AddCommand("sizeup", SCR_SizeUp_f);
	Cmd_AddCommand("sizedown", SCR_SizeDown_f);
	Cmd_AddCommand("sky", SCR_Sky_f);

	scr_initialized = true;
}

void
SCR_DrawNet(void)
{
	float scale = SCR_GetMenuScale();

	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged < CMD_BACKUP - 1)
	{
		return;
	}

	Draw_PicScaled(scr_vrect.x + 64 * scale, scr_vrect.y, "net", scale);
}

void
SCR_DrawPause(void)
{
	int w, h;
	float scale = SCR_GetMenuScale();

	if (!scr_showpause->value) /* turn off for screenshots */
	{
		return;
	}

	if (!cl_paused->value)
	{
		return;
	}

	Draw_GetPicSize(&w, &h, "pause");
	Draw_PicScaled((viddef.width - w * scale) / 2, viddef.height / 2 + 8 * scale, "pause", scale);
}

void
SCR_DrawLoading(void)
{
	int w, h;
	float scale = SCR_GetMenuScale();

	if (!scr_draw_loading)
	{
		return;
	}

	Draw_GetPicSize(&w, &h, "loading");
	Draw_PicScaled((viddef.width - w * scale) / 2, (viddef.height - h * scale) / 2, "loading", scale);
}

/*
 * Scroll it up or down
 */
void
SCR_RunConsole(void)
{
	/* src_conspeed must be a positiv integer,
	   otherwise things go wrong. Clamp it. */
	if (scr_conspeed->value < 0.1f)
	{
		Cvar_Set("scr_conspeed", "0.1");
	}

	/* decide on the height of the console */
	if (cls.key_dest == key_console)
	{
		scr_conlines = 0.5; /* half screen */
	}
	else
	{
		scr_conlines = 0; /* none visible */
	}

	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed->value * cls.rframetime;

		if (scr_conlines > scr_con_current)
		{
			scr_con_current = scr_conlines;
		}
	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed->value * cls.rframetime;

		if (scr_conlines < scr_con_current)
		{
			scr_con_current = scr_conlines;
		}
	}
}

void
SCR_DrawConsole(void)
{
	Con_CheckResize();

	if ((cls.state == ca_disconnected) || (cls.state == ca_connecting))
	{
		/* forced full screen console */
		Con_DrawConsole(1.0);
		return;
	}

	if ((cls.state != ca_active) || !cl.refresh_prepped)
	{
		/* connected, but can't render */
		Con_DrawConsole(0.5);
		Draw_Fill(0, viddef.height / 2, viddef.width, viddef.height / 2, 0);
		return;
	}

	if (scr_con_current)
	{
		Con_DrawConsole(scr_con_current);
	}
	else
	{
		if ((cls.key_dest == key_game) || (cls.key_dest == key_message))
		{
			Con_DrawNotify(); /* only draw notify in game */
		}
	}
}

void
SCR_BeginLoadingPlaque(void)
{
	S_StopAllSounds();
	cl.sound_prepped = false; /* don't play ambients */

	OGG_Stop();

	if (cls.disable_screen)
	{
		return;
	}

	if (developer->value)
	{
		/* Hack: When we are returning here (not drawing
		   the loading plaque) we don't reset the palette
		   later on. We might end up with the cinematic
		   palette applied to the world. Enforce the world
		   palette. */
		if (cl.cinematictime > 0)
		{
			R_SetPalette(NULL);
		}

		return;
	}

	if (cls.state == ca_disconnected)
	{
		/* if at console, don't bring up the plaque */
		return;
	}

	if (cls.key_dest == key_console)
	{
		return;
	}

	if (cl.cinematictime > 0)
	{
		scr_draw_loading = 2; /* clear to balack first */
	}
	else
	{
		scr_draw_loading = 1;
	}

	SCR_UpdateScreen();

	scr_draw_loading = false;

	SCR_StopCinematic();
	cls.disable_screen = Sys_Milliseconds();
	cls.disable_servercount = cl.servercount;
}

void
SCR_EndLoadingPlaque(void)
{
	cls.disable_screen = 0;
	Con_ClearNotify();
}

void
SCR_Loading_f(void)
{
	SCR_BeginLoadingPlaque();
}

void
SCR_TimeRefresh_f(void)
{
	int i;
	int start, stop;
	float time;

	if (cls.state != ca_active)
	{
		return;
	}

	start = Sys_Milliseconds();

	if (Cmd_Argc() == 2)
	{
		/* run without page flipping */
		int j;

		for (j = 0; j < 1000; j++)
		{
			R_BeginFrame(0);

			for (i = 0; i < 128; i++)
			{
				cl.refdef.viewangles[1] = i / 128.0f * 360.0f;
				R_RenderFrame(&cl.refdef);
			}

			R_EndFrame();
		}
	}
	else
	{
		for (i = 0; i < 128; i++)
		{
			cl.refdef.viewangles[1] = i / 128.0f * 360.0f;

			R_BeginFrame(0);
			R_RenderFrame(&cl.refdef);
			R_EndFrame();
		}
	}

	stop = Sys_Milliseconds();
	time = (stop - start) / 1000.0f;
	Com_Printf("%f seconds (%f fps)\n", time, 128 / time);
}

void
SCR_AddDirtyPoint(int x, int y)
{
	if (x < scr_dirty.x1)
	{
		scr_dirty.x1 = x;
	}

	if (x > scr_dirty.x2)
	{
		scr_dirty.x2 = x;
	}

	if (y < scr_dirty.y1)
	{
		scr_dirty.y1 = y;
	}

	if (y > scr_dirty.y2)
	{
		scr_dirty.y2 = y;
	}
}

void
SCR_DirtyScreen(void)
{
	SCR_AddDirtyPoint(0, 0);
	SCR_AddDirtyPoint(viddef.width - 1, viddef.height - 1);
}

/*
 * Clear any parts of the tiled background that were drawn on last frame
 */
void
SCR_TileClear(void)
{
	int i;
	int top, bottom, left, right;
	dirty_t clear;

	if (scr_con_current == 1.0)
	{
		return; /* full screen console */
	}

	if (scr_viewsize->value == 100)
	{
		return; /* full screen rendering */
	}

	if (cl.cinematictime > 0)
	{
		return; /* full screen cinematic */
	}


	/* This highly complicated pseudo damage tracking is very effective
	   with the soft render, it gives about 10% speedup. On the GL
	   renderers the speedup is negligible, but in introduce a bug:
	   OpenGL requires to redraw the borders after every glClear(),
	   which the damage tracking doesn't take into account. Fix this
	   by not using the damage tracking when running somethinge else
	   than the soft renderer. Just redraw the borders every frame. */
	if (strcmp(vid_renderer->string, "soft") != 0) {
		// Top
		Draw_TileClear(scr_vrect.x, 0, scr_vrect.width, scr_vrect.y, "backtile");

		// Bottom
		Draw_TileClear(scr_vrect.x, scr_vrect.y + scr_vrect.height, scr_vrect.width, viddef.height, "backtile");

		// Left
		Draw_TileClear(0, 0, scr_vrect.x, viddef.height, "backtile");

		// Right
		Draw_TileClear(scr_vrect.x + scr_vrect.width, 0, viddef.width, viddef.height, "backtile");
	} else {
		/* erase rect will be the union of the past three
		   frames so tripple buffering works properly */
		clear = scr_dirty;

		for (i = 0; i < 2; i++)
		{
			if (scr_old_dirty[i].x1 < clear.x1)
			{
				clear.x1 = scr_old_dirty[i].x1;
			}

			if (scr_old_dirty[i].x2 > clear.x2)
			{
				clear.x2 = scr_old_dirty[i].x2;
			}

			if (scr_old_dirty[i].y1 < clear.y1)
			{
				clear.y1 = scr_old_dirty[i].y1;
			}

			if (scr_old_dirty[i].y2 > clear.y2)
			{
				clear.y2 = scr_old_dirty[i].y2;
			}
		}

		scr_old_dirty[1] = scr_old_dirty[0];
		scr_old_dirty[0] = scr_dirty;

		scr_dirty.x1 = 9999;
		scr_dirty.x2 = -9999;
		scr_dirty.y1 = 9999;
		scr_dirty.y2 = -9999;

		/* don't bother with anything convered by the console */
		top = (int)(scr_con_current * viddef.height);

		if (top >= clear.y1)
		{
			clear.y1 = top;
		}

		if (clear.y2 <= clear.y1)
		{
			return; /* nothing disturbed */
		}

		top = scr_vrect.y;
		bottom = top + scr_vrect.height - 1;
		left = scr_vrect.x;
		right = left + scr_vrect.width - 1;

		if (clear.y1 < top)
		{
			/* clear above view screen */
			i = clear.y2 < top - 1 ? clear.y2 : top - 1;
			Draw_TileClear(clear.x1, clear.y1,
					clear.x2 - clear.x1 + 1, i - clear.y1 + 1, "backtile");
			clear.y1 = top;
		}

		if (clear.y2 > bottom)
		{
			/* clear below view screen */
			i = clear.y1 > bottom + 1 ? clear.y1 : bottom + 1;
			Draw_TileClear(clear.x1, i,
					clear.x2 - clear.x1 + 1, clear.y2 - i + 1, "backtile");
			clear.y2 = bottom;
		}

		if (clear.x1 < left)
		{
			/* clear left of view screen */
			i = clear.x2 < left - 1 ? clear.x2 : left - 1;
			Draw_TileClear(clear.x1, clear.y1,
					i - clear.x1 + 1, clear.y2 - clear.y1 + 1, "backtile");
			clear.x1 = left;
		}

		if (clear.x2 > right)
		{
			/* clear left of view screen */
			i = clear.x1 > right + 1 ? clear.x1 : right + 1;
			Draw_TileClear(i, clear.y1,
					clear.x2 - i + 1, clear.y2 - clear.y1 + 1, "backtile");
			clear.x2 = right;
		}
	}
}

#define STAT_MINUS 10
char *sb_nums[2][11] = {
	{
		"num_0", "num_1", "num_2", "num_3", "num_4", "num_5",
		"num_6", "num_7", "num_8", "num_9", "num_minus"
	},
	{
		"anum_0", "anum_1", "anum_2", "anum_3", "anum_4", "anum_5",
		"anum_6", "anum_7", "anum_8", "anum_9", "anum_minus"
	}
};

#define ICON_WIDTH 24
#define ICON_HEIGHT 24
#define CHAR_WIDTH 16
#define ICON_SPACE 8

/*
 * Allow embedded \n in the string
 */
void
SizeHUDString(char *string, int *w, int *h)
{
	int lines, width, current;

	lines = 1;
	width = 0;

	current = 0;

	while (*string)
	{
		if (*string == '\n')
		{
			lines++;
			current = 0;
		}
		else
		{
			current++;

			if (current > width)
			{
				width = current;
			}
		}

		string++;
	}

	*w = width * 8;
	*h = lines * 8;
}

void
DrawHUDStringScaled(char *string, int x, int y, int centerwidth, int xor, float factor)
{
	int margin;
	char line[1024];
	int width;
	int i;

	margin = x;

	while (*string)
	{
		/* scan out one line of text from the string */
		width = 0;

		while (*string && *string != '\n')
		{
			line[width++] = *string++;
		}

		line[width] = 0;

		if (centerwidth)
		{
			x = margin + (centerwidth - width * 8)*factor / 2;
		}

		else
		{
			x = margin;
		}

		for (i = 0; i < width; i++)
		{
			Draw_CharScaled(x, y, line[i] ^ xor, factor);
			x += 8*factor;
		}

		if (*string)
		{
			string++; /* skip the \n */
			y += 8*factor;
		}
	}
}

void
DrawHUDString(char *string, int x, int y, int centerwidth, int xor)
{
	DrawHUDStringScaled(string, x, y, centerwidth, xor, 1.0f);
}

void
SCR_DrawFieldScaled(int x, int y, int color, int width, int value, float factor)
{
	char num[16], *ptr;
	int l;
	int frame;

	if (width < 1)
	{
		return;
	}

	/* draw number string */
	if (width > 5)
	{
		width = 5;
	}

	SCR_AddDirtyPoint(x, y);
	SCR_AddDirtyPoint(x + (width * CHAR_WIDTH + 2)*factor, y + factor*24);

	Com_sprintf(num, sizeof(num), "%i", value);
	l = (int)strlen(num);

	if (l > width)
	{
		l = width;
	}

	x += (2 + CHAR_WIDTH * (width - l)) * factor;

	ptr = num;

	while (*ptr && l)
	{
		if (*ptr == '-')
		{
			frame = STAT_MINUS;
		}

		else
		{
			frame = *ptr - '0';
		}

		Draw_PicScaled(x, y, sb_nums[color][frame], factor);
		x += CHAR_WIDTH*factor;
		ptr++;
		l--;
	}
}

void
SCR_DrawField(int x, int y, int color, int width, int value)
{
	SCR_DrawFieldScaled(x, y, color, width, value, 1.0f);
}

/*
 * Allows rendering code to cache all needed sbar graphics
 */
void
SCR_TouchPics(void)
{
	int i, j;

	for (i = 0; i < 2; i++)
	{
		for (j = 0; j < 11; j++)
		{
			Draw_FindPic(sb_nums[i][j]);
		}
	}

	if (crosshair->value)
	{
		if ((crosshair->value > 3) || (crosshair->value < 0))
		{
			crosshair->value = 3;
		}

		Com_sprintf(crosshair_pic, sizeof(crosshair_pic), "ch%i",
				(int)(crosshair->value));
		Draw_GetPicSize(&crosshair_width, &crosshair_height, crosshair_pic);

		if (!crosshair_width)
		{
			crosshair_pic[0] = 0;
		}
	}
}

void
SCR_ExecuteLayoutString(char *s)
{
	int x, y;
	int value;
	char *token;
	int width;
	int index;
	clientinfo_t *ci;

	float scale = SCR_GetHUDScale();

	if ((cls.state != ca_active) || !cl.refresh_prepped)
	{
		return;
	}

	if (!s[0])
	{
		return;
	}

	x = 0;
	y = 0;

	while (s)
	{
		token = COM_Parse(&s);

		if (!strcmp(token, "xl"))
		{
			token = COM_Parse(&s);
			x = scale*(int)strtol(token, (char **)NULL, 10);
			continue;
		}

		if (!strcmp(token, "xr"))
		{
			token = COM_Parse(&s);
			x = viddef.width + scale*(int)strtol(token, (char **)NULL, 10);
			continue;
		}

		if (!strcmp(token, "xv"))
		{
			token = COM_Parse(&s);
			x = viddef.width / 2 - scale*160 + scale*(int)strtol(token, (char **)NULL, 10);
			continue;
		}

		if (!strcmp(token, "yt"))
		{
			token = COM_Parse(&s);
			y = scale*(int)strtol(token, (char **)NULL, 10);
			continue;
		}

		if (!strcmp(token, "yb"))
		{
			token = COM_Parse(&s);
			y = viddef.height + scale*(int)strtol(token, (char **)NULL, 10);
			continue;
		}

		if (!strcmp(token, "yv"))
		{
			token = COM_Parse(&s);
			y = viddef.height / 2 - scale*120 + scale*(int)strtol(token, (char **)NULL, 10);
			continue;
		}

		if (!strcmp(token, "pic"))
		{
			/* draw a pic from a stat number */
			token = COM_Parse(&s);
			index = (int)strtol(token, (char **)NULL, 10);

			if ((index < 0) || (index >= MAX_STATS))
			{
				Com_Error(ERR_DROP, "bad stats index %d (0x%x)", index, index);
			}

			value = cl.frame.playerstate.stats[index];

			if (value >= MAX_IMAGES)
			{
				Com_Error(ERR_DROP, "Pic >= MAX_IMAGES");
			}

			if (cl.configstrings[CS_IMAGES + value][0] != '\0')
			{
				SCR_AddDirtyPoint(x, y);
				SCR_AddDirtyPoint(x + 23*scale, y + 23*scale);
				Draw_PicScaled(x, y, cl.configstrings[CS_IMAGES + value], scale);
			}

			continue;
		}

		if (!strcmp(token, "client"))
		{
			/* draw a deathmatch client block */
			int score, ping, time;

			token = COM_Parse(&s);
			x = viddef.width / 2 - scale*160 + scale*(int)strtol(token, (char **)NULL, 10);
			token = COM_Parse(&s);
			y = viddef.height / 2 - scale*120 + scale*(int)strtol(token, (char **)NULL, 10);
			SCR_AddDirtyPoint(x, y);
			SCR_AddDirtyPoint(x + scale*159, y + scale*31);

			token = COM_Parse(&s);
			value = (int)strtol(token, (char **)NULL, 10);

			if ((value >= MAX_CLIENTS) || (value < 0))
			{
				Com_Error(ERR_DROP, "client >= MAX_CLIENTS");
			}

			ci = &cl.clientinfo[value];

			token = COM_Parse(&s);
			score = (int)strtol(token, (char **)NULL, 10);

			token = COM_Parse(&s);
			ping = (int)strtol(token, (char **)NULL, 10);

			token = COM_Parse(&s);
			time = (int)strtol(token, (char **)NULL, 10);

			DrawAltStringScaled(x + scale*32, y, ci->name, scale);
			DrawAltStringScaled(x + scale*32, y + scale*8, "Score: ", scale);
			DrawAltStringScaled(x + scale*(32 + 7 * 8), y + scale*8, va("%i", score), scale);
			DrawStringScaled(x + scale*32, y + scale*16, va("Ping:  %i", ping), scale);
			DrawStringScaled(x + scale*32, y + scale*24, va("Time:  %i", time), scale);

			if (!ci->icon)
			{
				ci = &cl.baseclientinfo;
			}

			Draw_PicScaled(x, y, ci->iconname, scale);
			continue;
		}

		if (!strcmp(token, "ctf"))
		{
			/* draw a ctf client block */
			int score, ping;
			char block[80];

			token = COM_Parse(&s);
			x = viddef.width / 2 - scale*160 + scale*(int)strtol(token, (char **)NULL, 10);
			token = COM_Parse(&s);
			y = viddef.height / 2 - scale*120 + scale*(int)strtol(token, (char **)NULL, 10);
			SCR_AddDirtyPoint(x, y);
			SCR_AddDirtyPoint(x + scale*159, y + scale*31);

			token = COM_Parse(&s);
			value = (int)strtol(token, (char **)NULL, 10);

			if ((value >= MAX_CLIENTS) || (value < 0))
			{
				Com_Error(ERR_DROP, "client >= MAX_CLIENTS");
			}

			ci = &cl.clientinfo[value];

			token = COM_Parse(&s);
			score = (int)strtol(token, (char **)NULL, 10);

			token = COM_Parse(&s);
			ping = (int)strtol(token, (char **)NULL, 10);

			if (ping > 999)
			{
				ping = 999;
			}

			sprintf(block, "%3d %3d %-12.12s", score, ping, ci->name);

			if (value == cl.playernum)
			{
				DrawAltStringScaled(x, y, block, scale);
			}

			else
			{
				DrawStringScaled(x, y, block, scale);
			}

			continue;
		}

		if (!strcmp(token, "picn"))
		{
			/* draw a pic from a name */
			token = COM_Parse(&s);
			SCR_AddDirtyPoint(x, y);
			SCR_AddDirtyPoint(x + scale*23, y + scale*23);
			Draw_PicScaled(x, y, (char *)token, scale);
			continue;
		}

		if (!strcmp(token, "num"))
		{
			/* draw a number */
			token = COM_Parse(&s);
			width = (int)strtol(token, (char **)NULL, 10);
			token = COM_Parse(&s);
			value = cl.frame.playerstate.stats[(int)strtol(token, (char **)NULL, 10)];
			SCR_DrawFieldScaled(x, y, 0, width, value, scale);
			continue;
		}

		if (!strcmp(token, "hnum"))
		{
			/* health number */
			int color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_HEALTH];

			if (value > 25)
			{
				color = 0;  /* green */
			}
			else if (value > 0)
			{
				color = (cl.frame.serverframe >> 2) & 1; /* flash */
			}
			else
			{
				color = 1;
			}

			if (cl.frame.playerstate.stats[STAT_FLASHES] & 1)
			{
				Draw_PicScaled(x, y, "field_3", scale);
			}

			SCR_DrawFieldScaled(x, y, color, width, value, scale);
			continue;
		}

		if (!strcmp(token, "anum"))
		{
			/* ammo number */
			int color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_AMMO];

			if (value > 5)
			{
				color = 0; /* green */
			}
			else if (value >= 0)
			{
				color = (cl.frame.serverframe >> 2) & 1; /* flash */
			}
			else
			{
				continue; /* negative number = don't show */
			}

			if (cl.frame.playerstate.stats[STAT_FLASHES] & 4)
			{
				Draw_PicScaled(x, y, "field_3", scale);
			}

			SCR_DrawFieldScaled(x, y, color, width, value, scale);
			continue;
		}

		if (!strcmp(token, "rnum"))
		{
			/* armor number */
			int color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_ARMOR];

			if (value < 1)
			{
				continue;
			}

			color = 0; /* green */

			if (cl.frame.playerstate.stats[STAT_FLASHES] & 2)
			{
				Draw_PicScaled(x, y, "field_3", scale);
			}

			SCR_DrawFieldScaled(x, y, color, width, value, scale);
			continue;
		}

		if (!strcmp(token, "stat_string"))
		{
			token = COM_Parse(&s);
			index = (int)strtol(token, (char **)NULL, 10);

			if ((index < 0) || (index >= MAX_STATS))
			{
				Com_Error(ERR_DROP, "Bad stat_string index");
			}

			index = cl.frame.playerstate.stats[index];

			if ((index < 0) || (index >= MAX_CONFIGSTRINGS))
			{
				Com_Error(ERR_DROP, "Bad stat_string index");
			}

			DrawStringScaled(x, y, cl.configstrings[index], scale);
			continue;
		}

		if (!strcmp(token, "cstring"))
		{
			token = COM_Parse(&s);
			DrawHUDStringScaled(token, x, y, 320, 0, scale); // FIXME: or scale 320 here?
			continue;
		}

		if (!strcmp(token, "string"))
		{
			token = COM_Parse(&s);
			DrawStringScaled(x, y, token, scale);
			continue;
		}

		if (!strcmp(token, "cstring2"))
		{
			token = COM_Parse(&s);
			DrawHUDStringScaled(token, x, y, 320, 0x80, scale); // FIXME: or scale 320 here?
			continue;
		}

		if (!strcmp(token, "string2"))
		{
			token = COM_Parse(&s);
			DrawAltStringScaled(x, y, token, scale);
			continue;
		}

		if (!strcmp(token, "if"))
		{
			/* draw a number */
			token = COM_Parse(&s);
			value = cl.frame.playerstate.stats[(int)strtol(token, (char **)NULL, 10)];

			if (!value)
			{
				/* skip to endif */
				while (s && strcmp(token, "endif"))
				{
					token = COM_Parse(&s);
				}
			}

			continue;
		}
	}
}

/*
 * The status bar is a small layout program that
 * is based on the stats array
 */
void
SCR_DrawStats(void)
{
	SCR_ExecuteLayoutString(cl.configstrings[CS_STATUSBAR]);
}

#define STAT_LAYOUTS 13

void
SCR_DrawLayout(void)
{
	if (!cl.frame.playerstate.stats[STAT_LAYOUTS])
	{
		return;
	}

	SCR_ExecuteLayoutString(cl.layout);
}


inline unsigned long sg_rdtscp(void) {
    unsigned int lo, hi;
    __asm__ volatile("rdtscp" : "=a" (lo), "=d" (hi)::"memory");
    return ((unsigned long)hi << 32) | lo;
}
// ----

#define FPS_LOG_INTERVAL_USEC (60ll * 1000ll * 1000ll)
#define FPS_STATS_INITIAL_CAPACITY 4096

typedef struct
{
	int *frame_times;
	int count;
	int capacity;
	long long total_frame_time;
	unsigned long long total_cycles;
	int min_frame_time;
	int max_frame_time;
	unsigned long min_cycles;
	unsigned long max_cycles;
} fps_stats_t;

static int
SCR_FrameTimeCompareDescending(const void *a, const void *b)
{
	int frame_time_a = *(const int *)a;
	int frame_time_b = *(const int *)b;

	if (frame_time_a < frame_time_b)
	{
		return 1;
	}

	if (frame_time_a > frame_time_b)
	{
		return -1;
	}

	return 0;
}

static void
SCR_ResetFPSStats(fps_stats_t *stats)
{
	stats->count = 0;
	stats->total_frame_time = 0;
	stats->total_cycles = 0;
	stats->min_frame_time = 0;
	stats->max_frame_time = 0;
	stats->min_cycles = 0;
	stats->max_cycles = 0;
}

static void
SCR_EnsureFPSStatsCapacity(fps_stats_t *stats)
{
	int new_capacity;
	int *new_frame_times;

	if (stats->count < stats->capacity)
	{
		return;
	}

	if (stats->capacity > (INT_MAX / 2))
	{
		Com_Error(ERR_FATAL, "%s: frame stats buffer overflow", __func__);
	}

	new_capacity = stats->capacity ? stats->capacity * 2 : FPS_STATS_INITIAL_CAPACITY;

	if (new_capacity > (INT_MAX / (int)sizeof(*new_frame_times)))
	{
		Com_Error(ERR_FATAL, "%s: frame stats buffer overflow", __func__);
	}

	new_frame_times = Z_Malloc(new_capacity * (int)sizeof(*new_frame_times));

	if (stats->frame_times)
	{
		memcpy(new_frame_times, stats->frame_times,
				stats->count * sizeof(*new_frame_times));
		Z_Free(stats->frame_times);
	}

	stats->frame_times = new_frame_times;
	stats->capacity = new_capacity;
}

static void
SCR_AddFPSStats(fps_stats_t *stats, int frame_time, unsigned long frame_cycles)
{
	SCR_EnsureFPSStatsCapacity(stats);

	if (!stats->count)
	{
		stats->min_frame_time = frame_time;
		stats->max_frame_time = frame_time;
		stats->min_cycles = frame_cycles;
		stats->max_cycles = frame_cycles;
	}
	else
	{
		if (frame_time < stats->min_frame_time)
		{
			stats->min_frame_time = frame_time;
		}

		if (frame_time > stats->max_frame_time)
		{
			stats->max_frame_time = frame_time;
		}

		if (frame_cycles < stats->min_cycles)
		{
			stats->min_cycles = frame_cycles;
		}

		if (frame_cycles > stats->max_cycles)
		{
			stats->max_cycles = frame_cycles;
		}
	}

	stats->frame_times[stats->count++] = frame_time;
	stats->total_frame_time += frame_time;
	stats->total_cycles += frame_cycles;
}

static double
SCR_CalcOnePercentLowFPS(fps_stats_t *stats)
{
	int slow_frames;
	long long slow_frame_time = 0;
	int i;

	if (!stats->count)
	{
		return 0.0;
	}

	qsort(stats->frame_times, stats->count, sizeof(*stats->frame_times),
			SCR_FrameTimeCompareDescending);

	slow_frames = (stats->count + 99) / 100;

	for (i = 0; i < slow_frames; i++)
	{
		slow_frame_time += stats->frame_times[i];
	}

	if (slow_frame_time <= 0)
	{
		return 0.0;
	}

	return (double)slow_frames * 1000000.0 / (double)slow_frame_time;
}

static void
SCR_PrintFPSStats(const char *title, fps_stats_t *stats)
{
	double avg_frame_time;
	double avg_fps;
	double one_percent_low;
	double min_fps;
	unsigned long avg_cycles;

	if (!stats->count)
	{
		return;
	}

	avg_frame_time = (double)stats->total_frame_time / (double)stats->count;
	avg_fps = 1000000.0 / avg_frame_time;
	one_percent_low = SCR_CalcOnePercentLowFPS(stats);
	min_fps = 1000000.0 / (double)stats->max_frame_time;
	avg_cycles = (unsigned long)(stats->total_cycles / (unsigned long long)stats->count);

	printf("%s\n", title);
	printf("FPS: avg %.2f, 1%% low %.2f, min %.2f\n",
			avg_fps, one_percent_low, min_fps);
	printf("Frame time: avg %.3f ms, max %.3f ms, min %.3f ms\n",
			avg_frame_time / 1000.0,
			(double)stats->max_frame_time / 1000.0,
			(double)stats->min_frame_time / 1000.0);
	printf("Cycles: avg %lu, max %lu, min %lu\n",
			avg_cycles, stats->max_cycles, stats->min_cycles);
}

void
SCR_Framecounter(void) {
	long long newtime;
	unsigned long newtime_cycles;
	static long long oldtime;
	static unsigned long oldtime_cycles;
	static long long log_start_time;
	static long long last_log_time;
	static qboolean initialized = false;
	static fps_stats_t minute_stats;
	static fps_stats_t lifetime_stats;
	int frame_time;
	unsigned long frame_cycles;
	float scale;

	cl_showfps->value = 3;

	/* skip statistics without show fps */
	if (cl_showfps->value < 1)
		return;

	newtime = Sys_Microseconds();
	newtime_cycles = sg_rdtscp();

	if (!initialized)
	{
		oldtime = newtime;
		oldtime_cycles = newtime_cycles;
		log_start_time = newtime;
		last_log_time = newtime;
		initialized = true;
		return;
	}

	if (((newtime - oldtime) <= 0) || ((newtime - oldtime) > INT_MAX))
	{
		oldtime = newtime;
		oldtime_cycles = newtime_cycles;
		return;
	}

	frame_time = (int)(newtime - oldtime);
	frame_cycles = newtime_cycles - oldtime_cycles;
	oldtime = newtime;
	oldtime_cycles = newtime_cycles;

	SCR_AddFPSStats(&minute_stats, frame_time, frame_cycles);
	SCR_AddFPSStats(&lifetime_stats, frame_time, frame_cycles);

	scale = SCR_GetConsoleScale();

	if (cl_showfps->value == 1) {
		char str[16];
		snprintf(str, sizeof(str), "%3.2ffps", 1000000.0 / (double)frame_time);
		DrawStringScaled(viddef.width - scale*(strlen(str)*8 + 2), 0, str, scale);
		SCR_AddDirtyPoint(viddef.width - scale*(strlen(str)*8 + 2), 0);
		SCR_AddDirtyPoint(viddef.width, 0);
	} else if (cl_showfps->value >= 2) {
		char str[128];
		snprintf(str, sizeof(str), "Avg: %7.2ffps, Min: %7.2ffps",
				1000000.0 / ((double)minute_stats.total_frame_time / (double)minute_stats.count),
				1000000.0 / (double)minute_stats.max_frame_time);
		DrawStringScaled(viddef.width - scale*(strlen(str)*8 + 2), 0, str, scale);
		SCR_AddDirtyPoint(viddef.width - scale*(strlen(str)*8 + 2), 0);
		SCR_AddDirtyPoint(viddef.width, 0);

		if (cl_showfps->value > 2)
		{
			snprintf(str, sizeof(str), "Max: %5.3fms, Min: %5.3fms, Avg: %5.3fms",
					(double)minute_stats.max_frame_time / 1000.0,
					(double)minute_stats.min_frame_time / 1000.0,
					((double)minute_stats.total_frame_time / (double)minute_stats.count) / 1000.0);
			DrawStringScaled(viddef.width - scale*(strlen(str)*8 + 2), scale*10, str, scale);
			SCR_AddDirtyPoint(viddef.width - scale*(strlen(str)*8 + 2), scale*10);
			SCR_AddDirtyPoint(viddef.width, scale+10);
		}
	}

	if ((newtime - last_log_time) >= FPS_LOG_INTERVAL_USEC)
	{
		char title[64];
		int lifetime_minutes = (int)((newtime - log_start_time) / FPS_LOG_INTERVAL_USEC);

		if (lifetime_minutes < 1)
		{
			lifetime_minutes = 1;
		}

		SCR_PrintFPSStats("STATS FOR LAST MINUTE", &minute_stats);
		printf("\n");
		snprintf(title, sizeof(title), "STATS FOR LIFETIME (%i MINUTES)", lifetime_minutes);
		SCR_PrintFPSStats(title, &lifetime_stats);
		printf("\n");
		fflush(stdout);

		SCR_ResetFPSStats(&minute_stats);
		last_log_time = newtime;
	}
}

// ----
/*
 * This is called every frame, and can also be called
 * explicitly to flush text to the screen.
 */
void
SCR_UpdateScreen(void)
{
	int numframes;
	int i;
	float separation[2] = {0, 0};
	float scale = SCR_GetMenuScale();

	/* if the screen is disabled (loading plaque is
	   up, or vid mode changing) do nothing at all */
	if (cls.disable_screen)
	{
		if (Sys_Milliseconds() - cls.disable_screen > 120000)
		{
			cls.disable_screen = 0;
			Com_Printf("Loading plaque timed out.\n");
		}

		return;
	}

	if (!scr_initialized || !con.initialized)
	{
		return; /* not initialized yet */
	}

	if ( gl1_stereo->value )
	{
		numframes = 2;
		separation[0] = -gl1_stereo_separation->value / 2;
		separation[1] = +gl1_stereo_separation->value / 2;
	}
	else
	{
		separation[0] = 0;
		separation[1] = 0;
		numframes = 1;
	}

	unsigned long fb_start_time = sg_rdtscp();
	for (i = 0; i < numframes; i++)
	{
		R_BeginFrame(separation[i]);

		if (scr_draw_loading == 2)
		{
			/* loading plaque over black screen */
			int w, h;

			R_EndWorldRenderpass();
			if(i == 0){
				R_SetPalette(NULL);
			}

			if(i == numframes - 1){
				scr_draw_loading = false;
			}

			Draw_GetPicSize(&w, &h, "loading");
			Draw_PicScaled((viddef.width - w * scale) / 2, (viddef.height - h * scale) / 2, "loading", scale);
		}

		/* if a cinematic is supposed to be running,
		   handle menus and console specially */
		else if (cl.cinematictime > 0)
		{
			if (cls.key_dest == key_menu)
			{
				if (cl.cinematicpalette_active)
				{
					R_SetPalette(NULL);
					cl.cinematicpalette_active = false;
				}

				R_EndWorldRenderpass();
				M_Draw();
			}
			else if (cls.key_dest == key_console)
			{
				if (cl.cinematicpalette_active)
				{
					R_SetPalette(NULL);
					cl.cinematicpalette_active = false;
				}

				R_EndWorldRenderpass();
				SCR_DrawConsole();
			}
			else
			{
				R_EndWorldRenderpass();
				SCR_DrawCinematic();
			}
		}
		else
		{
			/* make sure the game palette is active */
			if (cl.cinematicpalette_active)
			{
				R_SetPalette(NULL);
				cl.cinematicpalette_active = false;
			}

			/* do 3D refresh drawing, and then update the screen */
			SCR_CalcVrect();

			/* clear any dirty part of the background */
			SCR_TileClear();

			V_RenderView(separation[i]);

			SCR_DrawStats();

			if (cl.frame.playerstate.stats[STAT_LAYOUTS] & 1)
			{
				SCR_DrawLayout();
			}

			if (cl.frame.playerstate.stats[STAT_LAYOUTS] & 2)
			{
				CL_DrawInventory();
			}

			SCR_DrawNet();
			SCR_CheckDrawCenterString();

			if (scr_timegraph->value)
			{
				SCR_DebugGraph(cls.rframetime * 300, 0);
			}

			if (scr_debuggraph->value || scr_timegraph->value ||
				scr_netgraph->value)
			{
				SCR_DrawDebugGraph();
			}

			SCR_DrawPause();

			SCR_DrawConsole();

			M_Draw();

			SCR_DrawLoading();
		}
	}

	unsigned long fb_end_time = sg_rdtscp();
	cumulative_render_time += (fb_end_time - fb_start_time);
	num_frames++;

	SCR_Framecounter();
	fb_start_time = sg_rdtscp();
	R_EndFrame();
	fb_end_time = sg_rdtscp();
	cumulative_display_time += (fb_end_time - fb_start_time);
	//printf("-------------------Avg. render time: %lld; Avg. display time: %lld; Num Frames in Total: %lld---------------------------------------\n", (cumulative_render_time/num_frames), (cumulative_display_time/num_frames), num_frames);
	/* if(num_frames % 1000 == 0){
		printf("-------------------Avg. render time: %ld; Avg. display time: %ld; Num Frames in Total: %d---------------------------------------\n", (cumulative_render_time/num_frames), (cumulative_display_time/num_frames), num_frames);
	} */
}

static float
SCR_ClampScale(float scale)
{
	float f;

	f = viddef.width / 320.0f;
	if (scale > f)
	{
		scale = f;
	}

	f = viddef.height / 240.0f;
	if (scale > f)
	{
		scale = f;
	}

	if (scale < 1)
	{
		scale = 1;
	}

	return scale;
}

static float
SCR_GetDefaultScale(void)
{
	int i = viddef.width / 640;
	int j = viddef.height / 240;

	if (i > j)
	{
		i = j;
	}
	if (i < 1)
	{
		i = 1;
	}

	return i;
}

void
SCR_DrawCrosshair(void)
{
	float scale;

	if (!crosshair->value)
	{
		return;
	}

	if (crosshair->modified)
	{
		crosshair->modified = false;
		SCR_TouchPics();
	}

	if (!crosshair_pic[0])
	{
		return;
	}

	if (crosshair_scale->value < 0)
	{
		scale = SCR_GetDefaultScale();
	}
	else
	{
		scale = SCR_ClampScale(crosshair_scale->value);
	}

	Draw_PicScaled(scr_vrect.x + (scr_vrect.width - crosshair_width * scale) / 2,
			scr_vrect.y + (scr_vrect.height - crosshair_height * scale) / 2,
			crosshair_pic, scale);
}

float
SCR_GetHUDScale(void)
{
	float scale;

	if (!scr_initialized)
	{
		scale = 1;
	}
	else if (r_hudscale->value < 0)
	{
		scale = SCR_GetDefaultScale();
	}
	else if (r_hudscale->value == 0) /* HACK: allow scale 0 to hide the HUD */
	{
		scale = 0;
	}
	else
	{
		scale = SCR_ClampScale(r_hudscale->value);
	}

	return scale;
}

float
SCR_GetConsoleScale(void)
{
	float scale;

	if (!scr_initialized)
	{
		scale = 1;
	}
	else if (r_consolescale->value < 0)
	{
		scale = SCR_GetDefaultScale();
	}
	else
	{
		scale = SCR_ClampScale(r_consolescale->value);
	}

	return scale;
}

float
SCR_GetMenuScale(void)
{
	float scale;

	if (!scr_initialized)
	{
		scale = 1;
	}
	else if (r_menuscale->value < 0)
	{
		scale = SCR_GetDefaultScale();
	}
	else
	{
		scale = SCR_ClampScale(r_menuscale->value);
	}

	return scale;
}
