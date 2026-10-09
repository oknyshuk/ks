#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

template <typename T1> [[gnu::always_inline]] inline constexpr auto ShaderApiFast( T1 pShaderAPI ) { return pShaderAPI; }
