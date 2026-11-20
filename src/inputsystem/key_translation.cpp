// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation




import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.strtools;
#include "key_translation.h"
import std;

static ButtonCode_t s_pVirtualKeyToButtonCode[256];


static int s_pButtonCodeToVirtual[BUTTON_CODE_LAST];

#define JOYSTICK_NAMES_BUTTONS( x )	\
	"JOY1",		\
	"JOY2",		\
	"JOY3",		\
	"JOY4",		\
	"JOY5",		\
	"JOY6",		\
	"JOY7",		\
	"JOY8",		\
	"JOY9",		\
	"JOY10",	\
	"JOY11",	\
	"JOY12",	\
	"JOY13",	\
	"JOY14",	\
	"JOY15",	\
	"JOY16",	\
	"JOY17",	\
	"JOY18",	\
	"JOY19",	\
	"JOY20",	\
	"JOY21",	\
	"JOY22",	\
	"JOY23",	\
	"JOY24",	\
	"JOY25",	\
	"JOY26",	\
	"JOY27",	\
	"JOY28",	\
	"JOY29",	\
	"JOY30",	\
	"JOY31",	\
	"JOY32"

#define JOYSTICK_NAMES_POV( x ) \
	"POV_UP",\
	"POV_RIGHT",\
	"POV_DOWN",\
	"POV_LEFT"

#define JOYSTICK_NAMES_AXIS( x ) \
	"X AXIS POS",\
	"X AXIS NEG",\
	"Y AXIS POS",\
	"Y AXIS NEG",\
	"Z AXIS POS",\
	"Z AXIS NEG",\
	"R AXIS POS",\
	"R AXIS NEG",\
	"U AXIS POS",\
	"U AXIS NEG",\
	"V AXIS POS",\
	"V AXIS NEG"

static const char *s_pButtonCodeName[ ] =
{
	"",				// KEY_NONE
	"0",			// KEY_0,
	"1",			// KEY_1,
	"2",			// KEY_2,
	"3",			// KEY_3,
	"4",			// KEY_4,
	"5",			// KEY_5,
	"6",			// KEY_6,
	"7",			// KEY_7,
	"8",			// KEY_8,
	"9",			// KEY_9,
	"a",			// KEY_A,
	"b",			// KEY_B,
	"c",			// KEY_C,
	"d",			// KEY_D,
	"e",			// KEY_E,
	"f",			// KEY_F,
	"g",			// KEY_G,
	"h",			// KEY_H,
	"i",			// KEY_I,
	"j",			// KEY_J,
	"k",			// KEY_K,
	"l",			// KEY_L,
	"m",			// KEY_M,
	"n",			// KEY_N,
	"o",			// KEY_O,
	"p",			// KEY_P,
	"q",			// KEY_Q,
	"r",			// KEY_R,
	"s",			// KEY_S,
	"t",			// KEY_T,
	"u",			// KEY_U,
	"v",			// KEY_V,
	"w",			// KEY_W,
	"x",			// KEY_X,
	"y",			// KEY_Y,
	"z",			// KEY_Z,
	"KP_INS",		// KEY_PAD_0,
	"KP_END",		// KEY_PAD_1,
	"KP_DOWNARROW",	// KEY_PAD_2,
	"KP_PGDN",		// KEY_PAD_3,
	"KP_LEFTARROW",	// KEY_PAD_4,
	"KP_5",			// KEY_PAD_5,
	"KP_RIGHTARROW",// KEY_PAD_6,
	"KP_HOME",		// KEY_PAD_7,
	"KP_UPARROW",	// KEY_PAD_8,
	"KP_PGUP",		// KEY_PAD_9,
	"KP_SLASH",		// KEY_PAD_DIVIDE,
	"KP_MULTIPLY",	// KEY_PAD_MULTIPLY,
	"KP_MINUS",		// KEY_PAD_MINUS,
	"KP_PLUS",		// KEY_PAD_PLUS,
	"KP_ENTER",		// KEY_PAD_ENTER,
	"KP_DEL",		// KEY_PAD_DECIMAL,
	"[",			// KEY_LBRACKET,
	"]",			// KEY_RBRACKET,
	"SEMICOLON",	// KEY_SEMICOLON,
	"'",			// KEY_APOSTROPHE,
	"`",			// KEY_BACKQUOTE,
	",",			// KEY_COMMA,
	".",			// KEY_PERIOD,
	"/",			// KEY_SLASH,
	"\\",			// KEY_BACKSLASH,
	"-",			// KEY_MINUS,
	"=",			// KEY_EQUAL,
	"ENTER",		// KEY_ENTER,
	"SPACE",		// KEY_SPACE,
	"BACKSPACE",	// KEY_BACKSPACE,
	"TAB",			// KEY_TAB,
	"CAPSLOCK",		// KEY_CAPSLOCK,
	"NUMLOCK",		// KEY_NUMLOCK,
	"ESCAPE",		// KEY_ESCAPE,
	"SCROLLLOCK",	// KEY_SCROLLLOCK,
	"INS",			// KEY_INSERT,
	"DEL",			// KEY_DELETE,
	"HOME",			// KEY_HOME,
	"END",			// KEY_END,
	"PGUP",			// KEY_PAGEUP,
	"PGDN",			// KEY_PAGEDOWN,
	"PAUSE",		// KEY_BREAK,
	"SHIFT",		// KEY_LSHIFT,
	"RSHIFT",		// KEY_RSHIFT,
	"ALT",			// KEY_LALT,
	"RALT",			// KEY_RALT,
	"CTRL",			// KEY_LCONTROL,
	"RCTRL",		// KEY_RCONTROL,
	"LWIN",			// KEY_LWIN,
	"RWIN",			// KEY_RWIN,
	"APP",			// KEY_APP,
	"UPARROW",		// KEY_UP,
	"LEFTARROW",	// KEY_LEFT,
	"DOWNARROW",	// KEY_DOWN,
	"RIGHTARROW",	// KEY_RIGHT,
	"F1",			// KEY_F1,
	"F2",			// KEY_F2,
	"F3",			// KEY_F3,
	"F4",			// KEY_F4,
	"F5",			// KEY_F5,
	"F6",			// KEY_F6,
	"F7",			// KEY_F7,
	"F8",			// KEY_F8,
	"F9",			// KEY_F9,
	"F10",			// KEY_F10,
	"F11",			// KEY_F11,
	"F12",			// KEY_F12,

	// FIXME: CAPSLOCK/NUMLOCK/SCROLLLOCK all appear above. What are these for?!
	// They only appear in CInputWin32::UpdateToggleButtonState in vgui2
	"CAPSLOCKTOGGLE",	// KEY_CAPSLOCKTOGGLE,
	"NUMLOCKTOGGLE",	// KEY_NUMLOCKTOGGLE,
	"SCROLLLOCKTOGGLE", // KEY_SCROLLLOCKTOGGLE,

	// Mouse
	"MOUSE1",		// MOUSE_LEFT,
	"MOUSE2",		// MOUSE_RIGHT,
	"MOUSE3",		// MOUSE_MIDDLE,
	"MOUSE4",		// MOUSE_4,
	"MOUSE5",		// MOUSE_5,

	"MWHEELUP",		// MOUSE_WHEEL_UP
	"MWHEELDOWN",	// MOUSE_WHEEL_DOWN

	JOYSTICK_NAMES_BUTTONS( 0 ),
	JOYSTICK_NAMES_BUTTONS( 1 ),
	JOYSTICK_NAMES_BUTTONS( 2 ),
	JOYSTICK_NAMES_BUTTONS( 3 ),

	JOYSTICK_NAMES_POV( 0 ),
	JOYSTICK_NAMES_POV( 1 ),
	JOYSTICK_NAMES_POV( 2 ),
	JOYSTICK_NAMES_POV( 3 ),

	JOYSTICK_NAMES_AXIS( 0 ),
	JOYSTICK_NAMES_AXIS( 1 ),
	JOYSTICK_NAMES_AXIS( 2 ),
	JOYSTICK_NAMES_AXIS( 3 ),
};

#define JOYSTICK_ANALOG( x )	\
	"X AXIS",\
	"Y AXIS",\
	"Z AXIS",\
	"R AXIS",\
	"U AXIS",\
	"V AXIS"

static const char *s_pAnalogCodeName[ ] =
{
	"MOUSE_X",		// MOUSE_X = 0,
	"MOUSE_Y",		// MOUSE_Y,
	"MOUSE_XY",		// MOUSE_XY,		// Invoked when either x or y changes
	"MOUSE_WHEEL",	// MOUSE_WHEEL,

	JOYSTICK_ANALOG( 0 ),
	JOYSTICK_ANALOG( 1 ),
	JOYSTICK_ANALOG( 2 ),
	JOYSTICK_ANALOG( 3 ),
};

#define XCONTROLLERBUTTONS_BUTTONS( x ) \
	"A_BUTTON",\
	"B_BUTTON",\
	"X_BUTTON",\
	"Y_BUTTON",\
	"L_SHOULDER",\
	"R_SHOULDER",\
	"BACK",\
	"START",\
	"STICK1",\
	"STICK2",\
	"JOY11",\
	"JOY12",\
	"JOY13",\
	"JOY14",\
	"JOY15",\
	"JOY16",\
	"JOY17",\
	"JOY18",\
	"JOY19",\
	"JOY20",\
	"JOY21",\
	"JOY22",\
	"JOY23",\
	"JOY24",\
	"JOY25",\
	"JOY26",\
	"JOY27",\
	"JOY28",\
	"JOY29",\
	"JOY30",\
	"JOY31",\
	"JOY32"

#define XCONTROLLERBUTTONS_POV( x ) \
	"UP",\
	"RIGHT",\
	"DOWN",\
	"LEFT"

#define XCONTROLLERBUTTONS_AXIS( x ) \
	"S1_RIGHT",\
	"S1_LEFT",\
	"S1_DOWN",\
	"S1_UP",\
	"L_TRIGGER",\
	"R_TRIGGER",\
	"S2_RIGHT",\
	"S2_LEFT",\
	"S2_DOWN",\
	"S2_UP",\
	"V AXIS POS",\
	"V AXIS NEG"

static const char *s_pXControllerButtonCodeNames[ ] =
{
	XCONTROLLERBUTTONS_BUTTONS( 0 ),
	XCONTROLLERBUTTONS_BUTTONS( 1 ),
	XCONTROLLERBUTTONS_BUTTONS( 2 ),
	XCONTROLLERBUTTONS_BUTTONS( 3 ),

	XCONTROLLERBUTTONS_POV( 0 ),
	XCONTROLLERBUTTONS_POV( 1 ),
	XCONTROLLERBUTTONS_POV( 2 ),
	XCONTROLLERBUTTONS_POV( 3 ),

	XCONTROLLERBUTTONS_AXIS( 0 ),
	XCONTROLLERBUTTONS_AXIS( 1 ),
	XCONTROLLERBUTTONS_AXIS( 2 ),
	XCONTROLLERBUTTONS_AXIS( 3 ),
};

// this maps non-translated keyboard scan codes to engine key codes
// Google for 'Keyboard Scan Code Specification'
static ButtonCode_t s_pScanToButtonCode_QWERTY[128] = 
{ 
	//	0				1				2				3				4				5				6				7 
	//	8				9				A				B				C				D				E				F 
	KEY_NONE,		KEY_ESCAPE,		KEY_1,			KEY_2,			KEY_3,			KEY_4,			KEY_5,			KEY_6,			// 0
	KEY_7,			KEY_8,			KEY_9,			KEY_0,			KEY_MINUS,		KEY_EQUAL,		KEY_BACKSPACE,	KEY_TAB,		// 0 

	KEY_Q,			KEY_W,			KEY_E,			KEY_R,			KEY_T,			KEY_Y,			KEY_U,			KEY_I,			// 1
	KEY_O,			KEY_P,			KEY_LBRACKET,	KEY_RBRACKET,	KEY_ENTER,		KEY_LCONTROL,	KEY_A,			KEY_S,			// 1 

	KEY_D,			KEY_F,			KEY_G,			KEY_H,			KEY_J,			KEY_K,			KEY_L,			KEY_SEMICOLON,	// 2 
	KEY_APOSTROPHE,	KEY_BACKQUOTE,	KEY_LSHIFT,		KEY_BACKSLASH,	KEY_Z,			KEY_X,			KEY_C,			KEY_V,			// 2 

	KEY_B,			KEY_N,			KEY_M,			KEY_COMMA,		KEY_PERIOD,		KEY_SLASH,		KEY_RSHIFT,		KEY_PAD_MULTIPLY,// 3
	KEY_LALT,		KEY_SPACE,		KEY_CAPSLOCK,	KEY_F1,			KEY_F2,			KEY_F3,			KEY_F4,			KEY_F5,			// 3 

	KEY_F6,			KEY_F7,			KEY_F8,			KEY_F9,			KEY_F10,		KEY_NUMLOCK,	KEY_SCROLLLOCK,	KEY_HOME,		// 4
	KEY_UP,			KEY_PAGEUP,		KEY_PAD_MINUS,	KEY_LEFT,		KEY_PAD_5,		KEY_RIGHT,		KEY_PAD_PLUS,	KEY_END,		// 4 

	KEY_DOWN,		KEY_PAGEDOWN,	KEY_INSERT,		KEY_DELETE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_F11,		// 5
	KEY_F12,		KEY_BREAK,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		// 5

	KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		// 6
	KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		// 6 

	KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		// 7
	KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE,		KEY_NONE		// 7 
};

static ButtonCode_t s_pScanToButtonCode[128];


void ButtonCode_InitKeyTranslationTable()
{
	static_assert( sizeof(s_pButtonCodeName) / sizeof( const char * ) == BUTTON_CODE_LAST );
	static_assert( sizeof(s_pAnalogCodeName) / sizeof( const char * ) == ANALOG_CODE_LAST );

// For debugging, spews entire mapping

	// set virtual key translation table
	std::memset( s_pVirtualKeyToButtonCode, KEY_NONE, sizeof(s_pVirtualKeyToButtonCode) );

	s_pVirtualKeyToButtonCode['0']			=KEY_0;
	s_pVirtualKeyToButtonCode['1']			=KEY_1;
	s_pVirtualKeyToButtonCode['2']			=KEY_2;
	s_pVirtualKeyToButtonCode['3']			=KEY_3;
	s_pVirtualKeyToButtonCode['4']			=KEY_4;
	s_pVirtualKeyToButtonCode['5']			=KEY_5;
	s_pVirtualKeyToButtonCode['6']			=KEY_6;
	s_pVirtualKeyToButtonCode['7']			=KEY_7;
	s_pVirtualKeyToButtonCode['8']			=KEY_8;
	s_pVirtualKeyToButtonCode['9']			=KEY_9;
	s_pVirtualKeyToButtonCode['A']			=KEY_A;
	s_pVirtualKeyToButtonCode['B'] 			=KEY_B;
	s_pVirtualKeyToButtonCode['C'] 			=KEY_C;
	s_pVirtualKeyToButtonCode['D'] 			=KEY_D;
	s_pVirtualKeyToButtonCode['E']			=KEY_E;
	s_pVirtualKeyToButtonCode['F']			=KEY_F;
	s_pVirtualKeyToButtonCode['G']			=KEY_G;
	s_pVirtualKeyToButtonCode['H'] 			=KEY_H;
	s_pVirtualKeyToButtonCode['I']			=KEY_I;
	s_pVirtualKeyToButtonCode['J']			=KEY_J;
	s_pVirtualKeyToButtonCode['K']			=KEY_K;
	s_pVirtualKeyToButtonCode['L']			=KEY_L;
	s_pVirtualKeyToButtonCode['M']			=KEY_M;
	s_pVirtualKeyToButtonCode['N']			=KEY_N;
	s_pVirtualKeyToButtonCode['O']			=KEY_O;
	s_pVirtualKeyToButtonCode['P']			=KEY_P;
	s_pVirtualKeyToButtonCode['Q']			=KEY_Q;
	s_pVirtualKeyToButtonCode['R']			=KEY_R;
	s_pVirtualKeyToButtonCode['S']			=KEY_S;
	s_pVirtualKeyToButtonCode['T']			=KEY_T;
	s_pVirtualKeyToButtonCode['U']			=KEY_U;
	s_pVirtualKeyToButtonCode['V']			=KEY_V;
	s_pVirtualKeyToButtonCode['W']			=KEY_W;
	s_pVirtualKeyToButtonCode['X']			=KEY_X;
	s_pVirtualKeyToButtonCode['Y']			=KEY_Y;
	s_pVirtualKeyToButtonCode['Z']			=KEY_Z;

	// init the xkey translation table

	// create reverse table engine to virtual
	for ( int i = 0; i < std::size( s_pVirtualKeyToButtonCode ); i++ )
	{
		s_pButtonCodeToVirtual[ s_pVirtualKeyToButtonCode[i] ] = i;
	}

	s_pButtonCodeToVirtual[0] = 0;
}

ButtonCode_t ButtonCode_VirtualKeyToButtonCode( int keyCode )
{
	if ( keyCode < 0 || keyCode >= sizeof( s_pVirtualKeyToButtonCode ) / sizeof( s_pVirtualKeyToButtonCode[0] ) )
	{
		contract_assert( false );
		return KEY_NONE;
	}
	return s_pVirtualKeyToButtonCode[keyCode];
}

int ButtonCode_ButtonCodeToVirtualKey( ButtonCode_t code )
{
	return s_pButtonCodeToVirtual[code];
}

ButtonCode_t ButtonCode_XKeyToButtonCode( int nPort, int keyCode )
{
	return KEY_NONE;
}

// Convert back + forth between ButtonCode/AnalogCode + strings
const char *ButtonCode_ButtonCodeToString( ButtonCode_t code, bool bXController )
{
	if ( bXController )
	{
		if ( IsJoystickButtonCode( code ) )
		{
			int offset = ( code - JOYSTICK_FIRST_BUTTON ) % JOYSTICK_MAX_BUTTON_COUNT;

			return s_pXControllerButtonCodeNames[ offset ];
		}

		if ( IsJoystickPOVCode( code ) )
		{
			int offset = ( code - JOYSTICK_FIRST_POV_BUTTON ) % JOYSTICK_POV_BUTTON_COUNT;

			return s_pXControllerButtonCodeNames[ MAX_JOYSTICKS * JOYSTICK_MAX_BUTTON_COUNT + offset ];
		}

		if ( IsJoystickAxisCode( code ) )
		{
			int offset = ( code - JOYSTICK_FIRST_AXIS_BUTTON ) % JOYSTICK_AXIS_BUTTON_COUNT;

			return s_pXControllerButtonCodeNames[ MAX_JOYSTICKS * ( JOYSTICK_POV_BUTTON_COUNT + JOYSTICK_MAX_BUTTON_COUNT ) + offset ];
		}
	}

	return s_pButtonCodeName[ code ];
}

const char *AnalogCode_AnalogCodeToString( AnalogCode_t code )
{
	return s_pAnalogCodeName[ code ];
}

ButtonCode_t ButtonCode_StringToButtonCode( const char *pString, bool bXController )
{
	if ( !pString || !pString[0] )
		return BUTTON_CODE_INVALID;

	// Backward compat for screwed up previous joystick button names
	if ( !V_strnicmp( pString, "aux", 3 ) )
	{
		int nIndex = std::atoi( &pString[3] );
		if ( nIndex < 29 )
			return JOYSTICK_BUTTON( 0, nIndex );
		if ( ( nIndex >= 29 ) && ( nIndex <= 32 ) )
			return JOYSTICK_POV_BUTTON( 0, nIndex - 29 );
		return BUTTON_CODE_INVALID;
	}

  // map "COMMAND" to the LWIN key on non-OSX
  if ( !V_stricmp( pString, "command" ) )
  {
    return KEY_LWIN;
  }

	for ( int i = 0; i < BUTTON_CODE_LAST; ++i )
	{
		if ( !V_stricmp( s_pButtonCodeName[i], pString ) )
			return (ButtonCode_t)i;
	}

	if ( bXController )
	{
		for ( int i = 0; i < std::size( s_pXControllerButtonCodeNames ); ++i )
		{
			if ( !V_stricmp( s_pXControllerButtonCodeNames[i], pString ) )
				return (ButtonCode_t)(JOYSTICK_FIRST_BUTTON + i);
		}
	}

	return BUTTON_CODE_INVALID;
}

AnalogCode_t AnalogCode_StringToAnalogCode( const char *pString )
{
	if ( !pString || !pString[0] )
		return ANALOG_CODE_INVALID;

	for ( int i = 0; i < ANALOG_CODE_LAST; ++i )
	{
		if ( !V_stricmp( s_pAnalogCodeName[i], pString ) )
			return (AnalogCode_t)i;
	}

	return ANALOG_CODE_INVALID;
}

ButtonCode_t ButtonCode_ScanCodeToButtonCode( int lParam )
{
	int nScanCode = ( lParam >> 16 ) & 0xFF;
	if ( nScanCode > 127 )
		return KEY_NONE;

	ButtonCode_t result = s_pScanToButtonCode[nScanCode];

	bool bIsExtended = ( lParam & ( 1 << 24 ) ) != 0;
	if ( !bIsExtended )
	{
		switch ( result )
		{
		case KEY_HOME:
			return KEY_PAD_7;
		case KEY_UP:
			return KEY_PAD_8;
		case KEY_PAGEUP:
			return KEY_PAD_9;
		case KEY_LEFT:
			return KEY_PAD_4;
		case KEY_RIGHT:
			return KEY_PAD_6;
		case KEY_END:
			return KEY_PAD_1;
		case KEY_DOWN:
			return KEY_PAD_2;
		case KEY_PAGEDOWN:
			return KEY_PAD_3;
		case KEY_INSERT:
			return KEY_PAD_0;
		case KEY_DELETE:
			return KEY_PAD_DECIMAL;
		default:
			break;
		}
	}
	else
	{
		switch ( result )
		{
		case KEY_ENTER:
			return KEY_PAD_ENTER;
		case KEY_LALT:
			return KEY_RALT;
		case KEY_LCONTROL:
			return KEY_RCONTROL;
		case KEY_SLASH:
			return KEY_PAD_DIVIDE;
		case KEY_CAPSLOCK:
			return KEY_PAD_PLUS;
		}
	}

	return result;
}


//-----------------------------------------------------------------------------
// Update scan codes for foreign keyboards
//-----------------------------------------------------------------------------
void ButtonCode_UpdateScanCodeLayout( )
{
	// reset the keyboard
	std::memcpy( s_pScanToButtonCode, s_pScanToButtonCode_QWERTY, sizeof(s_pScanToButtonCode) );

}


//-----------------------------------------------------------------------------
// Purpose: Updates the current keyboard layout
//-----------------------------------------------------------------------------
static void key_updatelayout( const CCommand &args );
static ConCommand key_updatelayout_command( "key_updatelayout", key_updatelayout, "Updates game keyboard layout to current windows keyboard setting." );
static void key_updatelayout( const CCommand &args )
{
	ButtonCode_UpdateScanCodeLayout();
}
