//==========================================================================
// Mouse Injector for Dolphin
//==========================================================================
// Copyright (C) 2019-2020 Carnivorous
// All rights reserved.
//
// Mouse Injector is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
// for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, visit http://www.gnu.org/licenses/gpl-2.0.html
//==========================================================================
#include <stdint.h>
#include "../main.h"
#include "../memory.h"
#include "../mouse.h"
#include "game.h"

#define MAPMAKERXMIN 0x00400000 // mapmaker cursor limits
#define MAPMAKERYMIN 0x00200000
#define MAPMAKERXMAX 0x02400000
#define MAPMAKERYMAX 0x01980000
// TS2 ADDRESSES - OFFSET ADDRESSES BELOW (REQUIRES PLAYERBASE TO USE)
#define TS2_camx 0x815891E8 - 0x815890A0
#define TS2_camy 0x815891EC - 0x815890A0
#define TS2_crosshairx 0x81589958 - 0x815890A0
#define TS2_crosshairy 0x8158995C - 0x815890A0
// STATIC ADDRESSES BELOW
#define TS2_playerbase 0x804686CC // playable character pointer
#define TS2_player1base_coop 0x807FFC28 // first playable character pointer in split-screen coop
#define TS2_player2base 0x807FFEC8 // second playable character pointer in split-screen coop
#define TS2_fov 0x8046818C
#define TS2_yaxislimit 0x804686BC
#define TS2_mapmakerx 0x803E5DF0
#define TS2_mapmakery 0x803E5DF4

static uint8_t TS2_Status(void);
static void TS2_Inject(void);
static void TS2_InjectPlayer(const uint32_t playerbase, const int32_t xinput, const int32_t yinput, const float looksensitivity, const float crosshairsensitivity, const float fov, const float yaxislimit);
static uint8_t TS2_IsValidPlayerbase(const uint32_t playerbase);

static const GAMEDRIVER GAMEDRIVER_INTERFACE =
{
	"TimeSplitters 2",
	TS2_Status,
	TS2_Inject,
	1, // 1000 Hz tickrate
	1 // crosshair sway supported for driver
};

const GAMEDRIVER *GAME_TS2 = &GAMEDRIVER_INTERFACE;

//==========================================================================
// Purpose: return 1 if game is detected
//==========================================================================
static uint8_t TS2_Status(void)
{
	return (MEM_ReadUInt(0x80000000) == 0x47545345U && MEM_ReadUInt(0x80000004) == 0x34460000U); // check game header to see if it matches TS2
}
//==========================================================================
// Purpose: calculate mouse look and inject into current game
//==========================================================================
static void TS2_Inject(void)
{
	if(MEM_ReadUInt(0x8046DF70) == 0x3F6AAAABU && MEM_ReadUInt(0x8046CE94) == 0x3F6AAAABU) // force 16:9 hack
	{
		MEM_WriteUInt(0x8046DF70, 0x3F9B4852);
		MEM_WriteUInt(0x8046CE94, 0x3F9B4852);
	}
	if(MEM_ReadUInt(TS2_yaxislimit) != 0x42A00000U) // overwrite y axis limit from 40 degrees (SP) or 50 degrees (MP)
		MEM_WriteFloat(TS2_yaxislimit, 80.f);
	if(xmouse == 0 && ymouse == 0) // if mouse is idle
		return;
	const float looksensitivity = (float)sensitivity / 40.f;
	const float crosshairsensitivity = ((float)crosshair / 100.f) * looksensitivity;
	const float fov = MEM_ReadFloat(TS2_fov);
	const float yaxislimit = MEM_ReadFloat(TS2_yaxislimit);
	int32_t cursorx = MEM_ReadInt(TS2_mapmakerx), cursory = MEM_ReadInt(TS2_mapmakery);
	if(cursorx >= MAPMAKERXMIN && cursorx <= MAPMAKERXMAX && cursory >= MAPMAKERYMIN && cursory <= MAPMAKERYMAX) // if in mapmaker mode
	{
		cursorx += (int)((float)xmouse * 65535.f * looksensitivity); // calculate mapmaker cursor movement
		cursory += (int)((float)ymouse * 65535.f * looksensitivity);
		MEM_WriteInt(TS2_mapmakerx, ClampInt(cursorx, MAPMAKERXMIN, MAPMAKERXMAX));
		MEM_WriteInt(TS2_mapmakery, ClampInt(cursory, MAPMAKERYMIN, MAPMAKERYMAX));
		return;
	}
	const uint32_t playerbase = MEM_ReadUInt(TS2_playerbase);
	const uint32_t player1base_coop = MEM_ReadUInt(TS2_player1base_coop);
	const uint32_t player2base = MEM_ReadUInt(TS2_player2base);
	if(!TS2_IsValidPlayerbase(playerbase)) // if playerbase is invalid
		return;
	if(TS2_IsValidPlayerbase(player1base_coop) && TS2_IsValidPlayerbase(player2base) && player1base_coop != player2base) // split-screen coop
	{
		TS2_InjectPlayer(selectedplayer ? player2base : player1base_coop, xmouse, ymouse, looksensitivity, crosshairsensitivity, fov, yaxislimit);
	}
	else
	{
		TS2_InjectPlayer(playerbase, xmouse, ymouse, looksensitivity, crosshairsensitivity, fov, yaxislimit);
	}
}
//==========================================================================
// Purpose: calculate mouse look and inject into a TS2 player structure
//==========================================================================
static void TS2_InjectPlayer(const uint32_t playerbase, const int32_t xinput, const int32_t yinput, const float looksensitivity, const float crosshairsensitivity, const float fov, const float yaxislimit)
{
	if(xinput == 0 && yinput == 0) // if mouse is idle
		return;
	float camx = MEM_ReadFloat(playerbase + TS2_camx);
	float camy = MEM_ReadFloat(playerbase + TS2_camy);
	if(camx >= 0 && camx < 360 && camy >= -yaxislimit && camy <= yaxislimit && fov > 3)
	{
		camx -= (float)xinput / 10.f * looksensitivity * (fov / 60.f); // normal calculation method for X
		camy += (float)(!invertpitch ? -yinput : yinput) / 10.f * looksensitivity * (fov / 60.f); // normal calculation method for Y
		while(camx < 0)
			camx += 360;
		while(camx >= 360)
			camx -= 360;
		camy = ClampFloat(camy, -yaxislimit, yaxislimit);
		MEM_WriteFloat(playerbase + TS2_camx, camx);
		MEM_WriteFloat(playerbase + TS2_camy, camy);
		if(crosshair) // if crosshair sway is enabled
		{
			float crosshairx = MEM_ReadFloat(playerbase + TS2_crosshairx); // after camera x and y have been calculated and injected, calculate the crosshair/gun sway
			float crosshairy = MEM_ReadFloat(playerbase + TS2_crosshairy);
			crosshairx += (float)xinput / 80.f * crosshairsensitivity * (fov / 55.f);
			crosshairy += (float)(!invertpitch ? yinput : -yinput) / 80.f * crosshairsensitivity * (fov / 55.f);
			MEM_WriteFloat(playerbase + TS2_crosshairx, ClampFloat(crosshairx, -1.f, 1.f));
			MEM_WriteFloat(playerbase + TS2_crosshairy, ClampFloat(crosshairy, -1.f, 1.f));
		}
	}
}
//==========================================================================
// Purpose: validate a GameCube RAM pointer to a player structure
//==========================================================================
static uint8_t TS2_IsValidPlayerbase(const uint32_t playerbase)
{
	return (playerbase >= 0x80000000U && playerbase <= 0x817FF000U);
}
