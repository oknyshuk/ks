#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "baseentity.h"

class CBaseTempEntity;


//-----------------------------------------------------------------------------
// Purpose: A server object that is used to test all registered temp entities
//-----------------------------------------------------------------------------
class CTempEntTester : public CPointEntity
{
public:
	DECLARE_CLASS( CTempEntTester, CPointEntity );

	void				Spawn( void );
	void				Think( void );

	static	CBaseEntity *Create( const Vector &vecOrigin, const QAngle &vecAngles, const char *lifetime, const char *single_te );
private:
	// Current temp entity to test
	CBaseTempEntity		*m_pCurrent;

	// Lifetime
	float				m_fLifeTime;

	char				m_szClass[ 64 ];
};
