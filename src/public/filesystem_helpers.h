#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
import ks.platform;
import ks.simd;
import ks.tier1.characterset;
import ks.tier1.keyvalues;

// Call until it returns NULL. Each time you call it, it will parse out a token.
const char* ParseFile( const char* pFileBytes, char* pToken, bool* pWasQuoted, characterset_t *pCharSet = nullptr );
char* ParseFile( char* pFileBytes, char* pToken, bool* pWasQuoted );	// (same exact thing as the const version)
