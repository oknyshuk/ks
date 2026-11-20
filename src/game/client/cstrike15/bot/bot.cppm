// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module ks.client:cstrike15.bot.bot;



// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2003

/// version number is MAJOR.MINOR
inline constexpr auto BOT_VERSION_MAJOR = 1;
inline constexpr auto BOT_VERSION_MINOR = 50;

//--------------------------------------------------------------------------------------------------------
/**
 * Difficulty levels
 */
enum BotDifficultyType
{
	BOT_EASY = 0,
	BOT_NORMAL = 1,
	BOT_HARD = 2,
	BOT_EXPERT = 3,

	NUM_DIFFICULTY_LEVELS
};

#ifdef DEFINE_DIFFICULTY_NAMES
	const char *BotDifficultyName[] =
	{
		"EASY", "NORMAL", "HARD", "EXPERT", nullptr
	};
#else
	extern const char *BotDifficultyName[];
#endif

namespace BotProfileInputDevice
{
	enum Device
	{
		GAMEPAD				= 0,
		KB_MOUSE			= 1,
		PS3_MOVE			= 2,
		HYDRA				= 3,
		SHARPSHOOTER		= 4,

		COUNT,								// Auto list counter
		FORCE_INT32  = 0x7FFFFFFF			// Force the typedef to be int32
	};
};
typedef BotProfileInputDevice::Device  BotProfileDevice_t;

// Purpose: dll-agnostic routines (no dll dependencies here)

// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------
/**
 * Returns the token parsed by SharedParse()
 */
char *SharedGetToken( void );

//--------------------------------------------------------------------------------------------------------
/**
 * Sets the character used to delimit quoted strings.  Default is '\"'.  Be sure to set it back when done.
 */
void SharedSetQuoteChar( char c );

//--------------------------------------------------------------------------------------------------------
/**
 * Parse a token out of a string
 */
const char *SharedParse( const char *data );

//--------------------------------------------------------------------------------------------------------
/**
 * Returns true if additional data is waiting to be processed on this line
 */
bool SharedTokenWaiting( const char *buffer );

//--------------------------------------------------------------------------------------------------------
/**
 * Simple utility function to allocate memory and duplicate a string
 */
/*
inline char *CloneString( const char *str )
{
	char *cloneStr = new char [ strlen(str)+1 ];
	strcpy( cloneStr, str );
	return cloneStr;
}*/

//--------------------------------------------------------------------------------------------------------
/**
 * Simple utility function to allocate memory and duplicate a wide string
 */

//--------------------------------------------------------------------------------------------------------------
/**
 *  snprintf-alike that allows multiple prints into a buffer
 */
char * BufPrintf(char *buf, int& len, const char *fmt, ...);

//--------------------------------------------------------------------------------------------------------------
/**
 *  wide char version of BufPrintf
 */

//--------------------------------------------------------------------------------------------------------------
/**
 *  convenience function that prints an int into a static wchar_t*
 */
// dgoodenough - PS3 needs this guy as well.
// PS3_BUILDFIX
//--------------------------------------------------------------------------------------------------------------
/**
 *  convenience function that prints an int into a static char*
 */
const char * NumAsString( int val );

//--------------------------------------------------------------------------------------------------------------
/**
 *  convenience function that composes a string into a static char*
 */
char * SharedVarArgs(const char *format, ...);
