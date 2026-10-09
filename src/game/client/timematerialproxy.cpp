// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core;
#include "cdll_client_int.h"
#include "functionproxy.h"

#include "imaterialproxydict.h"

class CTimeMaterialProxy : public CResultProxy
{
public:
	virtual void OnBind( void *pC_BaseEntity );
};					    

void CTimeMaterialProxy::OnBind( void *pC_BaseEntity )
{
	SetFloatResult( gpGlobals->curtime );
}

EXPOSE_MATERIAL_PROXY( CTimeMaterialProxy, CurrentTime );
