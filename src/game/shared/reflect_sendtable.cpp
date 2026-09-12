// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include "cbase.h"
#include "reflect_sendtable.h"

#include "tier0/memdbgon.h"

namespace ks::reflect::net
{

SendProp make_prop( const PropDesc &d, std::span<const PropDesc> all )
{
	// Only some factories take a priority; the rest have no such parameter, so a priority asked
	// for on one of those kinds cannot be honoured and must not be silently dropped.
	const byte prio = d.priority < 0 ? SENDPROP_DEFAULT_PRIORITY : (byte)d.priority;
	switch ( d.kind )
	{
	case PropKind::Float:
		return SendPropFloat( d.name, d.offset, d.size, d.bits, d.flags, d.low, d.high,
		                      d.proxy ? d.proxy : SendProxy_FloatToFloat, prio );
	case PropKind::Time:
	case PropKind::ModelIndex:
	case PropKind::Bool:
	case PropKind::EHandle:
	case PropKind::StringT:
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
	case PropKind::Time:
		return SendPropTime( d.name, d.offset, d.size );
	case PropKind::Vector:
		return SendPropVector( d.name, d.offset, d.size, d.bits, d.flags, d.low, d.high,
		                       d.proxy ? d.proxy : SendProxy_VectorToVector, prio );
	case PropKind::VectorXY:
		return SendPropVectorXY( d.name, d.offset, d.size, d.bits, d.flags, d.low, d.high,
		                         d.proxy ? d.proxy : SendProxy_VectorXYToVectorXY, prio );
	case PropKind::QAngles:
		return SendPropQAngles( d.name, d.offset, d.size, d.bits, d.flags,
		                        d.proxy ? d.proxy : SendProxy_QAngles, prio );
	case PropKind::Angle:
		return SendPropAngle( d.name, d.offset, d.size, d.bits, d.flags,
		                      d.proxy ? d.proxy : SendProxy_AngleToFloat, prio );
	case PropKind::Int:
		// SendPropInt picks a width-matched proxy when passed none, so the null is meaningful.
		return SendPropInt( d.name, d.offset, d.size, d.bits, d.flags, d.proxy, prio );
	case PropKind::ModelIndex:
		return SendPropModelIndex( d.name, d.offset, d.size );
	case PropKind::Bool:
		return SendPropBool( d.name, d.offset, d.size );
	case PropKind::EHandle:
		// SendPropEHandle's third parameter is `flags`, not `sizeofVar`, and every one of the
		// tree's call sites passes SENDINFO's three values positionally -- so the handle's
		// size lands in the flags and every ehandle prop carries a spurious SPROP_NOSCALE.
		// Reproduced because the gate demands byte-identity; see the note in the header.
		return d.proxy ? SendPropEHandle( d.name, d.offset, d.size, kSizeofIgnore, d.proxy )
		               : SendPropEHandle( d.name, d.offset, d.size );
	case PropKind::String:
		return SendPropString( d.name, d.offset, d.size, d.flags,
		                       d.proxy ? d.proxy : SendProxy_StringToString, prio );
	case PropKind::StringT:
		return SendPropStringT( d.name, d.offset, d.size );
	case PropKind::Table:
		// Omitting the proxy means the factory's own default, which also carries
		// SPROP_PROXY_ALWAYS_YES; passing a null proxy explicitly would override both.
		return d.tableProxy
		    ? SendPropDataTable( d.name, d.offset, d.subtable(), d.tableProxy )
		    : SendPropDataTable( d.name, d.offset, d.subtable() );
	case PropKind::Array:
		// The element is a descriptor in the same array rather than something owned.
		return SendPropArray3( d.name, d.offset, d.stride, d.elements,
		                       make_prop( all[d.elem], all ) );
	case PropKind::ArrayInner:
		return InternalSendPropArray( d.elements, d.stride, d.name, d.lenProxy );
	case PropKind::UtlVector:
		return SendPropUtlVector( d.name, d.offset, d.size, d.ensure, d.max,
		                          make_prop( all[d.elem], all ) );
	case PropKind::Exclude:
		return SendPropExclude( d.name, d.excludeProp );
	default:
		// Fail the way a table difference fails, rather than continuing into a null table.
		Warning( "reflect: make_prop reached an unhandled kind for %s\n", d.name );
		exit( 70 );
		return SendProp();
	}
}

} // namespace ks::reflect::net
