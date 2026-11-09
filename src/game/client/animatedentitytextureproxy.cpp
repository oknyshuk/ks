// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatcharacter.h"
#include "baseanimatedtextureproxy.h"

#include "imaterialproxydict.h"

class CAnimatedEntityTextureProxy : public CBaseAnimatedTextureProxy
{
public:
	CAnimatedEntityTextureProxy() {}
	virtual ~CAnimatedEntityTextureProxy() {}

	virtual float GetAnimationStartTime( void* pBaseEntity );
	virtual void AnimationWrapped( void* pC_BaseEntity );

};

EXPOSE_MATERIAL_PROXY( CAnimatedEntityTextureProxy, AnimatedEntityTexture );

float CAnimatedEntityTextureProxy::GetAnimationStartTime( void* pArg )
{
	IClientRenderable *pRend = (IClientRenderable *)pArg;
	if (!pRend)
		return 0.0f;

	C_BaseEntity* pEntity = EntityCast<C_BaseEntity>( pRend->GetIClientUnknown()->GetBaseEntity() );
	if (pEntity)
	{
		return pEntity->GetTextureAnimationStartTime();
	}
	return 0.0f;
}

void CAnimatedEntityTextureProxy::AnimationWrapped( void* pArg )
{
	IClientRenderable *pRend = (IClientRenderable *)pArg;
	if (!pRend)
		return;

	C_BaseEntity* pEntity = EntityCast<C_BaseEntity>( pRend->GetIClientUnknown()->GetBaseEntity() );
	if (pEntity)
	{
		pEntity->TextureAnimationWrapped();
	}
}
