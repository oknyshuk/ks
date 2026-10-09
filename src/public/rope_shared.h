#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Shared definitions for rope.
inline constexpr auto ROPE_MAX_SEGMENTS = 10;
inline constexpr auto ROPE_TYPE1_NUMSEGMENTS = 4;
inline constexpr auto ROPE_TYPE2_NUMSEGMENTS = 2;

// Default rope gravity vector.
#define ROPE_GRAVITY			0, 0, -1500


// Rope flags.
inline constexpr auto ROPE_RESIZE = (1<<0); // Try to keep the rope dangling the same amount
									// even as the rope length changes.
inline constexpr auto ROPE_BARBED = (1<<1); // Hack option to draw like a barbed wire.
inline constexpr auto ROPE_COLLIDE = (1<<2); // Collide with the world?
inline constexpr auto ROPE_SIMULATE = (1<<3); // Is the rope valid?
inline constexpr auto ROPE_BREAKABLE = (1<<4); // Can the endpoints detach?
inline constexpr auto ROPE_USE_WIND = (1<<5); // Wind simulation on this rope.
inline constexpr auto ROPE_INITIAL_HANG = (1<<6); // By default, ropes will simulate for a bit internally when they
											// are created so they sag, but dynamically created ropes for things
											// like harpoons don't want this.
inline constexpr auto ROPE_PLAYER_WPN_ATTACH = (1<<7); // If this flag is set, then the second attachment must be a player.
											// The rope will attach to "buff_attach" on the player's active weapon.
											// (This is a flag because it requires special code on the client to
											// find the weapon).
inline constexpr auto ROPE_NO_GRAVITY = (1<<8); // Disable gravity on this rope.
inline constexpr auto ROPE_TONGUE_ATTACH = (1<<9); // Use the special rules unique to a tongue rope, without cutting open the class to inherit one function.
inline constexpr auto ROPE_NUMFLAGS = 10;


// This is added to all rope slacks so when a level designer enters a 
// slack of zero in the entity, it doesn't dangle so low.
inline constexpr auto ROPESLACK_FUDGEFACTOR = -100;


// Rope shader IDs.
inline constexpr auto ROPESHADER_BLACKCABLE = 0;
inline constexpr auto ROPESHADER_ROPE = 1;
inline constexpr auto ROPESHADER_CHAIN = 2;


// Rope locked points
enum
{
	ROPE_LOCK_START_POINT = 0x1,
	ROPE_LOCK_END_POINT = 0x2,
	ROPE_LOCK_START_DIRECTION = 0x4,
	ROPE_LOCK_END_DIRECTION = 0x8,
};


// Rope attachment points.
inline constexpr auto ROPE_ATTACHMENT_START_POINT = 1;
inline constexpr auto ROPE_ATTACHMENT_END_POINT = 2;
