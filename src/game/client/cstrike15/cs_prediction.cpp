// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "prediction.h"
#include "c_cs_player.h"
#include "igamemovement.h"

static CMoveData g_MoveData;
CMoveData *g_pMoveData = &g_MoveData;


class CCSPrediction : public CPrediction
{
DECLARE_CLASS( CCSPrediction, CPrediction );

public:
	virtual void	SetupMove( C_BasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );
	virtual void	FinishMove( C_BasePlayer *player, CUserCmd *ucmd, CMoveData *move );
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPrediction::SetupMove( C_BasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, 
	CMoveData *move )
{
	player->AvoidPhysicsProps( ucmd );

	// Call the default SetupMove code.
	BaseClass::SetupMove( player, ucmd, pHelper, move );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPrediction::FinishMove( C_BasePlayer *player, CUserCmd *ucmd, CMoveData *move )
{
	// Call the default FinishMove code.
	BaseClass::FinishMove( player, ucmd, move );
}


// Expose interface to engine
// Expose interface to engine
static CCSPrediction g_Prediction;

static InterfaceReg s_Reg_CCSPrediction_IPrediction( []() -> void * { return static_cast<IPrediction *>( &g_Prediction ); }, VCLIENT_PREDICTION_INTERFACE_VERSION );;

CPrediction *prediction = &g_Prediction;
