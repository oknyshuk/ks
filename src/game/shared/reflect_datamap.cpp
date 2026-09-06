// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include "cbase.h"
#include "reflect_datamap.h"

#include "tier0/memdbgon.h"

namespace ks::reflect::dmap
{

typedescription_t make_field( const FieldDesc &d, bool output )
{
	FieldDesc f = d;
#ifdef GAME_DLL
	// eventFuncs is entityoutput.h's, and is_output is always false on the client.
	if ( output ) f.ops = eventFuncs;
#else
	( void )output;
#endif
	return MakeField( f );
}

} // namespace ks::reflect::dmap
