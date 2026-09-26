// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)


#pragma once

//-------------------------------------------------------------------------------------------------

class IVPhysicsKeyParser;

IVPhysicsKeyParser* CreateVPhysicsKeyParser( const char* pKeyData, bool bIsPacked );
void DestroyVPhysicsKeyParser( IVPhysicsKeyParser* pParser );
