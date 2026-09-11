//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_BREAKABLEPROP_H
#define C_BREAKABLEPROP_H

#include "reflect_annotations.h"
#include "dt_recv.h"
#ifdef _WIN32
#pragma once
#endif

#include "player_pickup.h" 

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
	[[= ks::reflect::Net{ .enc = ks::reflect::ENC_QANGLES } ]] [[= ks::reflect::Proxy<RecvProxy_UnmodifiedQAngles, ks::reflect::WIRE_RECV>{} ]] QAngle m_qPreferredPlayerCarryAngles;

private:
	[[= ks::reflect::Net{} ]] bool m_bClientPhysics;
};

#endif // C_BREAKABLEPROP_H
