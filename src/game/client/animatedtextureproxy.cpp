// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "baseanimatedtextureproxy.h"

#include "imaterialproxydict.h"

class CAnimatedTextureProxy : public CBaseAnimatedTextureProxy
{
public:
	CAnimatedTextureProxy() {}
	virtual ~CAnimatedTextureProxy() {}
	virtual float GetAnimationStartTime( void* pBaseEntity );
};

EXPOSE_MATERIAL_PROXY( CAnimatedTextureProxy, AnimatedTexture );


float CAnimatedTextureProxy::GetAnimationStartTime( void* pBaseEntity )
{
	return 0;
}
