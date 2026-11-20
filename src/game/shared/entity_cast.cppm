// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.game.entity_cast;

import ks.core.basetypes;
import ks.platform;

export
{
// The engine interfaces return the game's entity as its interface base (IServerUnknown,
// IClientUnknown); this is the downcast. The interface is the entity's first base, so the cast
// does not move the pointer and also works where the entity class is still incomplete.
template <class T, class U>
inline T *EntityCast( U *pUnknown )
{
	return ( T * )pUnknown;
}
}
