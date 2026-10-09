#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

struct msurface2_t;
typedef msurface2_t *SurfaceHandle_t;
typedef msurface2_t * __restrict__ SurfaceHandleRestrict_t;
const SurfaceHandle_t SURFACE_HANDLE_INVALID = nullptr;
template <typename T1> [[gnu::always_inline]] inline auto IS_SURF_VALID( T1 surfID ) { return ( surfID != SURFACE_HANDLE_INVALID ); }
