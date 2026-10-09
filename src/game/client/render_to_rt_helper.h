#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2013 Valve Corporation

// Purpose: Class to help with rendering CMDLs & CMergedMDLs to textures.

import ks.core;
import std;
#include "irendertorthelperobject.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IVTFTexture;
class ITexture;

class CRenderToRTHelper	: public IRenderToRTHelper
{
public:
	CRenderToRTHelper();
	virtual ~CRenderToRTHelper();

	// Methods of IRenderToRTHelper
	virtual bool Init();
	virtual void Shutdown();
	virtual RenderToRTData_t *CreateRenderToRTData( IRenderToRTHelperObject *pObject, IVTFTexture *pResultVTF );
	virtual void StartRenderToRT( RenderToRTData_t *pRendertoRTData );
	virtual void DestroyRenderToRTData( RenderToRTData_t *pRendertoRTData );
	virtual bool Process();

	void LookAt( Camera_t &camera, const Vector &vecCenter, float flRadius, QAngle cameraAngles, Vector cameraOffset );

private:
	std::vector< RenderToRTData_t * > m_objectsToRender;
	RenderToRTData_t *m_pCurrentObjectToRender;
	MaterialLightingState_t m_LightingState;	

	CThreadEvent *m_pPixelsReadEvent;
	ITexture *m_pRenderTarget;
};
