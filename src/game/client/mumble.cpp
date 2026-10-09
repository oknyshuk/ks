// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: Provides an interface to enable positional audio support in Mumble


#include "rocketui/rmlui.h"
import ks.tier1;
import std;
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "view.h"
#include "mumble.h"

const char *COM_GetModDirectory(); // return the mod dir (rather than the complete -game param, which can be a path)

struct MumbleSharedMemory_t
{
	uint32_t uiVersion;
	uint32_t uiTick;
	float	fAvatarPosition[3];
	float	fAvatarFront[3];
	float	fAvatarTop[3];
	wchar_t	name[256];
	float	fCameraPosition[3];
	float	fCameraFront[3];
	float	fCameraTop[3];
	wchar_t	identity[256];
	uint32_t context_len;
	unsigned char context[256];
	wchar_t description[2048];
};

MumbleSharedMemory_t *g_pMumbleMemory = nullptr;
ks::platform::shared_memory *g_pMumbleMapping = nullptr;

ConVar sv_mumble_positionalaudio( "sv_mumble_positionalaudio", "1", FCVAR_REPLICATED, "Allows players using Mumble to have support for positional audio." );

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CMumbleSystem g_MumbleSystem;

IGameSystem *MumbleSystem()
{
	return &g_MumbleSystem;
}

bool CMumbleSystem::Init()
{
	return true;
}

void CMumbleSystem::LevelInitPostEntity()
{
	if ( g_pMumbleMemory )
		return;

	char memname[256];
	V_sprintf_safe( memname, "/MumbleLink.%d", (int)ks::platform::user_id() );

	g_pMumbleMapping = ks::platform::open_shared_memory( memname, sizeof(MumbleSharedMemory_t), nullptr );
	if ( g_pMumbleMapping == nullptr )
	{
		return;
	}

	g_pMumbleMemory = (MumbleSharedMemory_t *)ks::platform::shared_memory_data( g_pMumbleMapping );
}

void CMumbleSystem::LevelShutdownPreEntity()
{
	if ( g_pMumbleMemory )
	{
		ks::platform::close_shared_memory( g_pMumbleMapping );
		g_pMumbleMapping = nullptr;
		g_pMumbleMemory = nullptr;
	}
}

void VectorToMumbleFloatArray( const Vector &vec, float *rgfl )
{
	// Traditional left-handed coordinate system
	rgfl[0] = vec.x; // X
	rgfl[1] = vec.z; // Y is up
	rgfl[2] = vec.y; // Z is into the screen
}

void CMumbleSystem::PostRender()
{
	if ( !g_pMumbleMemory || !sv_mumble_positionalaudio.GetBool() )
		return;

	if ( g_pMumbleMemory->uiVersion != 2 )
	{
		V_wcscpy_safe( g_pMumbleMemory->name, L"Source: " );
		wchar_t wcsGameDir[MAX_PATH];
		V_UTF8ToUnicode( COM_GetModDirectory(), wcsGameDir, sizeof(wcsGameDir) );
		V_wcscat_safe( g_pMumbleMemory->name, wcsGameDir );

		V_wcscpy_safe( g_pMumbleMemory->description, L"Links Source engine games to Mumble." );
		g_pMumbleMemory->uiVersion = 2;
	}

	g_pMumbleMemory->uiTick++;

	Vector vecOriginPlayer, vecOriginCamera = MainViewOrigin( 0 );
	QAngle anglesPlayer, anglesCamera = MainViewAngles( 0 );

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		bool bIsOnTeam = pPlayer->GetTeamNumber() == TEAM_TERRORIST || pPlayer->GetTeamNumber() == TEAM_CT;
		if ( pPlayer->IsAlive() && bIsOnTeam )
		{
			vecOriginPlayer = pPlayer->EyePosition();
		}
		else
		{
			// a zero player origin disables positional audio
			vecOriginPlayer = vec3_origin;
		}
		anglesPlayer = pPlayer->GetAbsAngles();
	}
	else
	{
		vecOriginPlayer = vecOriginCamera;
		anglesPlayer = anglesCamera;
	}

	anglesPlayer.x = 0;

	Vector vecPlayerForward, vecPlayerUp, vecCameraForward, vecCameraUp;
	AngleVectors( anglesPlayer, &vecPlayerForward, nullptr, &vecPlayerUp );
	AngleVectors( anglesCamera, &vecCameraForward, nullptr, &vecCameraUp );

	// 1 Source unit is about one inch
	// 1 mumble unit = 1 meter
	vecOriginPlayer *= METERS_PER_INCH;
	vecOriginCamera *= METERS_PER_INCH;

	VectorToMumbleFloatArray( vecPlayerForward, g_pMumbleMemory->fAvatarFront );
	VectorToMumbleFloatArray( vecPlayerUp, g_pMumbleMemory->fAvatarTop );
	VectorToMumbleFloatArray( vecOriginPlayer, g_pMumbleMemory->fAvatarPosition );

	VectorToMumbleFloatArray( vecCameraForward, g_pMumbleMemory->fCameraFront );
	VectorToMumbleFloatArray( vecCameraUp, g_pMumbleMemory->fCameraTop );
	VectorToMumbleFloatArray( vecOriginCamera, g_pMumbleMemory->fCameraPosition );

	g_pMumbleMemory->context_len = 0;
}

