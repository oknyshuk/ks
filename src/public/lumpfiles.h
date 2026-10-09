#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

inline constexpr auto MAX_LUMPFILES = 0;

//-----------------------------------------------------------------------------
// Lump files
//-----------------------------------------------------------------------------
void GenerateLumpFileName( const char *bspfilename, char *lumpfilename, int iBufferSize, int iIndex );
