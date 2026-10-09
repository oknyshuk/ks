#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// DATA STRUCTURE INFO

inline constexpr auto MAX_NUM_ARGVS = 50;

// SYSTEM INFO
inline constexpr auto MAX_QPATH = 96; // max length of a game pathname
#define	MAX_OSPATH		260			// max length of a filesystem pathname

#define	ON_EPSILON		0.1			// point on plane side epsilon


// Resource counts;
// Must have this value in sync(-1) with const.h and effect_dispatch_data.cpp
#define MAX_MODEL_INDEX_BITS	12   // sent as a short
inline constexpr auto MAX_MODELS = (1<<MAX_MODEL_INDEX_BITS);

inline constexpr auto MAX_GENERIC_INDEX_BITS = 9;
inline constexpr auto MAX_GENERIC = (1<<MAX_GENERIC_INDEX_BITS);
inline constexpr auto MAX_DECAL_INDEX_BITS = 9;
inline constexpr auto MAX_BASE_DECALS = (1<<MAX_DECAL_INDEX_BITS);
