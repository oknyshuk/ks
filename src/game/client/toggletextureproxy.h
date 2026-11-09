#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "materialsystem/imaterialproxy.h"

class C_BaseEntity;



class CBaseToggleTextureProxy : public IMaterialProxy
{
public:
	CBaseToggleTextureProxy();
	virtual ~CBaseToggleTextureProxy();

	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial();

private:
	void Cleanup();

	C_BaseEntity *BindArgToEntity( void *pArg );

	IMaterialVar *m_TextureVar;
	IMaterialVar *m_TextureFrameNumVar;
	bool m_WrapAnimation;
};
