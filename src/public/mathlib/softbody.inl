#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "mathlib/softbodyenvironment.h"
import ks.core.memalloc;
inline void CSoftbody::Shutdown()
{
	m_pEnvironment->Unregister( this );
	MemAlloc_FreeAligned( m_pParticles );
	m_StickyBuffer.Clear();
}
