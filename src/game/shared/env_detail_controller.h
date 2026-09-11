
#include "reflect_annotations.h"
//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

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

#ifndef CLIENT_DLL
	virtual bool KeyValue( const char *szKeyName, const char *szValue );
#endif // !CLIENT_DLL

	CNetworkVar( float, m_flFadeStartDist, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkVar( float, m_flFadeEndDist, [[= ks::reflect::Net{ .bits = 32 } ]] );

	// ALWAYS transmit to all clients.
	virtual int UpdateTransmitState( void );
};

CEnvDetailController * GetDetailController();
