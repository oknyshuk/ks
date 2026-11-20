// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)


#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Core/FixedSizeFreeList.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/PhysicsScene.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/EmptyShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Ragdoll/Ragdoll.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <Jolt/Physics/Constraints/ConeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
import std;
import ks.core.logging;
import ks.core.platform;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.math.vector4d;
import ks.platform;
import ks.simd;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
import ks.tier1.text_writer;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
#include "compat/branch_overrides.h"
#include "vcollide_parse.h"
#include "vphysics/collision_set.h"
#include "vjolt_interface.h"

#include "vjolt_surfaceprops.h"

#include "vjolt_keyvalues_schema.h"

//-------------------------------------------------------------------------------------------------

const JoltKVSchemaFunc_t FillBaseProp =
{
	sizeof( surfacedata_t ),
	[]( KeyValues *pProp, void *pPtr, size_t size )
	{
		surfacedata_t *pSurfaceDataPtr = reinterpret_cast< surfacedata_t * >( pPtr );

		int nSurfaceIndex = JoltPhysicsSurfaceProps::GetInstance().GetSurfaceIndex( pProp->GetString() );
		if ( nSurfaceIndex == -1 )
		{
			Log_Warning( LOG_VJolt, "You must specify the base material %s before it can be used. Defaulting to 'default' as a base.\n", pProp->GetString() );
			// Always the default material.
			nSurfaceIndex = 0;
		}

		*pSurfaceDataPtr = *JoltPhysicsSurfaceProps::GetInstance().GetSurfaceData( nSurfaceIndex );
	}
};

const JoltKVSchemaFunc_t FillStringProp =
{
	0, // Varies.
	[]( KeyValues *pProp, void *pPtr, size_t size )
	{
		char *pszStringPtr = reinterpret_cast< char * >( pPtr );
		V_strncpy( pszStringPtr, pProp->GetString(), static_cast< strlen_t >( size ) );
	}
};

const JoltKVSchemaFunc_t FillIntProp =
{
	sizeof( int ),
	[]( KeyValues *pProp, void *pPtr, size_t size )
	{
		int *pIntPtr = reinterpret_cast< int * >( pPtr );
		*pIntPtr = pProp->GetInt();
	}
};

const JoltKVSchemaFunc_t FillFloatProp =
{
	sizeof( float ),
	[]( KeyValues *pProp, void *pPtr, size_t size )
	{
		float *pFloatPtr = reinterpret_cast< float * >( pPtr );
		*pFloatPtr = pProp->GetFloat();
	}
};

const JoltKVSchemaFunc_t FillUnsignedCharProp =
{
	sizeof( unsigned char ),
	[]( KeyValues *pProp, void *pPtr, size_t size )
	{
		unsigned char *pCharPtr = reinterpret_cast< unsigned char * >( pPtr );
		*pCharPtr = static_cast< unsigned char >( pProp->GetInt() );
	}
};

const JoltKVSchemaFunc_t FillBoolProp =
{
	sizeof( bool ),
	[]( KeyValues *pProp, void *pPtr, size_t size )
	{
		bool *pBoolPtr = reinterpret_cast< bool * >( pPtr );
		*pBoolPtr = pProp->GetBool();
	}
};

const JoltKVSchemaFunc_t FillVectorProp =
{
	sizeof( Vector ),
	[]( KeyValues *pProp, void *pPtr, size_t size )
	{
		Vector *pVectorPtr = reinterpret_cast< Vector * >( pPtr );
		sscanf( pProp->GetString(), "%f %f %f", &pVectorPtr->x, &pVectorPtr->y, &pVectorPtr->z );
	}
};

const JoltKVSchemaFunc_t FillVector4DProp =
{
	sizeof( Vector4D ),
	[]( KeyValues *pProp, void *pPtr, size_t size )
	{
		Vector4D *pVector4DPtr = reinterpret_cast< Vector4D * >( pPtr );
		sscanf( pProp->GetString(), "%f %f %f %f", &pVector4DPtr->x, &pVector4DPtr->y, &pVector4DPtr->z, &pVector4DPtr->w );
	}
};

const JoltKVSchemaFunc_t FillQAngleProp =
{
	sizeof( QAngle ),
	[]( KeyValues *pProp, void *pPtr, size_t size )
	{
		QAngle *pQAnglePtr = reinterpret_cast< QAngle * >( pPtr );
		sscanf( pProp->GetString(), "%f %f %f", &pQAnglePtr->x, &pQAnglePtr->y, &pQAnglePtr->z );
	}
};

const JoltKVSchemaFunc_t FillIntPairProp =
{
	sizeof( JoltPhysicsIntPair ),
	[]( KeyValues *pProp, void *pPtr, size_t size )
	{
		JoltPhysicsIntPair *pIntPairPtr = reinterpret_cast< JoltPhysicsIntPair * >( pPtr );
		sscanf( pProp->GetString(), "%d,%d", &pIntPairPtr->Index0, &pIntPairPtr->Index1 );
	}
};

const JoltKVSchemaFunc_t FillGameMaterialProp =
{
	sizeof( unsigned short ),
	[]( KeyValues *pProp, void *pPtr, size_t size )
	{
		const char *pValue = pProp->GetString();
		unsigned short *pShortPtr = reinterpret_cast< unsigned short * >( pPtr );

		if ( V_strlen( pValue ) == 1 && !V_isdigit( pValue[ 0 ] ) )
			*pShortPtr = FastASCIIToUpper( pValue[ 0 ] );
		else
			*pShortPtr = pProp->GetInt();
	}
};

const JoltKVSchemaFunc_t FillSoundProp =
{
	sizeof( unsigned short ),
	[]( KeyValues *pProp, void *pPtr, size_t size )
	{
		const char *pValue = pProp->GetString();
		unsigned short *pShortPtr = reinterpret_cast< unsigned short * >( pPtr );

		*pShortPtr = JoltPhysicsSurfaceProps::GetInstance().RegisterSound( pValue );
	}
};

const JoltKVSchemaFunc_t FillSurfaceProp =
{
	sizeof( int ),
	[]( KeyValues *pProp, void *pPtr, size_t size )
	{
		const char* pValue = pProp->GetString();
		int *pIntPtr = reinterpret_cast< int * >( pPtr );

		*pIntPtr = JoltPhysicsSurfaceProps::GetInstance().GetSurfaceIndex( pValue );
	}
};

//-------------------------------------------------------------------------------------------------

void ParseJoltKVSchema( KeyValues *pKV, const JoltKVSchemaProp_t *pDescs, uint count, void *pObj, void *pUnknownKeyObj, IVPhysicsKeyHandler *pUnknownKeyHandler )
{
	bool bHandled = false;

    for ( KeyValues* pProp = pKV->GetFirstSubKey(); pProp != nullptr; pProp = pProp->GetNextKey() )
    {
        const char *pName = pProp->GetName();
        
        for ( uint i = 0; i < count; i++ )
        {
            const JoltKVSchemaProp_t &desc = pDescs[ i ];

            if ( !V_stricmp( pName, desc.pszName ) )
            {
                contract_assert( ( desc.func.ptr_size == 0 || desc.size == desc.func.ptr_size ) && "Desc element size does not match function size" );

				int *pArraySize = nullptr;
				if ( desc.arrayOffset != static_cast<size_t>( ~0llu ) )
					pArraySize = reinterpret_cast< int * >( reinterpret_cast< char * >( pObj ) + desc.arrayOffset );

				char* pElement = reinterpret_cast< char * >( pObj ) + desc.offset;
				if ( pArraySize )
					pElement += *pArraySize * desc.size;

                desc.func.ReadFunc( pProp, reinterpret_cast< void * >( pElement ), desc.size);
				if ( desc.fixupFunc )
					desc.fixupFunc( pObj );
				if ( pArraySize )
					( *pArraySize )++;
				bHandled = true;
            }
        }

		if ( !bHandled && pUnknownKeyHandler )
			pUnknownKeyHandler->ParseKeyValue( pUnknownKeyObj, pName, pProp->GetString() );
    }
}

void ParseJoltKVCustom( KeyValues *pKV, void *pUnknownKeyObj, IVPhysicsKeyHandler* pUnknownKeyHandler )
{
	// Josh:
	// Parse out custom KV entries like "vehicle_sounds" etc
	// out recursively.

	for ( KeyValues* pProp = pKV->GetFirstSubKey(); pProp != nullptr; pProp = pProp->GetNextKey() )
	{
		if ( pUnknownKeyHandler )
			pUnknownKeyHandler->ParseKeyValue( pUnknownKeyObj, pProp->GetName(), pProp->GetString());

		ParseJoltKVCustom( pProp, pUnknownKeyObj, pUnknownKeyHandler );
	}
}

//-------------------------------------------------------------------------------------------------

KeyValues *HeaderlessKVBufferToKeyValues( const char *pszBuffer, const char *pszSetName )
{
	ks::text_writer buffer;
	buffer.put( "\"PhysProps\"\r\n{" );
	buffer.put( pszBuffer );
	buffer.put( "\r\n}" );

	KeyValues *pszKV = new KeyValues( pszSetName );

	if ( !pszKV->LoadFromBuffer( pszSetName, buffer.view() ) )
	{
		pszKV->deleteThis();
		pszKV = nullptr;
	}

	return pszKV;
}
