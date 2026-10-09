#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// NOTE: If you add a tex type, be sure to modify the s_pImpactEffect
// array in fx_impact.cpp to get an effect when that surface is shot.
inline constexpr auto CHAR_TEX_ANTLION = 'A';
inline constexpr auto CHAR_TEX_BLOODYFLESH = 'B';
inline constexpr auto CHAR_TEX_CONCRETE = 'C';
inline constexpr auto CHAR_TEX_DIRT = 'D';
inline constexpr auto CHAR_TEX_EGGSHELL = 'E'; ///< the egg sacs in the tunnels in ep2.
inline constexpr auto CHAR_TEX_FLESH = 'F';
inline constexpr auto CHAR_TEX_GRATE = 'G';
inline constexpr auto CHAR_TEX_ALIENFLESH = 'H';
inline constexpr auto CHAR_TEX_CLIP = 'I';
inline constexpr auto CHAR_TEX_GRASS = 'J'; // L4D addition
inline constexpr auto CHAR_TEX_SNOW = 'K';
inline constexpr auto CHAR_TEX_PLASTIC = 'L';
inline constexpr auto CHAR_TEX_METAL = 'M';
inline constexpr auto CHAR_TEX_SAND = 'N';
inline constexpr auto CHAR_TEX_FOLIAGE = 'O';
inline constexpr auto CHAR_TEX_COMPUTER = 'P';
inline constexpr auto CHAR_TEX_ASPHALT = 'Q'; // L4D addition
inline constexpr auto CHAR_TEX_BRICK = 'R'; // L4D addition
inline constexpr auto CHAR_TEX_SLOSH = 'S';
inline constexpr auto CHAR_TEX_TILE = 'T';
inline constexpr auto CHAR_TEX_CARDBOARD = 'U'; // L4D addition
inline constexpr auto CHAR_TEX_VENT = 'V';
inline constexpr auto CHAR_TEX_WOOD = 'W';
//#define CHAR_TEX_UNUSED		'X' ///< do not use - "fake" materials use this (ladders, wading, clips, etc)
inline constexpr auto CHAR_TEX_GLASS = 'Y';
inline constexpr auto CHAR_TEX_WARPSHIELD = 'Z'; ///< wierd-looking jello effect for advisor shield.

inline constexpr auto FIRST_CHAR_TEX = CHAR_TEX_ANTLION;
inline constexpr auto LAST_CHAR_TEX = CHAR_TEX_WARPSHIELD;

inline constexpr auto CHAR_TEX_CLAY = 1; // L4D addition
inline constexpr auto CHAR_TEX_PLASTER = 2; // L4D addition
inline constexpr auto CHAR_TEX_ROCK = 3; // L4D addition
inline constexpr auto CHAR_TEX_RUBBER = 4; // L4D addition
inline constexpr auto CHAR_TEX_SHEETROCK = 5; // L4D addition
inline constexpr auto CHAR_TEX_CLOTH = 6; // L4D addition
inline constexpr auto CHAR_TEX_CARPET = 7; // L4D addition
inline constexpr auto CHAR_TEX_PAPER = 8; // L4D addition
inline constexpr auto CHAR_TEX_UPHOLSTERY = 9; // L4D addition
inline constexpr auto CHAR_TEX_PUDDLE = 10; // L4D addition
inline constexpr auto CHAR_TEX_MUD = 11; // L4D addition

inline constexpr auto CHAR_TEX_SANDBARREL = 12; // CSGO addition (no penetration) - sand filled barrel

inline constexpr auto FIRST_L4D_CHAR_TEX = CHAR_TEX_CLAY;
inline constexpr auto LAST_L4D_CHAR_TEX = CHAR_TEX_SANDBARREL;

class IDecalEmitterSystem
{
public:
	virtual int	GetDecalIndexForName( char const *decalname ) = 0;
	virtual char const *TranslateDecalForGameMaterial( char const *decalName, unsigned char gamematerial ) = 0;
};

extern IDecalEmitterSystem *decalsystem;
