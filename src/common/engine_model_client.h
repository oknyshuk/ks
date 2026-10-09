#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2016 Valve Corporation

// Purpose: Sharing model structures and flags between engine and client

inline constexpr auto ENGINE_MODEL_CLIENT_MODELFLAG_RENDER_DISABLED = 0x0080; // excluded for compliance with government regulations

inline constexpr auto ENGINE_MODEL_CLIENT_MODELT_OFFSET_FLAGS = 280;

inline int EngineModelClientFlags( const void *pModel ) { return *reinterpret_cast< const int * >( ((const char*)pModel) + ENGINE_MODEL_CLIENT_MODELT_OFFSET_FLAGS ); }
