#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

// Purpose: Server-side counterpart to C_MemoryLog (see c_memorylog.h)

#include "igamesystem.h"

class CMemoryLog : public CAutoGameSystemPerFrame
{
public:
	// Methods of IGameSystem
	virtual void LevelInitPostEntity();

private:
};
