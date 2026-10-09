#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2012 Valve Corporation

// Purpose: Provide custom material swapping for weapons (when switch from world to view or vice versa)

#include "ehandle.h"
#include "igamesystem.h"

#if defined( CLIENT_DLL )
class C_BaseEntity;
typedef CHandle<C_BaseEntity> EHANDLE;
#else
class CBaseEntity;
typedef CHandle<CBaseEntity> EHANDLE;
#endif

class ICustomMaterial;

struct CCSPendingCustomMaterialSwap_t
{
	ICustomMaterial *m_pNewCustomMaterial;
	ICustomMaterial *m_pOldCustomMaterial;
	int m_nCustomMaterialIndex;
	EHANDLE m_hOwner;
};

class CCSCustomMaterialSwapManager : public CAutoGameSystem
{
public:
	CCSCustomMaterialSwapManager();
	virtual ~CCSCustomMaterialSwapManager();

	virtual bool Init();
	virtual void Shutdown();

	bool Process();

	void RequestMaterialSwap( EHANDLE hOwner, int nCustomMaterialIndex, ICustomMaterial *pNewCustomMaterialInterface );
	void ClearPendingSwaps( EHANDLE hOwner, int nCustomMaterialIndex );

private:
	void ClearAllPendingSwaps();

	std::vector< CCSPendingCustomMaterialSwap_t > m_pPendingSwaps;
};

extern CCSCustomMaterialSwapManager g_CSCustomMaterialSwapManager;
