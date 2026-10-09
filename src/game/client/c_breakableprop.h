#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"
#include "dt_recv.h"

#include "player_pickup.h" 
#include "c_baseanimating.h"
#include "client_class.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RecvProxy_UnmodifiedQAngles( const CRecvProxyData *pData, void *pStruct, void *pOut );

class [[= ks::reflect::NetTable{ .name = "DT_BreakableProp" } ]]
      C_BreakableProp : public C_BaseAnimating, public CDefaultPlayerPickupVPhysics
{
public:
	DECLARE_CLASS( C_BreakableProp, C_BaseAnimating );
	DECLARE_CLIENTCLASS();

	C_BreakableProp();

	virtual bool IsProp( void ) const
	{
		return true;
	};

	//IPlayerPickupVPhysics
	virtual bool HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer );
	virtual QAngle PreferredCarryAngles( void );

	virtual bool	ShouldPredict( void );
	virtual C_BasePlayer *GetPredictionOwner( void );
	virtual bool PredictionErrorShouldResetLatchedForAllPredictables( void ) { return false; }

	// Copy fade from another breakable prop
	void CopyFadeFrom( C_BreakableProp *pSource );
	virtual void OnDataChanged( DataUpdateType_t type );

	const QAngle &GetNetworkedPreferredPlayerCarryAngles( void ) { return m_qPreferredPlayerCarryAngles; }

protected:
	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::QAngles } ]] [[= ks::reflect::Proxy<RecvProxy_UnmodifiedQAngles, ks::reflect::WireSide::Recv>{} ]] QAngle m_qPreferredPlayerCarryAngles;

private:
	[[= ks::reflect::Net{} ]] bool m_bClientPhysics;
};
