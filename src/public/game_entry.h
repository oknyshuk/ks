#pragma once
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

import ks.platform;
import ks.simd;
import ks.tier1.createinterface;
import ks.tier1.interfaces;

// The factories of the game sides that are linked into this executable. The executable's main
// defines them (srceng: server and client, srcds: server only), so the engine reaches a side
// directly instead of looking a module up by name.
CreateInterfaceFn ServerGameFactory();
CreateInterfaceFn ClientGameFactory();	// null in an executable without client
