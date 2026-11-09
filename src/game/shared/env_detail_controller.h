#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"
#ifdef CLIENT_DLL
#include "c_baseentity.h"
#else
#include "baseentity.h"
#endif

#ifdef CLIENT_DLL
	#define CEnvDetailController C_EnvDetailController
#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Implementation of the class that controls detail prop fade distances
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_DetailController", .base = false } ]]
      CEnvDetailController : public CBaseEntity
{
public:
	DECLARE_CLASS( CEnvDetailController, CBaseEntity );
	DECLARE_NETWORKCLASS();

	CEnvDetailController();
	virtual ~CEnvDetailController();

using BaseClass::KeyValue_Impl;
	#ifndef CLIENT_DLL
	KS_VIRTUAL bool KeyValue_Impl( const char *szKeyName, const char *szValue );
#endif // !CLIENT_DLL

	CNetworkVar( float, m_flFadeStartDist, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkVar( float, m_flFadeEndDist, [[= ks::reflect::Net{ .bits = 32 } ]] );

	// ALWAYS transmit to all clients.
	KS_VIRTUAL int UpdateTransmitState_Impl( void );
};

CEnvDetailController * GetDetailController();
