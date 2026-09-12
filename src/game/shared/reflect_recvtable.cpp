// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include "cbase.h"
#include "reflect_recvtable.h"

#include "tier0/memdbgon.h"

namespace ks::reflect::recv
{

RecvProp make_prop( const PropDesc &d, std::span<const PropDesc> all )
{
	switch ( d.kind )
	{
	case PropKind::Float:
		return RecvPropFloat( d.name, d.offset, d.size, d.flags,
		                      d.proxy ? d.proxy : RecvProxy_FloatToFloat );
	case PropKind::Time:
		return RecvPropTime( d.name, d.offset, d.size );
	case PropKind::Vector:
		return RecvPropVector( d.name, d.offset, d.size, d.flags,
		                       d.proxy ? d.proxy : RecvProxy_VectorToVector );
	case PropKind::VectorXY:
		return RecvPropVectorXY( d.name, d.offset, d.size, d.flags,
		                         d.proxy ? d.proxy : RecvProxy_VectorXYToVectorXY );
	case PropKind::Int:
		// RecvPropInt picks a width-matched proxy when passed none, so the null is meaningful.
		return RecvPropInt( d.name, d.offset, d.size, d.flags, d.proxy );
	case PropKind::Bool:
		return RecvPropBool( d.name, d.offset, d.size );
	case PropKind::EHandle:
		return d.proxy ? RecvPropEHandle( d.name, d.offset, d.size, d.proxy )
		               : RecvPropEHandle( d.name, d.offset, d.size );
	case PropKind::String:
		return RecvPropString( d.name, d.offset, d.size, d.flags,
		                       d.proxy ? d.proxy : RecvProxy_StringToString );
	case PropKind::Table:
		// The factory's own default proxy is DataTableRecvProxy_StaticDataTable, so naming it is
		// the same as omitting it.
		return RecvPropDataTable( d.name, d.offset, d.flags, d.subtable(),
		                          d.tableProxy ? d.tableProxy : DataTableRecvProxy_StaticDataTable );
	case PropKind::Array:
		// The element is a descriptor in the same array rather than something owned.
		return RecvPropArray3( d.name, d.offset, d.stride, d.elements,
		                       make_prop( all[d.elem], all ) );
	case PropKind::ArrayInner:
		return InternalRecvPropArray( d.elements, d.stride, d.name, d.lenProxy );
	case PropKind::UtlVector:
		return RecvPropUtlVector( d.name, d.offset, d.size, d.resizeFn, d.ensure, d.max,
		                          make_prop( all[d.elem], all ) );
	default:
		// Error() is not fatal enough: execution continued into a table built from this empty
		// prop and the client died on a null dereference, so the gate crashed instead of
		// reporting. Exit with the same code a table difference uses.
		Warning( "reflect: recv make_prop reached an unhandled kind for %s\n", d.name );
		exit( 70 );
	}
}

} // namespace ks::reflect::recv
