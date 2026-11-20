// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2012 Valve Corporation

module;
#include "client_class.h"
#include "networkvar.h"
#include "reflect_annotations.h"
module ks.client:cstrike15.Effects.chicken;

import ks.core.basetypes;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import ks.game.ai_activity;
import :props;
import :particlemgr;

class CBaseAnimating;

using namespace ::ks::reflect;


// Purpose: Client side interactive, shootable chicken

class CCChicken : public CDynamicProp
{
	[[= ks::reflect::NetTable{ .name = "DT_CChicken" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CCChicken, CDynamicProp );
	DECLARE_CLIENTCLASS();

	CCChicken();
	virtual ~CCChicken();

private:
	CCChicken( const CCChicken& );				// not defined, not accessible

public:
	void SetClientSideHolidayHatAddon( bool bEnable );

public:
	virtual void Spawn();

	static void RecvProxy_Jumped( const CRecvProxyData *pData, void *pStruct, void *pOut );

	virtual void ClientThink( );

private:
	CHandle<CBaseAnimating> m_hHolidayHatAddon;
	Activity m_lastActivity;

	CNetworkVar( bool, m_jumpedThisFrame, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int } ]]
	                    [[= ks::reflect::Proxy<RecvProxy_Jumped, ks::reflect::WireSide::Recv>{} ]] );
	CNetworkVar( EHANDLE, m_leader, [[= ks::reflect::Net{} ]] );				// who we are following, or NULL

};
