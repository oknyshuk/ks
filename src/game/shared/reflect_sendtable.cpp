// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include "cbase.h"
#include "reflect_sendtable.h"

#include "tier0/memdbgon.h"

namespace ks::reflect::net
{

SendProp make_prop( const PropDesc &d )
{
	// Only some factories take a priority; the rest have no such parameter, so a priority asked
	// for on one of those kinds cannot be honoured and must not be silently dropped.
	const byte prio = d.priority < 0 ? SENDPROP_DEFAULT_PRIORITY : (byte)d.priority;
	switch ( d.kind )
	{
	case PROP_FLOAT:
		return SendPropFloat( d.name, d.offset, d.size, d.bits, d.flags, d.low, d.high,
		                      d.proxy ? d.proxy : SendProxy_FloatToFloat, prio );
	case PROP_TIME:
	case PROP_MODELINDEX:
	case PROP_BOOL:
	case PROP_EHANDLE:
	case PROP_STRINGT:
		if ( d.priority >= 0 )
		{
			Warning( "reflect: %s asks for priority %d but its SendProp factory takes none\n",
			         d.name, d.priority );
			exit( 70 );
		}
		break;
	}

	switch ( d.kind )
	{
	case PROP_TIME:
		return SendPropTime( d.name, d.offset, d.size );
	case PROP_VECTOR:
		return SendPropVector( d.name, d.offset, d.size, d.bits, d.flags, d.low, d.high,
		                       d.proxy ? d.proxy : SendProxy_VectorToVector, prio );
	case PROP_VECTORXY:
		return SendPropVectorXY( d.name, d.offset, d.size, d.bits, d.flags, d.low, d.high,
		                         d.proxy ? d.proxy : SendProxy_VectorXYToVectorXY, prio );
	case PROP_QANGLES:
		return SendPropQAngles( d.name, d.offset, d.size, d.bits, d.flags,
		                        d.proxy ? d.proxy : SendProxy_QAngles, prio );
	case PROP_ANGLE:
		return SendPropAngle( d.name, d.offset, d.size, d.bits, d.flags,
		                      d.proxy ? d.proxy : SendProxy_AngleToFloat, prio );
	case PROP_INT:
		// SendPropInt picks a width-matched proxy when passed none, so the null is meaningful.
		return SendPropInt( d.name, d.offset, d.size, d.bits, d.flags, d.proxy, prio );
	case PROP_MODELINDEX:
		return SendPropModelIndex( d.name, d.offset, d.size );
	case PROP_BOOL:
		return SendPropBool( d.name, d.offset, d.size );
	case PROP_EHANDLE:
		// SendPropEHandle's third parameter is `flags`, not `sizeofVar`, and every one of the
		// tree's call sites passes SENDINFO's three values positionally -- so the handle's
		// size lands in the flags and every ehandle prop carries a spurious SPROP_NOSCALE.
		// Reproduced because the gate demands byte-identity; see the note in the header.
		return d.proxy ? SendPropEHandle( d.name, d.offset, d.size, SIZEOF_IGNORE, d.proxy )
		               : SendPropEHandle( d.name, d.offset, d.size );
	case PROP_STRING:
		return SendPropString( d.name, d.offset, d.size, d.flags,
		                       d.proxy ? d.proxy : SendProxy_StringToString, prio );
	case PROP_STRINGT:
		return SendPropStringT( d.name, d.offset, d.size );
	default:
		// Fail the way a table difference fails, rather than continuing into a null table.
		Warning( "reflect: make_prop reached an unhandled kind for %s\n", d.name );
		exit( 70 );
		return SendProp();
	}
}

} // namespace ks::reflect::net
