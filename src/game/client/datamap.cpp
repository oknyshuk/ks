// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

module;
#include <stdlib.h>
#include "isaverestore.h"
#include "string_t.h"
#include "datamap.h"
#include "dt_utlvector_recv.h"
#include "reflect.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.platform;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import ks.tier1.strtools;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :sharedvar;
import :particlemgr;
import :util;
import :precache;
import :baseentity;
import ks.game.gamemovement;
import :noopanimcontroller;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: iterates through a typedescript data block, so it can insert key/value data into the block
// Input  : *pObject - pointer to the struct or class the data is to be insterted into
//			*pFields - description of the data
//			iNumFields - number of fields contained in pFields
//			char *szKeyName - name of the variable to look for
//			char *szValue - value to set the variable to
// Output : Returns true if the variable is found and set, false if the key is not found.
//-----------------------------------------------------------------------------
bool ParseKeyvalue( void *pObject, typedescription_t *pFields, int iNumFields, const char *szKeyName, const char *szValue )
{
	int i;
	typedescription_t 	*pField;

	for ( i = 0; i < iNumFields; i++ )
	{
		pField = &pFields[i];

		int fieldOffset = pField->fieldOffset;

		// Check the nested classes, but only if they aren't in array form.
		if ((pField->fieldType == FIELD_EMBEDDED) && (pField->fieldSize == 1))
		{
			for ( datamap_t *dmap = pField->td; dmap != nullptr; dmap = dmap->baseMap )
			{
				void *pEmbeddedObject = (void*)((char*)pObject + fieldOffset);
				if ( ParseKeyvalue( pEmbeddedObject, dmap->dataDesc, dmap->dataNumFields, szKeyName, szValue) )
					return true;
			}
		}

		if ( (pField->flags & FTYPEDESC_KEY) && !stricmp(pField->externalName, szKeyName) )
		{
			switch( pField->fieldType )
			{
			case FIELD_MODELNAME:
			case FIELD_SOUNDNAME:
			case FIELD_STRING:
				(*(string_t *)((char *)pObject + fieldOffset)) = AllocPooledString( szValue );
				return true;

			case FIELD_TIME:
			case FIELD_FLOAT:
				(*(float *)((char *)pObject + fieldOffset)) = std::atof( szValue );
				return true;

			case FIELD_BOOLEAN:
				(*(bool *)((char *)pObject + fieldOffset)) = (bool)(std::atoi( szValue ) != 0);
				return true;

			case FIELD_CHARACTER:
				(*(char *)((char *)pObject + fieldOffset)) = (char)atoi( szValue );
				return true;

			case FIELD_SHORT:
				(*(short *)((char *)pObject + fieldOffset)) = (short)atoi( szValue );
				return true;

			case FIELD_INTEGER:
			case FIELD_TICK:
				(*(int *)((char *)pObject + fieldOffset)) = atoi( szValue );
				return true;

			case FIELD_POSITION_VECTOR:
			case FIELD_VECTOR:
				UTIL_StringToVector( (float *)((char *)pObject + fieldOffset), szValue );
				return true;

			case FIELD_VMATRIX:
			case FIELD_VMATRIX_WORLDSPACE:
				UTIL_StringToFloatArray( (float *)((char *)pObject + fieldOffset), 16, szValue );
				return true;

			case FIELD_MATRIX3X4_WORLDSPACE:
				UTIL_StringToFloatArray( (float *)((char *)pObject + fieldOffset), 12, szValue );
				return true;

			case FIELD_COLOR32:
				V_StringToColor32( (color32 *) ((char *)pObject + fieldOffset), szValue );
				return true;

			case FIELD_CUSTOM:
			{
				SaveRestoreFieldInfo_t fieldInfo =
				{
					(char *)pObject + fieldOffset,
					pObject,
					pField
				};
				pField->pSaveRestoreOps->Parse( fieldInfo, szValue );
				return true;
			}

			default:
			case FIELD_INTERVAL: // Fixme, could write this if needed
			case FIELD_CLASSPTR:
			case FIELD_MODELINDEX:
			case FIELD_MATERIALINDEX:
			case FIELD_EDICT:
				Warning( "Bad field in entity!!\n" );
				contract_assert( false && "Bad field type %d for %s in entity %.16s!!\n" );
				break;
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: iterates through a typedescript data block, so it can insert key/value data into the block
// Input  : *pObject - pointer to the struct or class the data is to be insterted into
//			*pFields - description of the data
//			iNumFields - number of fields contained in pFields
//			char *szKeyName - name of the variable to look for
//			char *szValue - value to set the variable to
// Output : Returns true if the variable is found and set, false if the key is not found.
//-----------------------------------------------------------------------------
bool ExtractKeyvalue( void *pObject, typedescription_t *pFields, int iNumFields, const char *szKeyName, char *szValue, int iMaxLen )
{
	int i;
	typedescription_t 	*pField;

	for ( i = 0; i < iNumFields; i++ )
	{
		pField = &pFields[i];

		int fieldOffset = pField->fieldOffset;

		// Check the nested classes, but only if they aren't in array form.
		if ((pField->fieldType == FIELD_EMBEDDED) && (pField->fieldSize == 1))
		{
			for ( datamap_t *dmap = pField->td; dmap != nullptr; dmap = dmap->baseMap )
			{
				void *pEmbeddedObject = (void*)((char*)pObject + fieldOffset);
				if ( ExtractKeyvalue( pEmbeddedObject, dmap->dataDesc, dmap->dataNumFields, szKeyName, szValue, iMaxLen ) )
					return true;
			}
		}

		if ( (pField->flags & FTYPEDESC_KEY) && !stricmp(pField->externalName, szKeyName) )
		{
			switch( pField->fieldType )
			{
			case FIELD_MODELNAME:
			case FIELD_SOUNDNAME:
			case FIELD_STRING:
				V_strncpy( szValue, ((char *)pObject + fieldOffset), iMaxLen );
				return true;

			case FIELD_TIME:
			case FIELD_FLOAT:
				V_snprintf( szValue, iMaxLen, "%f", (*(float *)((char *)pObject + fieldOffset)) );
				return true;

			case FIELD_BOOLEAN:
				V_snprintf( szValue, iMaxLen, "%d", (*(bool *)((char *)pObject + fieldOffset)) != 0);
				return true;

			case FIELD_CHARACTER:
				V_snprintf( szValue, iMaxLen, "%d", (*(char *)((char *)pObject + fieldOffset)) );
				return true;

			case FIELD_SHORT:
				V_snprintf( szValue, iMaxLen, "%d", (*(short *)((char *)pObject + fieldOffset)) );
				return true;

			case FIELD_INTEGER:
			case FIELD_TICK:
				V_snprintf( szValue, iMaxLen, "%d", (*(int *)((char *)pObject + fieldOffset)) );
				return true;

			case FIELD_POSITION_VECTOR:
			case FIELD_VECTOR:
				V_snprintf( szValue, iMaxLen, "%f %f %f", 
							( (float *)((char *)pObject + fieldOffset) )[0],
					        ( (float *)((char *)pObject + fieldOffset) )[1],
							( (float *)((char *)pObject + fieldOffset) )[2] );
				return true;

			case FIELD_VMATRIX:
			case FIELD_VMATRIX_WORLDSPACE:
				//UTIL_StringToFloatArray( (float *)((char *)pObject + fieldOffset), 16, szValue );
				return false;

			case FIELD_MATRIX3X4_WORLDSPACE:
				//UTIL_StringToFloatArray( (float *)((char *)pObject + fieldOffset), 12, szValue );
				return false;

			case FIELD_COLOR32:
				V_snprintf( szValue, iMaxLen, "%d %d %d %d", 
							( (int *)((char *)pObject + fieldOffset) )[0],
							( (int *)((char *)pObject + fieldOffset) )[1],
							( (int *)((char *)pObject + fieldOffset) )[2],
							( (int *)((char *)pObject + fieldOffset) )[3] );
				return true;

			case FIELD_CUSTOM:
			{
				/*
				SaveRestoreFieldInfo_t fieldInfo =
				{
					(char *)pObject + fieldOffset,
					pObject,
					pField
				};
				pField->pSaveRestoreOps->Parse( fieldInfo, szValue );
				*/
				return false;
			}

			default:
			case FIELD_INTERVAL: // Fixme, could write this if needed
			case FIELD_CLASSPTR:
			case FIELD_MODELINDEX:
			case FIELD_MATERIALINDEX:
			case FIELD_EDICT:
				Warning( "Bad field in entity!!\n" );
				contract_assert( 0 );
				break;
			}
		}
	}

	return false;
}

namespace ksr::dmap
{
using namespace ::ks::reflect;

typedescription_t make_field( const FieldDesc &d, bool output )
{
	FieldDesc f = d;
	( void )output;
	return MakeField( f );
}

} // namespace ksr::dmap

namespace ksr::recv
{
using namespace ::ks::reflect;

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
		return RecvPropUtlVector( d.name, d.offset, d.size, d.vec, d.max,
		                          make_prop( all[d.elem], all ) );
	default:
		// Error() is not fatal enough: execution continued into a table built from this empty
		// prop and the client died on a null dereference, so the gate crashed instead of
		// reporting. Exit with the same code a table difference uses.
		Warning( "reflect: recv make_prop reached an unhandled kind for %s\n", d.name );
		std::exit( 70 );
	}
}

} // namespace ksr::recv
