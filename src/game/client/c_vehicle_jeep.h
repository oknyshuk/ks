#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.core;
#include "reflect_annotations.h"

import std;
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_prop_vehicle.h"
//#include "movevars_shared.h"
//#include "view.h"
#include "flashlighteffect.h"
//#include "c_baseplayer.h"
//#include "c_te_effect_dispatch.h"

//=============================================================================
//
// Client-side Jeep Class
//
class [[= ks::reflect::NetTable{ .name = "DT_PropJeep" } ]]
      C_PropJeep : public C_PropVehicleDriveable
{

	DECLARE_CLASS( C_PropJeep, C_PropVehicleDriveable );

public:

	DECLARE_CLIENTCLASS();

	C_PropJeep();
	~C_PropJeep();

public:

	void UpdateViewAngles( C_BasePlayer *pLocalPlayer, CUserCmd *pCmd );
	void DampenEyePosition( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles );

	void OnEnteredVehicle( C_BasePlayer *pPlayer );
	bool Simulate( void );

private:

	void DampenForwardMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime );
	void DampenUpMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime );
	void ComputePDControllerCoefficients( float *pCoefficientsOut, float flFrequency, float flDampening, float flDeltaTime );

private:

	Vector		m_vecLastEyePos;
	Vector		m_vecLastEyeTarget;
	Vector		m_vecEyeSpeed;
	Vector		m_vecTargetSpeed;

	float		m_flViewAngleDeltaTime;

	float		m_flJeepFOV;
	CHeadlightEffect *m_pHeadlight;
	[[= ks::reflect::Net{} ]] bool		m_bHeadlightIsOn;
};
