#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// contents flags are seperate bits
// a given brush can contribute multiple content bits
// multiple brushes can be in a single leaf

// these definitions also need to be in q_shared.h!

// lower bits are stronger, and will eat weaker brushes completely
inline constexpr auto CONTENTS_EMPTY = 0; // No contents

inline constexpr auto CONTENTS_SOLID = 0x1; // an eye is never valid in a solid
inline constexpr auto CONTENTS_WINDOW = 0x2; // translucent, but not watery (glass)
inline constexpr auto CONTENTS_AUX = 0x4;
inline constexpr auto CONTENTS_GRATE = 0x8; // alpha-tested "grate" textures.  Bullets/sight pass through, but solids don't
inline constexpr auto CONTENTS_SLIME = 0x10;
inline constexpr auto CONTENTS_WATER = 0x20;
inline constexpr auto CONTENTS_BLOCKLOS = 0x40; // block AI line of sight
inline constexpr auto CONTENTS_OPAQUE = 0x80; // things that cannot be seen through (may be non-solid though)
inline constexpr auto LAST_VISIBLE_CONTENTS = CONTENTS_OPAQUE;

inline constexpr auto ALL_VISIBLE_CONTENTS = (LAST_VISIBLE_CONTENTS | (LAST_VISIBLE_CONTENTS-1));

inline constexpr auto CONTENTS_TESTFOGVOLUME = 0x100;
inline constexpr auto CONTENTS_UNUSED = 0x200;

// unused 
// NOTE: If it's visible, grab from the top + update LAST_VISIBLE_CONTENTS
// if not visible, then grab from the bottom.
// CONTENTS_OPAQUE + SURF_NODRAW count as CONTENTS_OPAQUE (shadow-casting toolsblocklight textures)
inline constexpr auto CONTENTS_BLOCKLIGHT = 0x400;

inline constexpr auto CONTENTS_TEAM1 = 0x800; // per team contents used to differentiate collisions
inline constexpr auto CONTENTS_TEAM2 = 0x1000; // between players and objects on different teams

// ignore CONTENTS_OPAQUE on surfaces that have SURF_NODRAW
inline constexpr auto CONTENTS_IGNORE_NODRAW_OPAQUE = 0x2000;

// hits entities which are MOVETYPE_PUSH (doors, plats, etc.)
inline constexpr auto CONTENTS_MOVEABLE = 0x4000;

// remaining contents are non-visible, and don't eat brushes
inline constexpr auto CONTENTS_AREAPORTAL = 0x8000;

inline constexpr auto CONTENTS_PLAYERCLIP = 0x10000;
inline constexpr auto CONTENTS_MONSTERCLIP = 0x20000;

inline constexpr auto CONTENTS_BRUSH_PAINT = 0x40000;
inline constexpr auto CONTENTS_GRENADECLIP = 0x80000;
inline constexpr auto CONTENTS_UNUSED2 = 0x100000;
inline constexpr auto CONTENTS_UNUSED3 = 0x200000;
inline constexpr auto CONTENTS_UNUSED4 = 0x400000;
inline constexpr auto CONTENTS_UNUSED5 = 0x800000;

inline constexpr auto CONTENTS_ORIGIN = 0x1000000; // removed before bsping an entity

inline constexpr auto CONTENTS_MONSTER = 0x2000000; // should never be on a brush, only in game
inline constexpr auto CONTENTS_DEBRIS = 0x4000000;
inline constexpr auto CONTENTS_DETAIL = 0x8000000; // brushes to be added after vis leafs
inline constexpr auto CONTENTS_TRANSLUCENT = 0x10000000; // auto set if any surface has trans
inline constexpr auto CONTENTS_LADDER = 0x20000000;
inline constexpr auto CONTENTS_HITBOX = 0x40000000; // use accurate hitboxes on trace

// NOTE: These are stored in a short in the engine now.  Don't use more than 16 bits
inline constexpr auto SURF_LIGHT = 0x0001; // value will hold the light strength
inline constexpr auto SURF_SKY2D = 0x0002; // don't draw, indicates we should skylight + draw 2d sky but not draw the 3D skybox
inline constexpr auto SURF_SKY = 0x0004; // don't draw, but add to skybox
inline constexpr auto SURF_WARP = 0x0008; // turbulent water warp
inline constexpr auto SURF_TRANS = 0x0010;
inline constexpr auto SURF_NOPORTAL = 0x0020; // the surface can not have a portal placed on it
inline constexpr auto SURF_TRIGGER = 0x0040; // FIXME: This is an xbox hack to work around elimination of trigger surfaces, which breaks occluders
inline constexpr auto SURF_NODRAW = 0x0080; // don't bother referencing the texture

inline constexpr auto SURF_HINT = 0x0100; // make a primary bsp splitter

inline constexpr auto SURF_SKIP = 0x0200; // completely ignore, allowing non-closed brushes
inline constexpr auto SURF_NOLIGHT = 0x0400; // Don't calculate light
inline constexpr auto SURF_BUMPLIGHT = 0x0800; // calculate three lightmaps for the surface for bumpmapping
inline constexpr auto SURF_NOSHADOWS = 0x1000; // Don't receive shadows
inline constexpr auto SURF_NODECALS = 0x2000; // Don't receive decals
inline constexpr auto SURF_NOPAINT = SURF_NODECALS;	// the surface can not have paint placed on it
inline constexpr auto SURF_NOCHOP = 0x4000; // Don't subdivide patches on this surface
inline constexpr auto SURF_HITBOX = 0x8000; // surface is part of a hitbox



// -----------------------------------------------------
// spatial content masks - used for spatial queries (traceline,etc.)
// -----------------------------------------------------
inline constexpr auto MASK_ALL = (0xFFFFFFFF);
// everything that is normally solid
inline constexpr auto MASK_SOLID = (CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_MONSTER|CONTENTS_GRATE);
// everything that blocks player movement
inline constexpr auto MASK_PLAYERSOLID = (CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER|CONTENTS_GRATE);
// blocks npc movement
inline constexpr auto MASK_NPCSOLID = (CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_MONSTERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER|CONTENTS_GRATE);
// blocks fluid movement
inline constexpr auto MASK_NPCFLUID = (CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_MONSTERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER);
// water physics in these contents
inline constexpr auto MASK_WATER = (CONTENTS_WATER|CONTENTS_MOVEABLE|CONTENTS_SLIME);
// everything that blocks lighting
inline constexpr auto MASK_OPAQUE = (CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_OPAQUE);
// everything that blocks lighting, but with monsters added.
inline constexpr auto MASK_OPAQUE_AND_NPCS = (MASK_OPAQUE|CONTENTS_MONSTER);
// everything that blocks line of sight for AI
inline constexpr auto MASK_BLOCKLOS = (CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_BLOCKLOS);
// everything that blocks line of sight for AI plus NPCs
inline constexpr auto MASK_BLOCKLOS_AND_NPCS = (MASK_BLOCKLOS|CONTENTS_MONSTER);
// everything that blocks line of sight for players
inline constexpr auto MASK_VISIBLE = (MASK_OPAQUE|CONTENTS_IGNORE_NODRAW_OPAQUE);
// everything that blocks line of sight for players, but with monsters added.
inline constexpr auto MASK_VISIBLE_AND_NPCS = (MASK_OPAQUE_AND_NPCS|CONTENTS_IGNORE_NODRAW_OPAQUE);
// bullets see these as solid
inline constexpr auto MASK_SHOT = (CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_MONSTER|CONTENTS_WINDOW|CONTENTS_DEBRIS|CONTENTS_HITBOX);
// for finding floor height
inline constexpr auto MASK_FLOORTRACE = (CONTENTS_SOLID|CONTENTS_MOVEABLE|                 CONTENTS_WINDOW|CONTENTS_DEBRIS);
// for csgo weapon clipping
inline constexpr auto MASK_WEAPONCLIPPING = (CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_MONSTER|CONTENTS_WINDOW|CONTENTS_DEBRIS);
// bullets see these as solid, except monsters (world+brush only)
inline constexpr auto MASK_SHOT_BRUSHONLY = (CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_DEBRIS);
// non-raycasted weapons see this as solid (includes grates)
inline constexpr auto MASK_SHOT_HULL = (CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_MONSTER|CONTENTS_WINDOW|CONTENTS_DEBRIS|CONTENTS_GRATE);
// hits solids (not grates) and passes through everything else
inline constexpr auto MASK_SHOT_PORTAL = (CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_MONSTER);
// everything normally solid, except monsters (world+brush only)
inline constexpr auto MASK_SOLID_BRUSHONLY = (CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_GRATE);
// everything normally solid for player movement, except monsters (world+brush only)
inline constexpr auto MASK_PLAYERSOLID_BRUSHONLY = (CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_PLAYERCLIP|CONTENTS_GRATE);
// everything normally solid for npc movement, except monsters (world+brush only)
inline constexpr auto MASK_NPCSOLID_BRUSHONLY = (CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_MONSTERCLIP|CONTENTS_GRATE);
// just the world, used for route rebuilding
inline constexpr auto MASK_NPCWORLDSTATIC = (CONTENTS_SOLID|CONTENTS_WINDOW|CONTENTS_MONSTERCLIP|CONTENTS_GRATE);
// just the world, used for route rebuilding
inline constexpr auto MASK_NPCWORLDSTATIC_FLUID = (CONTENTS_SOLID|CONTENTS_WINDOW|CONTENTS_MONSTERCLIP);
// These are things that can split areaportals
inline constexpr auto MASK_SPLITAREAPORTAL = (CONTENTS_WATER|CONTENTS_SLIME);

// everything that blocks corpse movement
// UNDONE: Not used yet / may be deleted
inline constexpr auto MASK_DEADSOLID = (CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_GRATE);
