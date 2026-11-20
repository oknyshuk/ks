#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: This file contains code to allow us to associate client data with bsp leaves.

import ks.core.types;
import ks.platform;
import ks.simd;
#include "client_render_handle.h"
#include "engine/ivmodelinfo.h"


inline constexpr const char *CLIENTLEAFSYSTEM_INTERFACE_VERSION = "ClientLeafSystem002";


enum RenderableModelType_t
{
	RENDERABLE_MODEL_UNKNOWN_TYPE = -1,
	RENDERABLE_MODEL_ENTITY = 0,
	RENDERABLE_MODEL_STUDIOMDL,
	RENDERABLE_MODEL_STATIC_PROP,
	RENDERABLE_MODEL_BRUSH,
};


//-----------------------------------------------------------------------------
// The client leaf system
//-----------------------------------------------------------------------------
class IClientLeafSystemEngine
{
public:
	// Adds and removes renderables from the leaf lists
	// CreateRenderableHandle stores the handle inside pRenderable.
	virtual void CreateRenderableHandle( IClientRenderable* pRenderable, bool bRenderWithViewModels, RenderableTranslucencyType_t nType, RenderableModelType_t nModelType, uint32 nSplitscreenEnabled = 0xFFFFFFFF ) = 0; // = RENDERABLE_MODEL_UNKNOWN_TYPE ) = 0;
	virtual void RemoveRenderable( ClientRenderHandle_t handle ) = 0;
	virtual void AddRenderableToLeaves( ClientRenderHandle_t renderable, int nLeafCount, unsigned short *pLeaves ) = 0;
	virtual void SetTranslucencyType( ClientRenderHandle_t handle, RenderableTranslucencyType_t nType ) = 0;
	virtual void RenderInFastReflections( ClientRenderHandle_t handle, bool bRenderInFastReflections ) = 0;
	virtual void DisableShadowDepthRendering( ClientRenderHandle_t handle, bool bDisable ) = 0;
	virtual void DisableCSMRendering( ClientRenderHandle_t handle, bool bDisable ) = 0;
};
