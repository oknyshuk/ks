#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2013 Valve Corporation

// Purpose: Interface to help with rendering CMDLs & CMergedMDLs to textures.

#include "materialsystem/imaterialsystem.h"
#include "mathlib/camera.h"
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class ITexture;

class IRenderToRTHelperObject
{
public:
	virtual void Draw( const matrix3x4_t &rootToWorld ) = 0;
	virtual bool GetBoundingSphere( Vector &vecCenter, float &flRadius ) = 0;
	virtual ITexture *GetEnvCubeMap() = 0;
};

enum RenderToRTStage_t
{
	RENDER_TO_RT_STAGE_CREATED = 0,
	RENDER_TO_RT_STAGE_STARTED,
	RENDER_TO_RT_STAGE_WAITING_FOR_READ_BACK,
	RENDER_TO_RT_STAGE_WAITING_FOR_RESULT,
	RENDER_TO_RT_STAGE_DONE,

	RENDER_TO_RT_STAGE_UNDEFINED = -1
};

struct RenderToRTData_t
{
	RenderToRTData_t() 
		: m_pObject( nullptr )
		, m_pResultVTF( nullptr )
		, m_pszIconNameSuffix( nullptr )
		, m_stage( RENDER_TO_RT_STAGE_UNDEFINED )
	{
	}

	IRenderToRTHelperObject *m_pObject;
	IVTFTexture *m_pResultVTF;

	MaterialLightingState_t m_LightingState;
	QAngle m_cameraAngles;
	Vector m_cameraOffset;
	float m_cameraFOV;
	Camera_t m_Camera;
	matrix3x4_t m_rootToWorld;

	RenderToRTStage_t m_stage;

	const char *m_pszIconNameSuffix;

	bool m_bUsingExplicitModelCameraPosAnglesFromAttachment;

};

class IRenderToRTHelper
{
public:
	virtual bool Init() = 0;
	virtual void Shutdown() = 0;
	virtual RenderToRTData_t *CreateRenderToRTData( IRenderToRTHelperObject *pObject, IVTFTexture *pResultVTF ) = 0;
	virtual void StartRenderToRT( RenderToRTData_t *pRendertoRTData ) = 0;
	virtual void DestroyRenderToRTData( RenderToRTData_t *pRendertoRTData ) = 0;
	virtual bool Process() = 0;
};

inline constexpr const char *RENDER_TO_RT_HELPER_INTERFACE_VERSION = "RenderToRTHelper001";
