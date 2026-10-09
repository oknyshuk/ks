#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Exposes client-server neutral interfaces implemented in both places

#define random random_valve// stdlib.h defined random() and our class defn conflicts so under POSIX rename it using the preprocessor

class IFileSystem;
class IUniformRandomStream;
class CGaussianRandomStream;
class IEngineSound;
class IMapData;
class IGameTypes;

extern IFileSystem				*filesystem;
extern IUniformRandomStream		*random;
extern CGaussianRandomStream *randomgaussian;
extern IEngineSound				*enginesound;
extern IMapData					*g_pMapData;			// TODO: current implementations of the 
														// interface are in TF2, should probably move
														// to TF2/HL2 neutral territory
extern IGameTypes				*g_pGameTypes;
