// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bspflags.h"
#include "const.h"
#include "inputsystem/iinputsystem.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "edict.h"
#include "inputsystem/iinputstacksystem.h"
#include "rocketui/rocketui.h"
#include "cdll_int.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.cvar;
import ks.tier1.iconvar;
import ks.tier1.icvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import :props;
import :iinput;
import :baseentity;
import :sharedvar;
import :util;
import :particlemgr;
import :precache;
import ks.game.shareddefs;
import :achievements_and_stats_interface;
import :baseanimatedtextureproxy;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :noopanimcontroller;
import ks.core.numeric;
import ks.core.platform;
import ks.math.ssemath;
import ks.math.vector2d;
import ks.tier1.convar_serverbounded;
import ks.tier1.envelope;
import ks.tier1.exprevaluator;
import :cstrike15.cs_player;
import :cstrike15.cs_player_shared;
import ks.core.memalloc;
import ks.game.in_buttons;
import ks.tier1.bitbuf;
import ks.tier1.checksum_crc;
import ks.tier1.checksum_md5;
import ks.tier1.keyvalues;
import ks.core.cmdline;

class CBaseEntity;
class CBasePlayer;
struct kbutton_t;

using namespace ::ks::reflect;

//-------------------------------------------------- Constants

#define CAM_MIN_DIST 30.0
#define CAM_ANGLE_MOVE .5
#define MAX_ANGLE_DIFF 10.0
#define PITCH_MAX 90.0
#define PITCH_MIN 0
#define YAW_MAX  135.0
#define YAW_MIN	 -135.0
#define	DIST	 2
#define CAM_HULL_OFFSET		9.0    // the size of the bounding hull used for collision checking
static Vector CAM_HULL_MIN(-CAM_HULL_OFFSET,-CAM_HULL_OFFSET,-CAM_HULL_OFFSET);
static Vector CAM_HULL_MAX( CAM_HULL_OFFSET, CAM_HULL_OFFSET, CAM_HULL_OFFSET);

//-------------------------------------------------- Global Variables

static ConVar cam_snapto( side_registry(), "cam_snapto", "0", FCVAR_ARCHIVE );	 // snap to thirdperson view
static ConVar cam_ideallag( side_registry(), "cam_ideallag", "4.0", FCVAR_ARCHIVE, "Amount of lag used when matching offset to ideal angles in thirdperson view" );
static ConVar cam_idealdelta( side_registry(), "cam_idealdelta", "4.0", FCVAR_ARCHIVE, "Controls the speed when matching offset to ideal angles in thirdperson view" );
ConVar cam_idealyaw( side_registry(), "cam_idealyaw", "0", FCVAR_ARCHIVE | FCVAR_SERVER_CAN_EXECUTE );	 // thirdperson yaw
ConVar cam_idealpitch( side_registry(), "cam_idealpitch", "0", FCVAR_ARCHIVE | FCVAR_SERVER_CAN_EXECUTE );	 // thirperson pitch
ConVar cam_idealdist( side_registry(), "cam_idealdist", "150", FCVAR_ARCHIVE | FCVAR_SERVER_CAN_EXECUTE );	 // thirdperson distance
ConVar cam_idealdistright( side_registry(), "cam_idealdistright", "0", FCVAR_ARCHIVE | FCVAR_SERVER_CAN_EXECUTE );	 // thirdperson distance right;
ConVar cam_idealdistup( side_registry(), "cam_idealdistup", "0", FCVAR_ARCHIVE | FCVAR_SERVER_CAN_EXECUTE );	 // thirdperson distance up;
static ConVar cam_collision( side_registry(), "cam_collision", "1", FCVAR_ARCHIVE | FCVAR_SERVER_CAN_EXECUTE, "When in thirdperson and cam_collision is set to 1, an attempt is made to keep the camera from passing though walls." );
static ConVar cam_showangles( side_registry(), "cam_showangles", "0", FCVAR_CHEAT, "When in thirdperson, print viewangles/idealangles/cameraoffsets to the console." );
static ConVar c_maxpitch( side_registry(), "c_maxpitch", "90", FCVAR_ARCHIVE );
static ConVar c_minpitch( side_registry(), "c_minpitch", "0", FCVAR_ARCHIVE );
static ConVar c_maxyaw( side_registry(), "c_maxyaw",   "135", FCVAR_ARCHIVE );
static ConVar c_minyaw( side_registry(), "c_minyaw",   "-135", FCVAR_ARCHIVE );
static ConVar c_maxdistance( side_registry(), "c_maxdistance",   "200", FCVAR_ARCHIVE );
static ConVar c_mindistance( side_registry(), "c_mindistance",   "30", FCVAR_ARCHIVE );
static ConVar c_orthowidth( side_registry(), "c_orthowidth",   "100", FCVAR_ARCHIVE );
static ConVar c_orthoheight( side_registry(), "c_orthoheight",   "100", FCVAR_ARCHIVE );
static ConVar c_thirdpersonshoulder( side_registry(), "c_thirdpersonshoulder", "false", FCVAR_ARCHIVE ); // flag to indicate when we are using thirdperson-shoulder
static ConVar c_thirdpersonshoulderoffset( side_registry(), "c_thirdpersonshoulderoffset", "20.0", FCVAR_ARCHIVE ); // camera right offset for thirdperson-shoulder
static ConVar c_thirdpersonshoulderdist( side_registry(), "c_thirdpersonshoulderdist", "40.0", FCVAR_ARCHIVE ); // camera distance from the player when in thirdperson-shoulder
static ConVar c_thirdpersonshoulderheight( side_registry(), "c_thirdpersonshoulderheight", "5.0", FCVAR_ARCHIVE ); // camera height above the player
static ConVar c_thirdpersonshoulderaimdist( side_registry(), "c_thirdpersonshoulderaimdist", "120.0", FCVAR_ARCHIVE ); // the distance in front of the player to focus the camera

static kbutton_t cam_pitchup, cam_pitchdown, cam_yawleft, cam_yawright;
static kbutton_t cam_in, cam_out; // -- "cam_move" is unused

extern const ConVar *sv_cheats;
extern ConVar in_forceuser;
extern ConVar sv_allow_thirdperson;

static void cam_command( const CCommand &args );
static ConCommand cam_command_command( side_registry(), "cam_command", cam_command, "Tells camera to change modes", FCVAR_CHEAT );
static void cam_command( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		Msg( "cam_command <0, 1, or 2>\n" );
		return;
	}
	input->CAM_Command( V_atoi( args.Arg( 1 ) ) );
}

// ==============================
// CAM_ToThirdPerson
// ==============================
void CAM_ToThirdPerson(void)
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	input->CAM_ToThirdPerson();

	// Let the local player know
	CBasePlayer *localPlayer = CBasePlayer::GetLocalPlayer();
	if ( localPlayer )
	{
		localPlayer->ThirdPersonSwitch( true );
	}
}

static bool & Is_CAM_ThirdPerson_MayaMode(void)
{
	static bool s_b_CAM_ThirdPerson_MayaMode = false;
	return s_b_CAM_ThirdPerson_MayaMode;
}
void CAM_ToThirdPerson_MayaMode(void)
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	bool &rb = Is_CAM_ThirdPerson_MayaMode();
	rb = !rb;
}

// ==============================
// CAM_ToFirstPerson
// ==============================
void CAM_ToFirstPerson(void) 
{ 
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	input->CAM_ToFirstPerson();

	// Let the local player know
	CBasePlayer *localPlayer = CBasePlayer::GetLocalPlayer();
	if ( localPlayer )
	{
		localPlayer->ThirdPersonSwitch( false );
	}
	c_thirdpersonshoulder.SetValue( false );
	cam_idealdist.SetValue( cam_idealdist.GetDefault() );
}

/*
==============================
CAM_ToThirdPersonShoulder

==============================
*/
void CAM_ToThirdPersonShoulder(void)
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	input->CAM_ToThirdPersonShoulder();
}

void CInput::CAM_ToThirdPersonShoulder()
{
	PerUserInput_t &user = GetPerUser();

	if( c_thirdpersonshoulder.GetBool() )
	{
		user.m_nCamCommand = 2; // CAM_COMMAND_TOFIRSTPERSON
		c_thirdpersonshoulder.SetValue( false );
		cam_idealdist.SetValue( cam_idealdist.GetDefault() );
	}
	else
	{
		user.m_nCamCommand = 1; // CAM_COMMAND_TOTHIRDPERSON
		c_thirdpersonshoulder.SetValue( true );
		cam_idealdist.SetValue( c_thirdpersonshoulderdist.GetFloat() );
	}
}

/*
==============================
CAM_ToThirdPersonOverview

==============================
*/
void CAM_ToThirdPersonOverview(void)
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	input->CAM_ToThirdPersonOverview();
}

void CInput::CAM_ToThirdPersonOverview()
{
// 	PerUserInput_t &user = GetPerUser();
// 
// 	if( c_thirdpersonshoulder.GetBool() )
// 	{
// 		user.m_nCamCommand = 2; // CAM_COMMAND_TOFIRSTPERSON
// 		c_thirdpersonshoulder.SetValue( false );
// 		cam_idealdist.SetValue( cam_idealdist.GetDefault() );
// 	}
// 	else
// 	{
// 		user.m_nCamCommand = 1; // CAM_COMMAND_TOTHIRDPERSON
// 		c_thirdpersonshoulder.SetValue( true );
// 		cam_idealdist.SetValue( c_thirdpersonshoulderdist.GetFloat() );
// 	}

	//m_CameraIsThirdPersonOverview

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	if ( !user.m_fCameraInThirdPerson )
	{
		user.m_fCameraInThirdPerson = true;
		CBaseEntity::UpdateVisibilityAllEntities();
	}
	user.m_CameraIsThirdPersonOverview = true;
	user.m_nCamCommand = 0;
}

bool CInput::CAM_IsThirdPersonOverview( int nSlot /*=-1*/ )
{
	if ( !g_bEngineIsHLTV )
		return false;

	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		PerUserInput_t &user = GetPerUser();
		return user.m_CameraIsThirdPersonOverview;
	}
	return m_PerUser[ nSlot ].m_CameraIsThirdPersonOverview;
}

//==============================
// CAM_ToOrthographic
// ==============================
void CAM_ToOrthographic(void) 
{ 
	input->CAM_ToOrthographic();
}

void CAM_StartMouseMove( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	input->CAM_StartMouseMove();
}

void CAM_EndMouseMove( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	input->CAM_EndMouseMove();
}

void CAM_StartDistance( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	input->CAM_StartDistance();
}

void CAM_EndDistance( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	input->CAM_EndDistance();
}

void CAM_ToggleSnapto( void )
{ 
	cam_snapto.SetValue( !cam_snapto.GetInt() );
}

float MoveToward( float cur, float goal, float lag )
{
	if( cur != goal )
	{
		if( abs( cur - goal ) > 180.0 )
		{
			if( cur < goal )
				cur += 360.0;
			else
				cur -= 360.0;
		}

		if( cur < goal )
		{
			if( cur < goal - 1.0 )
				cur += ( goal - cur ) / lag;
			else
				cur = goal;
		}
		else
		{
			if( cur > goal + 1.0 )
				cur -= ( cur - goal ) / lag;
			else
				cur = goal;
		}
	}

	// bring cur back into range
	if( cur < 0 )
		cur += 360.0;
	else if( cur >= 360 )
		cur -= 360;

	return cur;
}

void CInput::CAM_Command( int command )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	user.m_nCamCommand = command;
}

void CInput::CAM_Think( void )
{
	CVProfScope _( "CAM_Think", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();

	if ( !sv_cheats )
	{
		sv_cheats = cvar->FindVar( "sv_cheats" );
	}

	// If cheats have been disabled, pull us back out of third-person view.
	if ( sv_cheats && !sv_cheats->GetBool() && user.m_fCameraInThirdPerson && GameRules() && !GameRules()->AllowThirdPersonCamera() )
	{
		CAM_ToFirstPerson();
		return;
	}

	if ( user.m_pCameraThirdData )
	{
		return CAM_CameraThirdThink();
	}

	Vector idealAngles;
	Vector camOffset;
	float flSensitivity;
	QAngle viewangles;
	
	switch( user.m_nCamCommand )
	{
	case CAM_COMMAND_TOTHIRDPERSON:
		CAM_ToThirdPerson();
		break;
		
	case CAM_COMMAND_TOFIRSTPERSON:
		CAM_ToFirstPerson();
		break;
		
	case CAM_COMMAND_NONE:
	default:
		break;
	}
	
	if( !user.m_fCameraInThirdPerson )
		return;

	CBasePlayer* localPlayer = CBasePlayer::GetLocalPlayer();
	// In Maya-mode
	if ( Is_CAM_ThirdPerson_MayaMode() )
	{
		// Unless explicitly moving the camera, don't move it
		user.m_fCameraInterceptingMouse = user.m_fCameraMovingWithMouse =
			g_pInputSystem->IsButtonDown( KEY_LALT ) || g_pInputSystem->IsButtonDown( KEY_RALT );
		//if ( !user.m_fCameraMovingWithMouse )
		//	return;

		// Zero-out camera-control kbutton_t structures
		memset( &cam_pitchup, 0, sizeof( cam_pitchup ) );
		memset( &cam_pitchdown, 0, sizeof( cam_pitchdown ) );
		memset( &cam_yawleft, 0, sizeof( cam_yawleft ) );
		memset( &cam_yawright, 0, sizeof( cam_yawright ) );
		memset( &cam_in, 0, sizeof( cam_in ) );
		memset( &cam_out, 0, sizeof( cam_out ) );

		// Unless left or right mouse button is down, don't do anything
		if ( /* Left+Middle Button Down */ g_pInputSystem->IsButtonDown( MOUSE_LEFT ) && g_pInputSystem->IsButtonDown( MOUSE_MIDDLE ) )
		{
			// Do only zoom in/out camera adjustment
			user.m_fCameraDistanceMove = true;
		}
		else if ( /* Left Button Down */ g_pInputSystem->IsButtonDown( MOUSE_LEFT ) )
		{
			// Do only rotational camera movement
			user.m_fCameraDistanceMove = false;
		}
		else if ( /* Right Button Down */ g_pInputSystem->IsButtonDown( MOUSE_RIGHT ) )
		{
			// Do only zoom in/out camera adjustment
			user.m_fCameraDistanceMove = true;
		}
		else
		{
			// Neither left or right buttons down, don't do anything
			ResetMouse();
			//return;
		}
	}
	
	idealAngles[ PITCH ] = cam_idealpitch.GetFloat();
	idealAngles[ YAW ]   = cam_idealyaw.GetFloat();
	idealAngles[ DIST ]  = cam_idealdist.GetFloat();

	//
	//movement of the camera with the mouse
	//
	if ( user.m_fCameraMovingWithMouse )
	{
		int cpx, cpy;

		//get windows cursor position
		GetMousePos (cpx, cpy);

		user.m_nCameraX = cpx;
		user.m_nCameraY = cpy;
		
		//check for X delta values and adjust accordingly
		//eventually adjust YAW based on amount of movement
		//don't do any movement of the cam using YAW/PITCH if we are zooming in/out the camera	
		if (!user.m_fCameraDistanceMove)
		{
			int x, y;
			GetWindowCenter( x,  y );
			
			if ( Is_CAM_ThirdPerson_MayaMode() )
			{
				if (user.m_nCameraX>x)
				{
					idealAngles[ YAW ] += (CAM_ANGLE_MOVE)*((user.m_nCameraX-x)/2);
				}
				else if (user.m_nCameraX<x)
				{
					idealAngles[ YAW ] -= (CAM_ANGLE_MOVE)* ((x-user.m_nCameraX)/2);
				}
				if (user.m_nCameraY > y)
				{
					idealAngles[PITCH] +=(CAM_ANGLE_MOVE)* ((user.m_nCameraY-y)/2);
				}
				else if  (user.m_nCameraY<y)
				{
					idealAngles[PITCH] -= (CAM_ANGLE_MOVE)*((y-user.m_nCameraY)/2);
				}
			}
			else
			{
				//keep the camera within certain limits around the player (ie avoid certain bad viewing angles)  
				if (user.m_nCameraX>x)
				{
					//if ((idealAngles[YAW]>=225.0)||(idealAngles[YAW]<135.0))
					if (idealAngles[YAW]<c_maxyaw.GetFloat())
					{
						idealAngles[ YAW ] += (CAM_ANGLE_MOVE)*((user.m_nCameraX-x)/2);
					}
					if (idealAngles[YAW]>c_maxyaw.GetFloat())
					{
					
						idealAngles[YAW]=c_maxyaw.GetFloat();
					}
				}
				else if (user.m_nCameraX<x)
				{
					//if ((idealAngles[YAW]<=135.0)||(idealAngles[YAW]>225.0))
					if (idealAngles[YAW]>c_minyaw.GetFloat())
					{
						idealAngles[ YAW ] -= (CAM_ANGLE_MOVE)* ((x-user.m_nCameraX)/2);
					
					}
					if (idealAngles[YAW]<c_minyaw.GetFloat())
					{
						idealAngles[YAW]=c_minyaw.GetFloat();
					
					}
				}
			
				//check for y delta values and adjust accordingly
				//eventually adjust PITCH based on amount of movement
				//also make sure camera is within bounds
				if (user.m_nCameraY > y)
				{
					if(idealAngles[PITCH]<c_maxpitch.GetFloat())
					{
						idealAngles[PITCH] +=(CAM_ANGLE_MOVE)* ((user.m_nCameraY-y)/2);
					}
					if (idealAngles[PITCH]>c_maxpitch.GetFloat())
					{
						idealAngles[PITCH]=c_maxpitch.GetFloat();
					}
				}
				else if  (user.m_nCameraY<y)
				{
					if (idealAngles[PITCH]>c_minpitch.GetFloat())
					{
						idealAngles[PITCH] -= (CAM_ANGLE_MOVE)*((y-user.m_nCameraY)/2);
					}
					if (idealAngles[PITCH]<c_minpitch.GetFloat())
					{
						idealAngles[PITCH]=c_minpitch.GetFloat();
					}
				}
			}
			
			//set old mouse coordinates to current mouse coordinates
			//since we are done with the mouse
			
			if ( ( flSensitivity = GetHud().GetSensitivity() ) != 0 )
			{
				user.m_nCameraOldX=user.m_nCameraX*flSensitivity;
				user.m_nCameraOldY=user.m_nCameraY*flSensitivity;
			}
			else
			{
				user.m_nCameraOldX=user.m_nCameraX;
				user.m_nCameraOldY=user.m_nCameraY;
			}

			ResetMouse();
		}
	}
	
	//Nathan code here
	if( input->KeyState( &cam_pitchup ) )
		idealAngles[ PITCH ] += cam_idealdelta.GetFloat();
	else if( input->KeyState( &cam_pitchdown ) )
		idealAngles[ PITCH ] -= cam_idealdelta.GetFloat();
	
	if( input->KeyState( &cam_yawleft ) )
		idealAngles[ YAW ] -= cam_idealdelta.GetFloat();
	else if( input->KeyState( &cam_yawright ) )
		idealAngles[ YAW ] += cam_idealdelta.GetFloat();
	
	if( input->KeyState( &cam_in ) )
	{
		idealAngles[ DIST ] -= 2*cam_idealdelta.GetFloat();
		if( idealAngles[ DIST ] < CAM_MIN_DIST )
		{
			// If we go back into first person, reset the angle
			idealAngles[ PITCH ] = 0;
			idealAngles[ YAW ] = 0;
			idealAngles[ DIST ] = CAM_MIN_DIST;
		}
		
	}
	else if( input->KeyState( &cam_out ) )
		idealAngles[ DIST ] += 2*cam_idealdelta.GetFloat();
	
	if (user.m_fCameraDistanceMove)
	{
		int x, y;
		GetWindowCenter( x, y );

		if (user.m_nCameraY>y)
		{
			if(idealAngles[ DIST ]<c_maxdistance.GetFloat())
			{
				idealAngles[ DIST ] +=cam_idealdelta.GetFloat() * ((user.m_nCameraY-y)/2);
			}
			if (idealAngles[ DIST ]>c_maxdistance.GetFloat())
			{
				idealAngles[ DIST ]=c_maxdistance.GetFloat();
			}
		}
		else if (user.m_nCameraY<y)
		{
			if (idealAngles[ DIST ]>c_mindistance.GetFloat())
			{
				idealAngles[ DIST ] -= (cam_idealdelta.GetFloat())*((y-user.m_nCameraY)/2);
			}
			if (idealAngles[ DIST ]<c_mindistance.GetFloat())
			{
				idealAngles[ DIST ]=c_mindistance.GetFloat();
			}
		}
		//set old mouse coordinates to current mouse coordinates
		//since we are done with the mouse
		user.m_nCameraOldX=user.m_nCameraX*GetHud().GetSensitivity();
		user.m_nCameraOldY=user.m_nCameraY*GetHud().GetSensitivity();

		ResetMouse();
	}

	// Obtain engine view angles and if they popped while the camera was static,
	// fix the camera angles as well
	engine->GetViewAngles( viewangles );
	static QAngle s_oldAngles = viewangles;
	if ( Is_CAM_ThirdPerson_MayaMode() && ( s_oldAngles != viewangles ) )
	{
		idealAngles[ PITCH ] += s_oldAngles[ PITCH ] - viewangles[ PITCH ];
		idealAngles[  YAW  ] += s_oldAngles[  YAW  ] - viewangles[  YAW  ];
		s_oldAngles = viewangles;
	}

	// bring the pitch values back into a range that MoveToward can handle
	if ( idealAngles[ PITCH ] > 180 )
		idealAngles[ PITCH ] -= 360;
	else if ( idealAngles[ PITCH ] < -180 )
		idealAngles[ PITCH ] += 360;

	// bring the yaw values back into a range that MoveToward can handle
	// --
	// Vitaliy: going with >= 180 and <= -180.
	// This introduces a potential discontinuity when looking directly at model face
	// as camera yaw will be jumping from +180 to -180 and back, but when working with
	// the camera allows smooth rotational transitions from left to right and back.
	// Otherwise one of the transitions that has ">"-comparison will be locked.
	// --
	if ( idealAngles[ YAW ] >= 180 )
		idealAngles[ YAW ] -= 360;
	else if ( idealAngles[ YAW ] <= -180 )
		idealAngles[ YAW ] += 360;

	// clamp pitch, yaw and dist...
	if ( !Is_CAM_ThirdPerson_MayaMode() )
	{
		idealAngles[ PITCH ] = clamp( idealAngles[ PITCH ], c_minpitch.GetFloat(), c_maxpitch.GetFloat() );
		idealAngles[ YAW ]   = clamp( idealAngles[ YAW ], c_minyaw.GetFloat(), c_maxyaw.GetFloat() );
		idealAngles[ DIST ]  = clamp( idealAngles[ DIST ], c_mindistance.GetFloat(), c_maxdistance.GetFloat() );
	}

	// update ideal angles
	cam_idealpitch.SetValue( idealAngles[ PITCH ] );
	cam_idealyaw.SetValue( idealAngles[ YAW ] );
	cam_idealdist.SetValue( idealAngles[ DIST ] );
	
	// Move the CameraOffset "towards" the idealAngles
	// Note: CameraOffset = viewangle + idealAngle
	VectorCopy( user.m_vecCameraOffset, camOffset );
	
	if( cam_snapto.GetInt() )
	{
		camOffset[ YAW ] = cam_idealyaw.GetFloat() + viewangles[ YAW ];
		camOffset[ PITCH ] = cam_idealpitch.GetFloat() + viewangles[ PITCH ];
		camOffset[ DIST ] = cam_idealdist.GetFloat();
	}
	else
	{
		float lag = std::max<std::common_type_t<int, float>>( 1, 1 + cam_ideallag.GetFloat() );

		if( camOffset[ YAW ] - viewangles[ YAW ] != cam_idealyaw.GetFloat() )
			camOffset[ YAW ] = MoveToward( AngleNormalizePositive( camOffset[ YAW ] ), AngleNormalizePositive( cam_idealyaw.GetFloat() + viewangles[ YAW ] ), lag );
		
		if( camOffset[ PITCH ] - viewangles[ PITCH ] != cam_idealpitch.GetFloat() )
			camOffset[ PITCH ] = MoveToward( camOffset[ PITCH ], cam_idealpitch.GetFloat() + viewangles[ PITCH ], lag );
		
		if( abs( camOffset[ DIST ] - cam_idealdist.GetFloat() ) < 2.0 )
			camOffset[ DIST ] = cam_idealdist.GetFloat();
		else
			camOffset[ DIST ] += ( cam_idealdist.GetFloat() - camOffset[ DIST ] ) / lag;
	}

	// move the camera closer to the player if it hit something
	if ( cam_collision.GetInt() && localPlayer )
	{
		Vector camForward;

		// find our player's origin, and from there, the eye position
		Vector origin = localPlayer->GetLocalOrigin();
		origin += localPlayer->GetViewOffset();

		// get the forward vector
		AngleVectors( QAngle(camOffset[ PITCH ], camOffset[ YAW ], 0), &camForward, nullptr, nullptr );

		// use our previously #defined hull to collision trace
		trace_t trace;
		CTraceFilterSimple traceFilter( localPlayer, COLLISION_GROUP_NONE );
		UTIL_TraceHull( origin, origin - (camForward * camOffset[ DIST ]),
			CAM_HULL_MIN, CAM_HULL_MAX,
			MASK_SOLID, &traceFilter, &trace );

		// move the camera closer if it hit something
		if( trace.fraction < 1.0 )
		{
			camOffset[ DIST ] *= trace.fraction;
		}

		// For now, I'd rather see the insade of a player model than punch the camera through a wall
		// might try the fade out trick at some point
		//if( camOffset[ DIST ] < CAM_MIN_DIST )
		//    camOffset[ DIST ] = CAM_MIN_DIST; // clamp up to minimum
	}

	if ( cam_showangles.GetInt() )
	{
		engine->Con_NPrintf( 4, "Pitch: %6.1f   Yaw: %6.1f %38s", viewangles[ PITCH ], viewangles[ YAW ], "view angles" );
		engine->Con_NPrintf( 6, "Pitch: %6.1f   Yaw: %6.1f   Dist: %6.1f %19s", cam_idealpitch.GetFloat(), cam_idealyaw.GetFloat(), cam_idealdist.GetFloat(), "ideal angles" );
		engine->Con_NPrintf( 8, "Pitch: %6.1f   Yaw: %6.1f   Dist: %6.1f %16s", user.m_vecCameraOffset[ PITCH ], user.m_vecCameraOffset[ YAW ],user. m_vecCameraOffset[ DIST ], "camera offset" );
	}

	user.m_vecCameraOffset[ PITCH ] = camOffset[ PITCH ];
	user.m_vecCameraOffset[ YAW ]   = camOffset[ YAW ];
	user.m_vecCameraOffset[ DIST ]  = camOffset[ DIST ];
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void ClampRange180( float &value )
{
	if ( value >= 180.0f )
	{
		value -= 360.0f;
	}
	else if ( value <= -180.0f )
	{
		value += 360.0f;
	}
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CInput::CAM_SetCameraThirdData( CameraThirdData_t *pCameraData, const QAngle &vecCameraOffset )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	user.m_pCameraThirdData = pCameraData;

	user.m_vecCameraOffset[PITCH] = vecCameraOffset[PITCH];
	user.m_vecCameraOffset[YAW] = vecCameraOffset[YAW];
	user.m_vecCameraOffset[DIST] = vecCameraOffset[DIST];
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CInput::CAM_CameraThirdThink( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	// Verify data.
	if ( !user.m_pCameraThirdData )
		return;

	// Verify that we are in third person mode.
	if( !user.m_fCameraInThirdPerson )
		return;

	// Opting out of camera movement
	if ( user.m_pCameraThirdData->m_flLag == -1.0f )
		return;

	// Obtain engine view angles and if they popped while the camera was static, fix the camera angles as well.
	QAngle angView;
	engine->GetViewAngles( angView );

	// Move the CameraOffset "towards" the idealAngles, Note: CameraOffset = viewangle + idealAngle
	Vector vecCamOffset;
	VectorCopy( user.m_vecCameraOffset, vecCamOffset );

	// Move the camera.
	float flLag = std::max<std::common_type_t<int, float>>( 1, 1 + user.m_pCameraThirdData->m_flLag );
	if( vecCamOffset[PITCH] - angView[PITCH] != user.m_pCameraThirdData->m_flPitch )
	{
		vecCamOffset[PITCH] = MoveToward( vecCamOffset[PITCH], ( user.m_pCameraThirdData->m_flPitch + angView[PITCH] ), flLag );
	}
	if( vecCamOffset[YAW] - angView[YAW] != user.m_pCameraThirdData->m_flYaw )
	{
		vecCamOffset[YAW] = MoveToward( vecCamOffset[YAW], ( user.m_pCameraThirdData->m_flYaw + angView[YAW] ), flLag );
	}
	if( abs( vecCamOffset[DIST] - user.m_pCameraThirdData->m_flDist ) < 2.0 )
	{
		vecCamOffset[DIST] = user.m_pCameraThirdData->m_flDist;
	}
	else
	{
		vecCamOffset[DIST] += ( user.m_pCameraThirdData->m_flDist - vecCamOffset[DIST] ) / flLag;
	}

	CBasePlayer* pLocalPlayer = CBasePlayer::GetLocalPlayer();
	if ( pLocalPlayer )
	{
		Vector vecForward;

		// Find our player's origin, and from there, the eye position.
		Vector vecOrigin = pLocalPlayer->GetThirdPersonViewPosition();

		// Get the forward vector
		AngleVectors( QAngle( vecCamOffset[PITCH], vecCamOffset[YAW], 0 ), &vecForward, nullptr, nullptr );

		// Collision trace and move the camera closer if we hit something.
		CTraceFilterSkipTwoEntities filter( pLocalPlayer, nullptr );

		trace_t trace;
		UTIL_TraceHull( vecOrigin, vecOrigin - ( vecForward * vecCamOffset[DIST] ), user.m_pCameraThirdData->m_vecHullMin, user.m_pCameraThirdData->m_vecHullMax, MASK_SOLID, &filter, &trace );

		if( trace.fraction < 1.0 )
		{
			vecCamOffset[DIST] *= trace.fraction;
		}
	}

	ClampRange180( vecCamOffset[PITCH] );
	ClampRange180( vecCamOffset[YAW] );

	user.m_vecCameraOffset[PITCH] = vecCamOffset[PITCH];
	user.m_vecCameraOffset[YAW] = vecCamOffset[YAW];
	user.m_vecCameraOffset[DIST] = vecCamOffset[DIST];
}

void CAM_PitchUpDown( const CCommand &args ) { KeyDown( &cam_pitchup, args[1] ); }
void CAM_PitchUpUp( const CCommand &args ) { KeyUp( &cam_pitchup, args[1] ); }
void CAM_PitchDownDown( const CCommand &args ) { KeyDown( &cam_pitchdown, args[1] ); }
void CAM_PitchDownUp( const CCommand &args ) { KeyUp( &cam_pitchdown, args[1] ); }
void CAM_YawLeftDown( const CCommand &args ) { KeyDown( &cam_yawleft, args[1] ); }
void CAM_YawLeftUp( const CCommand &args ) { KeyUp( &cam_yawleft, args[1] ); }
void CAM_YawRightDown( const CCommand &args ) { KeyDown( &cam_yawright, args[1] ); }
void CAM_YawRightUp( const CCommand &args ) { KeyUp( &cam_yawright, args[1] ); }
void CAM_InDown( const CCommand &args ) { KeyDown( &cam_in, args[1] ); }
void CAM_InUp( const CCommand &args ) { KeyUp( &cam_in, args[1] ); }
void CAM_OutDown( const CCommand &args ) { KeyDown( &cam_out, args[1] ); }
void CAM_OutUp( const CCommand &args ) { KeyUp( &cam_out, args[1] ); }

void CInput::CAM_ToThirdPerson(void)
{ 
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	QAngle viewangles;

	engine->GetViewAngles( viewangles );

	if( !user.m_fCameraInThirdPerson )
	{
		user.m_fCameraInThirdPerson = true; 
		
		user.m_vecCameraOffset[ YAW ] = viewangles[ YAW ]; 
		user.m_vecCameraOffset[ PITCH ] = viewangles[ PITCH ]; 
		user.m_vecCameraOffset[ DIST ] = CAM_MIN_DIST;

		CBaseEntity::UpdateVisibilityAllEntities();
	}

	user.m_nCamCommand = 0;
}

void CInput::CAM_ToFirstPerson(void)
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	if ( user.m_fCameraInThirdPerson )
	{
		user.m_fCameraInThirdPerson = false;
		CBaseEntity::UpdateVisibilityAllEntities();
	}
	user.m_nCamCommand = 0;
}

bool CInput::CAM_IsOrthographic(void) const
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	const PerUserInput_t &user = GetPerUser();
	return user.m_CameraIsOrthographic;
}

void CInput::CAM_OrthographicSize(float& w, float& h) const
{
	w = c_orthowidth.GetFloat(); h = c_orthoheight.GetFloat();
}

void CInput::CAM_ToOrthographic(void)
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	if ( user.m_fCameraInThirdPerson )
	{
		user.m_fCameraInThirdPerson = false;
		CBaseEntity::UpdateVisibilityAllEntities();
	}
	user.m_CameraIsOrthographic = true;
	user.m_nCamCommand = 0;
}

void CInput::CAM_StartMouseMove(void)
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	float flSensitivity;
		
	//only move the cam with mouse if we are in third person.
	if ( user.m_fCameraInThirdPerson )
	{
		//set appropriate flags and initialize the old mouse position
		//variables for mouse camera movement
		if (!user.m_fCameraMovingWithMouse)
		{
			int cpx, cpy;

			user.m_fCameraMovingWithMouse=true;
			user.m_fCameraInterceptingMouse=true;

			GetMousePos(cpx, cpy);

			user.m_nCameraX = cpx;
			user.m_nCameraY = cpy;

			if ( ( flSensitivity = GetHud().GetSensitivity() ) != 0 )
			{
				user.m_nCameraOldX=user.m_nCameraX*flSensitivity;
				user.m_nCameraOldY=user.m_nCameraY*flSensitivity;
			}
			else
			{
				user.m_nCameraOldX=user.m_nCameraX;
				user.m_nCameraOldY=user.m_nCameraY;
			}
		}
	}
	//we are not in 3rd person view..therefore do not allow camera movement
	else
	{   
		user.m_fCameraMovingWithMouse=false;
		user.m_fCameraInterceptingMouse=false;
	}
}

/*
==============================
CAM_EndMouseMove

the key has been released for camera movement
tell the engine that mouse camera movement is off
==============================
*/
void CInput::CAM_EndMouseMove(void)
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	user.m_fCameraMovingWithMouse=false;
	user.m_fCameraInterceptingMouse=false;
}

/*
==============================
CAM_StartDistance

routines to start the process of moving the cam in or out 
using the mouse
==============================
*/
void CInput::CAM_StartDistance(void)
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	//only move the cam with mouse if we are in third person.
	if ( user.m_fCameraInThirdPerson )
	{
	  //set appropriate flags and initialize the old mouse position
	  //variables for mouse camera movement
	  if (!user.m_fCameraDistanceMove)
	  {
		  int cpx, cpy;

		  user.m_fCameraDistanceMove=true;
		  user.m_fCameraMovingWithMouse=true;
		  user.m_fCameraInterceptingMouse=true;

		  GetMousePos(cpx, cpy);

		  user.m_nCameraX = cpx;
		  user.m_nCameraY = cpy;

		  user.m_nCameraOldX=user.m_nCameraX*GetHud().GetSensitivity();
		  user.m_nCameraOldY=user.m_nCameraY*GetHud().GetSensitivity();
	  }
	}
	//we are not in 3rd person view..therefore do not allow camera movement
	else
	{   
		user.m_fCameraDistanceMove=false;
		user.m_fCameraMovingWithMouse=false;
		user.m_fCameraInterceptingMouse=false;
	}
}

/*
==============================
CAM_EndDistance

the key has been released for camera movement
tell the engine that mouse camera movement is off
==============================
*/
void CInput::CAM_EndDistance(void)
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	user.m_fCameraDistanceMove=false;
	user.m_fCameraMovingWithMouse=false;
	user.m_fCameraInterceptingMouse=false;
}

/*
==============================
CAM_IsThirdPerson

==============================
*/
int CInput::CAM_IsThirdPerson( int nSlot /*=-1*/ )
{
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		if ( GetPerUser().m_fCameraInThirdPerson || GetPerUser().m_CameraIsThirdPersonOverview )
			return true;
		else
			return false;
	}

	if ( m_PerUser[ nSlot ].m_fCameraInThirdPerson || m_PerUser[ nSlot ].m_CameraIsThirdPersonOverview )
		return true;
	else
		return false;
}

/*
==============================
CAM_GetCameraOffset

==============================
*/
void CInput::CAM_GetCameraOffset( Vector& ofs )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	VectorCopy( user.m_vecCameraOffset, ofs );
}

/*
==============================
CAM_InterceptingMouse

==============================
*/
int CInput::CAM_InterceptingMouse( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	return user.m_fCameraInterceptingMouse;
}

static ConCommand startpitchup( side_registry(), "+campitchup", CAM_PitchUpDown );
static ConCommand endpitcup( side_registry(), "-campitchup", CAM_PitchUpUp );
static ConCommand startpitchdown( side_registry(), "+campitchdown", CAM_PitchDownDown );
static ConCommand endpitchdown( side_registry(), "-campitchdown", CAM_PitchDownUp );
static ConCommand startcamyawleft( side_registry(), "+camyawleft", CAM_YawLeftDown );
static ConCommand endcamyawleft( side_registry(), "-camyawleft", CAM_YawLeftUp );
static ConCommand startcamyawright( side_registry(), "+camyawright", CAM_YawRightDown );
static ConCommand endcamyawright( side_registry(), "-camyawright", CAM_YawRightUp );
static ConCommand startcamin( side_registry(), "+camin", CAM_InDown );
static ConCommand endcamin( side_registry(), "-camin", CAM_InUp );
static ConCommand startcamout( side_registry(), "+camout", CAM_OutDown );
static ConCommand camout( side_registry(), "-camout", CAM_OutUp );
static ConCommand thirdperson( side_registry(), "thirdperson", CAM_ToThirdPerson, "Switch to thirdperson camera.", FCVAR_CHEAT|FCVAR_SERVER_CAN_EXECUTE );
static ConCommand thirdperson_mayamode( side_registry(), "thirdperson_mayamode", ::CAM_ToThirdPerson_MayaMode, "Switch to thirdperson Maya-like camera controls.", FCVAR_CHEAT );
static ConCommand firstperson( side_registry(), "firstperson", CAM_ToFirstPerson, "Switch to firstperson camera.", FCVAR_SERVER_CAN_EXECUTE );
static ConCommand thirdpersonshoulder( side_registry(), "thirdpersonshoulder", CAM_ToThirdPersonShoulder, "Switch to thirdperson-shoulder camera.", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
static ConCommand thirdpersonoverview( side_registry(), "thirdpersonoverview", CAM_ToThirdPersonOverview, "Switch to thirdperson-overview camera.", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
static ConCommand camortho( side_registry(), "camortho", CAM_ToOrthographic, "Switch to orthographic camera.", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
static ConCommand startcammousemove( side_registry(), "+cammousemove",::CAM_StartMouseMove);
static ConCommand endcammousemove( side_registry(), "-cammousemove",::CAM_EndMouseMove);
static ConCommand startcamdistance( side_registry(), "+camdistance", ::CAM_StartDistance );
static ConCommand endcamdistance( side_registry(), "-camdistance", ::CAM_EndDistance );
static ConCommand snapto( side_registry(), "snapto", CAM_ToggleSnapto );
/*
==============================
Init_Camera

==============================
*/
void CInput::Init_Camera( void )
{	
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		m_PerUser[ i ].m_CameraIsOrthographic = false;
		m_PerUser[ i ].m_CameraIsThirdPersonOverview = false;
	}
}

#undef CAM_MIN_DIST
#undef CAM_ANGLE_MOVE
#undef MAX_ANGLE_DIFF
#undef PITCH_MAX
#undef PITCH_MIN
#undef YAW_MAX
#undef YAW_MIN
#undef DIST
#undef CAM_HULL_OFFSET

class CUserCmd;
class CBasePlayer;
class CCSPlayer;
class CWeaponCSBase;

using namespace ::ks::reflect;

// Purpose: Joystick handling function

// Control like a joystick
static constexpr auto JOY_ABSOLUTE_AXIS = 0x00000000;
// Control like a mouse, spinner, trackball
static constexpr auto JOY_RELATIVE_AXIS = 0x00000010;

// Set the joystick being force disabled just as we write the config
// This allows us to chose this option in the menu with a controller without accidentally disabling our only mode of input
void JoystickForceDisabled_ChangeCallback( ConVar *pConVar, char const *pOldString, float flOldValue );
static ConVar joystick_force_disabled_set_from_options( side_registry(), "joystick_force_disabled_set_from_options", "1", FCVAR_ARCHIVE, "Sets controllers enabled/disabled just before the config is written.", JoystickForceDisabled_ChangeCallback );
static ConVar joystick_force_disabled( side_registry(), "joystick_force_disabled", "1", FCVAR_ARCHIVE, "Prevents any and all joystick input for cases where a piece of hardware is incorrectly identified as a joystick an sends bad signals." );
void JoystickForceDisabled_ChangeCallback( ConVar *pConVar, char const *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	if ( var.IsValid() && !var.GetBool() )
	{
		// Enabling joystick happens immediately, rather than being delayed
		if ( joystick_force_disabled.GetBool() )
		{
			joystick_force_disabled.SetValue( false );
		}
	}
}

static ConVar joy_variable_frametime( side_registry(), "joy_variable_frametime", "1", 0 );

// Axis mapping
static ConVar joy_name( side_registry(), "joy_name", "joystick", FCVAR_ARCHIVE );
static ConVar joy_advanced( side_registry(), "joy_advanced", "0", FCVAR_ARCHIVE );
static ConVar joy_advaxisx( side_registry(), "joy_advaxisx", "0", FCVAR_ARCHIVE );
static ConVar joy_advaxisy( side_registry(), "joy_advaxisy", "0", FCVAR_ARCHIVE );
static ConVar joy_advaxisz( side_registry(), "joy_advaxisz", "0", FCVAR_ARCHIVE );
static ConVar joy_advaxisr( side_registry(), "joy_advaxisr", "0", FCVAR_ARCHIVE );
static ConVar joy_advaxisu( side_registry(), "joy_advaxisu", "0", FCVAR_ARCHIVE );
static ConVar joy_advaxisv( side_registry(), "joy_advaxisv", "0", FCVAR_ARCHIVE );

// Basic "dead zone" and sensitivity
static ConVar joy_forwardthreshold( side_registry(), "joy_forwardthreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_sidethreshold( side_registry(), "joy_sidethreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_pitchthreshold( side_registry(), "joy_pitchthreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_yawthreshold( side_registry(), "joy_yawthreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_forwardsensitivity( side_registry(), "joy_forwardsensitivity", "-1", FCVAR_ARCHIVE );
static ConVar joy_sidesensitivity( side_registry(), "joy_sidesensitivity", "1", FCVAR_ARCHIVE );
static ConVar joy_pitchsensitivity( side_registry(), "joy_pitchsensitivity", "-1", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS, "joystick pitch sensitivity", true, -5.0f, true, -0.1f );
static ConVar joy_yawsensitivity( side_registry(), "joy_yawsensitivity", "-1", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS, "joystick yaw sensitivity", true, -5.0f, true, -0.1f );

// Advanced sensitivity and response
static ConVar joy_response_move( side_registry(), "joy_response_move", "1", FCVAR_ARCHIVE, "'Movement' stick response mode: 0=Linear, 1=quadratic, 2=cubic, 3=quadratic extreme, 4=power function(i.e., pow(x,1/sensitivity)), 5=two-stage" );

ConVar joy_response_move_vehicle(side_registry(), "joy_response_move_vehicle", "6");
static ConVar joy_response_look( side_registry(), "joy_response_look", "0", FCVAR_ARCHIVE, "'Look' stick response mode: 0=Default, 1=Acceleration Promotion" );
static ConVar joy_response_look_pitch( side_registry(), "joy_response_look_pitch", "1", FCVAR_ARCHIVE, "'Look' stick response mode for pitch: 0=Default, 1=Acceleration Promotion" );

static ConVar joy_lowend( side_registry(), "joy_lowend", "1", FCVAR_ARCHIVE );
static ConVar joy_lowend_linear( side_registry(), "joy_lowend_linear", "0.55", FCVAR_ARCHIVE );
static ConVar joy_lowmap( side_registry(), "joy_lowmap", "1", FCVAR_ARCHIVE );
static ConVar joy_gamma( side_registry(), "joy_gamma", "0.2", FCVAR_ARCHIVE );
static ConVar joy_accelscale( side_registry(), "joy_accelscale", "3.5", FCVAR_ARCHIVE);
static ConVar joy_accelscalepoly( side_registry(), "joy_accelscalepoly", "0.4", FCVAR_ARCHIVE);
static ConVar joy_accelmax( side_registry(), "joy_accelmax", "1.0", FCVAR_ARCHIVE);
static ConVar joy_autoAimDampenMethod( side_registry(), "joy_autoAimDampenMethod", "0", FCVAR_ARCHIVE );
static ConVar joy_autoaimdampenrange( side_registry(), "joy_autoaimdampenrange", "0", FCVAR_ARCHIVE, "The stick range where autoaim dampening is applied. 0 = off" );
static ConVar joy_autoaimdampen( side_registry(), "joy_autoaimdampen", "0", FCVAR_ARCHIVE, "How much to scale user stick input when the gun is pointing at a valid target." );
// smooth out of the auto-aim at this amount per second
static ConVar joy_autoaim_dampen_smoothout_speed( side_registry(), "joy_autoaim_dampen_smoothout_speed", "0.25" ); // percentage per second.  0.5 == 50 percentage points per second

static ConVar joy_curvepoint_1( side_registry(), "joy_curvepoint_1", "0.001", FCVAR_ARCHIVE, "", true, 0.001, true, 5 );
static ConVar joy_curvepoint_2( side_registry(), "joy_curvepoint_2", "0.4", FCVAR_ARCHIVE, "", true, 0.001, true, 5 );
static ConVar joy_curvepoint_3( side_registry(), "joy_curvepoint_3", "0.75", FCVAR_ARCHIVE, "", true, 0.001, true, 5 );
static ConVar joy_curvepoint_4( side_registry(), "joy_curvepoint_4", "1", FCVAR_ARCHIVE, "", true, 0.001, true, 5 );
static ConVar joy_curvepoint_end( side_registry(), "joy_curvepoint_end", "2", FCVAR_ARCHIVE, "", true, 0.001, true, 5 );

static ConVar joy_vehicle_turn_lowend(side_registry(), "joy_vehicle_turn_lowend", "0.7");
static ConVar joy_vehicle_turn_lowmap(side_registry(), "joy_vehicle_turn_lowmap", "0.4");

static ConVar joy_sensitive_step0( side_registry(), "joy_sensitive_step0", "0.1", FCVAR_ARCHIVE);
static ConVar joy_sensitive_step1( side_registry(), "joy_sensitive_step1", "0.4", FCVAR_ARCHIVE);
static ConVar joy_sensitive_step2( side_registry(), "joy_sensitive_step2", "0.90", FCVAR_ARCHIVE);
static ConVar joy_circle_correct( side_registry(), "joy_circle_correct", "1", FCVAR_ARCHIVE);

// Misc
static ConVar joy_diagonalpov( side_registry(), "joy_diagonalpov", "0", FCVAR_ARCHIVE, "POV manipulator operates on diagonal axes, too." );
static ConVar joy_display_input(side_registry(), "joy_display_input", "0", FCVAR_ARCHIVE);
static ConVar joy_wwhack2( side_registry(), "joy_wingmanwarrior_turnhack", "0", FCVAR_ARCHIVE, "Wingman warrior hack related to turn axes." );
ConVar joy_autosprint(side_registry(), "joy_autosprint", "0", 0, "Automatically sprint when moving with an analog joystick" );

static ConVar joy_inverty(side_registry(), "joy_inverty", "0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS, "Whether to invert the Y axis of the joystick for looking." );

// XBox Defaults
static ConVar joy_yawsensitivity_default( side_registry(), "joy_yawsensitivity_default", "-1.0", FCVAR_NONE );
static ConVar joy_pitchsensitivity_default( side_registry(), "joy_pitchsensitivity_default", "-1.0", FCVAR_NONE );
static ConVar sv_stickysprint_default( side_registry(), "sv_stickysprint_default", "0", FCVAR_NONE );
static ConVar joy_lookspin_default( side_registry(), "joy_lookspin_default", "0.35", FCVAR_NONE );

static ConVar joy_cfg_preset( side_registry(), "joy_cfg_preset", "1", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS );

void joy_movement_stick_Callback( ConVar *var, const char *pOldString, float flOldValue )
{
	if ( engine )
	{
		engine->ClientCmd_Unrestricted( "joyadvancedupdate silent\n" );
	}
}

static ConVar joy_movement_stick(side_registry(), "joy_movement_stick", "0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS, "Which stick controls movement : 0 = left stick, 1 = right stick, 2 = legacy controls", joy_movement_stick_Callback );

static ConVar joy_xcontroller_cfg_loaded( side_registry(), "joy_xcontroller_cfg_loaded", "0", 0, "If 0, the 360controller.cfg file will be executed on startup & option changes." );

ConVar joy_no_accel_jump( side_registry(), "joy_no_accel_jump", "0", FCVAR_ARCHIVE );

// Motion controller
static ConVar mc_dead_zone_radius( side_registry(), "mc_dead_zone_radius", "0.06", FCVAR_ARCHIVE, "0 to 0.9. 0 being just around the center of the screen and 1 being the edges of the screen.", true, 0.0f, true, 0.9f );
static ConVar mc_accel_band_size( side_registry(), "mc_accel_band_size", "0.5", FCVAR_ARCHIVE, "Percentage of half the screen width or height.", true, 0.01f, true, 2.0f );
static ConVar mc_max_yawrate( side_registry(), "mc_max_yawrate", "230.0", FCVAR_ARCHIVE, "(degrees/sec)", true, 10.0,true, 720.0 );
static ConVar mc_max_pitchrate( side_registry(), "mc_max_pitchrate", "100.0", FCVAR_ARCHIVE, "(degrees/sec)", true, 10.0,true, 720.0 );
static ConVar mc_turnPctPegged(side_registry(), "mc_turnPctPegged", "1.0", FCVAR_DEVELOPMENTONLY, "pegged at above this amount" );
static ConVar mc_turnPctPeggedMultiplier(side_registry(), "mc_turnPctPeggedMultiplier", "1.0", FCVAR_DEVELOPMENTONLY, "speed multiplier when pegged" );
static ConVar mc_turn_curve(side_registry(), "mc_turn_curve", "0", FCVAR_DEVELOPMENTONLY, "What type of acceleration curve to use for turning.");
static ConVar mc_screen_clamp( side_registry(), "mc_screen_clamp", "0.8f", FCVAR_DEVELOPMENTONLY, "Clamps the cursor to this much of the screen.");
static ConVar mc_force_aim_x(side_registry(), "mc_force_aim_x", "0", FCVAR_DEVELOPMENTONLY, "debug for testing player's aim");
static ConVar mc_force_aim_y(side_registry(), "mc_force_aim_y", "0", FCVAR_DEVELOPMENTONLY, "debug for testing player's aim");
static ConVar mc_always_lock_ret_on_zoom( side_registry(), "mc_always_lock_ret_on_zoom", "1", FCVAR_DEVELOPMENTONLY, "Always lock the reticle when zoomed (even for partial zoom weapons)");
static ConVar mc_max_dampening(side_registry(), "mc_max_dampening", "0.9", FCVAR_DEVELOPMENTONLY, "dampening player's aim");
static ConVar mc_dampening_blend_amount(side_registry(), "mc_dampening_blend_amount", "0.0", FCVAR_DEVELOPMENTONLY, "dampening player's aim");

static ConVar mc_max_turn_dampening(side_registry(), "mc_max_turn_dampening", "0.8", FCVAR_DEVELOPMENTONLY, "dampening player's aim while scoped");
static ConVar mc_turn_dampening_blend_amount(side_registry(), "mc_turn_dampening_blend_amount", "0.02", FCVAR_DEVELOPMENTONLY, "dampening player's aim while scoped");

static ConVar mc_zoom_out_cursor_offset_blend(side_registry(), "mc_zoom_out_cursor_offset_blend", "0.05", FCVAR_DEVELOPMENTONLY, "0.0 means snap to the new amount.");

static ConVar mc_zoomed_out_dead_zone_radius( side_registry(), "mc_zoomed_out_dead_zone_radius", "0.1", FCVAR_DEVELOPMENTONLY, "0 to 0.9. 0 being just around the center of the screen and 1 being the edges of the screen.", true, 0.0f, true, 0.9f );
static ConVar mc_zoomed_aim_style(side_registry(), "mc_zoomed_aim_style", "1", FCVAR_DEVELOPMENTONLY, "0-analog stick style. 1-pointer style.");

extern ConVar lookspring;
extern ConVar cl_forwardspeed;
extern ConVar lookstrafe;
extern ConVar in_joystick;
extern ConVar_ServerBounded *m_pitch;
extern ConVar l_pitchspeed;
extern ConVar cl_sidespeed;
extern ConVar cl_yawspeed;
extern ConVar cl_pitchdown;
extern ConVar cl_pitchup;
extern ConVar cl_pitchspeed;
extern ConVar cam_idealpitch;
extern ConVar cam_idealyaw;
extern ConVar thirdperson_platformer;
extern ConVar thirdperson_screenspace;

//-----------------------------------------------
// Response curve function for the move axes
//-----------------------------------------------
static float ResponseCurve( int curve, float x, int axis, float sensitivity )
{
	switch ( curve )
	{
	case 1:
		// quadratic
		if ( x < 0 )
			return -(x*x) * sensitivity;
		return x*x * sensitivity;

	case 2:
		// cubic
		return x*x*x*sensitivity;

	case 3:
		{
		// quadratic extreme
		float extreme = 1.0f;
		if ( fabs( x ) >= 0.95f )
		{
			extreme = 1.5f;
		}
		if ( x < 0 )
			return -extreme * x*x*sensitivity;
		return extreme * x*x*sensitivity;
		}
	case 4:
		{
			float flScale = sensitivity < 0.0f ? -1.0f : 1.0f;

			sensitivity = clamp( fabs( sensitivity ), 1.0e-8f, 1000.0f );

			float oneOverSens = 1.0f / sensitivity;
		
			if ( x < 0.0f )
			{
				flScale = -flScale;
			}

			float retval = clamp( std::powf( fabs( x ), oneOverSens ), 0.0f, 1.0f );
			return retval * flScale;
		}
		break;
	case 5:
		{
			float out = x;

			if( fabs(out) <= 0.6f )
			{
				out *= 0.5f;
			}

			out = out * sensitivity;
			return out;
		}
		break;
	case 6: // Custom for driving a vehicle!
		{
			if( axis == YAW )
			{
				// This code only wants to affect YAW axis (the left and right axis), which 
				// is used for turning in the car. We fall-through and use a linear curve on 
				// the PITCH axis, which is the vehicle's throttle. REALLY, these are the 'forward'
				// and 'side' axes, but we don't have constants for those, so we re-use the same
				// axis convention as the look stick. (sjb)
				float sign = 1;

				if( x  < 0.0 )
					sign = -1;

				x = fabs(x);

				if( x <= joy_vehicle_turn_lowend.GetFloat() )
					x = RemapVal( x, 0.0f, joy_vehicle_turn_lowend.GetFloat(), 0.0f, joy_vehicle_turn_lowmap.GetFloat() );
				else
					x = RemapVal( x, joy_vehicle_turn_lowend.GetFloat(), 1.0f, joy_vehicle_turn_lowmap.GetFloat(), 1.0f );

				return x * sensitivity * sign;
			}
			//else
			//	fall through and just return x*sensitivity below (as if using default curve)
		}
		//The idea is to create a max large walk zone surrounded by a max run zone.
	case 7:
		{
			float xAbs = fabs(x);
			if(xAbs < joy_sensitive_step0.GetFloat())
			{
				return 0;
			}
			else if (xAbs < joy_sensitive_step2.GetFloat())
			{
				return (85.0f/cl_forwardspeed.GetFloat()) * ((x < 0)? -1.0f : 1.0f);
			}
			else
			{
				return ((x < 0)? -1.0f : 1.0f);
			}
		}
		break;
	case 8: //same concept as above but with smooth speeds
		{
			float xAbs = fabs(x);
			if(xAbs < joy_sensitive_step0.GetFloat())
			{
				return 0;
			}
			else if (xAbs < joy_sensitive_step2.GetFloat())
			{
				float maxSpeed = (85.0f/cl_forwardspeed.GetFloat());
				float t = (xAbs-joy_sensitive_step0.GetFloat())
					/ (joy_sensitive_step2.GetFloat()-joy_sensitive_step0.GetFloat());
				float speed = t*maxSpeed;
				return speed * ((x < 0)? -1.0f : 1.0f);
			}
			else
			{
				float maxSpeed = 1.0f;
				float minSpeed = (85.0f/cl_forwardspeed.GetFloat());
				float t = (xAbs-joy_sensitive_step2.GetFloat())
					/ (1.0f-joy_sensitive_step2.GetFloat());
				float speed = t*(maxSpeed-minSpeed) + minSpeed;
				return speed * ((x < 0)? -1.0f : 1.0f);
			}
		}
		break;
	case 9: //same concept as above but with smooth speeds for walking and a hard speed for running
		{
			float xAbs = fabs(x);
			if(xAbs < joy_sensitive_step0.GetFloat())
			{
				return 0;
			}
			else if (xAbs < joy_sensitive_step1.GetFloat())
			{
				float maxSpeed = (85.0f/cl_forwardspeed.GetFloat());
				float t = (xAbs-joy_sensitive_step0.GetFloat())
					/ (joy_sensitive_step1.GetFloat()-joy_sensitive_step0.GetFloat());
				float speed = t*maxSpeed;
				return speed * ((x < 0)? -1.0f : 1.0f);
			}
			else if (xAbs < joy_sensitive_step2.GetFloat())
			{
				return (85.0f/cl_forwardspeed.GetFloat()) * ((x < 0)? -1.0f : 1.0f);
			}
			else
			{
				return ((x < 0)? -1.0f : 1.0f);
			}
		}
		break;
	}

	// linear
	return x*sensitivity;
}

//-----------------------------------------------
// If we have a valid autoaim target, dampen the 
// player's stick input if it is moving away from
// the target.
//
// This assists the player staying on target.
//-----------------------------------------------
float CInput::AutoAimDampening( float x, int axis, float dist )
{
	if ( joy_autoAimDampenMethod.GetInt() == 1 )	
	{
// disabled 6/29/15 -mtw
/*
		// $FIXME(hpe) Split screen

		// Help the user stay on target if the feature is enabled and the user
		// is not making a gross stick movement.
		if ( joy_autoaimdampen.GetFloat() > 0.0f && fabs(x) < joy_autoaimdampenrange.GetFloat() )
		{
			CCSPlayer *pLocalPlayer = CCSPlayer::GetLocalCSPlayer();

			if ( pLocalPlayer && pLocalPlayer->IsCursorOnAutoAimTarget() )
			{
				m_lastAutoAimValue = joy_autoaimdampen.GetFloat();
				return m_lastAutoAimValue;
			}
		}

		if ( m_lastAutoAimValue < 1.0f )
		{
			m_lastAutoAimValue += joy_autoaim_dampen_smoothout_speed.GetFloat() * gpGlobals->frametime;
			if ( m_lastAutoAimValue >= 1.0f )
			{
				m_lastAutoAimValue = 1.0f;
			}
		}
*/
		return m_lastAutoAimValue;// No dampening.
	}
	else
	{
		// Help the user stay on target if the feature is enabled and the user
		// is not making a gross stick movement.
		if ( joy_autoaimdampen.GetFloat() > 0.0f && fabs(x) < joy_autoaimdampenrange.GetFloat() )
		{
			// Get the player
			CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer();
			if ( pLocalPlayer )
			{
				// Get the autoaim target
				if ( pLocalPlayer->m_Local.m_bAutoAimTarget )
				{
					return joy_autoaimdampen.GetFloat();
				}
			}
		}
	}

	return 1.0f;// No dampening.
}

//-----------------------------------------------
// This structure holds persistent information used
// to make decisions about how to modulate analog
// stick input.
//-----------------------------------------------
typedef struct 
{
	float	envelopeScale[2];
	bool	peggedAxis[2];
	bool	axisPeggedDir[2];
} envelope_t;

envelope_t	controlEnvelope[ MAX_SPLITSCREEN_PLAYERS ];

static bool IsJoystickPegged( float input, float otherAxis )
{

	static float fPower = 0.9f; // pc

	float fMinimumVal = 0.01f; // accomodate dead zone
	float algorythmX = abs(input); 
	float algorythmY = std::max( abs(otherAxis),fMinimumVal ); 

	float fltempAlgorythmSample = std::pow(  std::pow(algorythmX,fPower)+pow(algorythmY,fPower),fPower); 

	float flJoyAddititiveDistComparison = 0.94f;
	bool result = fltempAlgorythmSample >= flJoyAddititiveDistComparison;

	return result;
}

//-----------------------------------------------
// Response curve function specifically for the 
// 'look' analog stick.
//
// when AXIS == YAW, otherAxisValue contains the 
// value for the pitch of the control stick, and
// vice-versa.
//-----------------------------------------------
ConVar joy_pegged(side_registry(), "joy_pegged", "0.75");// Once the stick is pushed this far, it's assumed pegged.
ConVar joy_virtual_peg(side_registry(), "joy_virtual_peg", "0");
float CInput::ResponseCurveLookDefault( int nSlot, float x, int axis, float otherAxis, float dist, float frametime )
{
	envelope_t &envelope = controlEnvelope[ std::max( nSlot, 0 ) ];
	float input = x;
	// float maxX = 1.0; used in prev algorithm 
	float flJoyDist = dist;//std::max( dist, std::min( joy_pegged.GetFloat(), sqrt(x*x + otherAxis*otherAxis) ) );

	bool bPegged = ( flJoyDist >= joy_pegged.GetFloat() ) || IsJoystickPegged( x, otherAxis );

	// Make X positive to make things easier, just remember whether we have to flip it back!
	bool negative = false;
	if( x < 0.0f )
	{
		negative = true;
		x *= -1;
	}

	if ( otherAxis < 0.0f )
	{
		otherAxis *= -1;
	}

	if( axis == YAW && joy_virtual_peg.GetBool() )
	{
		if( x >= 0.95f )
		{
			// User has pegged the stick
			envelope.peggedAxis[axis] = true;
			envelope.axisPeggedDir[axis] = negative;
		}
		
		if( envelope.peggedAxis[axis] == true )
		{
			// User doesn't have the stick pegged on this axis, but they used to. 
			// If the stick is physically pegged, pretend this axis is still pegged.
			if( bPegged && negative == envelope.axisPeggedDir[axis] )
			{
				// If the user still has the stick physically pegged and hasn't changed direction on
				// this axis, keep pretending they have the stick pegged on this axis.
				x = 1.0f;
			}
			else
			{
				envelope.peggedAxis[axis] = false;
			}
		}
	}

	// Perform the two-stage mapping.
	float tmpDist = dist;
	if( tmpDist > joy_lowend.GetFloat() || x > joy_lowend_linear.GetFloat() )
	{
		tmpDist = RemapValClamped( tmpDist, joy_lowend.GetFloat(), 1.0f, joy_lowmap.GetFloat(), 1.0f );

		// Accelerate.
		if( envelope.envelopeScale[axis] < 1.0f )
		{
			envelope.envelopeScale[axis] += ( frametime * joy_accelscale.GetFloat() );
			if( envelope.envelopeScale[axis] > 1.0f )
			{
				envelope.envelopeScale[axis] = 1.0f;
			}
		}

		float delta = tmpDist - joy_lowmap.GetFloat();
		tmpDist = joy_lowmap.GetFloat() + (delta * envelope.envelopeScale[axis]);
	}
	else
	{
		// Shut off acceleration
		envelope.envelopeScale[axis] = 0.0f;

		tmpDist = RemapValClamped( tmpDist, 0.0f, joy_lowend.GetFloat(), 0.0f, joy_lowmap.GetFloat() ); 

		if( tmpDist > 0.0f && joy_display_input.GetBool() )
		{
			Msg("AXIS == %d : 2:  x = :%f, otherAxis = %f\n", axis, x, otherAxis );
		}
	}

	if ( dist > 0.01f )
	{
		float newX = x;
		if ( axis == YAW )
		{
			float input = x / dist;
			input = clamp( input, -1.0f, 1.0f );
			float theta = std::acos( input );
			newX = std::cos( theta ) * tmpDist;
			//Msg( "input: %f, theta:%f, x: %f\n", input, theta, newX );
		}
		else
		{
			float input = x / dist;
			input = clamp( input, -1.0f, 1.0f );
			float theta = std::asin( input );
			newX = std::sin( theta ) * tmpDist;
			//Msg( "input: %f, theta:%f, x: %f\n", input, theta, newX );
		}
		x = newX;
	}
	/*(
	// Perform the two-stage mapping.
	if( x > joy_lowend.GetFloat() )
	{
		float highmap = 1.0f - joy_lowmap.GetFloat();
		float xNormal = x - joy_lowend.GetFloat();
		float xNormalMax = maxX - joy_lowend.GetFloat();

		float factor = xNormal / ( 1.0f - joy_lowend.GetFloat() );
		float factorMax = xNormalMax / ( 1.0f - joy_lowend.GetFloat() );
		x = joy_lowmap.GetFloat() + (highmap * factor);
		maxX = joy_lowmap.GetFloat() + (highmap * factorMax);

		//if( x > 0.0f && joy_display_input.GetBool() )
		//{
		//	Msg("AXIS == %d : 1a:  x = :%f\n", axis, x );
		//}

		// Accelerate.
		if( envelope.envelopeScale[axis] < 1.0f )
		{
			envelope.envelopeScale[axis] += ( frametime * joy_accelscale.GetFloat() );
			if( envelope.envelopeScale[axis] > 1.0f )
			{
				envelope.envelopeScale[axis] = 1.0f;
			}
		}

		float delta = x - joy_lowmap.GetFloat();
		float deltaMax = maxX - joy_lowmap.GetFloat();
		x = joy_lowmap.GetFloat() + (delta * envelope.envelopeScale[axis]);
		maxX = joy_lowmap.GetFloat() + (deltaMax * envelope.envelopeScale[axis]);

		if( x > 0.0f && joy_display_input.GetBool() )
		{
			Msg("AXIS == %d : 1b:  x = :%f, otherAxis = %f\n", axis, x, otherAxis );
		}
	}
	else
	{
		// Shut off acceleration
		envelope.envelopeScale[axis] = 0.0f;
		float factor = x / joy_lowend.GetFloat();

		x = (joy_lowmap.GetFloat() * factor);

		if( x > 0.0f && joy_display_input.GetBool() )
		{
			Msg("AXIS == %d : 2:  x = :%f, otherAxis = %f\n", axis, x, otherAxis );
		}
	}
	*/
	x *= AutoAimDampening( input, axis, dist );

	//float flDiagDiff = abs(x - otherAxis);
	//x *= std::min( maxX, 1+(otherAxis) );

	if( x > 0.0f && joy_display_input.GetBool() )
	{
		Msg("AXIS == %d : In:%f Out:%f Frametime:%f\n", axis, input, x, frametime );
	}

	if( negative )
	{
		x *= -1;
	}

	return x;
}

ConVar joy_accel_filter(side_registry(), "joy_accel_filter", "0.2");// If the non-accelerated axis is pushed farther than this, then accelerate it, too.
ConVar joy_useNewAcecelMethod(side_registry(), "joy_useNewAcecelMethod","1");
ConVar joy_useNewJoystickPegged( side_registry(), "joy_useNewJoystickPeggedTest", "0" );
float CInput::ResponseCurveLookAccelerated( int nSlot, float x, int axis, float otherAxis, float dist, float frametime )
{
	envelope_t &envelope = controlEnvelope[ std::max( nSlot, 0 ) ];

	float input = x;

	float flJoyDist = ( std::sqrt(x*x + otherAxis * otherAxis) );
	bool bIsPegged = ( flJoyDist>= joy_pegged.GetFloat() );
	if ( joy_useNewAcecelMethod.GetBool() || joy_useNewJoystickPegged.GetBool() )
	{
		bIsPegged = IsJoystickPegged( input, otherAxis );
	}

	float curvParam = joy_gamma.GetFloat() * 2.0f - 1.0f;

	// Make X positive to make arithmetic easier for the rest of this function, and
	// remember whether we have to flip it back!
	bool negative = false;
	if( x < 0.0f )
	{
		negative = true;
		x *= -1;
	}

	// Perform the two-stage mapping.
	bool bDoAcceleration = false;// Assume we won't accelerate the input

	if( bIsPegged && x > joy_accel_filter.GetFloat() )
	{
		// Accelerate this axis, since the stick is pegged and 
		// this axis is pressed farther than the acceleration filter
		// Take the lowmap value, or the input, whichever is higher, since 
		// we don't necesarily know whether this is the axis which is pegged
		if( !joy_no_accel_jump.GetBool() )
		{
			x = std::max( joy_lowmap.GetFloat(), x );
		}

		bDoAcceleration = true;
	}
	else
	{
		// Joystick is languishing in the low-end, turn off acceleration.
		envelope.envelopeScale[axis] = 0.0f;
		float factor = x / joy_lowend.GetFloat();
		if ( joy_useNewAcecelMethod.GetBool() )
		{
			float divisor = factor * curvParam + 1;
			//ReleaseAssert(divisor);
			if (divisor != 0.0f)
				x = joy_lowmap.GetFloat() * ( factor * ( curvParam + 1 ) / divisor );		
		}
		else
		{
			x = joy_lowmap.GetFloat() * factor;
		}
	}

	if( bDoAcceleration )
	{
		float flMax = joy_accelmax.GetFloat();
		if( envelope.envelopeScale[axis] < flMax && !joy_useNewAcecelMethod.GetBool() )
		{
			envelope.envelopeScale[axis] += ( frametime * joy_accelscale.GetFloat() );
			if( envelope.envelopeScale[axis] > flMax )
			{
				envelope.envelopeScale[axis] = flMax;
			}
		}
		float delta = x - joy_lowmap.GetFloat();
		x = joy_lowmap.GetFloat() + (delta * envelope.envelopeScale[axis]);

		if ( joy_useNewAcecelMethod.GetBool() )
		{
			float factor = x / joy_lowend.GetFloat();
			float divisor = factor * curvParam + 1;
			//ReleaseAssert(divisor);
			float minx = 0.0f;
			if (divisor != 0.0f)
				minx = joy_lowmap.GetFloat() * ( factor * ( curvParam + 1 ) / divisor );

			x = std::max( x, minx );		
		}
	}

	x *= AutoAimDampening( input, axis, dist );

	if( axis == YAW && input != 0.0f && joy_display_input.GetBool() )
	{
		Msg("In:%f Out:%f Frametime:%f\n", input, x, frametime );
	}

	if( negative )
	{
		x *= -1;
	}

	return x;
}

//-----------------------------------------------
//-----------------------------------------------
float CInput::ResponseCurveLookPolynomial( int nSlot, float x, int axis, float otherAxis, float dist, float frametime )
{
	// Make X positive to make things easier, just remember whether we have to flip it back!
	bool negative = false;
	if( x < 0.0f )
	{
		negative = true;
		x *= -1;
	}

	if ( otherAxis < 0.0f )
	{
		otherAxis *= -1;
	}

	envelope_t &envelope = controlEnvelope[ std::max( nSlot, 0 ) ];
	float input = x;
	float scale = std::min( 1.0f, std::sqrt(x*x+otherAxis*otherAxis) );
	bool bPegged = ( scale >= joy_pegged.GetFloat() ) || IsJoystickPegged( x, otherAxis );

	if( axis == YAW && joy_virtual_peg.GetBool() )
	{
		if( x >= 0.95f )
		{
			// User has pegged the stick
			envelope.peggedAxis[axis] = true;
			envelope.axisPeggedDir[axis] = negative;
		}

		if( envelope.peggedAxis[axis] == true )
		{
			// User doesn't have the stick pegged on this axis, but they used to. 
			// If the stick is physically pegged, pretend this axis is still pegged.
			if( bPegged && negative == envelope.axisPeggedDir[axis] )
			{
				// If the user still has the stick physically pegged and hasn't changed direction on
				// this axis, keep pretending they have the stick pegged on this axis.
				x = 1.0f;
			}
			else
			{
				envelope.peggedAxis[axis] = false;
			}
		}
	}

	//if ( bPegged )
	//{
	//	x = 1.0f;
	//}

	float flMaxOutput = 1;
	//float flMaxAccel = joy_accelmax.GetFloat();
	if( envelope.envelopeScale[axis] < 1.0f )
	{
		envelope.envelopeScale[axis] += ( frametime * joy_accelscale.GetFloat() );
		if( envelope.envelopeScale[axis] > 1.0f )
		{
			envelope.envelopeScale[axis] = 1.0f;
		}
	}
	
	//x = joy_curvepoint_3.GetFloat() + (delta * envelope.envelopeScale[axis]);

	if ( x > 0 && scale > 0 )
	{
		x = (joy_curvepoint_end.GetFloat()*sqrt(x*x*x*x*x*x) + joy_curvepoint_4.GetFloat()*sqrt(x*x*x*x*x) + joy_curvepoint_3.GetFloat()*sqrt(x*x*x*x) + joy_curvepoint_2.GetFloat()*sqrt(x*x*x) + joy_curvepoint_1.GetFloat()*sqrt(x*x) + 0.00001f*sqrt(x)) * joy_accelscalepoly.GetFloat();
		flMaxOutput = (joy_curvepoint_end.GetFloat() + joy_curvepoint_4.GetFloat() + joy_curvepoint_3.GetFloat() + joy_curvepoint_2.GetFloat() + joy_curvepoint_1.GetFloat() + 0.00001f) * joy_accelscalepoly.GetFloat();

		if( x > 0.0f && joy_display_input.GetBool() )
		{
			//Msg("scale = %f........\n", scale );
			Msg("AXIS == %d : 1b:  x = :%f, scale = %f\n", axis, x, scale );
		}

		x *= 1+((otherAxis+scale)*0.75);
	}
	else
	{
		envelope.envelopeScale[axis] = 0.0f;
	}

	// account for pushing diagonally
	float flDiagonal = 0;
	/*
	if ( x > otherAxis )
		flDiagonal = (scale-otherAxis) * (otherAxis/x);
	else if ( otherAxis > x )
		flDiagonal = (scale-x) * (x/otherAxis);

	x = std::min( flMaxOutput, x + (flDiagonal*scale) );
	*/
	//if ( bPegged )
	//	x = flMaxOutput;

	x *= envelope.envelopeScale[axis];
	x = std::min( flMaxOutput, x );

	if( x > 0.0f && joy_display_input.GetBool() )
		Msg("flDiagonal == %f : otherAxis = :%f : x = :%f, flMaxOutput = %f\n", flDiagonal, otherAxis, x, flMaxOutput );

	x *= AutoAimDampening( input, axis, dist );

	if( x > 0.0f && joy_display_input.GetBool() )
	{
		Msg("AXIS == %d : flJoyDist:%f In:%f Out:%f Frametime:%f\n", axis, scale, input, x, frametime );
	}

	if( negative )
	{
		x *= -1;
	}

	return x;
}

//-----------------------------------------------
//-----------------------------------------------
float CInput::ResponseCurveLook( int nSlot, int curve, float x, int axis, float otherAxis, float dist, float frametime )
{
	switch( curve )
	{
	case 1://Promotion of acceleration
		return ResponseCurveLookAccelerated( nSlot, x, axis, otherAxis, dist, frametime );
		break;

	case 2://Modern
		return ResponseCurveLookPolynomial( nSlot, x, axis, otherAxis, dist, frametime );
		break;

	default:
		return ResponseCurveLookDefault( nSlot, x, axis, otherAxis, dist, frametime );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Advanced joystick setup
//-----------------------------------------------------------------------------
void CInput::Joystick_Advanced( bool bSilent )
{
	m_fJoystickAdvancedInit = true;

	// called whenever an update is needed
	int	i;
	DWORD dwTemp;

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );

		PerUserInput_t &user = GetPerUser();

		// Initialize all the maps
		for ( i = 0; i < MAX_JOYSTICK_AXES; i++ )
		{
			user.m_rgAxes[i].AxisMap = GAME_AXIS_NONE;
			user.m_rgAxes[i].ControlMap = JOY_ABSOLUTE_AXIS;
		}

		if ( !joy_advanced.GetBool() )
		{
			// default joystick initialization
			// 2 axes only with joystick control
			user.m_rgAxes[JOY_AXIS_X].AxisMap = GAME_AXIS_YAW;
			user.m_rgAxes[JOY_AXIS_Y].AxisMap = GAME_AXIS_FORWARD;
		}
		else
		{
			if ( !bSilent && 
				hh == 0 && V_stricmp( joy_name.GetString(), "joystick") )
			{
				// notify user of advanced controller
				Msg( "Using joystick '%s' configuration\n", joy_name.GetString() );
			}

			static SplitScreenConVarRef s_joy_movement_stick( "joy_movement_stick" );

			bool bJoyMovementStick = s_joy_movement_stick.GetBool( hh );

			// advanced initialization here
			// data supplied by user via joy_axisn cvars
			dwTemp = ( bJoyMovementStick ) ? (DWORD)joy_advaxisu.GetInt() : (DWORD)joy_advaxisx.GetInt();
			user.m_rgAxes[JOY_AXIS_X].AxisMap = dwTemp & 0x0000000f;
			user.m_rgAxes[JOY_AXIS_X].ControlMap = dwTemp & JOY_RELATIVE_AXIS;
	
			dwTemp = ( bJoyMovementStick ) ? (DWORD)joy_advaxisr.GetInt() : (DWORD)joy_advaxisy.GetInt();
			user.m_rgAxes[JOY_AXIS_Y].AxisMap = dwTemp & 0x0000000f;
			user.m_rgAxes[JOY_AXIS_Y].ControlMap = dwTemp & JOY_RELATIVE_AXIS;

			dwTemp = (DWORD)joy_advaxisz.GetInt();
			user.m_rgAxes[JOY_AXIS_Z].AxisMap = dwTemp & 0x0000000f;
			user.m_rgAxes[JOY_AXIS_Z].ControlMap = dwTemp & JOY_RELATIVE_AXIS;

			dwTemp = ( bJoyMovementStick ) ? (DWORD)joy_advaxisy.GetInt() : (DWORD)joy_advaxisr.GetInt();
			user.m_rgAxes[JOY_AXIS_R].AxisMap = dwTemp & 0x0000000f;
			user.m_rgAxes[JOY_AXIS_R].ControlMap = dwTemp & JOY_RELATIVE_AXIS;

			dwTemp = ( bJoyMovementStick ) ? (DWORD)joy_advaxisx.GetInt() : (DWORD)joy_advaxisu.GetInt();
			user.m_rgAxes[JOY_AXIS_U].AxisMap = dwTemp & 0x0000000f;
			user.m_rgAxes[JOY_AXIS_U].ControlMap = dwTemp & JOY_RELATIVE_AXIS;
	
			dwTemp = (DWORD)joy_advaxisv.GetInt();
			user.m_rgAxes[JOY_AXIS_V].AxisMap = dwTemp & 0x0000000f;
			user.m_rgAxes[JOY_AXIS_V].ControlMap = dwTemp & JOY_RELATIVE_AXIS;

			if ( !bSilent )
			{
				Msg( "Advanced joystick settings initialized for joystick %d\n------------\n", hh + 1 );
				DescribeJoystickAxis( hh, "x axis", &user.m_rgAxes[JOY_AXIS_X] );
				DescribeJoystickAxis( hh, "y axis", &user.m_rgAxes[JOY_AXIS_Y] );
				DescribeJoystickAxis( hh, "z axis", &user.m_rgAxes[JOY_AXIS_Z] );
				DescribeJoystickAxis( hh, "r axis", &user.m_rgAxes[JOY_AXIS_R] );
				DescribeJoystickAxis( hh, "u axis", &user.m_rgAxes[JOY_AXIS_U] );
				DescribeJoystickAxis( hh, "v axis", &user.m_rgAxes[JOY_AXIS_V] );
			}
		}
	}

	// [Forrest] For CStrike 1.5 we want to load 360controller.cfg on Xbox as well as PC.
	// If we have an xcontroller on the PC, load the cfg file if it hasn't been loaded.
	// [Forrest] engine->ClientCmd didn't go through (FCVAR_CLIENTCMD_CAN_EXECUTE prevented running command).
	// Changed it to engine->ClientCmd_Unrestricted.
	ConVarRef var( "joy_xcontroller_found" );
	if ( var.IsValid() && var.GetBool() && in_joystick.GetBool() )
	{
		if ( joy_xcontroller_cfg_loaded.GetBool() == false )
		{
			for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
			{
				engine->ClientCmd_Unrestricted( "exec controller"".cfg", false, i, false );
			}
			joy_xcontroller_cfg_loaded.SetValue( 1 );
		}
	}
	else if ( joy_xcontroller_cfg_loaded.GetBool() )
	{
		for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
		{
			engine->ClientCmd_Unrestricted( "exec undo360controller.cfg", false, i, false );
		}
		joy_xcontroller_cfg_loaded.SetValue( 0 );
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : char const
//-----------------------------------------------------------------------------
char const *CInput::DescribeAxis( int index )
{
// $FIXME(hpe) the string command syntax differs from xbla; need to verify 
	switch ( index )
	{
	case GAME_AXIS_FORWARD:
		return "forward";
	case GAME_AXIS_PITCH:
		return "pitch";
	case GAME_AXIS_SIDE:
		return "strafe";
	case GAME_AXIS_YAW:
		return "yaw";
	case GAME_AXIS_NONE:
	default:
		return "n/a";
	}

	return "n/a";
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *axis - 
//			*mapping - 
//-----------------------------------------------------------------------------
void CInput::DescribeJoystickAxis( int nJoystick, char const *axis, joy_axis_t *mapping )
{
	if ( !mapping->AxisMap )
	{
		Msg( "joy%d %s:  unmapped\n", nJoystick + 1, axis );
	}
	else
	{
		Msg( "joy%d %s:  %s (%s)\n",
			nJoystick + 1,
			axis, 
			DescribeAxis( mapping->AxisMap ),
			mapping->ControlMap != 0 ? "relative" : "absolute" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Allow joystick to issue key events
// Not currently used - controller button events are pumped through the windprocs. KWD
//-----------------------------------------------------------------------------
void CInput::ControllerCommands( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Scales the raw analog value to lie withing the axis range (full range - deadzone )
//-----------------------------------------------------------------------------
float CInput::ScaleAxisValue( const float axisValue, const float axisThreshold )
{
	// Xbox scales the range of all axes in the inputsystem. PC can't do that because each axis mapping
	// has a (potentially) unique threshold value.  If all axes were restricted to a single threshold
	// as they are on the Xbox, this function could move to inputsystem and be slightly more optimal.
	float result = 0.f;
	if ( axisValue < -axisThreshold )
	{
		result = ( axisValue + axisThreshold ) / ( MAX_BUTTONSAMPLE - axisThreshold );
	}
	else if ( axisValue > axisThreshold )
	{
		result = ( axisValue - axisThreshold ) / ( MAX_BUTTONSAMPLE - axisThreshold );
	}

	return result;
}

void CInput::Joystick_SetSampleTime(float frametime)
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		m_PerUser[ i ].m_flRemainingJoystickSampleTime = frametime;
	}
}

float CInput::Joystick_GetPitch( void )
{
	if ( !ControllerModeActive() )
		return 0.0f;

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	PerUserInput_t &user = GetPerUser( nSlot );

	return user.m_flPreviousJoystickPitch;
}

float CInput::Joystick_GetYaw( void )
{
	if ( !ControllerModeActive() )
		return 0.0f;

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	PerUserInput_t &user = GetPerUser( nSlot );

	return user.m_flPreviousJoystickYaw;
}

void CInput::Joystick_Querry( float &forward, float &side, float &pitch, float &yaw )
{
	bool bAbsoluteYaw, bAbsolutePitch;
	JoyStickSampleAxes( forward, side, pitch, yaw, bAbsoluteYaw, bAbsolutePitch );
}

void CInput::Joystick_ForceRecentering( int nStick, bool bSet /*= true*/ )
{
	if ( nStick < 0 || nStick > 1 )
		return;

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	PerUserInput_t &user = GetPerUser( nSlot );

	user.m_bForceJoystickRecentering[ nStick ] = bSet;
}

extern void IN_ForceSpeedUp( );
extern void IN_ForceSpeedDown( );

bool CInput::ControllerModeActive( void )
{
	return ( in_joystick.GetInt() != 0 && m_bControllerMode );
}

//--------------------------------------------------------------------
// See if we want to use the joystick
//--------------------------------------------------------------------
bool CInput::JoyStickActive()
{
	// verify joystick is available and that the user wants to use it
	if ( !in_joystick.GetInt() || 0 == inputsystem->GetJoystickCount() )
		return false; 

	// Skip out if vgui or gameui is active
	if ( !g_pInputStackSystem->IsTopmostEnabledContext( m_hInputContext ) )
		return false;

	return true;
}

//--------------------------------------------------------------------
// Reads joystick values
//--------------------------------------------------------------------
void CInput::JoyStickSampleAxes( float &forward, float &side, float &pitch, float &yaw, bool &bAbsoluteYaw, bool &bAbsolutePitch )
{
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	PerUserInput_t &user = GetPerUser( nSlot );

	struct axis_t
	{
		float	value;
		int		controlType;
	};
	axis_t gameAxes[ MAX_GAME_AXES ];
	memset( &gameAxes, 0, sizeof(gameAxes) );

	// Get each joystick axis value, and normalize the range
	for ( int i = 0; i < MAX_JOYSTICK_AXES; ++i )
	{
		if ( GAME_AXIS_NONE == user.m_rgAxes[i].AxisMap )
			continue;

		float fAxisValue = inputsystem->GetAnalogValue( (AnalogCode_t)JOYSTICK_AXIS( GET_ACTIVE_SPLITSCREEN_SLOT(), i ) );

		if ( joy_wwhack2.GetInt() != 0 )
		{
			// this is a special formula for the Logitech WingMan Warrior
			// y=ax^b; where a = 300 and b = 1.3
			// also x values are in increments of 800 (so this is factored out)
			// then bounds check result to level out excessively high spin rates
			float fTemp = 300.0 * std::pow(abs(fAxisValue) / 800.0, 1.3);
			if (fTemp > 14000.0)
				fTemp = 14000.0;
			// restore direction information
			fAxisValue = (fAxisValue > 0.0) ? fTemp : -fTemp;
		}

		unsigned int idx = user.m_rgAxes[i].AxisMap;
		gameAxes[idx].value = fAxisValue;
		gameAxes[idx].controlType = user.m_rgAxes[i].ControlMap;
	}

	// Before these axes are allowed to return values must bring them back to mostly centered
	if ( user.m_bForceJoystickRecentering[ 0 ] )
	{
		if ( std::fabsf( gameAxes[GAME_AXIS_FORWARD].value ) < 0.1f && fabsf( gameAxes[GAME_AXIS_SIDE].value ) < 0.1f )
		{
			user.m_bForceJoystickRecentering[ 0 ] = false;
		}

		gameAxes[GAME_AXIS_FORWARD].value = 0.0f;
		gameAxes[GAME_AXIS_SIDE].value = 0.0f;
	}

	// Before these axes are allowed to return values must bring them back to mostly centered
	if ( user.m_bForceJoystickRecentering[ 1 ] )
	{
		if ( std::fabsf( gameAxes[GAME_AXIS_PITCH].value ) < 0.1f && fabsf( gameAxes[GAME_AXIS_YAW].value ) < 0.1f )
		{
			user.m_bForceJoystickRecentering[ 1 ] = false;
		}

		gameAxes[GAME_AXIS_PITCH].value = 0.0f;
		gameAxes[GAME_AXIS_YAW].value = 0.0f;
	}

	// Re-map the axis values if necessary, based on the joystick configuration
	if ( (joy_advanced.GetInt() == 0) && (in_jlook.GetPerUser( nSlot ).state & 1) )
	{
		// user wants forward control to become pitch control
		gameAxes[GAME_AXIS_PITCH] = gameAxes[GAME_AXIS_FORWARD];
		gameAxes[GAME_AXIS_FORWARD].value = 0;

		// if mouse invert is on, invert the joystick pitch value
		// Note: only absolute control support here - joy_advanced = 0
		if ( m_pitch->GetFloat() < 0.0 )
		{
			gameAxes[GAME_AXIS_PITCH].value *= -1;
		}
	}

	if ( (in_strafe.GetPerUser( nSlot ).state & 1) || lookstrafe.GetFloat() && (in_jlook.GetPerUser( nSlot ).state & 1) )
	{
		// user wants yaw control to become side control
		gameAxes[GAME_AXIS_SIDE] = gameAxes[GAME_AXIS_YAW];
		gameAxes[GAME_AXIS_YAW].value = 0;
	}

	static SplitScreenConVarRef joy_movement_stick("joy_movement_stick");
	if( joy_movement_stick.IsValid() && joy_movement_stick.GetInt( nSlot ) == 2 )
	{
		axis_t swap = gameAxes[GAME_AXIS_YAW];
		gameAxes[GAME_AXIS_YAW] = gameAxes[GAME_AXIS_SIDE];
		gameAxes[GAME_AXIS_SIDE] = swap;
	}

	forward	= ScaleAxisValue( gameAxes[GAME_AXIS_FORWARD].value, MAX_BUTTONSAMPLE * joy_forwardthreshold.GetFloat() );
	side	= ScaleAxisValue( gameAxes[GAME_AXIS_SIDE].value, MAX_BUTTONSAMPLE * joy_sidethreshold.GetFloat()  );
	pitch	= ScaleAxisValue( gameAxes[GAME_AXIS_PITCH].value, MAX_BUTTONSAMPLE * joy_pitchthreshold.GetFloat()  );
	yaw		= ScaleAxisValue( gameAxes[GAME_AXIS_YAW].value, MAX_BUTTONSAMPLE * joy_yawthreshold.GetFloat()  );

	bAbsoluteYaw = ( JOY_ABSOLUTE_AXIS == gameAxes[GAME_AXIS_YAW].controlType );
	bAbsolutePitch = ( JOY_ABSOLUTE_AXIS == gameAxes[GAME_AXIS_PITCH].controlType );

	// If we're inverting our joystick, do so
	static SplitScreenConVarRef s_joy_inverty( "joy_inverty" );
	bool isInverted = s_joy_inverty.IsValid() && s_joy_inverty.GetBool( nSlot );
	if ( !isInverted )
	{
		pitch *= -1.0f;
	}
}

//--------------------------------------------------------------------
// drive yaw, pitch and move like a screen relative platformer game
//--------------------------------------------------------------------
void CInput::JoyStickThirdPersonPlatformer( CUserCmd *cmd, float &forward, float &side, float &pitch, float &yaw )
{
	// Get starting angles
	QAngle viewangles;
	engine->GetViewAngles( viewangles );

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	PerUserInput_t &user = GetPerUser( nSlot );

	if ( forward || side )
	{
		// apply turn control [ YAW ]
		// factor in the camera offset, so that the move direction is relative to the thirdperson camera
		viewangles[ YAW ] = RAD2DEG(std::atan2(-side, -forward)) + user.m_vecCameraOffset[ YAW ];
		engine->SetViewAngles( viewangles );

		// apply movement
		Vector2D moveDir( forward, side );
		cmd->forwardmove += moveDir.Length() * cl_forwardspeed.GetFloat();
	}

	if ( pitch || yaw )
	{
		static SplitScreenConVarRef s_joy_yawsensitivity( "joy_yawsensitivity" );
		static SplitScreenConVarRef s_joy_pitchsensitivity( "joy_pitchsensitivity" );

		// look around with the camera
		user.m_vecCameraOffset[ PITCH ] += pitch * s_joy_pitchsensitivity.GetFloat( nSlot );
		user.m_vecCameraOffset[ YAW ]   += yaw * s_joy_yawsensitivity.GetFloat( nSlot );
	}

	if ( forward || side || pitch || yaw )
	{
		// update the ideal pitch and yaw
		cam_idealpitch.SetValue( user.m_vecCameraOffset[ PITCH ] - viewangles[ PITCH ] );
		cam_idealyaw.SetValue( user.m_vecCameraOffset[ YAW ] - viewangles[ YAW ] );
	}
}

//-----------------------------------------------
// Turns viewangles based on sampled joystick
//-----------------------------------------------
void CInput::JoyStickTurn( CUserCmd *cmd, float &yaw, float &pitch, float frametime, bool bAbsoluteYaw, bool bAbsolutePitch )
{
	// Get starting angles
	QAngle viewangles;
	engine->GetViewAngles( viewangles );

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	// PerUserInput_t &user = GetPerUser( nSlot );

	static SplitScreenConVarRef s_joy_yawsensitivity( "joy_yawsensitivity" );
	static SplitScreenConVarRef s_joy_pitchsensitivity( "joy_pitchsensitivity" );

	CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer();
	bool bSpecThird = pLocalPlayer && pLocalPlayer->GetObserverMode() == OBS_MODE_CHASE;

	Vector2D move( yaw, pitch );
	float dist = move.Length();
	bool	bVariableFrametime = joy_variable_frametime.GetBool();
	float	lookFrametime = bVariableFrametime ? frametime : gpGlobals->frametime;
	float   aspeed = lookFrametime * GetHud().GetFOVSensitivityAdjust();
	if ( bSpecThird )
		aspeed *= 1.5;

	// No quick turn so just sample the joystick
	float angle = 0.0f;

	// Sample the joystick.
	if ( bVariableFrametime || frametime != gpGlobals->frametime )
	{
		if ( bAbsoluteYaw )
		{
			float fAxisValue = ResponseCurveLook( nSlot, joy_response_look.GetInt(), yaw, YAW, pitch, dist, lookFrametime );
			angle = fAxisValue * s_joy_yawsensitivity.GetFloat( nSlot ) * aspeed * cl_yawspeed.GetFloat();
		}
		else
		{
			angle = yaw * s_joy_yawsensitivity.GetFloat( nSlot ) * aspeed * 180.0;
		}
	}

	// Update and apply turn control.  This may produce a new angle if we're doing a quick turn.
	angle = UpdateAndGetQuickTurnYaw( nSlot, lookFrametime, angle );

	viewangles[YAW] += angle;
	cmd->mousedx = angle;

	// apply look control
	if ( in_jlook.GetPerUser( nSlot ).state & 1 )
	{
		float angle = 0;
		if ( bVariableFrametime || frametime != gpGlobals->frametime )
		{
			if ( bAbsolutePitch )
			{
				float fAxisValue = ResponseCurveLook( nSlot, joy_response_look_pitch.GetInt(), pitch, PITCH, yaw, dist, lookFrametime );
				angle = fAxisValue * s_joy_pitchsensitivity.GetFloat( nSlot ) * aspeed * cl_pitchspeed.GetFloat();
			}
			else
			{
				angle = pitch * s_joy_pitchsensitivity.GetFloat( nSlot ) * aspeed * 180.0;
			}
		}
		viewangles[PITCH] += angle;
		cmd->mousedy = angle;
		view->StopPitchDrift();
		if ( pitch == 0.f && lookspring.GetFloat() == 0.f )
		{
			// no pitch movement
			// disable pitch return-to-center unless requested by user
			// *** this code can be removed when the lookspring bug is fixed
			// *** the bug always has the lookspring feature on
			view->StopPitchDrift();
		}
	}

	viewangles[PITCH] = clamp( viewangles[ PITCH ], -cl_pitchup.GetFloat(), cl_pitchdown.GetFloat() );
	engine->SetViewAngles( viewangles );
}

//---------------------------------------------------------------------
// Calculates strafe and forward/back motion based on sampled joystick
//---------------------------------------------------------------------
void CInput::JoyStickForwardSideControl( float forward, float side, float &joyForwardMove, float &joySideMove )
{
	joyForwardMove = joySideMove = 0.0f;

	// apply forward and side control
	if ( joy_response_move.GetInt() > 6 && joy_circle_correct.GetBool() )
	{
		// ok the 360 controller is scaled to a circular area.  our movement is scaled to the square two axis, 
		// so diagonal needs to be scaled properly to full speed.

		bool bInWalk = true;
		float scale = std::min(1.0f,std::sqrt(forward*forward+side*side));
		if ( scale > 0.01f )
		{
			float val;
			if ( scale > joy_sensitive_step2.GetFloat() )
			{
				bInWalk = false;
			}
			float scaledVal = ResponseCurve( joy_response_move.GetInt(), scale, PITCH, std::fabsf( joy_forwardsensitivity.GetFloat() ) );
			val = scaledVal * ( ( forward * Sign( joy_forwardsensitivity.GetFloat() ) ) / scale );
			joyForwardMove += val * cl_forwardspeed.GetFloat();

			scaledVal = ResponseCurve( joy_response_move.GetInt(), scale, PITCH, fabsf( joy_sidesensitivity.GetFloat() ) );
			val = scaledVal  * ( ( side * Sign( joy_sidesensitivity.GetFloat() ) ) / scale );
			joySideMove += val * cl_sidespeed.GetFloat();

			// big hack here, if we are not moving past the joy_sensitive_step2 thresh hold then walk.
			if ( bInWalk )
			{
				IN_ForceSpeedDown();
			}
			else
			{
				IN_ForceSpeedUp();
			}
		}
		else
		{
			IN_ForceSpeedUp();
		}
	}
	else
	{
		// apply forward and side control
		CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer();

		int iResponseCurve = 0;
		{
			iResponseCurve = joy_response_move.GetInt();
		}	

		float val = ResponseCurve( iResponseCurve, forward, PITCH, joy_forwardsensitivity.GetFloat() );
		joyForwardMove	+= val * cl_forwardspeed.GetFloat();
		val = ResponseCurve( iResponseCurve, side, YAW, joy_sidesensitivity.GetFloat() );
		joySideMove		+= val * cl_sidespeed.GetFloat();
	}
}

// expects a -1.0 - 1.0 value
// returns a 0.0 - 1.2 value, signed to match the input
float CInput::HandleMotionControllerInputSmoothing( float flDeadZonePct, float val )
{	
	bool isPositive = val > 0.0f;
	float absVal = abs(val);
	if ( absVal <= flDeadZonePct )
		return 0.0f;

	// Allow player to point off the screen if they've made the dead zone the size of the screen.
	float flBandSize = mc_accel_band_size.GetFloat();
	float normalizedAfterDeadzone = (absVal - flDeadZonePct) / flBandSize;
	
	float result = 0.0f;
	if ( normalizedAfterDeadzone > mc_turnPctPegged.GetFloat() )
	{
		// in our high acceleration zone, bump up the value
		result = mc_turnPctPeggedMultiplier.GetFloat();
	}
	else
	{
		// low acceleration
		// X*X method
		switch (mc_turn_curve.GetInt() )
		{
		case 0:
			result = normalizedAfterDeadzone;
			break;
		case 1:
			result = normalizedAfterDeadzone * normalizedAfterDeadzone;
			break;
		case 2:
			result = normalizedAfterDeadzone * normalizedAfterDeadzone * normalizedAfterDeadzone;
			break;

		}
	}	

	result = isPositive ? result : result * -1.0f;
	return result;
}

//-----------------------------------------------------------------------------
// Purpose: Apply motion controller to CUserCmd creation
// Input  : frametime - 
//			*cmd - 
//-----------------------------------------------------------------------------
void CInput::MotionControllerMove( float frametime, CUserCmd *cmd )
{
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	CCSPlayer* pPlayer = CCSPlayer::GetLocalCSPlayer();
	contract_assert( pPlayer );

	// [dkorus] make sure our max turn rate is based on our zoom level sensitivity
	float flDeadZonePct = mc_dead_zone_radius.GetFloat();
	float flScreenClamp = mc_screen_clamp.GetFloat();

	int width=0, height=0;
	materials->GetBackBufferDimensions( width, height );

	float fX = inputsystem->GetMotionControllerPosX();
	float fY = inputsystem->GetMotionControllerPosY();

	static float s_lastCursorValueX = fX;
	static float s_lastCursorValueY = fY;

	static QAngle s_referenceDirection;
	static float s_FOVSensitivityAdjust = 1.0f;
	static float s_FOVOffsetX = 0.0f;
	static float s_FOVOffsetY = 0.0f;
	static float s_TargetFOVOffsetX = 0.0f;
	static float s_TargetFOVOffsetY = 0.0f;

	// Don't consider pointer input unless we're actively in the game.  Otherwise our view will change outside of when we want it to.
	int iObserverMode = pPlayer->GetObserverMode();
	bool ignorePointerInput = ( iObserverMode == OBS_MODE_DEATHCAM || iObserverMode == OBS_MODE_FREEZECAM );

	// If we're in the pause menu, then lock the cursor to the screen.
	if ( ignorePointerInput  )
	{
		// Move this to center screen.
		fX = s_lastCursorValueX * 0.85f;
		fY = s_lastCursorValueY * 0.85f;
	}

	bool bLookingAtTarget = pPlayer->IsCursorOnAutoAimTarget();

	// Free moving cursor dampening code.
	static float s_fDampeningValue = 0.0f;  // s_fDampeningValue: 0.0 don't dampen, 1.0 fully locked in place.
	float fTargetDampening = 0.0f;
	if ( bLookingAtTarget )
	{
		fTargetDampening = mc_max_dampening.GetFloat();
	}
	// This little bit of code gives us a frame rate independent blend value.
	float blend_t = 1.0f - std::pow( mc_dampening_blend_amount.GetFloat(), frametime );
	s_fDampeningValue += ( fTargetDampening - s_fDampeningValue )*blend_t;

	// Turn Dampening code.
	static float s_fTurnDampeningValue = 0.0f;
	float fTargetTurnDampening = 0.0f;
	if ( bLookingAtTarget )
	{
		fTargetTurnDampening = mc_max_turn_dampening.GetFloat();
	}
	// This little bit of code gives us a frame rate independent blend value.
	blend_t = 1.0f - pow( mc_turn_dampening_blend_amount.GetFloat(), frametime );
	s_fTurnDampeningValue += ( fTargetTurnDampening - s_fTurnDampeningValue )*blend_t;

	// handle turning the view with the motion controller
	QAngle currentViewAngles;
	engine->GetViewAngles( currentViewAngles );

	if ( mc_zoomed_aim_style.GetInt() == 1 )
	{
		if ( !pPlayer->m_bIsScoped )
		{
			// Only set the reference direction to the current view when we are not zoomed in.
			s_referenceDirection = currentViewAngles;
		}
	}
	else
	{
		s_referenceDirection = currentViewAngles;
	}

	CWeaponCSBase *pWeapon = ( CWeaponCSBase* )pPlayer->GetActiveWeapon();

	float fTurnDampeningMultiplier = 1.0f;
	if ( pWeapon )
	{
		if ( mc_zoomed_aim_style.GetInt() == 1 )
		{
			// For non scoped weapons like the bomb and knife, only do clamping if we're not scoped.
			if ( !pWeapon->WantReticleShown() && !pPlayer->m_bIsScoped )
			{
				flScreenClamp = 0.0f;
				flDeadZonePct = mc_zoomed_out_dead_zone_radius.GetFloat();
			}
		}
		else
		{
			// Lock reticle if we're using a scoped weapon with our motion controller.
			// We do this by setting the screen clamp to zero so that we move like with an analog stick.
			// We also want to lock when we have a sniper rifle because the reticule will not show up.
			if ( !pWeapon->WantReticleShown() || 
				( mc_always_lock_ret_on_zoom.GetBool() && pPlayer->m_bIsScoped ) )
			{
				flScreenClamp = 0.0f;

				// If in addition to not showing the reticule we are also zoomed in, apply dampening to turning.
				if ( pPlayer->m_bIsScoped )
				{
					// If we're zoomed in, we want to apply the dampening to turning to help lock onto the enemies.
					fTurnDampeningMultiplier = (1.0f - s_fTurnDampeningValue);
					// Zoomed in weapons get a zero dead zone to make aiming smoother.
					flDeadZonePct = 0.0f;
				}
				else
				{
					// We add a bit of deadzone when zoomed out so that world navigation is easier.
					// It also helps to keep your aim on your target after firing if the weapon pops back to zoomed out.
					flDeadZonePct = mc_zoomed_out_dead_zone_radius.GetFloat();
				}
			}

		}

	}

	// We increase the deadzone size for pitch by 1.5
	const float flPitchDeadZoneScale = 1.5f;
	float pitchDeadZone = clamp( flDeadZonePct * flPitchDeadZoneScale, 0.0f, 0.9f );

	// Calculate the pitch and yaw deltas.
	const float flCurrFOVScale = GetHud().GetFOVSensitivityAdjust();

	if ( s_FOVSensitivityAdjust != flCurrFOVScale )
	{
		if ( flCurrFOVScale == 1.0f )
		{
			// Set the offset to put the cursor in the middle of the screen.
			s_FOVOffsetX = -s_lastCursorValueX * flCurrFOVScale;
			s_FOVOffsetY = -s_lastCursorValueY * flCurrFOVScale;

			// Now we want to zero the offset over time.
			s_TargetFOVOffsetX = 0.0f;
			s_TargetFOVOffsetY = 0.0f;
		}
		else
		{
			// We're zooming in on an offset position, so we need to an offset compensation here.
			// save off the offsets from the last cursor value.
			// The last cursor position on the screen was
			float lastAdjustedX = s_lastCursorValueX * s_FOVSensitivityAdjust + s_FOVOffsetX;
			float lastAdjustedY = s_lastCursorValueY * s_FOVSensitivityAdjust + s_FOVOffsetY;

			// With the new FOV scale, the offsets to give us the same position is:
			s_TargetFOVOffsetX = lastAdjustedX - s_lastCursorValueX * flCurrFOVScale;
			s_TargetFOVOffsetY = lastAdjustedY - s_lastCursorValueY * flCurrFOVScale;

			// Set the offset immediately since we don't need or want to blend to this.
			// The blending is for zooming out when the cursor will not be pointing where we were looking.
			s_FOVOffsetX = s_TargetFOVOffsetX;
			s_FOVOffsetY = s_TargetFOVOffsetY;
		}
		s_FOVSensitivityAdjust = flCurrFOVScale;
	}

	blend_t = 1.0f - pow( mc_zoom_out_cursor_offset_blend.GetFloat(), frametime );
	s_FOVOffsetX += ( s_TargetFOVOffsetX - s_FOVOffsetX ) * blend_t;
	s_FOVOffsetY += ( s_TargetFOVOffsetY - s_FOVOffsetY ) * blend_t;

	float flMaxTurnRate = mc_max_yawrate.GetFloat() * s_FOVSensitivityAdjust; 
	float flMaxPitchRate = mc_max_pitchrate.GetFloat() * s_FOVSensitivityAdjust;
	float smoothedX = HandleMotionControllerInputSmoothing( flDeadZonePct, fX );
	float smoothedY = HandleMotionControllerInputSmoothing( pitchDeadZone, fY );
	float deltaYaw = -smoothedX * flMaxTurnRate * frametime * fTurnDampeningMultiplier;
	float deltaPitch = -smoothedY * flMaxPitchRate * frametime * fTurnDampeningMultiplier;

	// Update and apply turn control.  This may produce a new angle if we're doing a quick turn.
	deltaYaw = UpdateAndGetQuickTurnYaw( nSlot, frametime, deltaYaw );

	s_referenceDirection[YAW] += deltaYaw;
	s_referenceDirection[PITCH] += deltaPitch;

	if ( joy_autoAimDampenMethod.GetInt() == 1 )
	{
		// If we are dampening, we reduce the amount we update towards our target vector.
		fX = s_lastCursorValueX + (fX - s_lastCursorValueX) * (1.0f - s_fDampeningValue);
		fY = s_lastCursorValueY + (fY - s_lastCursorValueY) * (1.0f - s_fDampeningValue);
	}

	fX = clamp( fX, -flScreenClamp, flScreenClamp );
	fY = clamp( fY, -flScreenClamp, flScreenClamp );

	float adjustedX = fX * s_FOVSensitivityAdjust + s_FOVOffsetX;
	float adjustedY = fY * s_FOVSensitivityAdjust + s_FOVOffsetY;

	Vector forward, right, up;
	// Get the orientation matrix for the camera.
	AngleVectors (s_referenceDirection, &forward, &right, &up );

	// Forward project the cursor position into the world.
	// dist is the distance from the camera to the screen.
	// NOTE!  GAME_FOV_YAW needs to be the same value returned from CCSGameRules::DefaultFOV().
	const float GAME_FOV_YAW = 90.0f;
	float dist = ( width * 0.5f ) / std::tanf( GAME_FOV_YAW*0.5f );
	Vector aimDirection = forward*dist + up*height*0.5f*adjustedY + right*width*0.5f*adjustedX;
	aimDirection.NormalizeInPlace();
	pPlayer->SetAimDirection( aimDirection );

	Vector PureForward( 1.0f, 0.0f, 0.0f );
	Vector PureUp( 0.0f, 0.0f, 1.0f);
	Vector PureRight( 0.0f, -1.0f, 0.0f );
	QAngle viewOffset;

	Vector pureOffset = PureForward*dist + PureUp*height*0.5f*adjustedY + PureRight*width*0.5f*adjustedX;
	VectorAngles( pureOffset, viewOffset );
	
	if ( mc_force_aim_x.GetFloat() != 0.0f )
	{
		Vector eyePos = pPlayer->EyePosition();
		Vector forcedAimPoint = forward*dist + up*height*0.5f*mc_force_aim_y.GetFloat() + right*width*0.5f*mc_force_aim_x.GetFloat();

		const float DURATION = 0.15f;
		DebugDrawLine( eyePos + forcedAimPoint, eyePos + forcedAimPoint + up*height*0.05f, 0,255,0, true, DURATION );
		DebugDrawLine( eyePos + forcedAimPoint, eyePos + forcedAimPoint + right*width*0.05f, 255, 0, 0, true, DURATION );
		DebugDrawLine( eyePos + forcedAimPoint, eyePos + forcedAimPoint - up*height*0.05f, 255, 255, 0, true, DURATION );
		DebugDrawLine( eyePos + forcedAimPoint, eyePos + forcedAimPoint - right*width*0.05f, 255,0,255, true, DURATION );

		forcedAimPoint.NormalizeInPlace();
		pPlayer->SetAimDirection( forcedAimPoint );
		
		Vector pureOffset2 = PureForward*dist + PureUp*height*0.5f*mc_force_aim_y.GetFloat() + PureRight*width*0.5f*mc_force_aim_x.GetFloat();
		VectorAngles( pureOffset2, viewOffset );
	}

	pPlayer->SetEyeAngleOffset( viewOffset );

	// Update the camera's angle.
	if ( mc_zoomed_aim_style.GetInt() == 1 && pPlayer->m_bIsScoped )
	{
		QAngle aimDirectionAngles;
		VectorAngles(aimDirection, aimDirectionAngles);
		engine->SetViewAngles( aimDirectionAngles );
	}
	else
	{
		engine->SetViewAngles( s_referenceDirection );
	}

	s_lastCursorValueX = fX;
	s_lastCursorValueY = fY;

	cmd->aimdirection = pPlayer->GetAimDirection();
	cmd->mousedx = deltaYaw;
	cmd->mousedy = deltaPitch;
}

//-----------------------------------------------------------------------------
// Purpose: Apply joystick to CUserCmd creation
// Input  : frametime - 
//			*cmd - 
//-----------------------------------------------------------------------------
void CInput::JoyStickMove( float frametime, CUserCmd *cmd )
{
	// complete initialization if first time in ( needed as cvars are not available at initialization time )
	if ( !m_fJoystickAdvancedInit )
	{
		Joystick_Advanced( false );
	}

	// verify joystick is available and that the user wants to use it
	if ( !in_joystick.GetInt() || 0 == inputsystem->GetJoystickCount() )
		return; 

	// Skip out if vgui is active
	if ( RocketUI()->IsConsumingInput() )
		return;

	// Don't move if GameUI is visible
	if ( engineui->IsGameUIVisible() )
		return;

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	PerUserInput_t &user = GetPerUser( nSlot );

	// Sample the axes, apply the input, and consume sample time.
	if ( user.m_flRemainingJoystickSampleTime > 0 )
	{
		frametime = std::min(user.m_flRemainingJoystickSampleTime, frametime);
		user.m_flRemainingJoystickSampleTime -= frametime;

		float forward, side, pitch, yaw;
		bool bAbsoluteYaw, bAbsolutePitch;

		JoyStickSampleAxes( forward, side, pitch, yaw, bAbsoluteYaw, bAbsolutePitch );
		
		if ( !m_bControllerMode )
		{
			if ( std::fabsf(forward) > 0.1f || fabsf(side) > 0.1f || fabsf(pitch) > 0.1f || fabsf(yaw) > 0.1f )
			{
				m_bControllerMode = true;
			}
		}
			
		if ( CAM_IsThirdPerson() && thirdperson_platformer.GetInt() )
		{
			JoyStickThirdPersonPlatformer( cmd, forward, side, pitch, yaw );
			return;
		}

		float	joyForwardMove, joySideMove;
		JoyStickForwardSideControl( forward, side, joyForwardMove, joySideMove );

		// Cache off the input sample values in case we run out of sample time.
		user.m_flPreviousJoystickForwardMove = joyForwardMove;
		user.m_flPreviousJoystickSideMove = joySideMove;
		user.m_flPreviousJoystickYaw = yaw;
		user.m_flPreviousJoystickPitch = pitch;
		user.m_bPreviousJoystickUseAbsoluteYaw = bAbsoluteYaw;
		user.m_bPreviousJoystickUseAbsolutePitch = bAbsolutePitch;
	}

	if ( JoyStickActive() )
	{
		// If we are using a motion controller, then we use the pointing device for updating the look direction.
		if( inputsystem->MotionControllerActive())
		{
			MotionControllerMove( frametime, cmd );
		}
		else
		{
			JoyStickTurn( cmd,
				user.m_flPreviousJoystickYaw,
				user.m_flPreviousJoystickPitch,
				frametime,
				user.m_bPreviousJoystickUseAbsoluteYaw,
				user.m_bPreviousJoystickUseAbsolutePitch );
		}

		JoyStickApplyMovement( cmd,
			user.m_flPreviousJoystickForwardMove,
			user.m_flPreviousJoystickSideMove );
	}
}

//--------------------------------------------------------------
// Applies the calculated forward/side movement to the UserCmd
//--------------------------------------------------------------
void CInput::JoyStickApplyMovement( CUserCmd *cmd, float joyForwardMove, float joySideMove )
{
	// apply player motion relative to screen space
	if ( CAM_IsThirdPerson() && thirdperson_screenspace.GetInt() )
	{
		float ideal_yaw = cam_idealyaw.GetFloat();
		float ideal_sin = std::sin(DEG2RAD(ideal_yaw));
		float ideal_cos = std::cos(DEG2RAD(ideal_yaw));
		float side_movement = ideal_cos*joySideMove - ideal_sin*joyForwardMove;
		float forward_movement = ideal_cos*joyForwardMove + ideal_sin*joySideMove;
		cmd->forwardmove += forward_movement;
		cmd->sidemove += side_movement;
	}
	else
	{
		cmd->forwardmove += joyForwardMove;
		cmd->sidemove += joySideMove;
	}

	CCommand tmp;
	if ( FloatMakePositive(joyForwardMove) >= joy_autosprint.GetFloat() || FloatMakePositive(joySideMove) >= joy_autosprint.GetFloat() )
	{
		KeyDown( &in_joyspeed, nullptr );
	}
	else
	{
		KeyUp( &in_joyspeed, nullptr );
	}
}

float CInput::UpdateAndGetQuickTurnYaw( int nSlot, float frametime, float angle )
{
	PerUserInput_t &user = GetPerUser( nSlot );

	if ( user.m_flSpinFrameTime )
	{
		// apply specified yaw velocity until duration expires
		float delta = frametime;
		if ( user.m_flSpinFrameTime - delta <= 0 )
		{
			// effect expired, avoid floating point creep
			delta = user.m_flSpinFrameTime;
			user.m_flSpinFrameTime = 0;
		}
		else
		{
			user.m_flSpinFrameTime -= delta;
		}

		// Modify the angle if we're doing a quick turn.
		angle = user.m_flSpinRate * delta;
	}

	// Update the spin rate
	CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer( nSlot );
	if ( ( in_lookspin.GetPerUser( nSlot ).state & 2 ) && !user.m_flSpinFrameTime && pLocalPlayer && !pLocalPlayer->IsObserver() )
	{
		// user has actuated a new spin boost
		float spinFrameTime = joy_lookspin_default.GetFloat();
		user.m_flSpinFrameTime = spinFrameTime;
		// yaw velocity is in last known direction
		if ( user.m_flLastYawAngle >= 0 )
		{ 
			user.m_flSpinRate = 180.0f/spinFrameTime;
		}
		else
		{
			user.m_flSpinRate = -180.0f/spinFrameTime;
		}
	}

	// Save off the last angle if non zero.
	if ( angle != 0.0f )
	{
		// track angular direction
		user.m_flLastYawAngle = angle;
	}

	return angle;
}

class CUserCmd;
class CBaseCombatWeapon;
class CBasePlayer;
struct kbutton_t;

using namespace ::ks::reflect;

// Purpose: builds an intended movement command to send to the server

extern ConVar cam_idealpitch;
extern ConVar cam_idealyaw;
// For showing/hiding the scoreboard

int in_impulse[ MAX_SPLITSCREEN_PLAYERS ];
static int in_cancel[ MAX_SPLITSCREEN_PLAYERS ];
ConVar cl_anglespeedkey( side_registry(), "cl_anglespeedkey", "0.67", 0 );
ConVar cl_yawspeed( side_registry(), "cl_yawspeed", "210", 0 );
ConVar cl_pitchspeed( side_registry(), "cl_pitchspeed", "225", 0 );
ConVar cl_pitchdown( side_registry(), "cl_pitchdown", "89", FCVAR_CHEAT );
ConVar cl_pitchup( side_registry(), "cl_pitchup", "89", FCVAR_CHEAT );
ConVar cl_upspeed( side_registry(), "cl_upspeed", "320", FCVAR_CHEAT );
ConVar lookspring( side_registry(), "lookspring", "0", FCVAR_ARCHIVE );
ConVar lookstrafe( side_registry(), "lookstrafe", "0", FCVAR_ARCHIVE );

inline constexpr const char *MAX_LINEAR_SPEED = "450";

ConVar cl_sidespeed( side_registry(), "cl_sidespeed", MAX_LINEAR_SPEED, FCVAR_CHEAT );
ConVar cl_forwardspeed( side_registry(), "cl_forwardspeed", MAX_LINEAR_SPEED, FCVAR_CHEAT );
ConVar cl_backspeed( side_registry(), "cl_backspeed", MAX_LINEAR_SPEED, FCVAR_CHEAT );

void IN_JoystickChangedCallback_f( ConVar *pConVar, const char *pOldString, float flOldValue );
ConVar in_joystick( side_registry(), "joystick", "1", FCVAR_ARCHIVE, "True if the joystick is enabled, false otherwise.", true, 0.0f, true, 1.0f, IN_JoystickChangedCallback_f );

ConVar thirdperson_platformer( side_registry(), "thirdperson_platformer", "0", 0, "Player will aim in the direction they are moving." );
ConVar thirdperson_screenspace( side_registry(), "thirdperson_screenspace", "0", 0, "Movement will be relative to the camera, eg: left means screen-left" );

ConVar sv_noclipduringpause( side_registry(), "sv_noclipduringpause", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "If cheats are enabled, then you can noclip with the game paused (for doing screenshots, etc.)." );
static ConVar cl_lagcomp_errorcheck( side_registry(), "cl_lagcomp_errorcheck", "0", 0, "Player index of other player to check for position errors." );

static ConVar option_duck_method( side_registry(), "option_duck_method", "0", FCVAR_ARCHIVE | FCVAR_SS  );// 0 = HOLD to duck, 1 = Duck is a toggle
static ConVar option_speed_method( side_registry(), "option_speed_method", "0", FCVAR_ARCHIVE | FCVAR_SS );// 0 = HOLD to go slow speed, 1 = speed is a toggle
static ConVar round_start_reset_duck( side_registry(), "round_start_reset_duck", "0", 0 );
static ConVar round_start_reset_speed( side_registry(), "round_start_reset_speed", "0", 0 );

bool UsingMouselook( int nSlot ) 
{
	static SplitScreenConVarRef s_MouseLook( "cl_mouselook" );
	return s_MouseLook.GetBool( nSlot );
}

ConVar in_forceuser( side_registry(), "in_forceuser", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Force user input to this split screen player." );
static ConVar ss_mimic( side_registry(), "ss_mimic", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Split screen users mimic base player's CUserCmds" );

static void SplitScreenTeleport( int nSlot )
{
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer( nSlot );
	if ( !pPlayer )
		return;

	Vector vecOrigin = pPlayer->GetAbsOrigin();
	QAngle angles = pPlayer->GetAbsAngles();

	int nOther = ( nSlot + 1 ) % MAX_SPLITSCREEN_PLAYERS;

	if ( CBasePlayer::GetLocalPlayer( nOther ) )
	{
		char cmd[ 256 ];
		V_snprintf( cmd, sizeof( cmd ), "cmd%d setpos %f %f %f;setang %f %f %f\n",
			nOther + 1,
			vecOrigin.x, vecOrigin.y, vecOrigin.z,
			angles.x, angles.y, angles.z );
		engine->ClientCmd( cmd );
	}
}

static void ss_teleport( const CCommand &args );
static ConCommand ss_teleport_command( side_registry(), "ss_teleport", ss_teleport, "Teleport other splitscreen player to my location.", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
static void ss_teleport( const CCommand &args )
{
	SplitScreenTeleport( GET_ACTIVE_SPLITSCREEN_SLOT() );
}

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition

===============================================================================
*/
kbutton_t	in_speed;
kbutton_t	in_walk;
kbutton_t	in_jlook;
kbutton_t	in_strafe;
kbutton_t	in_commandermousemove;
kbutton_t	in_forward;
kbutton_t	in_back;
kbutton_t	in_moveleft;
kbutton_t	in_moveright;
// Display the netgraph
kbutton_t	in_graph;  
kbutton_t	in_joyspeed;		// auto-speed key from the joystick (only works for player movement, not vehicles)
kbutton_t	in_ducktoggle;
kbutton_t	in_speedtoggle;
kbutton_t	in_lookspin;

kbutton_t	in_attack;
kbutton_t	in_attack2;
kbutton_t	in_zoom;

static	kbutton_t	in_klook;
static	kbutton_t	in_left;
static	kbutton_t	in_right;
static	kbutton_t	in_lookup;
static	kbutton_t	in_lookdown;
static	kbutton_t	in_use;
static	kbutton_t	in_jump;

static	kbutton_t	in_up;
static	kbutton_t	in_down;
static	kbutton_t	in_duck;
static	kbutton_t	in_reload;
static	kbutton_t	in_alt1;
static	kbutton_t	in_alt2;
static	kbutton_t	in_score;
static	kbutton_t	in_break;
static  kbutton_t   in_grenade1;
static  kbutton_t   in_grenade2;

bool		joystick_forced_speed = false;

/*
===========
IN_CenterView_f
===========
*/
void IN_CenterView_f (void)
{
	QAngle viewangles;

	if ( UsingMouselook( GET_ACTIVE_SPLITSCREEN_SLOT() ) == false )
	{
		if ( !::input->CAM_InterceptingMouse() )
		{
			engine->GetViewAngles( viewangles );
			viewangles[PITCH] = 0;
			engine->SetViewAngles( viewangles );
		}
	}
}

/*
===========
IN_Joystick_Advanced_f
===========
*/
void IN_Joystick_Advanced_f (const CCommand& args)
{
	::input->Joystick_Advanced( args.ArgC() == 2 );
}

void IN_JoystickChangedCallback_f( ConVar *pConVar, const char *pOldString, float flOldValue )
{
	::input->Joystick_Advanced( true );
}

/*
============
KB_ConvertString

Removes references to +use and replaces them with the keyname in the output string.  If
 a binding is unfound, then the original text is retained.
NOTE:  Only works for text with +word in it.
============
*/
int KB_ConvertString( char *in, char **ppout )
{
	char sz[ 4096 ];
	char binding[ 64 ];
	char *p;
	char *pOut;
	char *pEnd;
	const char *pBinding;

	if ( !ppout )
		return 0;

	*ppout = nullptr;
	p = in;
	pOut = sz;
	while ( *p )
	{
		if ( *p == '+' )
		{
			pEnd = binding;
			while ( *p && ( V_isalnum( *p ) || ( pEnd == binding ) ) && ( ( pEnd - binding ) < 63 ) )
			{
				*pEnd++ = *p++;
			}

			*pEnd =  '\0';

			pBinding = nullptr;
			if ( strlen( binding + 1 ) > 0 )
			{
				// See if there is a binding for binding?
				pBinding = engine->Key_LookupBinding( binding + 1 );
			}

			if ( pBinding )
			{
				*pOut++ = '[';
				pEnd = (char *)pBinding;
			}
			else
			{
				pEnd = binding;
			}

			while ( *pEnd )
			{
				*pOut++ = *pEnd++;
			}

			if ( pBinding )
			{
				*pOut++ = ']';
			}
		}
		else
		{
			*pOut++ = *p++;
		}
	}

	*pOut = '\0';

	int maxlen = strlen( sz ) + 1;
	pOut = ( char * )MemAlloc_Alloc( maxlen );
	V_strncpy( pOut, sz, maxlen );
	*ppout = pOut;

	return 1;
}

/*
==============================
FindKey

Allows the engine to request a kbutton handler by name, if the key exists.
==============================
*/
kbutton_t *CInput::FindKey( const char *name )
{
	CKeyboardKey *p;
	p = m_pKeys;
	while ( p )
	{
		if ( !V_stricmp( name, p->name ) )
		{
			return p->pkey;
		}

		p = p->next;
	}
	return nullptr;
}

/*
============
AddKeyButton

Add a kbutton_t * to the list of pointers the engine can retrieve via KB_Find
============
*/
void CInput::AddKeyButton( const char *name, kbutton_t *pkb )
{
	CKeyboardKey *p;	
	kbutton_t *kb;

	kb = FindKey( name );
	
	if ( kb )
		return;

	p = new CKeyboardKey;

	V_strncpy( p->name, name, sizeof( p->name ) );
	p->pkey = pkb;

	p->next = m_pKeys;
	m_pKeys = p;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CInput::CInput( void )
{
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		m_PerUser[ i ].m_pCommands = nullptr;
		m_PerUser[ i ].m_pCameraThirdData = nullptr;
		m_PerUser[ i ].m_pVerifiedCommands = nullptr;
	}
	m_lastAutoAimValue = 1.0f; 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CInput::~CInput( void )
{
}

/*
============
Init_Keyboard

Add kbutton_t definitions that the engine can query if needed
============
*/
void CInput::Init_Keyboard( void )
{
	m_pKeys = nullptr;

	AddKeyButton( "in_graph", &in_graph );
	AddKeyButton( "in_jlook", &in_jlook );
	AddKeyButton( "in_reload", &in_reload );
}

/*
============
Shutdown_Keyboard

Clear kblist
============
*/
void CInput::Shutdown_Keyboard( void )
{
	CKeyboardKey *p, *n;
	p = m_pKeys;
	while ( p )
	{
		n = p->next;
		delete p;
		p = n;
	}
	m_pKeys = nullptr;
}

kbutton_t::Split_t &kbutton_t::GetPerUser( int nSlot /*=-1*/ )
{
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	}
	return m_PerUser[ nSlot ];
}

/*
============
KeyDown
============
*/
void KeyDown( kbutton_t *b, const char *c )
{
	kbutton_t::Split_t &data = b->GetPerUser();

	int		k = -1;
	if ( c && c[0] )
	{
		k = atoi(c);
	}

	if (k == data.down[0] || k == data.down[1])
		return;		// repeating key
	
	if (!data.down[0])
		data.down[0] = k;
	else if (!data.down[1])
		data.down[1] = k;
	else
	{
		if ( c[0] )
		{
			DevMsg( 1,"Three keys down for a button '%c' '%c' '%c'!\n", data.down[0], data.down[1], c[0]);
		}
		return;
	}
	
	if (data.state & 1)
		return;		// still down
	data.state |= 1 + 2;	// down + impulse down
}

/*
============
KeyUp
============
*/
void KeyUp( kbutton_t *b, const char *c )
{	
	kbutton_t::Split_t &data = b->GetPerUser();
	if ( !c || !c[0] )
	{
		data.down[0] = data.down[1] = 0;
		data.state = 4;	// impulse up
		return;
	}

	int k = atoi(c);

	if (data.down[0] == k)
		data.down[0] = 0;
	else if (data.down[1] == k)
		data.down[1] = 0;
	else
		return;		// key up without coresponding down (menu pass through)

	if (data.down[0] || data.down[1])
	{
		//Msg ("Keys down for button: '%c' '%c' '%c' (%d,%d,%d)!\n", data.down[0], data.down[1], c, data.down[0], data.down[1], c);
		return;		// some other key is still holding it down
	}

	if (!(data.state & 1))
		return;		// still up (this should not happen)

	data.state &= ~1;		// now up
	data.state |= 4; 		// impulse up
}

void IN_ClearDuckToggle()
{
	if ( ::input->KeyState( &in_ducktoggle ) )
	{
		KeyUp( &in_ducktoggle, nullptr ); 
	}
}

void IN_ClearSpeedToggle()
{
	if ( ::input->KeyState( &in_speedtoggle ) )
	{
		KeyUp( &in_speedtoggle, nullptr ); 
	}
}
void IN_ForceSpeedDown( ) {joystick_forced_speed = true;}
void IN_ForceSpeedUp( ) {joystick_forced_speed = false;}
void IN_CommanderMouseMoveDown( const CCommand &args ) {KeyDown(&in_commandermousemove, args[1] );}
void IN_CommanderMouseMoveUp( const CCommand &args ) {KeyUp(&in_commandermousemove, args[1] );}
void IN_BreakDown( const CCommand &args ) { KeyDown( &in_break , args[1] );}
void IN_BreakUp( const CCommand &args )
{ 
	KeyUp( &in_break, args[1] ); 
};
void IN_KLookDown ( const CCommand &args ) {KeyDown(&in_klook, args[1] );}
void IN_KLookUp ( const CCommand &args ) {KeyUp(&in_klook, args[1] );}
void IN_JLookDown ( const CCommand &args ) {KeyDown(&in_jlook, args[1] );}
void IN_JLookUp ( const CCommand &args ) {KeyUp(&in_jlook, args[1] );}
void IN_UpDown( const CCommand &args ) {KeyDown(&in_up, args[1] );}
void IN_UpUp( const CCommand &args ) {KeyUp(&in_up, args[1] );}
void IN_DownDown( const CCommand &args ) {KeyDown(&in_down, args[1] );}
void IN_DownUp( const CCommand &args ) {KeyUp(&in_down, args[1] );}
void IN_LeftDown( const CCommand &args ) {KeyDown(&in_left, args[1] );}
void IN_LeftUp( const CCommand &args ) {KeyUp(&in_left, args[1] );}
void IN_RightDown( const CCommand &args ) {KeyDown(&in_right, args[1] );}
void IN_RightUp( const CCommand &args ) {KeyUp(&in_right, args[1] );}
void IN_ForwardDown( const CCommand &args ) {KeyDown(&in_forward, args[1] );}
void IN_ForwardUp( const CCommand &args ) {KeyUp(&in_forward, args[1] );}
void IN_BackDown( const CCommand &args ) {KeyDown(&in_back, args[1] );}
void IN_BackUp( const CCommand &args ) {KeyUp(&in_back, args[1] );}
void IN_LookupDown( const CCommand &args ) {KeyDown(&in_lookup, args[1] );}
void IN_LookupUp( const CCommand &args ) {KeyUp(&in_lookup, args[1] );}
void IN_LookdownDown( const CCommand &args ) {KeyDown(&in_lookdown, args[1] );}
void IN_LookdownUp( const CCommand &args ) {KeyUp(&in_lookdown, args[1] );}
void IN_MoveleftDown( const CCommand &args ) {KeyDown(&in_moveleft, args[1] );}
void IN_MoveleftUp( const CCommand &args ) {KeyUp(&in_moveleft, args[1] );}
void IN_MoverightDown( const CCommand &args ) {KeyDown(&in_moveright, args[1] );}
void IN_MoverightUp( const CCommand &args ) {KeyUp(&in_moveright, args[1] );}
void IN_StrafeDown( const CCommand &args ) {KeyDown(&in_strafe, args[1] );}
void IN_StrafeUp( const CCommand &args ) {KeyUp(&in_strafe, args[1] );}
void IN_Attack2Down( const CCommand &args ) { KeyDown(&in_attack2, args[1] );}
void IN_Attack2Up( const CCommand &args ) {KeyUp(&in_attack2, args[1] );}
void IN_UseDown ( const CCommand &args ) {KeyDown(&in_use, args[1] );}
void IN_UseUp ( const CCommand &args ) {KeyUp(&in_use, args[1] );}
void IN_JumpDown ( const CCommand &args ) {KeyDown(&in_jump, args[1] );}
void IN_JumpUp ( const CCommand &args ) 
{	
	KeyUp(&in_jump, args[1] );
}

void IN_DuckToggle( const CCommand &args ) 
{ 
	if ( ::input->KeyState(&in_ducktoggle) )
	{
		IN_ClearDuckToggle();
	}
	else
	{
		KeyDown( &in_ducktoggle, args[1] ); 
	}
}

void IN_DuckDown( const CCommand &args ) 
{
	SplitScreenConVarRef option_duck_method( "option_duck_method" );

	if ( option_duck_method.IsValid() && option_duck_method.GetBool( GET_ACTIVE_SPLITSCREEN_SLOT() ) )
	{
		IN_DuckToggle( args );
	}
	else
	{
		KeyDown(&in_duck, args[1] );
		IN_ClearDuckToggle();
	}
}
void IN_DuckUp( const CCommand &args ) 
{
	SplitScreenConVarRef option_duck_method( "option_duck_method" );

	if ( option_duck_method.IsValid() && option_duck_method.GetBool( GET_ACTIVE_SPLITSCREEN_SLOT() ) )
	{
		// intentionally blank
	}
	else
	{
		KeyUp(&in_duck, args[1] );
		IN_ClearDuckToggle();
	}
}

void IN_SpeedToggle( const CCommand &args ) 
{ 
	if ( ::input->KeyState(&in_speedtoggle) )
	{
		IN_ClearSpeedToggle();
	}
	else
	{
		KeyDown( &in_speedtoggle, args[1] ); 
	}
}
void IN_SpeedDown( const CCommand &args ) 
{
	SplitScreenConVarRef option_speed_method( "option_speed_method" );

	if ( option_speed_method.IsValid() && option_speed_method.GetBool( GET_ACTIVE_SPLITSCREEN_SLOT() ) )
	{
		IN_SpeedToggle( args );
	}
	else
	{
		KeyDown(&in_speed, args[1] );
		IN_ClearSpeedToggle();
	}
}
void IN_SpeedUp( const CCommand &args ) 
{
	SplitScreenConVarRef option_speed_method( "option_speed_method" );
	 
	if ( option_speed_method.IsValid() && option_speed_method.GetBool( GET_ACTIVE_SPLITSCREEN_SLOT() ) )
	{
		// intentionally blank
	}
	else
	{
		KeyUp(&in_speed, args[1] );
		IN_ClearSpeedToggle();
	}
}
void IN_WalkDown( const CCommand &args ) {KeyDown(&in_walk, args[1] );}
void IN_WalkUp( const CCommand &args ) {KeyUp(&in_walk, args[1] );}

void IN_ReloadDown( const CCommand &args ) {KeyDown(&in_reload, args[1] );}
void IN_ReloadUp( const CCommand &args ) {KeyUp(&in_reload, args[1] );}
void IN_Alt1Down( const CCommand &args ) {KeyDown(&in_alt1, args[1] );}
void IN_Alt1Up( const CCommand &args ) {KeyUp(&in_alt1, args[1] );}
void IN_Alt2Down( const CCommand &args ) {KeyDown(&in_alt2, args[1] );}
void IN_Alt2Up( const CCommand &args ) {KeyUp(&in_alt2, args[1] );}
void IN_GraphDown( const CCommand &args ) {KeyDown(&in_graph, args[1] );}
void IN_GraphUp( const CCommand &args ) {KeyUp(&in_graph, args[1] );}
void IN_ZoomDown( const CCommand &args ) {KeyDown(&in_zoom, args[1] );}
void IN_ZoomUp( const CCommand &args ) {KeyUp(&in_zoom, args[1] );}
void IN_ZoomInDown( const CCommand &args ) {KeyDown(&in_grenade1, args[1] );}
void IN_ZoomInUp( const CCommand &args ) {KeyUp(&in_grenade1, args[1] );}
void IN_ZoomOutDown( const CCommand &args ) {KeyDown(&in_grenade2, args[1] );}
void IN_ZoomOutUp( const CCommand &args ) {KeyUp(&in_grenade2, args[1] );}
void IN_Grenade1Up( const CCommand &args ) { KeyUp( &in_grenade1, args[1] ); }
void IN_Grenade1Down( const CCommand &args ) { KeyDown( &in_grenade1, args[1] ); }
void IN_Grenade2Up( const CCommand &args ) { KeyUp( &in_grenade2, args[1] ); }
void IN_Grenade2Down( const CCommand &args ) { KeyDown( &in_grenade2, args[1] ); }
void IN_XboxStub( const CCommand &args ) { /*do nothing*/ }

void IN_AttackDown( const CCommand &args )
{
	KeyDown( &in_attack, args[1] );
}

void IN_AttackUp( const CCommand &args )
{
	KeyUp( &in_attack, args[1] );
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	in_cancel[ GET_ACTIVE_SPLITSCREEN_SLOT() ] = 0;
}

// Special handling
void IN_Cancel( const CCommand &args )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	in_cancel[ GET_ACTIVE_SPLITSCREEN_SLOT() ] = 1;
}

void IN_Impulse( const CCommand &args )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	in_impulse[ GET_ACTIVE_SPLITSCREEN_SLOT() ] = atoi( args[1] );
}

void IN_ScoreDown( const CCommand &args )
{
	KeyDown( &in_score, args[1] );
}

void IN_ScoreUp( const CCommand &args )
{
	KeyUp( &in_score, args[1] );
}

void IN_LookSpinDown( const CCommand &args ) {KeyDown( &in_lookspin, args[1] );}
void IN_LookSpinUp( const CCommand &args ) {KeyUp( &in_lookspin, args[1] );}

/*
============
KeyEvent

Return 1 to allow engine to process the key, otherwise, act on it as needed
============
*/
int CInput::KeyEvent( int down, ButtonCode_t code, const char *pszCurrentBinding )
{

	// Deal with camera intercepting the mouse
	if ( down && 
		( ( code == MOUSE_LEFT ) || ( code == MOUSE_RIGHT ) || ( code == MOUSE_MIDDLE ) || ( code == MOUSE_WHEEL_UP ) || ( code == MOUSE_WHEEL_DOWN ) ) )
	{
		ConVarRef cl_mouseenable( "cl_mouseenable" );
		if ( GetPerUser().m_fCameraInterceptingMouse ||  !cl_mouseenable.GetBool() )
			return 0;
	}

	if ( GetClientMode() )
		return GetClientMode()->KeyInput(down, code, pszCurrentBinding);

	return 1;
}

/*
===============
KeyState

Returns 0.25 if a key was pressed and released during the frame,
0.5 if it was pressed and held
0 if held then released, and
1.0 if held for the entire time
===============
*/
float CInput::KeyState ( kbutton_t *key )
{
	kbutton_t::Split_t &data = key->GetPerUser();

	float		val = 0.0;
	int			impulsedown, impulseup, down;
	
	impulsedown = data.state & 2;
	impulseup	= data.state & 4;
	down		= data.state & 1;
	
	if ( impulsedown && !impulseup )
	{
		// pressed and held this frame?
		val = down ? 0.5 : 0.0;
	}

	if ( impulseup && !impulsedown )
	{
		// released this frame?
		val = down ? 0.0 : 0.0;
	}

	if ( !impulsedown && !impulseup )
	{
		// held the entire frame?
		val = down ? 1.0 : 0.0;
	}

	if ( impulsedown && impulseup )
	{
		if ( down )
		{
			// released and re-pressed this frame
			val = 0.75;	
		}
		else
		{
			// pressed and released this frame
			val = 0.25;	
		}
	}

	// clear impulses
	data.state &= 1;		
	return val;
}

void CInput::IN_SetSampleTime( float frametime )
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		m_PerUser[ i ].m_flKeyboardSampleTime = frametime;
	}
}

/*
==============================
DetermineKeySpeed

==============================
*/
static ConVar in_usekeyboardsampletime( side_registry(), "in_usekeyboardsampletime", "1", 0, "Use keyboard sample time smoothing." );

float CInput::DetermineKeySpeed( int nSlot, float frametime )
{
	if ( in_usekeyboardsampletime.GetBool() )
	{
		PerUserInput_t &user = GetPerUser( nSlot );

		if ( user.m_flKeyboardSampleTime <= 0 )
			return 0.0f;
	
		frametime = std::min( user.m_flKeyboardSampleTime, frametime );
		user.m_flKeyboardSampleTime -= frametime;
	}
	
	float speed;

	speed = frametime;

	if ( in_speed.GetPerUser( nSlot ).state & 1 )
	{
		speed *= cl_anglespeedkey.GetFloat();
	}

	return speed;
}

/*
==============================
AdjustYaw

==============================
*/
void CInput::AdjustYaw( int nSlot, float speed, QAngle& viewangles )
{
	if ( !(in_strafe.GetPerUser( nSlot ).state & 1) )
	{
		viewangles[YAW] -= speed*cl_yawspeed.GetFloat() * KeyState (&in_right);
		viewangles[YAW] += speed*cl_yawspeed.GetFloat() * KeyState (&in_left);
	}

	const PerUserInput_t &user = GetPerUser( nSlot );

	// thirdperson platformer mode
	// use movement keys to aim the player relative to the thirdperson camera
	if ( CAM_IsThirdPerson() && thirdperson_platformer.GetInt() )
	{
		float side = KeyState(&in_moveleft) - KeyState(&in_moveright);
		float forward = KeyState(&in_forward) - KeyState(&in_back);

		if ( side || forward )
		{
			viewangles[YAW] = RAD2DEG(std::atan2(side, forward)) + user.m_vecCameraOffset[ YAW ];
		}
		if ( side || forward || KeyState (&in_right) || KeyState (&in_left) )
		{
			cam_idealyaw.SetValue( user.m_vecCameraOffset[ YAW ] - viewangles[ YAW ] );
		}
	}
}

/*
==============================
AdjustPitch

==============================
*/
void CInput::AdjustPitch( int nSlot, float speed, QAngle& viewangles )
{
	// only allow keyboard looking if mouse look is disabled
	if ( !UsingMouselook( nSlot ) )
	{
		float	up, down;

		if ( in_klook.GetPerUser( nSlot ).state & 1 )
		{
			view->StopPitchDrift ();
			viewangles[PITCH] -= speed*cl_pitchspeed.GetFloat() * KeyState (&in_forward);
			viewangles[PITCH] += speed*cl_pitchspeed.GetFloat() * KeyState (&in_back);
		}

		up		= KeyState ( &in_lookup );
		down	= KeyState ( &in_lookdown );
		
		viewangles[PITCH] -= speed*cl_pitchspeed.GetFloat() * up;
		viewangles[PITCH] += speed*cl_pitchspeed.GetFloat() * down;

		if ( up || down )
		{
			view->StopPitchDrift ();
		}
	}	
}

/*
==============================
ClampAngles

==============================
*/
void CInput::ClampAngles( QAngle& viewangles )
{
	if ( viewangles[PITCH] > cl_pitchdown.GetFloat() )
	{
		viewangles[PITCH] = cl_pitchdown.GetFloat();
	}
	if ( viewangles[PITCH] < -cl_pitchup.GetFloat() )
	{
		viewangles[PITCH] = -cl_pitchup.GetFloat();
	}

// Don't constrain Roll in Portal because the player can be upside down! -Jeep
	if ( viewangles[ROLL] > 50 )
	{
		viewangles[ROLL] = 50;
	}
	if ( viewangles[ROLL] < -50 )
	{
		viewangles[ROLL] = -50;
	}
}

/*
================
AdjustAngles

Moves the local angle positions
================
*/
void CInput::AdjustAngles ( int nSlot, float frametime )
{
	float	speed;
	QAngle viewangles;
	
	// Determine control scaling factor ( multiplies time )
	speed = DetermineKeySpeed( nSlot, frametime );
	if ( speed <= 0.0f )
	{
		return;
	}

	// Retrieve latest view direction from engine
	engine->GetViewAngles( viewangles );

	// Undo tilting from previous frame
	viewangles -= GetPerUser().m_angPreviousViewAnglesTilt;

	// Apply tilting effects here (it affects aim)
	QAngle vecAnglesBeforeTilt = viewangles;
	GetViewEffects()->CalcTilt();
	GetViewEffects()->ApplyTilt( viewangles, 1.0f );

	// Remember the tilt delta so we can undo it before applying tilt next frame
	GetPerUser().m_angPreviousViewAnglesTilt = viewangles - vecAnglesBeforeTilt;

	// Adjust YAW
	AdjustYaw( nSlot, speed, viewangles );

	// Adjust PITCH if keyboard looking
	AdjustPitch( nSlot, speed, viewangles );
	
	// Make sure values are legitimate
	ClampAngles( viewangles );

	// Store new view angles into engine view direction
	engine->SetViewAngles( viewangles );
}

/*
==============================
ComputeSideMove

==============================
*/
void CInput::ComputeSideMove( int nSlot, CUserCmd *cmd )
{
	// thirdperson platformer movement
	if ( CAM_IsThirdPerson() && thirdperson_platformer.GetInt() )
	{
		// no sideways movement in this mode
		return;
	}

	// thirdperson screenspace movement
	if ( CAM_IsThirdPerson() && thirdperson_screenspace.GetInt() )
	{
		float ideal_yaw = cam_idealyaw.GetFloat();
		float ideal_sin = std::sin(DEG2RAD(ideal_yaw));
		float ideal_cos = std::cos(DEG2RAD(ideal_yaw));
		
		float movement = ideal_cos*KeyState(&in_moveright)
			+  ideal_sin*KeyState(&in_back)
			+ -ideal_cos*KeyState(&in_moveleft)
			+ -ideal_sin*KeyState(&in_forward);

		cmd->sidemove += cl_sidespeed.GetFloat() * movement;

		return;
	}

	// If strafing, check left and right keys and act like moveleft and moveright keys
	if ( in_strafe.GetPerUser( nSlot ).state & 1 )
	{
		cmd->sidemove += cl_sidespeed.GetFloat() * KeyState (&in_right);
		cmd->sidemove -= cl_sidespeed.GetFloat() * KeyState (&in_left);
	}

	// Otherwise, check strafe keys
	cmd->sidemove += cl_sidespeed.GetFloat() * KeyState (&in_moveright);
	cmd->sidemove -= cl_sidespeed.GetFloat() * KeyState (&in_moveleft);
}

/*
==============================
ComputeUpwardMove

==============================
*/
void CInput::ComputeUpwardMove( int nSlot, CUserCmd *cmd )
{
	cmd->upmove += cl_upspeed.GetFloat() * KeyState (&in_up);
	cmd->upmove -= cl_upspeed.GetFloat() * KeyState (&in_down);
}

/*
==============================
ComputeForwardMove

==============================
*/
void CInput::ComputeForwardMove( int nSlot, CUserCmd *cmd )
{
	// thirdperson platformer movement
	if ( CAM_IsThirdPerson() && thirdperson_platformer.GetInt() )
	{
		// movement is always forward in this mode
		float movement = KeyState(&in_forward)
			|| KeyState(&in_moveright)
			|| KeyState(&in_back)
			|| KeyState(&in_moveleft);

		cmd->forwardmove += cl_forwardspeed.GetFloat() * movement;

		return;
	}

	// thirdperson screenspace movement
	if ( CAM_IsThirdPerson() && thirdperson_screenspace.GetInt() )
	{
		float ideal_yaw = cam_idealyaw.GetFloat();
		float ideal_sin = std::sin(DEG2RAD(ideal_yaw));
		float ideal_cos = std::cos(DEG2RAD(ideal_yaw));
		
		float movement = ideal_cos*KeyState(&in_forward)
			+  ideal_sin*KeyState(&in_moveright)
			+ -ideal_cos*KeyState(&in_back)
			+ -ideal_sin*KeyState(&in_moveleft);

		cmd->forwardmove += cl_forwardspeed.GetFloat() * movement;

		return;
	}

	if ( !(in_klook.GetPerUser( nSlot ).state & 1 ) )
	{	
		cmd->forwardmove += cl_forwardspeed.GetFloat() * KeyState (&in_forward);
		cmd->forwardmove -= cl_backspeed.GetFloat() * KeyState (&in_back);
	}	
}

/*
==============================
ScaleMovements

==============================
*/
void CInput::ScaleMovements( CUserCmd *cmd )
{
	// float spd;

	// clip to maxspeed
	// FIXME FIXME:  This doesn't work
	return;

	/*
	spd = engine->GetClientMaxspeed();
	if ( spd == 0.0 )
		return;

	// Scale the speed so that the total velocity is not > spd
	float fmov = sqrt( (cmd->forwardmove*cmd->forwardmove) + (cmd->sidemove*cmd->sidemove) + (cmd->upmove*cmd->upmove) );

	if ( fmov > spd && fmov > 0.0 )
	{
		float fratio = spd / fmov;

		if ( !IsNoClipping() ) 
		{
			cmd->forwardmove	*= fratio;
			cmd->sidemove		*= fratio;
			cmd->upmove			*= fratio;
		}
	}
	*/
}

/*
===========
ControllerMove
===========
*/
void CInput::ControllerMove( int nSlot, float frametime, CUserCmd *cmd )
{
	ConVarRef cl_mouseenable( "cl_mouseenable" );

	if ( 		 cl_mouseenable.GetBool() && 
		 nSlot == in_forceuser.GetInt() && 
		 g_pInputSystem->IsDeviceReadingInput( INPUT_DEVICE_KEYBOARD_MOUSE ) )
	{
		if ( !GetPerUser( nSlot ).m_fCameraInterceptingMouse && m_fMouseActive )
		{
			MouseMove( nSlot, cmd );
		}
	}

	JoyStickMove( frametime, cmd);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *weapon - 
//-----------------------------------------------------------------------------
void CInput::MakeWeaponSelection( CBaseCombatWeapon *weapon )
{
	GetPerUser().m_hSelectedWeapon = weapon;
}

CInput::PerUserInput_t &CInput::GetPerUser( int nSlot /*=-1*/ )
{
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		return m_PerUser[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
	}
	return m_PerUser[ nSlot ];
}

const CInput::PerUserInput_t &CInput::GetPerUser( int nSlot /*=-1*/ ) const
{
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		return m_PerUser[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
	}
	return m_PerUser[ nSlot ];
}

void CInput::ExtraMouseSample( float frametime, bool active )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	static CUserCmd dummy[ MAX_SPLITSCREEN_PLAYERS ];
	CUserCmd *cmd = &dummy[ nSlot ];

	cmd->Reset();

	QAngle viewangles;

	if ( active )
	{
		// Determine view angles
		AdjustAngles ( nSlot, frametime );

		// Determine sideways movement
		if ( g_pInputSystem->IsDeviceReadingInput( INPUT_DEVICE_KEYBOARD_MOUSE ) )
			ComputeSideMove( nSlot, cmd );

		// Determine vertical movement
		ComputeUpwardMove( nSlot, cmd );

		// Determine forward movement
		if ( g_pInputSystem->IsDeviceReadingInput( INPUT_DEVICE_KEYBOARD_MOUSE ) )
			ComputeForwardMove( nSlot, cmd );

		// Scale based on holding speed key or having too fast of a velocity based on client maximum speed.
		ScaleMovements( cmd );

		// Allow mice and other controllers to add their inputs
		ControllerMove( nSlot, frametime, cmd );
	}

	// Retrieve view angles from engine ( could have been set in AdjustAngles above )
	engine->GetViewAngles( viewangles );

	if ( round_start_reset_duck.GetBool( ) == true )
	{
		IN_ClearDuckToggle( );
		round_start_reset_duck.SetValue( false );
	}
	
	if ( round_start_reset_speed.GetBool( ) == true )
	{
		IN_ClearSpeedToggle( );
		joystick_forced_speed = false;
		round_start_reset_speed.SetValue( false );
	}	// Set button and flag bits, don't blow away state

	cmd->buttons = GetButtonBits( false );

	// Use new view angles if alive, otherwise user last angles we stored off.
	VectorCopy( viewangles, cmd->viewangles );
	VectorCopy( viewangles, GetPerUser().m_angPreviousViewAngles );

	// Let the move manager override anything it wants to.
	if ( GetClientMode()->CreateMove( frametime, cmd ) )
	{
		// Get current view angles after the client mode tweaks with it
		engine->SetViewAngles( cmd->viewangles );
		prediction->SetLocalViewAngles( cmd->viewangles );
	}

	CheckPaused( cmd );

	CheckSplitScreenMimic( nSlot, cmd, &dummy[ 0 ] );
}

/*
================
CreateMove

Send the intended movement message to the server
if active == 1 then we are 1) not playing back demos ( where our commands are ignored ) and
2 ) we have finished signing on to server
================
*/

void CInput::CreateMove ( int sequence_number, float input_sample_frametime, bool active )
{	
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	CUserCmd *cmd = &GetPerUser( nSlot ).m_pCommands[ sequence_number % MULTIPLAYER_BACKUP];
	CVerifiedUserCmd *pVerified = &GetPerUser( nSlot ).m_pVerifiedCommands[ sequence_number % MULTIPLAYER_BACKUP];

	cmd->Reset();

	cmd->command_number = sequence_number;
	cmd->tick_count = gpGlobals->tickcount;

	QAngle viewangles;

	if ( active || sv_noclipduringpause.GetInt() )
	{
		if ( nSlot == in_forceuser.GetInt() )
		{
			// Determine view angles
			AdjustAngles ( nSlot, input_sample_frametime );

			// Determine sideways movement
			if ( g_pInputSystem->IsDeviceReadingInput( INPUT_DEVICE_KEYBOARD_MOUSE ) )
				ComputeSideMove( nSlot, cmd );

			// Determine vertical movement
			ComputeUpwardMove( nSlot, cmd );

			// Determine forward movement
			if ( g_pInputSystem->IsDeviceReadingInput( INPUT_DEVICE_KEYBOARD_MOUSE ) )
				ComputeForwardMove( nSlot, cmd );

			// Scale based on holding speed key or having too fast of a velocity based on client maximum
			//  speed.
			ScaleMovements( cmd );
		}

		// Allow mice and other controllers to add their inputs
		ControllerMove( nSlot, input_sample_frametime, cmd );
	}
	else
	{
		// need to run and reset mouse input so that there is no view pop when unpausing
		if ( !GetPerUser( nSlot ).m_fCameraInterceptingMouse && m_fMouseActive )
		{
			float mx, my;
			GetAccumulatedMouseDeltasAndResetAccumulators( nSlot, &mx, &my );
			ResetMouse();
		}
	}
	// Retreive view angles from engine ( could have been set in IN_AdjustAngles above )
	engine->GetViewAngles( viewangles );

	cmd->impulse = in_impulse[ nSlot ];
	in_impulse[ nSlot ] = 0;

	// Latch and clear weapon selection
	if ( GetPerUser( nSlot ).m_hSelectedWeapon != nullptr )
	{
		CBaseCombatWeapon *weapon = GetPerUser( nSlot ).m_hSelectedWeapon;

		cmd->weaponselect = weapon->entindex();
		cmd->weaponsubtype = weapon->GetSubType();

		// Always clear weapon selection
		GetPerUser( nSlot ).m_hSelectedWeapon = nullptr;
	}

	// Set button and flag bits
	cmd->buttons = GetButtonBits( true );

	// Using joystick?
	if ( ( g_pInputSystem->IsDeviceReadingInput( INPUT_DEVICE_GAMEPAD ) && in_joystick.GetInt() ) || g_pInputSystem->MotionControllerActive() )
	{
		if ( cmd->forwardmove > 0 )
		{
			cmd->buttons |= IN_FORWARD;
		}
		else if ( cmd->forwardmove < 0 )
		{
			cmd->buttons |= IN_BACK;
		}
	}

	// Use new view angles if alive, otherwise user last angles we stored off.
	VectorCopy( viewangles, cmd->viewangles );
	VectorCopy( viewangles, GetPerUser( nSlot ).m_angPreviousViewAngles );

	// Let the move manager override anything it wants to.
	if ( GetClientMode()->CreateMove( input_sample_frametime, cmd ) )
	{
		// Get current view angles after the client mode tweaks with it
		engine->SetViewAngles( cmd->viewangles );
	}

	CheckPaused( cmd );

	CUserCmd *pPlayer0Command = &m_PerUser[ 0 ].m_pCommands[ sequence_number % MULTIPLAYER_BACKUP ];
	CheckSplitScreenMimic( nSlot, cmd, pPlayer0Command );

	GetPerUser( nSlot ).m_flLastForwardMove = cmd->forwardmove;

	cmd->random_seed = MD5_PseudoRandom( sequence_number ) & 0x7fffffff;

	HLTVCamera()->CreateMove( cmd );

	pVerified->m_cmd = *cmd;
	pVerified->m_crc = cmd->GetChecksum();
}

void CInput::CheckSplitScreenMimic( int nSlot, CUserCmd *cmd, CUserCmd *pPlayer0Command )
{
	// ss_mimic 2 is more of a "follow" mode
	int nMimicMode = ss_mimic.GetInt();
	if ( nMimicMode <= 0 || nSlot == 0 )
		return;

	*cmd = *pPlayer0Command;

	// We can't copy these over, since we send the weapon index it'll make the server attach the weapon to the other split screen player, even
	//  though he doesn't own it!!!
	cmd->weaponsubtype = 0;
	cmd->weaponselect = 0;

	// Get current view angles after the client mode tweaks with it
	engine->SetViewAngles( cmd->viewangles );

	int nLeader = ( nSlot + 1 ) % MAX_SPLITSCREEN_PLAYERS;

	CBasePlayer *pLeader = CBasePlayer::GetLocalPlayer( nLeader );
	CBasePlayer *pFollower = CBasePlayer::GetLocalPlayer( nSlot );

	if ( !pLeader || !pFollower )
		return;

	Vector leaderPos = pLeader->GetAbsOrigin();
	Vector followerPos = pFollower->GetAbsOrigin();

	float flFarDist = 256.0f * 256.0f;
	float flNearDist = 64.0f * 64.0f;

	Vector delta = leaderPos - followerPos;
	float flLength2DSqr = delta.Length2DSqr();
	if ( flLength2DSqr > flFarDist )
	{
		SplitScreenTeleport( nLeader );
	}
	else if ( flLength2DSqr > flNearDist )
	{
		// Run toward other guy
		cmd->buttons &= ~( IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT );

		Vector lookDir;
		Vector rightDir;
		AngleVectors( cmd->viewangles, &lookDir, &rightDir, nullptr );
		lookDir.z = 0.0f;
		lookDir.NormalizeInPlace();
		Vector moveDir = delta;
		moveDir.z = 0.0f;
		moveDir.NormalizeInPlace();

		// This is the cos of the angle between them
		float fdot = lookDir.Dot( moveDir );
		float rdot = rightDir.Dot( moveDir );

		cmd->forwardmove = fdot * cl_forwardspeed.GetFloat();
		cmd->sidemove = rdot * cl_sidespeed.GetFloat();

		// We'll only be moving fwd or sideways
		cmd->upmove = 0.0f;

		if ( cmd->forwardmove > 0.0f )
		{
			cmd->buttons |= IN_FORWARD;
		}
		else if ( cmd->forwardmove < 0.0f )
		{
			cmd->buttons |= IN_BACK;
		}

		if ( cmd->sidemove > 0.0f )
		{
			cmd->buttons |= IN_MOVELEFT;
		}
		else if ( cmd->sidemove < 0.0f )
		{
			cmd->buttons |= IN_MOVERIGHT;
		}
	}
	else
	{
		// Stop movement buttons
		cmd->forwardmove = cmd->sidemove = cmd->upmove = 0.0f;
		cmd->buttons &= ~( IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT );
	}
}

void CInput::CheckPaused( CUserCmd *cmd )
{
	if ( !engine->IsPaused() )
		return;
	cmd->buttons = 0;
	cmd->forwardmove = 0;
	cmd->sidemove = 0;
	cmd->upmove = 0;

	// Don't allow changing weapons while paused
	cmd->weaponselect = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buf - 
//			buffersize - 
//			slot - 
//-----------------------------------------------------------------------------
void CInput::EncodeUserCmdToBuffer( int nSlot, bf_write& buf, int sequence_number )
{
	CUserCmd nullcmd;
	CUserCmd *cmd = GetUserCmd( nSlot, sequence_number);

	WriteUsercmd( &buf, cmd, &nullcmd );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buf - 
//			buffersize - 
//			slot - 
//-----------------------------------------------------------------------------
void CInput::DecodeUserCmdFromBuffer( int nSlot, bf_read& buf, int sequence_number )
{
	CUserCmd nullcmd;
	CUserCmd *cmd = &GetPerUser( nSlot ).m_pCommands[ sequence_number % MULTIPLAYER_BACKUP ];

	ReadUsercmd( &buf, cmd, &nullcmd );
}

void CInput::ValidateUserCmd( CUserCmd *usercmd, int sequence_number )
{
	// Validate that the usercmd hasn't been changed
	CRC32_t crc = usercmd->GetChecksum();
	if ( crc != GetPerUser().m_pVerifiedCommands[ sequence_number % MULTIPLAYER_BACKUP ].m_crc )
	{
		*usercmd = GetPerUser().m_pVerifiedCommands[ sequence_number % MULTIPLAYER_BACKUP ].m_cmd;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *buf - 
//			from - 
//			to - 
//-----------------------------------------------------------------------------
bool CInput::WriteUsercmdDeltaToBuffer( int nSlot, bf_write *buf, int from, int to, bool isnewcommand )
{
	contract_assert( GetPerUser( nSlot ).m_pCommands );

	CUserCmd nullcmd;
	CUserCmd *f, *t;
	int startbit;

	startbit = buf->GetNumBitsWritten();
	if ( from == -1 )
	{
		f = &nullcmd;
	}
	else
	{
		f = GetUserCmd( nSlot, from );

		if ( !f )
		{
			// DevMsg( "WARNING! User command delta too old (from %i, to %i)\n", from, to );
			f = &nullcmd;
		}
		else
		{
			ValidateUserCmd( f, from );
		}
	}

	t = GetUserCmd( nSlot, to );

	if ( !t )
	{
		// DevMsg( "WARNING! User command too old (from %i, to %i)\n", from, to );
		t = &nullcmd;
	}
	else
	{
		ValidateUserCmd( t, to );
	}

	// Write it into the buffer
	WriteUsercmd( buf, t, f );

	if ( buf->IsOverflowed() )
	{
		int endbit;
		endbit = buf->GetNumBitsWritten();
		Msg( "WARNING! User command buffer overflow(%i %i), last cmd was %i bits long\n",
			from, to,  endbit - startbit );

		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : slot - 
// Output : CUserCmd
//-----------------------------------------------------------------------------
CUserCmd *CInput::GetUserCmd( int nSlot, int sequence_number )
{
	contract_assert( GetPerUser( nSlot ).m_pCommands );

	CUserCmd *usercmd = &GetPerUser( nSlot ).m_pCommands[ sequence_number % MULTIPLAYER_BACKUP ];

	if ( usercmd->command_number != sequence_number )
	{
		return nullptr;	// usercmd was overwritten by newer command
	}

	return usercmd;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bits - 
//			in_button - 
//			in_ignore - 
//			*button - 
//			reset - 
// Output : static void
//-----------------------------------------------------------------------------
static void CalcButtonBits( int nSlot, int& bits, int in_button, int in_ignore, kbutton_t *button, bool reset )
{
	kbutton_t::Split_t *pButtonState = &button->GetPerUser( nSlot );

	// Down or still down?
	if ( pButtonState->state & 3 )
	{
		bits |= in_button;
	}

	int clearmask = ~2;
	if ( in_ignore & in_button )
	{
		// This gets taken care of below in the GetButtonBits code
		//bits &= ~in_button;
		// Remove "still down" as well as "just down"
		clearmask = ~3;
	}

	if ( reset )
	{
		pButtonState->state &= clearmask;
	}
}

/*
============
GetButtonBits

Returns appropriate button info for keyboard and mouse state
Set bResetState to 1 to clear old state info
============
*/
int CInput::GetButtonBits( bool bResetState )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	int bits = 0;
	
	int ignore = GetPerUser( nSlot ).m_nClearInputState;
	CalcButtonBits( nSlot, bits, IN_SPEED, ignore, &in_speed, bResetState );
	CalcButtonBits( nSlot, bits, IN_WALK, ignore, &in_walk, bResetState );
	CalcButtonBits( nSlot, bits, IN_ATTACK, ignore, &in_attack, bResetState );
	CalcButtonBits( nSlot, bits, IN_DUCK, ignore, &in_duck, bResetState );
	CalcButtonBits( nSlot, bits, IN_JUMP, ignore, &in_jump, bResetState );
	CalcButtonBits( nSlot, bits, IN_FORWARD, ignore, &in_forward, bResetState );
	CalcButtonBits( nSlot, bits, IN_BACK, ignore, &in_back, bResetState );
	CalcButtonBits( nSlot, bits, IN_USE, ignore, &in_use, bResetState );
	CalcButtonBits( nSlot, bits, IN_LEFT, ignore, &in_left, bResetState );
	CalcButtonBits( nSlot, bits, IN_RIGHT, ignore, &in_right, bResetState );
	CalcButtonBits( nSlot, bits, IN_MOVELEFT, ignore, &in_moveleft, bResetState );
	CalcButtonBits( nSlot, bits, IN_MOVERIGHT, ignore, &in_moveright, bResetState );
	CalcButtonBits( nSlot, bits, IN_ATTACK2, ignore, &in_attack2, bResetState );
	CalcButtonBits( nSlot, bits, IN_RELOAD, ignore, &in_reload, bResetState );
	CalcButtonBits( nSlot, bits, IN_ALT1, ignore, &in_alt1, bResetState );
	CalcButtonBits( nSlot, bits, IN_ALT2, ignore, &in_alt2, bResetState );
	CalcButtonBits( nSlot, bits, IN_SCORE, ignore, &in_score, bResetState );
	CalcButtonBits( nSlot, bits, IN_ZOOM, ignore, &in_zoom, bResetState );
	CalcButtonBits( nSlot, bits, IN_GRENADE1, ignore, &in_grenade1, bResetState );
	CalcButtonBits( nSlot, bits, IN_GRENADE2, ignore, &in_grenade2, bResetState );
	CalcButtonBits( nSlot, bits, IN_LOOKSPIN, ignore, &in_lookspin, bResetState );

	if ( KeyState(&in_ducktoggle) )
	{
		bits |= IN_DUCK;
	}

	// dkorus: handle the toggle OR the joystick feature to force a move speed when going slow
	if ( KeyState(&in_speedtoggle) || joystick_forced_speed )
	{
		bits |= IN_SPEED;
	}

	if ( in_cancel[ nSlot ] )
	{
		bits |= IN_CANCEL;
	}

	if ( GetHud( nSlot ).m_iKeyBits & IN_WEAPON1 )
	{
		bits |= IN_WEAPON1;
	}

	if ( GetHud( nSlot ).m_iKeyBits & IN_WEAPON2 )
	{
		bits |= IN_WEAPON2;
	}

	// Clear out any residual
	bits &= ~ignore;

	if ( bResetState )
	{
		GetPerUser( nSlot ).m_nClearInputState = 0;
	}

	return bits;
}

//-----------------------------------------------------------------------------
// Causes an input to have to be re-pressed to become active
//-----------------------------------------------------------------------------
void CInput::ClearInputButton( int bits )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();

	if ( GET_ACTIVE_SPLITSCREEN_SLOT() != in_forceuser.GetInt() )
	{
		return;
	}

	GetPerUser().m_nClearInputState |= bits;
}

/*
==============================
GetLookSpring

==============================
*/
float CInput::GetLookSpring( void )
{
	return lookspring.GetInt();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CInput::GetLastForwardMove( void )
{
	return GetPerUser().m_flLastForwardMove;
}

static ConCommand startcommandermousemove(side_registry(), "+commandermousemove", IN_CommanderMouseMoveDown);
static ConCommand endcommandermousemove(side_registry(), "-commandermousemove", IN_CommanderMouseMoveUp);
static ConCommand startmoveup(side_registry(), "+moveup",IN_UpDown);
static ConCommand endmoveup(side_registry(), "-moveup",IN_UpUp);
static ConCommand startmovedown(side_registry(), "+movedown",IN_DownDown);
static ConCommand endmovedown(side_registry(), "-movedown",IN_DownUp);
static ConCommand startleft(side_registry(), "+left",IN_LeftDown);
static ConCommand endleft(side_registry(), "-left",IN_LeftUp);
static ConCommand startright(side_registry(), "+right",IN_RightDown);
static ConCommand endright(side_registry(), "-right",IN_RightUp);
static ConCommand startforward(side_registry(), "+forward",IN_ForwardDown);
static ConCommand endforward(side_registry(), "-forward",IN_ForwardUp);
static ConCommand startback(side_registry(), "+back",IN_BackDown);
static ConCommand endback(side_registry(), "-back",IN_BackUp);
static ConCommand startlookup(side_registry(), "+lookup", IN_LookupDown);
static ConCommand endlookup(side_registry(), "-lookup", IN_LookupUp);
static ConCommand startlookdown(side_registry(), "+lookdown", IN_LookdownDown);
static ConCommand lookdown(side_registry(), "-lookdown", IN_LookdownUp);
static ConCommand startstrafe(side_registry(), "+strafe", IN_StrafeDown);
static ConCommand endstrafe(side_registry(), "-strafe", IN_StrafeUp);
static ConCommand startmoveleft(side_registry(), "+moveleft", IN_MoveleftDown);
static ConCommand endmoveleft(side_registry(), "-moveleft", IN_MoveleftUp);
static ConCommand startmoveright(side_registry(), "+moveright", IN_MoverightDown);
static ConCommand endmoveright(side_registry(), "-moveright", IN_MoverightUp);
static ConCommand startspeed(side_registry(), "+speed", IN_SpeedDown);
static ConCommand endspeed(side_registry(), "-speed", IN_SpeedUp);
static ConCommand startwalk(side_registry(), "+walk", IN_WalkDown);
static ConCommand endwalk(side_registry(), "-walk", IN_WalkUp);
static ConCommand startattack(side_registry(), "+attack", IN_AttackDown);
static ConCommand endattack(side_registry(), "-attack", IN_AttackUp);
static ConCommand startattack2(side_registry(), "+attack2", IN_Attack2Down);
static ConCommand endattack2(side_registry(), "-attack2", IN_Attack2Up);
static ConCommand startuse(side_registry(), "+use", IN_UseDown);
static ConCommand enduse(side_registry(), "-use", IN_UseUp);
static ConCommand startjump(side_registry(), "+jump", IN_JumpDown);
static ConCommand endjump(side_registry(), "-jump", IN_JumpUp);
static ConCommand impulse(side_registry(), "impulse", IN_Impulse);
static ConCommand startklook(side_registry(), "+klook", IN_KLookDown);
static ConCommand endklook(side_registry(), "-klook", IN_KLookUp);
static ConCommand startjlook(side_registry(), "+jlook", IN_JLookDown);
static ConCommand endjlook(side_registry(), "-jlook", IN_JLookUp);
static ConCommand startduck(side_registry(), "+duck", IN_DuckDown);
static ConCommand endduck(side_registry(), "-duck", IN_DuckUp);
static ConCommand startreload(side_registry(), "+reload", IN_ReloadDown);
static ConCommand endreload(side_registry(), "-reload", IN_ReloadUp);
static ConCommand startalt1(side_registry(), "+alt1", IN_Alt1Down);
static ConCommand endalt1(side_registry(), "-alt1", IN_Alt1Up);
static ConCommand startalt2(side_registry(), "+alt2", IN_Alt2Down);
static ConCommand endalt2(side_registry(), "-alt2", IN_Alt2Up);
static ConCommand startscore(side_registry(), "+score", IN_ScoreDown);
static ConCommand endscore(side_registry(), "-score", IN_ScoreUp);
static ConCommand startshowscores(side_registry(), "+showscores", IN_ScoreDown);
static ConCommand endshowscores(side_registry(), "-showscores", IN_ScoreUp);
static ConCommand startgraph(side_registry(), "+graph", IN_GraphDown);
static ConCommand endgraph(side_registry(), "-graph", IN_GraphUp);
static ConCommand startbreak(side_registry(), "+break",IN_BreakDown);
static ConCommand endbreak(side_registry(), "-break",IN_BreakUp);
static ConCommand force_centerview(side_registry(), "force_centerview", IN_CenterView_f);
static ConCommand joyadvancedupdate(side_registry(), "joyadvancedupdate", IN_Joystick_Advanced_f, "", FCVAR_CLIENTCMD_CAN_EXECUTE);
static ConCommand startzoom(side_registry(), "+zoom", IN_ZoomDown);
static ConCommand endzoom(side_registry(), "-zoom", IN_ZoomUp);
static ConCommand startzoomin(side_registry(), "+zoom_in", IN_ZoomInDown);
static ConCommand endzoomin(side_registry(), "-zoom_in", IN_ZoomInUp);
static ConCommand startzoomout(side_registry(), "+zoom_out", IN_ZoomOutDown);
static ConCommand endzoomout(side_registry(), "-zoom_out", IN_ZoomOutUp);
static ConCommand endgrenade1( side_registry(), "-grenade1", IN_Grenade1Up );
static ConCommand startgrenade1( side_registry(), "+grenade1", IN_Grenade1Down );
static ConCommand endgrenade2( side_registry(), "-grenade2", IN_Grenade2Up );
static ConCommand startgrenade2( side_registry(), "+grenade2", IN_Grenade2Down );
static ConCommand startlookspin(side_registry(), "+lookspin", IN_LookSpinDown);
static ConCommand endlookspin(side_registry(), "-lookspin", IN_LookSpinUp);
static ConCommand toggle_duck( side_registry(), "toggle_duck", IN_DuckToggle );

// Xbox 360 stub commands
static ConCommand xboxmove(side_registry(), "xmove", IN_XboxStub);
static ConCommand xboxlook(side_registry(), "xlook", IN_XboxStub);

/*
============
Init_All
============
*/
void CInput::Init_All (void)
{
	InitMouse( );

	m_hInputContext = engine->GetInputContext( ENGINE_INPUT_CONTEXT_GAME );

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		contract_assert( !m_PerUser[ i ].m_pCommands );
		contract_assert( !m_PerUser[ i ].m_pVerifiedCommands );

		m_PerUser[ i ].m_pCommands = new CUserCmd[ MULTIPLAYER_BACKUP ];
		m_PerUser[ i ].m_pVerifiedCommands = new CVerifiedUserCmd[ MULTIPLAYER_BACKUP ];
	}

	m_fMouseInitialized	= false;
	m_fRestoreSPI		= false;
	m_fMouseActive		= false;
	V_memset( m_rgOrigMouseParms, 0, sizeof( m_rgOrigMouseParms ) );
	V_memset( m_rgNewMouseParms, 0, sizeof( m_rgNewMouseParms ) );
	V_memset( m_rgCheckMouseParam, 0, sizeof( m_rgCheckMouseParam ) );

	m_rgNewMouseParms[ MOUSE_ACCEL_THRESHHOLD1 ] = 0;	// no 2x
	m_rgNewMouseParms[ MOUSE_ACCEL_THRESHHOLD2 ] = 0;	// no 4x
	m_rgNewMouseParms[ MOUSE_SPEED_FACTOR ] = 1;		// 0 = disabled, 1 = threshold 1 enabled, 2 = threshold 2 enabled

	m_fMouseParmsValid	= false;
	m_fJoystickAdvancedInit = false;
	m_bControllerMode = false;
	m_fAccumulatedMouseMove = 0.0f;
	m_lastAutoAimValue = 1.0f;

	// Initialize inputs
	Init_Mouse ();
	Init_Keyboard();
		
	// Initialize third person camera controls.
	Init_Camera();
}

/*
============
Shutdown_All
============
*/
void CInput::Shutdown_All(void)
{
	DeactivateMouse();
	Shutdown_Keyboard();

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		delete[] m_PerUser[ i ].m_pCommands;
		m_PerUser[ i ].m_pCommands = nullptr;

		delete[] m_PerUser[ i ].m_pVerifiedCommands;
		m_PerUser[ i ].m_pVerifiedCommands = nullptr;
	}
}

void CInput::LevelInit( void )
{
}

class CUserCmd;

// Purpose: Mouse input routines

// up / down
#define	PITCH	0
// left / right
#define	YAW		1

extern ConVar lookstrafe;
extern ConVar cl_pitchdown;
extern ConVar cl_pitchup;
extern const ConVar *sv_cheats;

static float ConVar_m_pitch_GetFloat( const ConVar_ServerBounded &bounded )
{
	if ( !sv_cheats )
		sv_cheats = cvar->FindVar( "sv_cheats" );

	float flBaseValue;
	// If sv_cheats is on then it can be anything.
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	if ( nSlot != 0 )
	{
		static SplitScreenConVarRef s_m_pitch( "m_pitch" );
		flBaseValue = s_m_pitch.GetFloat( nSlot );
	}
	else
	{
		flBaseValue = bounded.GetBaseFloatValue();
	}
	if ( !sv_cheats || sv_cheats->GetBool() )
		return flBaseValue;

	// If sv_cheats is off than it can only be 0.022 or -0.022 (if they've reversed the mouse in the options).		
	if ( flBaseValue > 0 )
		return 0.022f;
	else
		return -0.022f;
}

static ConVar_ServerBounded cvar_m_pitch( ConVar_m_pitch_GetFloat, "m_pitch","0.022", FCVAR_ARCHIVE|FCVAR_SS, "Mouse pitch factor." );
ConVar_ServerBounded *m_pitch = &cvar_m_pitch;

extern ConVar cam_idealyaw;
extern ConVar cam_idealpitch;
extern ConVar thirdperson_platformer;

ConVar sensitivity( side_registry(), "sensitivity","2.5", FCVAR_ARCHIVE, "Mouse sensitivity.", true, 0.0001f, true, 1000 );

static ConVar m_side( side_registry(), "m_side","0.8", FCVAR_ARCHIVE, "Mouse side factor.", true, 0.0001f, true, 1000 );
static ConVar m_yaw( side_registry(), "m_yaw","0.022", FCVAR_ARCHIVE, "Mouse yaw factor.", true, 0.0001f, true, 1000 );
static ConVar m_forward( side_registry(), "m_forward","1", FCVAR_ARCHIVE, "Mouse forward factor.", true, 0.0001f, true, 1000 );

static ConVar m_customaccel( side_registry(), "m_customaccel", "0", FCVAR_ARCHIVE, "Custom mouse acceleration:"
	"\n0: custom accelaration disabled"
	"\n1: mouse_acceleration = min(m_customaccel_max, pow(raw_mouse_delta, m_customaccel_exponent) * m_customaccel_scale + sensitivity)"
	"\n2: Same as 1, with but x and y sensitivity are scaled by m_pitch and m_yaw respectively."
	"\n3: mouse_acceleration = pow(raw_mouse_delta, m_customaccel_exponent - 1) * sensitivity"
	);
static ConVar m_customaccel_scale( side_registry(), "m_customaccel_scale", "0.04", FCVAR_ARCHIVE, "Custom mouse acceleration value.", true, 0, true, 10 );
static ConVar m_customaccel_max( side_registry(), "m_customaccel_max", "0", FCVAR_ARCHIVE, "Max mouse move scale factor, 0 for no limit" );
static ConVar m_customaccel_exponent( side_registry(), "m_customaccel_exponent", "1.05", FCVAR_ARCHIVE, "Mouse move is raised to this power before being scaled by scale factor.", true, 0.0001f, true, 10 );

static ConVar m_mousespeed( side_registry(), "m_mousespeed", "1", FCVAR_ARCHIVE, "Windows mouse acceleration (0 to disable, 1 to enable [Windows 2000: enable initial threshold], 2 to enable secondary threshold [Windows 2000 only]).", true, 0, true, 2 );
static ConVar m_mouseaccel1( side_registry(), "m_mouseaccel1", "0", FCVAR_ARCHIVE, "Windows mouse acceleration initial threshold (2x movement).", true, 0, false, 0.0f );
static ConVar m_mouseaccel2( side_registry(), "m_mouseaccel2", "0", FCVAR_ARCHIVE, "Windows mouse acceleration secondary threshold (4x movement).", true, 0, false, 0.0f );

ConVar cl_mouselook( side_registry(), "cl_mouselook", "1", FCVAR_ARCHIVE | FCVAR_NOT_CONNECTED | FCVAR_SS, "Set to 1 to use mouse for look, 0 for keyboard look. Cannot be set while connected to a server." );

ConVar cl_mouseenable( side_registry(), "cl_mouseenable", "1", FCVAR_RELEASE );

void cl_mouseenable_changed_callback( ConVar *var, const char *pOldValue, float flOldValue )
{
	if( cl_mouseenable.GetBool() == true )
		input->ClearStates();
}

void CInput::InitMouse( void )
{
		cl_mouseenable.InstallChangeCallback( cl_mouseenable_changed_callback, false );
}

//-----------------------------------------------------------------------------
// Purpose: Hides cursor and starts accumulation/re-centering
//-----------------------------------------------------------------------------
void CInput::ActivateMouse (void)
{
	if ( m_fMouseActive )
		return;

	if ( m_fMouseInitialized )
	{
		if ( m_fMouseParmsValid )
		{
		}
		m_fMouseActive = true;

		// re-center the mouse if we're controlling the camera with it, otherwise just reset the cursor
		if ( GetPerUser().m_fCameraInterceptingMouse || cl_mouseenable.GetBool() )
			ResetMouse();
		else
			g_pInputSystem->ResetCursorIcon();

		g_pInputStackSystem->SetCursorIcon( m_hInputContext, INPUT_CURSOR_HANDLE_INVALID );
        float dx, dy;
		engine->GetMouseDelta( dx, dy, true );
		ClearStates();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gives back the cursor and stops centering of mouse
//-----------------------------------------------------------------------------
void CInput::DeactivateMouse (void)
{
	// This gets called whenever the mouse should be inactive. We only respond to it if we had 
	// previously activated the mouse. We'll show the cursor in here.
	if ( !m_fMouseActive )
		return;

	if ( m_fMouseInitialized )
	{
		if ( m_fRestoreSPI )
		{
		}
		m_fMouseActive = false;
		g_pInputStackSystem->SetCursorIcon( m_hInputContext, g_pInputSystem->GetStandardCursor( INPUT_CURSOR_ARROW ) );
        // now put the mouse back in the middle of the screen
		ResetMouse();

		// Clear accumulated error, too
		for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
			GetPerUser().m_flAccumulatedMouseXMovement = 0;
			GetPerUser().m_flAccumulatedMouseYMovement = 0;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInput::CheckMouseAcclerationVars()
{
	// Don't change them if the mouse is inactive, invalid, or not using parameters for restore
	if ( !m_fMouseActive ||
		 !m_fMouseInitialized || 
		 !m_fMouseParmsValid || 
		 !m_fRestoreSPI )
	{
		return;
	}

	int values[ NUM_MOUSE_PARAMS ];

	values[ MOUSE_SPEED_FACTOR ]		= m_mousespeed.GetInt();
	values[ MOUSE_ACCEL_THRESHHOLD1 ]	= m_mouseaccel1.GetInt();
	values[ MOUSE_ACCEL_THRESHHOLD2 ]	= m_mouseaccel2.GetInt();

	bool dirty = false;

	int i;
	for ( i = 0; i < NUM_MOUSE_PARAMS; i++ )
	{
		if ( !m_rgCheckMouseParam[ i ] )
			continue;

		if ( values[ i ] != m_rgNewMouseParms[ i ] )
		{
			dirty = true;
			m_rgNewMouseParms[ i ] = values[ i ];

			char const *name = "";
			switch ( i )
			{
			default:
			case MOUSE_SPEED_FACTOR:
				name = "m_mousespeed";
				break;
			case MOUSE_ACCEL_THRESHHOLD1:
				name = "m_mouseaccel1";
				break;
			case MOUSE_ACCEL_THRESHHOLD2:
				name = "m_mouseaccel2";
				break;
			}

			char sz[ 256 ];
			V_snprintf( sz, sizeof( sz ), "Mouse parameter '%s' set to %i\n", name, values[ i ] );
			DevMsg( "%s", sz );
		}
	}

	if ( dirty )
	{
		// Update them
	}
}

//-----------------------------------------------------------------------------
// Purpose: One-time initialization
//-----------------------------------------------------------------------------
void CInput::Init_Mouse (void)
{
	if ( CommandLine()->FindParm("-nomouse" ) ) 
		return; 

	m_fMouseInitialized = true;

	m_fMouseParmsValid = false;

	if ( CommandLine()->FindParm ("-useforcedmparms" ) ) 
	{
		m_fMouseParmsValid = false;
		if ( m_fMouseParmsValid )
		{
			if ( CommandLine()->FindParm ("-noforcemspd" ) ) 
			{
				m_rgNewMouseParms[ MOUSE_SPEED_FACTOR ] = m_rgOrigMouseParms[ MOUSE_SPEED_FACTOR ];

/*
				int mouseAccel[3];
				SystemParametersInfo(SPI_GETMOUSE, 0, &mouseAccel, 0); mouseAccel[2] = 0; 
				bool ok = SystemParametersInfo(SPI_SETMOUSE, 0, &mouseAccel, SPIF_UPDATEINIFILE); 
				
				// Now check registry and close/re-open Control Panel > Mouse and see 'Enhance pointer precision' is OFF 
				mouseAccel[2] = 1; 
				ok = SystemParametersInfo(SPI_SETMOUSE, 0, &mouseAccel, SPIF_UPDATEINIFILE); 
				
				// Now check registry and close/re-open Control Panel > Mouse and see 'Enhance pointer precision' is ON
*/
			}
			else
			{
				m_rgCheckMouseParam[ MOUSE_SPEED_FACTOR ] = 1;
			}

			if ( CommandLine()->FindParm ("-noforcemaccel" ) ) 
			{
				m_rgNewMouseParms[ MOUSE_ACCEL_THRESHHOLD1 ] = m_rgOrigMouseParms[ MOUSE_ACCEL_THRESHHOLD1 ];
				m_rgNewMouseParms[ MOUSE_ACCEL_THRESHHOLD2 ] = m_rgOrigMouseParms[ MOUSE_ACCEL_THRESHHOLD2 ];
			}
			else
			{
				m_rgCheckMouseParam[ MOUSE_ACCEL_THRESHHOLD1 ] = true;
				m_rgCheckMouseParam[ MOUSE_ACCEL_THRESHHOLD2 ] = true;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the center point of the engine window
// Input  : int&x - 
//			y - 
//-----------------------------------------------------------------------------
void CInput::GetWindowCenter( int&x, int& y )
{
	int w, h;
	engine->GetScreenSize( w, h );

	x = w >> 1;
	y = h >> 1;
}

//-----------------------------------------------------------------------------
// Purpose: Recenter the mouse
//-----------------------------------------------------------------------------
void CInput::ResetMouse( void )
{
	int x, y;
	GetWindowCenter( x,  y );
	SetMousePos( x, y );	

	// There are cases when coming back from an alt+tab, the mouse is outside of the client rect.  Because of this, our earlier call
	// to SetCursorIcon is overridden, and we're stuck with a cursor on the screen in front of the crosshairs.  We should actually be calling
	// ShowCursor to enable/disable the cursor, but that's another story.
	//
	// On WINDOWS_PC this should be ok to call every frame without a perf hit.  Inside ResetCursorIcon, we call ::SetCursor.  
	// According to the msdn docs for SetCursor:
	// "The cursor is set only if the new cursor is different from the previous cursor; otherwise, the function returns immediately."
	g_pInputSystem->ResetCursorIcon();
}

//-----------------------------------------------------------------------------
// Purpose: GetAccumulatedMouse -- the mouse can be sampled multiple times per frame and
//  these results are accumulated each time. This function gets the accumulated mouse changes and resets the accumulators
// Input  : *mx - 
//			*my - 
//-----------------------------------------------------------------------------
void CInput::GetAccumulatedMouseDeltasAndResetAccumulators( int nSlot, float *mx, float *my )
{
	contract_assert( mx );
	contract_assert( my );

	PerUserInput_t &user = GetPerUser( nSlot );

	// SDL relative mouse mode is the only source: read the accumulated device delta
	// and reset it. (Was selected by m_rawinput; the alternative measured how far the
	// cursor had drifted from the window centre after warping it back every sample,
	// which cannot coexist with SDL holding the pointer.)
	engine->GetMouseDelta( *mx, *my );

	user.m_flAccumulatedMouseXMovement = 0;
	user.m_flAccumulatedMouseYMovement = 0;
}

//-----------------------------------------------------------------------------
// Purpose: GetMouseDelta -- Filters the mouse and stores results in old position
// Input  : mx - 
//			my - 
//			*oldx - 
//			*oldy - 
//			*x - 
//			*y - 
//-----------------------------------------------------------------------------
void CInput::GetMouseDelta( int nSlot, float inmousex, float inmousey, float *pOutMouseX, float *pOutMouseY )
{
	// Apply filtering?
	*pOutMouseX = inmousex;
	*pOutMouseY = inmousey;
}

//-----------------------------------------------------------------------------
// Purpose: Multiplies mouse values by sensitivity.  Note that for windows mouse settings
//  the input x,y offsets are already scaled based on that.  The custom acceleration, therefore,
//  is totally engine-specific and applies as a post-process to allow more user tuning.
// Input  : *x - 
//			*y - 
//-----------------------------------------------------------------------------
void CInput::ScaleMouse( int nSlot, float *x, float *y )
{
    float mx = *x;
	float my = *y;

	float flHudSensitivity = GetHud( nSlot ).GetSensitivity();

	float mouse_sensitivity = ( flHudSensitivity != 0 ) ? flHudSensitivity : sensitivity.GetFloat();

	if ( m_customaccel.GetInt() == 1 ||  m_customaccel.GetInt() == 2 ) 
	{ 
		float raw_mouse_movement_distance = std::sqrt( mx * mx + my * my );
		float acceleration_scale = m_customaccel_scale.GetFloat();
		float accelerated_sensitivity_max = m_customaccel_max.GetFloat();
		float accelerated_sensitivity_exponent = m_customaccel_exponent.GetFloat();
		float accelerated_sensitivity = ( (float)std::pow( raw_mouse_movement_distance, accelerated_sensitivity_exponent ) * acceleration_scale + mouse_sensitivity );

		if ( accelerated_sensitivity_max > 0.0001f && 
			accelerated_sensitivity > accelerated_sensitivity_max )
		{
			accelerated_sensitivity = accelerated_sensitivity_max;
		}

		*x *= accelerated_sensitivity; 
		*y *= accelerated_sensitivity; 

		// Further re-scale by yaw and pitch magnitude if user requests alternate mode 2/4
		// This means that they will need to up their value for m_customaccel_scale greatly (>40x) since m_pitch/yaw default
		//  to 0.022
		if ( m_customaccel.GetInt() == 2 || m_customaccel.GetInt() == 4 )
		{ 
			*x *= m_yaw.GetFloat(); 
			*y *= m_pitch->GetFloat(); 
		} 
	}
	else if ( m_customaccel.GetInt() == 3 )
	{
		float raw_mouse_movement_distance_squared = mx * mx + my * my;
		float fExp = std::max( 0.0f, ((m_customaccel_exponent.GetFloat() - 1.0f) / 2.0f) );
		float accelerated_sensitivity = std::powf( raw_mouse_movement_distance_squared, fExp ) * mouse_sensitivity;

		*x *= accelerated_sensitivity; 
		*y *= accelerated_sensitivity; 
	}
	else
	{ 
		*x *= mouse_sensitivity;
		*y *= mouse_sensitivity;
	}
}

//-----------------------------------------------------------------------------
// Purpose: ApplyMouse -- applies mouse deltas to CUserCmd
// Input  : viewangles - 
//			*cmd - 
//			mouse_x - 
//			mouse_y - 
//-----------------------------------------------------------------------------
void CInput::ApplyMouse( int nSlot, QAngle& viewangles, CUserCmd *cmd, float mouse_x, float mouse_y )
{
	PerUserInput_t &user = GetPerUser( nSlot );

	//roll the view angles so roll is 0 (the HL2 assumed state) and mouse adjustments are relative to the screen.
	//Assuming roll is unchanging, we want mouse left to translate to screen left at all times (same for right, up, and down)
	
	if ( !((in_strafe.GetPerUser( nSlot ).state & 1) || lookstrafe.GetInt()) )
	{
		if ( CAM_IsThirdPerson() && thirdperson_platformer.GetInt() )
		{
			if ( mouse_x )
			{
				// use the mouse to orbit the camera around the player, and update the idealAngle
				user.m_vecCameraOffset[ YAW ] -= m_yaw.GetFloat() * mouse_x;
				cam_idealyaw.SetValue( user.m_vecCameraOffset[ YAW ] - viewangles[ YAW ] );

				// why doesn't this work??? CInput::AdjustYaw is why
				//cam_idealyaw.SetValue( cam_idealyaw.GetFloat() - m_yaw.GetFloat() * mouse_x );
			}
		}
		else
		{
			// Otherwize, use mouse to spin around vertical axis

			{
				viewangles[YAW] -= m_yaw.GetFloat() * mouse_x;
			}
		}
	}
	else
	{
		// If holding strafe key or mlooking and have lookstrafe set to true, then apply
		//  horizontal mouse movement to sidemove.
		cmd->sidemove += m_side.GetFloat() * mouse_x;
	}

	// If mouselooking and not holding strafe key, then use vertical mouse
	//  to adjust view pitch.
	if (!(in_strafe.GetPerUser( nSlot ).state & 1))
	{
		if ( CAM_IsThirdPerson() && thirdperson_platformer.GetInt() )
		{
			if ( mouse_y )
			{
				// use the mouse to orbit the camera around the player, and update the idealAngle
				user.m_vecCameraOffset[ PITCH ] += m_pitch->GetFloat() * mouse_y;
				cam_idealpitch.SetValue( user.m_vecCameraOffset[ PITCH ] - viewangles[ PITCH ] );

				// why doesn't this work??? CInput::AdjustYaw is why
				//cam_idealpitch.SetValue( cam_idealpitch.GetFloat() + m_pitch->GetFloat() * mouse_y );
			}
		}
		else
		{
			{
				viewangles[PITCH] += m_pitch->GetFloat() * mouse_y;
			}

			// Check pitch bounds
			if (viewangles[PITCH] > cl_pitchdown.GetFloat())
			{
				viewangles[PITCH] = cl_pitchdown.GetFloat();
			}
			if (viewangles[PITCH] < -cl_pitchup.GetFloat())
			{
				viewangles[PITCH] = -cl_pitchup.GetFloat();
			}
		}		
	}
	else
	{
		// Otherwise if holding strafe key and noclipping, then move upward
/*		if ((in_strafe.state & 1) && IsNoClipping() )
		{
			cmd->upmove -= m_forward.GetFloat() * mouse_y;
		} 
		else */
		{
			// Default is to apply vertical mouse movement as a forward key press.
			cmd->forwardmove -= m_forward.GetFloat() * mouse_y;
		}
	}

	// Finally, add mouse state to usercmd.
	// NOTE:  Does rounding to int cause any issues?  ywb 1/17/04
    cmd->mousedx = (int)mouse_x;
	cmd->mousedy = (int)mouse_y;
}

extern bool UsingMouselook( int nSlot );

//-----------------------------------------------------------------------------
// Purpose: AccumulateMouse
//-----------------------------------------------------------------------------
void CInput::AccumulateMouse( int nSlot )
{
	if( !cl_mouseenable.GetBool() )
	{
        return;
	}

	if( !UsingMouselook( nSlot ) )
	{
        return;
	}

	// Nothing to accumulate for mouselook: SDL relative mouse mode delivers device
	// deltas, latched in GetAccumulatedMouseDeltasAndResetAccumulators(). What is left
	// is keeping a *visible* cursor inside the window while the third-person camera or
	// the UI owns it, since those work in absolute coordinates.
	PerUserInput_t &user = GetPerUser( nSlot );

	if ( ( user.m_fCameraInterceptingMouse || RocketUI()->IsConsumingInput() ) && m_fMouseActive )
	{
		int w, h;
		engine->GetScreenSize( w, h );

		int ox, oy;
		GetMousePos( ox, oy );
		SetMousePos( clamp( ox, 0, w - 1 ), clamp( oy, 0, h - 1 ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get raw mouse position
// Input  : &ox - 
//			&oy - 
//-----------------------------------------------------------------------------
void CInput::GetMousePos(int &ox, int &oy)
{
	g_pInputSystem->GetCursorPosition( &ox, &oy );
}

//-----------------------------------------------------------------------------
// Purpose: Force raw mouse position
// Input  : x - 
//			y - 
//-----------------------------------------------------------------------------
void CInput::SetMousePos( int x, int y )
{
    g_pInputStackSystem->SetCursorPosition( m_hInputContext, x, y );
}

//-----------------------------------------------------------------------------
// Purpose: MouseMove -- main entry point for applying mouse
// Input  : *cmd - 
//-----------------------------------------------------------------------------
void CInput::MouseMove( int nSlot, CUserCmd *cmd )
{
	float	mouse_x, mouse_y;
	float	mx, my;
	QAngle	viewangles;

	// Get view angles from engine
	engine->GetViewAngles( viewangles );

	// Validate mouse speed/acceleration settings
	CheckMouseAcclerationVars();

	// Don't drift pitch at all while mouselooking.
	view->StopPitchDrift ();

	//jjb - this disables normal mouse control if the user is trying to 
	//      move the camera, or if the mouse cursor is visible 
	if ( !GetPerUser( nSlot ).m_fCameraInterceptingMouse && g_pInputStackSystem->IsTopmostEnabledContext( m_hInputContext ) && cl_mouseenable.GetBool() )
	{
		// Sample mouse one more time
		AccumulateMouse( nSlot );

		// Latch accumulated mouse movements and reset accumulators
		GetAccumulatedMouseDeltasAndResetAccumulators( nSlot, &mx, &my );

		// Filter, etc. the delta values and place into mouse_x and mouse_y
		GetMouseDelta( nSlot, mx, my, &mouse_x, &mouse_y );

		if ( ControllerModeActive() )
		{
			// accumulate mouse movements and if we go over a certain threshold, switch out of controller mode
			m_fAccumulatedMouseMove += std::fabsf( mouse_x ) + fabsf( mouse_y );

			//Msg( "total_mouse_move = %f\n", m_fAccumulatedMouseMove );
			if ( m_fAccumulatedMouseMove > 30.0f )
			{
				m_bControllerMode = false;
				m_fAccumulatedMouseMove = 0.0f;
			}
		}

		// Apply scaling factor
		ScaleMouse( nSlot, &mouse_x, &mouse_y );

		// Let the client mode at the mouse input before it's used
		GetClientMode()->OverrideMouseInput( &mouse_x, &mouse_y );

		// Add mouse X/Y movement to cmd
		ApplyMouse( nSlot, viewangles, cmd, mouse_x, mouse_y );

		// Re-center the mouse.
		ResetMouse();
	}

	// Store out the new viewangles.
	engine->SetViewAngles( viewangles );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mx - 
//			*my - 
//			*unclampedx - 
//			*unclampedy - 
//-----------------------------------------------------------------------------
void CInput::GetFullscreenMousePos( int *mx, int *my, int *unclampedx /*=NULL*/, int *unclampedy /*=NULL*/ )
{
	contract_assert( mx );
	contract_assert( my );

	if ( g_pInputStackSystem->IsTopmostEnabledContext( m_hInputContext ) )
		return;

	int x, y;
	GetWindowCenter( x,  y );

	int		current_posx, current_posy;

	GetMousePos(current_posx, current_posy);

	current_posx -= x;
	current_posy -= y;

	// Now need to add back in mid point of viewport
	//

	int w, h;
	engine->GetScreenSize( w, h );
	current_posx += w  / 2;
	current_posy += h / 2;

	if ( unclampedx )
	{
		*unclampedx = current_posx;
	}

	if ( unclampedy )
	{
		*unclampedy = current_posy;
	}

	// Clamp
	current_posx = std::max( 0, current_posx );
	current_posx = std::min( ScreenWidth(), current_posx );

	current_posy = std::max( 0, current_posy );
	current_posy = std::min( ScreenHeight(), current_posy );

	*mx = current_posx;
	*my = current_posy;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
//-----------------------------------------------------------------------------
void CInput::SetFullscreenMousePos( int mx, int my )
{
	SetMousePos( mx, my );
}

//-----------------------------------------------------------------------------
// Purpose: ClearStates -- Resets mouse accumulators so you don't get a pop when returning to trapped mouse
//-----------------------------------------------------------------------------
void CInput::ClearStates (void)
{
	if ( !m_fMouseActive )
		return;

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetPerUser().m_flAccumulatedMouseXMovement = 0;
		GetPerUser().m_flAccumulatedMouseYMovement = 0;
	}

	// Drop any delta SDL has already accumulated, so a state change (menu, console,
	// level load) cannot fling the view with motion from before it.
	float flDiscardX, flDiscardY;
	engine->GetMouseDelta( flDiscardX, flDiscardY );
}

#undef PITCH
#undef YAW
