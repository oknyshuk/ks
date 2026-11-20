#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

#include "togl/rendermechanism.h"
import ks.core.basetypes;

#ifndef ZeroMemory
#define ZeroMemory( ptr, size ) memset( ptr, 0, size )
#endif

	void Sleep( unsigned int ms );
	bool IsIconic( VD3DHWND hWnd );
	BOOL ClientToScreen( VD3DHWND hWnd, LPPOINT pPoint );
	void* GetCurrentThread();
	void SetThreadAffinityMask( void *hThread, int nMask );
	void GlobalMemoryStatus( MEMORYSTATUS *pOut );
	void GetClientRect( void *hWnd, RECT *destRect );
