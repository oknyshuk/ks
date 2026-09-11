//========= Copyright (c) 1996-2012, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side interactive, shootable chicken
//
// $NoKeywords: $
//=============================================================================//


#ifndef C_CHICKEN_H
#define C_CHICKEN_H


#include "c_props.h"

#define CChicken C_CChicken

class [[= ks::reflect::NetTable{ .name = "DT_CChicken" } ]]
      C_CChicken : public C_DynamicProp
{
public:
	DECLARE_CLASS( C_CChicken, C_DynamicProp );
	DECLARE_CLIENTCLASS();

	C_CChicken();
	virtual ~C_CChicken();

private:
	C_CChicken( const C_CChicken& );				// not defined, not accessible

public:
	void SetClientSideHolidayHatAddon( bool bEnable );

public:
	virtual void Spawn();

	static void RecvProxy_Jumped( const CRecvProxyData *pData, void *pStruct, void *pOut );

	virtual void ClientThink( );

private:
	CHandle<C_BaseAnimating> m_hHolidayHatAddon;
	Activity m_lastActivity;

	CNetworkVar( bool, m_jumpedThisFrame, [[= ks::reflect::Net{ .enc = ks::reflect::ENC_INT } ]]
	                    [[= ks::reflect::Proxy<RecvProxy_Jumped, ks::reflect::WIRE_RECV>{} ]] );
	CNetworkVar( EHANDLE, m_leader, [[= ks::reflect::Net{} ]] );				// who we are following, or NULL

};

#endif // C_CHICKEN_H
