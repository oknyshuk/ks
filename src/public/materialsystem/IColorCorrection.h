#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.basetypes;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import std;
#include "bitmap/imageformat.h"

typedef std::uintptr_t ColorCorrectionHandle_t;
struct ShaderColorCorrectionInfo_t;

class IColorCorrectionSystem
{
public:
	void Init();
	void Shutdown();

	ColorCorrectionHandle_t AddLookup( const char *pName );
	bool RemoveLookup( ColorCorrectionHandle_t handle );

	void  SetLookupWeight( ColorCorrectionHandle_t handle, float flWeight );
	float GetLookupWeight( ColorCorrectionHandle_t handle );
	float GetLookupWeight( int i );

	void LockLookup();
	void LockLookup( ColorCorrectionHandle_t handle );

	void UnlockLookup();
	void UnlockLookup( ColorCorrectionHandle_t handle, bool bDownload = true );

	void SetLookup( RGBX5551_t inColor, color24 outColor );
	void SetLookup( ColorCorrectionHandle_t handle, RGBX5551_t inColor, color24 outColor );

	color24 GetLookup( RGBX5551_t inColor );
	color24 GetLookup( ColorCorrectionHandle_t handle, RGBX5551_t inColor );

	void LoadLookup( const char *pLookupName );
	void LoadLookup( ColorCorrectionHandle_t handle, const char *pLookupName );

	void CopyLookup( const color24 *pSrcColorCorrection );
	void CopyLookup( ColorCorrectionHandle_t handle, const color24 *pSrcColorCorrection );

	void ResetLookup( ColorCorrectionHandle_t handle );
	void ResetLookup( );

	void ReleaseTextures( );
	void RestoreTextures( );

	void ResetLookupWeights( );

	int GetNumLookups( );

	color24 ConvertToColor24( RGBX5551_t inColor );

	void SetResetable( ColorCorrectionHandle_t handle, bool bResetable );

	void EnableColorCorrection( bool bEnable );

	// FIXME: Move this to a private interface only the material system can see?
	void GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo );

	void OnProceduralRegenComplete( ColorCorrectionHandle_t handle );

	ColorCorrectionHandle_t FindLookup( const char *pName );
};
