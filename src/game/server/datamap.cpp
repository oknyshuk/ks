// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

module;
#include <stdlib.h>
#include "isaverestore.h"
#include "networkvar.h"
#include "string_t.h"
#include "eiface.h"
#include "netmessages.h"
#include "responserules/response_types.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kind_list.h"
#include "datamap.h"
#include "bspfile.h"
#include "dt_utlvector_send.h"
#include "reflect.h"
module ks.server;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.platform;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import ks.tier1.strtools;
import :vscript;
import :fish;
import :sharedvar;
import :player;
import :baseentity;
import ks.game.shareddefs;
import :nav_4;
import ks.tier1.interfaces;
import :cstrike15.Effects.chicken;
import :cstrike15.cs_player;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_team;
import :cstrike15.hostage.cs_simple_hostage;
import :entity_dispatch;
import :entity_kind;
import :entityutil;
import :cstrike15.bot.bot;
import :cstrike15.cs_player_2;
import :cstrike15.cs_playeranimstate;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :entity_2;
import ks.core.wchartypes;

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

class CBaseEntity;

using namespace ::ks::reflect;

// The dispatchers of CBaseEntity (see entity_dispatch.cppm): for every method of the list a call of the
// thunk of the kind that runs it for the entity's kind.

[[noreturn]] void EntityKindFailure( const char *pszWhat, const char *pszClassname, int nKind )
{
	Error( "entity kind contract violated: %s (entity '%s', kind %d): entities must be made by the entity factory (_CreateEntityTemplate)\n",
		pszWhat, pszClassname, nKind );
	__builtin_trap();
}

[[noreturn]] void EntityDispatchFailure( EntityKind kind, const char *pszMethod )
{
	EntityKindFailure( pszMethod, kind == EntityKind::None ? "(no kind)" : "(not an instantiable kind)", static_cast<int>( kind ) );
}

#define KS_ENT_DEFINE_DISPATCHER( Ret, Name, Const, Params, Args, ... ) \
	Ret CBaseEntity::Name Params Const \
	{ \
		const auto fn = entity_dispatch::table< KS_ENT_MEMBER( Ret, Name, Const, Params ) >[static_cast<int>( \
			entity_dispatch::owner< KS_ENT_MEMBER( Ret, Name, Const, Params ) >[static_cast<int>( m_kind )] )]; \
		if ( !fn ) EntityDispatchFailure( m_kind, #Name ); \
		return fn( KS_ENT_ARGS Args ); \
	}
KS_ENTITY_METHODS( KS_ENT_DEFINE_DISPATCHER )
#undef KS_ENT_DEFINE_DISPATCHER

namespace ksr::dmap
{
using namespace ::ks::reflect;

typedescription_t make_field( const FieldDesc &d, bool output )
{
	FieldDesc f = d;
	// eventFuncs is entityoutput.h's, and is_output is always false on the client.
	if ( output ) f.ops = eventFuncs;
	return MakeField( f );
}

} // namespace ksr::dmap

namespace ksr::net
{
using namespace ::ks::reflect;

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
			std::exit( 70 );
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
		return SendPropUtlVector( d.name, d.offset, d.size, d.vec, d.max,
		                          make_prop( all[d.elem], all ) );
	case PropKind::Exclude:
		return SendPropExclude( d.name, d.excludeProp );
	default:
		// Fail the way a table difference fails, rather than continuing into a null table.
		Warning( "reflect: make_prop reached an unhandled kind for %s\n", d.name );
		std::exit( 70 );
		return SendProp();
	}
}

} // namespace ksr::net
