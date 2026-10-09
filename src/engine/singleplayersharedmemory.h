#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation

import std;
import ks.tier1;
#include "ispsharedmemory.h"

class CSPSharedMemory;

class CSPSharedMemoryManager
{
public:
	ISPSharedMemory *GetSharedMemory( const char *handle, int ent_num );
	~CSPSharedMemoryManager( void );

private:
	std::vector< CSPSharedMemory * > m_SharedSpaces;
	friend class CSPSharedMemory;
};

extern CSPSharedMemoryManager *g_pSinglePlayerSharedMemoryManager;
