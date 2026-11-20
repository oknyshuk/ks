// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.platform;
import ks.simd;
import ks.tier1.convar;

#ifdef _DEBUG

ConVar net_synctags( "net_synctags", "0", 0, "Insert tokens into the net stream to find client/server mismatches." );

#endif
