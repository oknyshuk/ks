#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Network data for screen shake and screen fade.

import std;
import ks.core.basetypes;
import ks.core.types;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;

//
// Commands for the screen shake effect.
enum ShakeCommand_t
{
	SHAKE_START = 0,		// Starts the screen shake for all players within the radius.
	SHAKE_STOP,				// Stops the screen shake for all players within the radius.
	SHAKE_AMPLITUDE,		// Modifies the amplitude of an active screen shake for all players within the radius.
	SHAKE_FREQUENCY,		// Modifies the frequency of an active screen shake for all players within the radius.
	SHAKE_START_RUMBLEONLY,	// Starts a shake effect that only rumbles the controller, no screen effect.
	SHAKE_START_NORUMBLE,	// Starts a shake that does NOT rumble the controller.
};

// This structure must have a working copy/assignment constructor. 
// At the time of this writing, the implicit one works properly.

struct ScreenShake_t
{
	ShakeCommand_t		command;
	float	amplitude;
	float	frequency;
	float	duration;
	Vector  direction;

	inline ScreenShake_t() : direction(0,0,0) {};
	inline ScreenShake_t( ShakeCommand_t _command, float _amplitude, float _frequency, 
		float _duration, const Vector &_direction );
};


//
// Screen shake message.
//
extern int gmsgShake;


//
// Commands for the screen tilt effect.
//

struct ScreenTilt_t
{
	int		command;
	bool	easeInOut;
	QAngle	angle;
	float	duration;
	float	time;
};

// Fade in/out
extern int gmsgFade;

inline constexpr auto FFADE_IN = 0x0001; // Just here so we don't pass 0 into the function
inline constexpr auto FFADE_OUT = 0x0002; // Fade out (not in)
inline constexpr auto FFADE_MODULATE = 0x0004; // Modulate (don't blend)
inline constexpr auto FFADE_STAYOUT = 0x0008; // ignores the duration, stays faded out until new ScreenFade message received
inline constexpr auto FFADE_PURGE = 0x0010; // Purges all other fades, replacing them with this one

inline constexpr auto SCREENFADE_FRACBITS = 9; // which leaves 16-this for the integer part
// This structure is sent over the net to describe a screen fade event
struct ScreenFade_t
{
	unsigned short 	duration;		// FIXED 16 bit, with SCREENFADE_FRACBITS fractional, seconds duration
	unsigned short 	holdTime;		// FIXED 16 bit, with SCREENFADE_FRACBITS fractional, seconds duration until reset (fade & hold)
	short			fadeFlags;		// flags
	byte			r, g, b, a;		// fade to color ( max alpha )
};


// inline funcs:

inline ScreenShake_t::ScreenShake_t( ShakeCommand_t _command, float _amplitude, float _frequency, 
									float _duration, const Vector &_direction ) :
	command(_command), amplitude(_amplitude), frequency(_frequency),
		duration(_duration), direction(_direction)
{}
