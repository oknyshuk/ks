#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.vstdlib;
#include "materialsystem/imaterialsystem.h"
import std;


class IMaterialInternal;

//-----------------------------------------------------------------------------
// Special call queue that knows (a) single threaded access, and (b) all
// functions called after last function added
//-----------------------------------------------------------------------------

class CMatCallQueue : public ICallQueue
{
public:
	CMatCallQueue()
	{
		static int nStackCount = 0;
		CFmtStr stackName( "CMatCallQueue.m_Allocator[%d]", nStackCount++ );
#ifdef DEDICATED
		m_Allocator.Init( (const char *)stackName, 2*1024, 0, 0, 16 );
#else
		m_Allocator.Init( (const char *)stackName, 2*1024*1024, 64*1024, 256*1024, 16 );
#endif
	}

	void CallQueued()
	{
		CPerfStatsScope _perfStatsScope3171( "RenderThread", PERF_STATS_SLOT_RENDERTHREAD );
		ICallQueue::CallQueued();
	}
};

inline constexpr auto MATCONFIG_FLAGS_SUPPORT_EDITOR = ( 1 << 0 );
inline constexpr auto MATCONFIG_FLAGS_SUPPORT_GBUFFER = ( 1 << 1 );
//-----------------------------------------------------------------------------
// Additional interfaces used internally to the library
//-----------------------------------------------------------------------------
class IMaterialSystemInternal : public IMaterialSystem
{
public:
	// Returns the current material
	IMaterial* GetCurrentMaterial();

	int GetLightmapPage( void );

	// Gets the maximum lightmap page size...
	int GetLightmapWidth( int lightmap ) const;
	int GetLightmapHeight( int lightmap ) const;

	ITexture *GetLocalCubemap( void );

//	virtual bool RenderZOnlyWithHeightClipEnabled( void ) = 0;
	void ForceDepthFuncEquals( bool bEnable );
	enum MaterialHeightClipMode_t GetHeightClipMode( void );

	// FIXME: Remove? Here for debugging shaders in CShaderSystem
	void AddMaterialToMaterialList( IMaterialInternal *pMaterial );
	void RemoveMaterial( IMaterialInternal *pMaterial );
	void RemoveMaterialSubRect( IMaterialInternal *pMaterial );
	bool InFlashlightMode() const;
	bool IsCascadedShadowMapping() const;

	// Can we use editor materials?
	bool CanUseEditorMaterials() const;
	int GetConfigurationFlags( void ) const;
	const char *GetForcedTextureLoadPathID();

	CMatCallQueue *GetRenderCallQueue();

	void UnbindMaterial( IMaterial *pMaterial );
	uint GetRenderThreadId() const;
};
