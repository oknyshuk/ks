// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include "cbase.h"
#include "reflect_recvtable.h"

#include "tier0/memdbgon.h"

namespace ks::reflect::recv
{

RecvProp make_prop( const PropDesc &d )
{
	switch ( d.kind )
	{
	case PROP_FLOAT:
		return RecvPropFloat( d.name, d.offset, d.size, d.flags,
		                      d.proxy ? d.proxy : RecvProxy_FloatToFloat );
	case PROP_TIME:
		return RecvPropTime( d.name, d.offset, d.size );
	case PROP_VECTOR:
		return RecvPropVector( d.name, d.offset, d.size, d.flags,
		                       d.proxy ? d.proxy : RecvProxy_VectorToVector );
	case PROP_VECTORXY:
		return RecvPropVectorXY( d.name, d.offset, d.size, d.flags,
		                         d.proxy ? d.proxy : RecvProxy_VectorXYToVectorXY );
	case PROP_INT:
		// RecvPropInt picks a width-matched proxy when passed none, so the null is meaningful.
		return RecvPropInt( d.name, d.offset, d.size, d.flags, d.proxy );
	case PROP_BOOL:
		return RecvPropBool( d.name, d.offset, d.size );
	case PROP_EHANDLE:
		return d.proxy ? RecvPropEHandle( d.name, d.offset, d.size, d.proxy )
		               : RecvPropEHandle( d.name, d.offset, d.size );
	case PROP_STRING:
		return RecvPropString( d.name, d.offset, d.size, d.flags,
		                       d.proxy ? d.proxy : RecvProxy_StringToString );
	default:
		// Error() is not fatal enough: execution continued into a table built from this empty
		// prop and the client died on a null dereference, so the gate crashed instead of
		// reporting. Exit with the same code a table difference uses.
		Warning( "reflect: recv make_prop reached an unhandled kind for %s\n", d.name );
		exit( 70 );
	}
}

} // namespace ks::reflect::recv
