// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include "view_shared.h"
module ks.client:cstrike15.cs_view_scene;

import :particlemgr;
import :util;

class CViewRender;

//-----------------------------------------------------------------------------
// Purpose: Implements the interview to view rendering for the client .dll
//-----------------------------------------------------------------------------
class CCSViewRender : public CViewRender
{
public:
	CCSViewRender();

	virtual void Init( void );

	virtual void Render2DEffectsPreHUD( const CViewSetup &view );
	virtual void Render2DEffectsPostHUD( const CViewSetup &view );
	virtual void RenderPlayerSprites( void );
	virtual void RenderSmokeOverlay( bool bPreViewModel = true );

private:

	void PerformFlashbangEffect( const CViewSetup &view );
	void PerformNightVisionEffect( const CViewSetup &view );
	
	ITexture *m_pFlashTexture;
};
