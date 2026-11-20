// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation

// Purpose: particle system code

import std;
import ks.core.basetypes;
import ks.core.color;
import ks.core.dbg;
import ks.core.memalloc;
import ks.core.numeric;
import ks.core.platform;
import ks.core.random;
import ks.core.threadtools;
import ks.core.types;
import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.fltx4;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.math.vector2d;
import ks.math.vector4d;
import ks.math.vmatrix;
import ks.platform;
import ks.simd;
import ks.tier1.byte_reader;
import ks.tier1.byte_writer;
import ks.tier1.exprevaluator;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.map_ops;
import ks.tier1.strtools;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
import ks.tier1.utlsoacontainer;
import ks.tier1.vector_ops;
#include "particles/particles.h"
#include "bitmap/psheet.h"
#include "filesystem.h"
#include "tier2/fileutils.h"
#include "dmxloader/dmxloader.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imesh.h"
#include "random_floats.h"
#include "vtf/vtf.h"
#include "studio.h"
#include "particles_internal.h"
#include "ivrenderview.h"
#include "materialsystem/imaterialsystem.h"




// rename table from the great rename 
static char *s_RemapOperatorNameTable[]={
	"alpha_fade", "Alpha Fade and Decay",
	"alpha_fade_in_random", "Alpha Fade In Random",
	"alpha_fade_out_random", "Alpha Fade Out Random",
	"basic_movement", "Movement Basic",
	"color_fade", "Color Fade",
	"controlpoint_light", "Color Light From Control Point",
	"Dampen Movement Relative to Control Point", "Movement Dampen Relative to Control Point",
	"Distance Between Control Points Scale", "Remap Distance Between Two Control Points to Scalar",
	"Distance to Control Points Scale", "Remap Distance to Control Point to Scalar",
	"lifespan_decay", "Lifespan Decay",
	"lock to bone",	"Movement Lock to Bone",
	"postion_lock_to_controlpoint", "Movement Lock to Control Point",
	"maintain position along path", "Movement Maintain Position Along Path",
	"Match Particle Velocities", "Movement Match Particle Velocities",
	"Max Velocity", "Movement Max Velocity",
	"noise", "Noise Scalar",
	"vector noise", "Noise Vector",
	"oscillate_scalar", "Oscillate Scalar",
	"oscillate_vector", "Oscillate Vector",
	"Orient Rotation to 2D Direction", "Rotation Orient to 2D Direction",
	"radius_scale", "Radius Scale",
	"Random Cull", "Cull Random",
	"remap_scalar", "Remap Scalar",
	"rotation_movement", "Rotation Basic",
	"rotation_spin", "Rotation Spin Roll",
	"rotation_spin yaw", "Rotation Spin Yaw",
	"alpha_random", "Alpha Random",
	"color_random", "Color Random",
	"create from parent particles", "Position From Parent Particles",
	"Create In Hierarchy", "Position In CP Hierarchy",
	"random position along path", "Position Along Path Random",
	"random position on model", "Position on Model Random",
	"sequential position along path", "Position Along Path Sequential",
	"position_offset_random", "Position Modify Offset Random",
	"position_warp_random", "Position Modify Warp Random",
	"position_within_box", "Position Within Box Random",
	"position_within_sphere", "Position Within Sphere Random",
	"Inherit Velocity", "Velocity Inherit from Control Point",
	"Initial Repulsion Velocity", "Velocity Repulse from World",
	"Initial Velocity Noise", "Velocity Noise",
	"Initial Scalar Noise", "Remap Noise to Scalar",
	"Lifespan from distance to world", "Lifetime from Time to Impact",
	"Pre-Age Noise", "Lifetime Pre-Age Noise",
	"lifetime_random", "Lifetime Random",
	"radius_random", "Radius Random",
	"random yaw", "Rotation Yaw Random",
	"Randomly Flip Yaw", "Rotation Yaw Flip Random",
	"rotation_random", "Rotation Random",
	"rotation_speed_random", "Rotation Speed Random",
	"sequence_random", "Sequence Random",
	"second_sequence_random", "Sequence Two Random",
	"trail_length_random", "Trail Length Random",
	"velocity_random", "Velocity Random",
};

static char const *RemapOperatorName( char const *pOpName )
{
	for( int i = 0 ; i < std::size( s_RemapOperatorNameTable ) ; i += 2 )
	{
		if ( V_stricmp( pOpName, s_RemapOperatorNameTable[i] ) == 0 )
		{
			return s_RemapOperatorNameTable[i + 1 ];
		}
	}
	return pOpName;
}


//-----------------------------------------------------------------------------
// Default implementation of particle system mgr
//-----------------------------------------------------------------------------
static CParticleSystemMgr s_ParticleSystemMgr;
CParticleSystemMgr *g_pParticleSystemMgr = &s_ParticleSystemMgr;
CParticleSystemMgr::ParticleAttribute_t CParticleSystemMgr::s_AttributeTable[ MAX_PARTICLE_ATTRIBUTES ];


int g_nParticle_Multiplier = 1;
	

//-----------------------------------------------------------------------------
// Particle dictionary
//-----------------------------------------------------------------------------
class CParticleSystemDictionary
{
public:
	~CParticleSystemDictionary();

	CParticleSystemDefinition* AddParticleSystem( CDmxElement *pParticleSystem );
	int Count() const;
	int NameCount() const;
	CParticleSystemDefinition* GetParticleSystem( int i );
	ParticleSystemHandle_t FindParticleSystemHandle( const char *pName );
	ParticleSystemHandle_t FindOrAddParticleSystemHandle( const char *pName );
	CParticleSystemDefinition* FindParticleSystem( ParticleSystemHandle_t h );
	CParticleSystemDefinition* FindParticleSystem( const char *pName );
	CParticleSystemDefinition* FindParticleSystem( const DmObjectId_t &id );
	
	CParticleSystemDefinition* operator[]( int idx )
	{
		return m_ParticleNameMap[ idx ].get();
	}

private:
	typedef std::vector< CParticleSystemDefinition* > ParticleIdMap_t;

	void DestroyExistingElement( CDmxElement *pElement );

	// Handles index m_ParticleNameMap, in first-seen order; a slot may be empty until defined.
	std::vector< std::unique_ptr< CParticleSystemDefinition > > m_ParticleNameMap;
	ks::idict< ParticleSystemHandle_t > m_ParticleNameIndex;
	ParticleIdMap_t m_ParticleIdMap;
};


//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CParticleSystemDictionary::~CParticleSystemDictionary()
{
	int nCount = std::ssize( m_ParticleIdMap );
	for ( int i = 0; i < nCount; ++i )
	{
		delete m_ParticleIdMap[i];
	}
}


//-----------------------------------------------------------------------------
// Destroys an existing element, returns if this element should be added to the name list
//-----------------------------------------------------------------------------
void CParticleSystemDictionary::DestroyExistingElement( CDmxElement *pElement )
{
	const char *pParticleSystemName = pElement->GetName();
	bool bPreventNameBasedLookup = pElement->GetValue<bool>( "preventNameBasedLookup" );
	if ( !bPreventNameBasedLookup )
	{
		if ( ParticleSystemHandle_t *pHandle = ks::find_ptr( m_ParticleNameIndex, pParticleSystemName ) )
		{
			m_ParticleNameMap[ *pHandle ].reset();
		}
		return;
	}
	
	// Use id based lookup instead
	int nCount = std::ssize( m_ParticleIdMap );
	const DmObjectId_t& id = pElement->GetId();
	for ( int i = 0; i < nCount; ++i )
	{
		// Was already removed by the name lookup
		if ( !IsUniqueIdEqual( m_ParticleIdMap[i]->GetId(), id ) )
			continue;

		CParticleSystemDefinition *pDef = m_ParticleIdMap[ i ];
		ks::swap_remove( m_ParticleIdMap, i );
		delete pDef;
		break;
	}
}


//-----------------------------------------------------------------------------
// Adds a destructor
//-----------------------------------------------------------------------------
CParticleSystemDefinition* CParticleSystemDictionary::AddParticleSystem( CDmxElement *pParticleSystem )
{
	if ( V_stricmp( pParticleSystem->GetTypeString(), "DmeParticleSystemDefinition" ) )
		return nullptr;

	DestroyExistingElement( pParticleSystem );

	CParticleSystemDefinition *pDef;

	// Must add the def to the maps before Read() because Read() may create new child particle systems
	bool bPreventNameBasedLookup = pParticleSystem->GetValue<bool>( "preventNameBasedLookup" );
	if ( !bPreventNameBasedLookup )
	{
		ParticleSystemHandle_t h = FindOrAddParticleSystemHandle( pParticleSystem->GetName() );
		m_ParticleNameMap[ h ] = std::make_unique< CParticleSystemDefinition >();
		pDef = m_ParticleNameMap[ h ].get();
	}
	else
	{
		pDef = new CParticleSystemDefinition;
		m_ParticleIdMap.push_back( pDef );
	}

	pDef->Read( pParticleSystem );
	return pDef;
}

int CParticleSystemDictionary::NameCount() const
{
	return std::ssize( m_ParticleNameMap );
}

int CParticleSystemDictionary::Count() const
{
	return std::ssize( m_ParticleIdMap );
}

CParticleSystemDefinition* CParticleSystemDictionary::GetParticleSystem( int i )
{
	return m_ParticleIdMap[i];
}

ParticleSystemHandle_t CParticleSystemDictionary::FindParticleSystemHandle( const char *pName )
{
	auto it = m_ParticleNameIndex.find( pName );
	return it == m_ParticleNameIndex.end() ? INVALID_PARTICLE_SYSTEM_HANDLE : it->second;
}

ParticleSystemHandle_t CParticleSystemDictionary::FindOrAddParticleSystemHandle( const char *pName )
{
	auto [it, bNew] = m_ParticleNameIndex.try_emplace( pName, (ParticleSystemHandle_t)m_ParticleNameMap.size() );
	if ( bNew )
	{
		m_ParticleNameMap.emplace_back();
	}
	return it->second;
}

CParticleSystemDefinition* CParticleSystemDictionary::FindParticleSystem( ParticleSystemHandle_t h )
{
	if ( h == INVALID_PARTICLE_SYSTEM_HANDLE || h < 0 || h >= std::ssize( m_ParticleNameMap ) )
		return nullptr;
	return m_ParticleNameMap[ h ].get();
}

CParticleSystemDefinition* CParticleSystemDictionary::FindParticleSystem( const char *pName )
{
	if ( ParticleSystemHandle_t *pHandle = ks::find_ptr( m_ParticleNameIndex, pName ) )
		return m_ParticleNameMap[ *pHandle ].get();
	return nullptr;
}

CParticleSystemDefinition* CParticleSystemDictionary::FindParticleSystem( const DmObjectId_t &id )
{
	int nCount = std::ssize( m_ParticleIdMap );
	for ( int i = 0; i < nCount; ++i )
	{
		if ( IsUniqueIdEqual( m_ParticleIdMap[i]->GetId(), id ) )
			return m_ParticleIdMap[i];
	}
	return nullptr;
}


//-----------------------------------------------------------------------------
// For editing, create a faked particle operator definition for children
// The only thing used in here is GetUnpackStructure.
//-----------------------------------------------------------------------------
BEGIN_DMXELEMENT_UNPACK( ParticleChildrenInfo_t ) 
	DMXELEMENT_UNPACK_FIELD( "delay", "0.0", float, m_flDelay )
	DMXELEMENT_UNPACK_FIELD( "end cap effect", "0", bool, m_bEndCap )
END_DMXELEMENT_UNPACK( ParticleChildrenInfo_t, s_ChildrenInfoUnpack )

static IParticleOperatorDefinition s_ChildOperatorDefinition( &s_ChildrenInfoUnpack );


//-----------------------------------------------------------------------------
//
// CParticleSystemDefinition
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Unpack structure for CParticleSystemDefinition
//-----------------------------------------------------------------------------
BEGIN_DMXELEMENT_UNPACK( CParticleSystemDefinition ) 
	DMXELEMENT_UNPACK_FIELD( "max_particles", "1000", int, m_nMaxParticles )
	DMXELEMENT_UNPACK_FIELD( "initial_particles", "0", int, m_nInitialParticles )
	DMXELEMENT_UNPACK_FIELD_UTLSTRING_USERDATA( "material", "vgui/white", m_MaterialName, "vmtPicker" )
	DMXELEMENT_UNPACK_FIELD( "bounding_box_min", "-10 -10 -10", Vector, m_BoundingBoxMin )
	DMXELEMENT_UNPACK_FIELD( "bounding_box_max", "10 10 10", Vector, m_BoundingBoxMax )
	DMXELEMENT_UNPACK_FIELD( "cull_radius", "0", float, m_flCullRadius )
	DMXELEMENT_UNPACK_FIELD( "cull_cost", "1", float, m_flCullFillCost )
	DMXELEMENT_UNPACK_FIELD( "cull_control_point", "0", int, m_nCullControlPoint )
	DMXELEMENT_UNPACK_FIELD_UTLSTRING( "cull_replacement_definition", "", m_CullReplacementName )
	DMXELEMENT_UNPACK_FIELD_UTLSTRING( "fallback replacement definition", "", m_FallbackReplacementName )
	DMXELEMENT_UNPACK_FIELD( "fallback max count", "-1", int, m_nFallbackMaxCount )
	DMXELEMENT_UNPACK_FIELD( "radius", "5", float, m_flConstantRadius )
	DMXELEMENT_UNPACK_FIELD( "color", "255 255 255 255", Color, m_ConstantColor )
	DMXELEMENT_UNPACK_FIELD( "rotation", "0", float, m_flConstantRotation )
	DMXELEMENT_UNPACK_FIELD( "rotation_speed", "0", float, m_flConstantRotationSpeed )
	DMXELEMENT_UNPACK_FIELD( "normal", "0 0 1", Vector, m_ConstantNormal )	
	DMXELEMENT_UNPACK_FIELD_USERDATA( "sequence_number", "0", int, m_nConstantSequenceNumber, "sheetsequencepicker" )
	DMXELEMENT_UNPACK_FIELD_USERDATA( "sequence_number 1", "0", int, m_nConstantSequenceNumber1, "sheetsequencepicker_second" )
	DMXELEMENT_UNPACK_FIELD( "group id", "0", int, m_nGroupID )
	DMXELEMENT_UNPACK_FIELD( "maximum time step", "0.1", float, m_flMaximumTimeStep )
	DMXELEMENT_UNPACK_FIELD( "maximum sim tick rate", "0.0", float, m_flMaximumSimTime )
	DMXELEMENT_UNPACK_FIELD( "minimum sim tick rate", "0.0", float, m_flMinimumSimTime )
	DMXELEMENT_UNPACK_FIELD( "minimum rendered frames", "0", int, m_nMinimumFrames )
	DMXELEMENT_UNPACK_FIELD( "control point to disable rendering if it is the camera", "-1", int, m_nSkipRenderControlPoint )
	DMXELEMENT_UNPACK_FIELD( "control point to only enable rendering if it is the camera", "-1", int, m_nAllowRenderControlPoint )
	DMXELEMENT_UNPACK_FIELD( "maximum draw distance", "100000.0", float, m_flMaxDrawDistance )
	DMXELEMENT_UNPACK_FIELD( "time to sleep when not drawn", "8", float, m_flNoDrawTimeToGoToSleep )
	DMXELEMENT_UNPACK_FIELD( "Sort particles", "1", bool, m_bShouldSort )
	DMXELEMENT_UNPACK_FIELD( "batch particle systems", "0", bool, m_bShouldBatch )
	DMXELEMENT_UNPACK_FIELD( "view model effect", "0", bool, m_bViewModelEffect )
	DMXELEMENT_UNPACK_FIELD( "screen space effect", "0", bool, m_bScreenSpaceEffect )
	DMXELEMENT_UNPACK_FIELD( "draw through leafsystem", "1", bool, m_bDrawThroughLeafSystem )
    DMXELEMENT_UNPACK_FIELD( "maximum portal recursion depth", "8", int, m_nMaxRecursionDepth )
    DMXELEMENT_UNPACK_FIELD( "aggregation radius", "0", float, m_flAggregateRadius )
    DMXELEMENT_UNPACK_FIELD( "minimum free particles to aggregate", "0", int, m_nAggregationMinAvailableParticles )
    DMXELEMENT_UNPACK_FIELD( "minimum simulation time step", "0", float, m_flMinimumTimeStep )
	DMXELEMENT_UNPACK_FIELD( "minimum CPU level", "0", int, m_nMinCPULevel )
	DMXELEMENT_UNPACK_FIELD( "minimum GPU level", "0", int, m_nMinGPULevel )
    DMXELEMENT_UNPACK_FIELD( "freeze simulation after time", "1000000000", float, m_flStopSimulationAfterTime )
END_DMXELEMENT_UNPACK( CParticleSystemDefinition, s_pParticleSystemDefinitionUnpack )



//-----------------------------------------------------------------------------
//
// CParticleOperatorDefinition begins here
// A template describing how a particle system will function
//
//-----------------------------------------------------------------------------
void CParticleSystemDefinition::UnlinkAllCollections()
{
	while ( m_pFirstCollection )
	{
		m_pFirstCollection->UnlinkFromDefList();
	}

	contract_assert( m_nFallbackCurrentCount == 0 );
}

const char *CParticleSystemDefinition::GetName() const
{
	return m_Name.c_str();
}


//-----------------------------------------------------------------------------
// Should we always precache this?
//-----------------------------------------------------------------------------
bool CParticleSystemDefinition::ShouldAlwaysPrecache() const
{
	return m_bAlwaysPrecache;
}


//-----------------------------------------------------------------------------
// Precache/uncache
//-----------------------------------------------------------------------------
void CParticleSystemDefinition::Precache()
{
	if ( m_bIsPrecached )
		return;

	m_bIsPrecached = true;
#ifndef DEDICATED
	if ( !UTIL_IsDedicatedServer() &&  g_pMaterialSystem )
	{
		m_Material.Init( MaterialName(), TEXTURE_GROUP_OTHER, true );
		if ( m_Material->HasProxy() )
		{
			Warning( "Material %s used by particle systems cannot use proxies!\n", m_Material->GetName() );
			m_Material.Init( "debug/particleerror", TEXTURE_GROUP_OTHER, true );
		}
		g_pParticleSystemMgr->FindOrLoadSheet( this );

		// NOTE: Subtle. This has to be called after HasProxy, which will
		// have loaded all material vars. The "queue friendly" version of a material
		// doesn't precache material vars
		m_Material->GetColorModulation( &m_vecMaterialModulation[0], &m_vecMaterialModulation[1], &m_vecMaterialModulation[2] );
		m_vecMaterialModulation[3] = m_Material->GetAlphaModulation();
	}
#endif

	if ( HasFallback() )
	{
		CParticleSystemDefinition *pFallback = GetFallbackReplacementDefinition();
		if ( pFallback )
		{
			pFallback->Precache();
		}
	}
	// call the precache method of the renderers in case they need assets
	for( int i = 0; i < std::ssize( m_Renderers ); i++ )
	{
		CParticleOperatorInstance *pOp = m_Renderers[i].get();
		pOp->Precache();
	}

	int nChildCount = std::ssize( m_Children );
	for ( int i = 0; i < nChildCount; ++i )
	{
		CParticleSystemDefinition *pChild;
		if ( m_Children[i].m_bUseNameBasedLookup )
		{
			pChild = g_pParticleSystemMgr->FindParticleSystem( m_Children[i].m_Name );
		}
		else
		{
			pChild = g_pParticleSystemMgr->FindParticleSystem( m_Children[i].m_Id );
		}

		if ( pChild )
		{
			pChild->Precache();
		}
	}
}

void CParticleSystemDefinition::Uncache()
{
	// Always release material reference if held, regardless of precache state.
	// This ensures proper cleanup at shutdown even if tracking state got out of sync.
	m_Material.Shutdown();

	if ( !m_bIsPrecached )
		return;

	m_bIsPrecached = false;

	if ( HasFallback() )
	{
		CParticleSystemDefinition *pFallback = GetFallbackReplacementDefinition();
		if ( pFallback )
		{
			pFallback->Uncache();
		}
	}
	for( int i = 0; i < std::ssize( m_Renderers ); i++ )
	{
		CParticleOperatorInstance *pOp = m_Renderers[i].get();
		pOp->Uncache();
	}

	int nChildCount = std::ssize( m_Children );
	for ( int i = 0; i < nChildCount; ++i )
	{
		CParticleSystemDefinition *pChild;
		if ( m_Children[i].m_bUseNameBasedLookup )
		{
			pChild = g_pParticleSystemMgr->FindParticleSystem( m_Children[i].m_Name );
		}
		else
		{
			pChild = g_pParticleSystemMgr->FindParticleSystem( m_Children[i].m_Id );
		}

		if ( pChild )
		{
			pChild->Uncache();
		}
	}
}


//-----------------------------------------------------------------------------
// Has this been precached?
//-----------------------------------------------------------------------------
bool CParticleSystemDefinition::IsPrecached() const
{
	return m_bIsPrecached;
}

//-----------------------------------------------------------------------------
// Helper methods to help with unserialization
//-----------------------------------------------------------------------------
void CParticleSystemDefinition::ParseOperators(
	const char *pszOpKey, ParticleFunctionType_t nFunctionType,
	CDmxElement *pElement,
	ParticleOperatorList_t &outList)
{
	const CDmxAttribute* pAttribute = pElement->GetAttribute( pszOpKey );
	if ( !pAttribute || pAttribute->GetType() != AT_ELEMENT_ARRAY )
		return;

	const std::vector< IParticleOperatorDefinition * > &flist = g_pParticleSystemMgr->GetAvailableParticleOperatorList( nFunctionType );

	const std::vector< CDmxElement* >& ops = pAttribute->GetArray<CDmxElement*>( );
	int nCount = std::ssize( ops );
	for ( int i = 0; i < nCount; ++i )
	{
		const char *pOrigName = ops[i]->GetValueString( "functionName" );
		char const *pOpName = RemapOperatorName( pOrigName );
		if ( pOpName != pOrigName )
		{
			pElement->SetValue( "functionName", pOpName );
		}
		bool bFound = false;
		int nFunctionCount = std::ssize( flist );
		for( int j = 0; j < nFunctionCount; ++j )
		{
			if ( V_stricmp( pOpName, flist[j]->GetName() ) )
				continue;

			// found it!
			bFound = true;

			std::unique_ptr< CParticleOperatorInstance, CParticleOperatorDeleter > pNewRef( flist[j]->CreateInstance( ops[i]->GetId() ) );
			const DmxElementUnpackStructure_t *pUnpack = flist[j]->GetUnpackStructure();
			if ( pUnpack )
			{
				ops[i]->UnpackIntoStructure( pNewRef.get(), pUnpack );
			}
			pNewRef.get()->InitParams( this );
			pNewRef->CheckForFastPath();

			m_nAttributeReadMask |= pNewRef.get()->GetReadAttributes();
			m_nControlPointReadMask |= pNewRef.get()->GetReadControlPointMask();
			m_nControlPointNonPositionalMask |= pNewRef.get()->GetNonPositionalControlPointMask();

			switch( nFunctionType )
			{
			case FUNCTION_INITIALIZER:
			case FUNCTION_EMITTER:
				m_nPerParticleInitializedAttributeMask |= pNewRef.get()->GetWrittenAttributes();
				contract_assert( pNewRef.get()->GetReadInitialAttributes() == 0 );
				break;

			case FUNCTION_OPERATOR:
			case FUNCTION_FORCEGENERATOR:
			case FUNCTION_CONSTRAINT:
				m_nPerParticleUpdatedAttributeMask |= pNewRef.get()->GetWrittenAttributes();
				m_nInitialAttributeReadMask |= pNewRef.get()->GetReadInitialAttributes();
				break;
				
			case FUNCTION_RENDERER:
				m_nPerParticleUpdatedAttributeMask |= pNewRef.get()->GetWrittenAttributes();
				m_nInitialAttributeReadMask |= pNewRef.get()->GetReadInitialAttributes();
				break;
			}

			// Special case: Reading particle ID means we're reading the initial particle id
			if ( ( pNewRef.get()->GetReadAttributes() | pNewRef.get()->GetReadInitialAttributes() ) & PARTICLE_ATTRIBUTE_PARTICLE_ID_MASK )
			{
				m_nInitialAttributeReadMask |= PARTICLE_ATTRIBUTE_PARTICLE_ID_MASK;
				m_nPerParticleInitializedAttributeMask |= PARTICLE_ATTRIBUTE_PARTICLE_ID_MASK;
			}

			outList.push_back( std::move( pNewRef ) );
			break;
		}

		if ( !bFound )
		{
			if ( std::ssize( flist ) )							// don't warn if no ops of that type defined (server)
				Warning( "Didn't find particle function %s\n", pOpName );
		}
	}
}

void CParticleSystemDefinition::ParseChildren( CDmxElement *pElement )
{
	const std::vector< CDmxElement* >& children = pElement->GetArray<CDmxElement*>( "children" );
	int nCount = std::ssize( children );
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxElement *pChild = children[i]->GetValue<CDmxElement*>( "child" );
		if ( !pChild || V_stricmp( pChild->GetTypeString(), "DmeParticleSystemDefinition" ) )
			continue;

		int j = std::ssize( m_Children ); m_Children.emplace_back();
		children[i]->UnpackIntoStructure( &m_Children[j], s_ChildrenInfoUnpack );
		m_Children[j].m_bUseNameBasedLookup = !pChild->GetValue<bool>( "preventNameBasedLookup" );
		if ( m_Children[j].m_bUseNameBasedLookup )
		{
			m_Children[j].m_Name = g_pParticleSystemMgr->FindOrAddParticleSystemIndex( pChild->GetName() );
		}
		else
		{
			CopyUniqueId( pChild->GetId(), &m_Children[j].m_Id );
		}

		// Check to see if this child has been encountered already, and if not, then
		// create a new particle definition for this child
		g_pParticleSystemMgr->AddParticleSystem( pChild );
	}
}

void CParticleSystemDefinition::Read( CDmxElement *pElement )
{
	m_Name = pElement->GetName();
	CopyUniqueId( pElement->GetId(), &m_Id );
	pElement->UnpackIntoStructure( this, s_pParticleSystemDefinitionUnpack );

	if ( m_nInitialParticles < 0 )
	{
		m_nInitialParticles = 0;
	}
	if ( m_nMaxParticles < 1 )
	{
		m_nMaxParticles = 1;
	}
	m_nMaxParticles *= g_nParticle_Multiplier;
	m_nMaxParticles = std::min( m_nMaxParticles, MAX_PARTICLES_IN_A_SYSTEM );
	if ( m_flCullRadius > 0 )
	{
		m_nControlPointReadMask |= 1ULL << m_nCullControlPoint;
	}

	ParseOperators( "renderers", FUNCTION_RENDERER, pElement, m_Renderers );
	ParseOperators( "operators", FUNCTION_OPERATOR, pElement, m_Operators );
	ParseOperators( "initializers", FUNCTION_INITIALIZER, pElement, m_Initializers );
	ParseOperators( "emitters", FUNCTION_EMITTER, pElement, m_Emitters );
	ParseChildren( pElement );
	ParseOperators( "forces", FUNCTION_FORCEGENERATOR, pElement, m_ForceGenerators );
	ParseOperators( "constraints", FUNCTION_CONSTRAINT, pElement, m_Constraints );
	SetupContextData();
}

IMaterial *CParticleSystemDefinition::GetMaterial() const
{
	// NOTE: This has to be this way to ensure we don't load every freaking material @ startup
	contract_assert( IsPrecached() );
	if ( !IsPrecached() )
		return nullptr;
	return (IMaterial *) ( (const IMaterial *) m_Material );
}


int CParticleSystemDefinition::GetSheetIndex() const
{
	contract_assert( IsSheetIndexCached() );
	return m_nSheetIndex;
}

void CParticleSystemDefinition::CacheSheetIndex( int nSheetIndex )
{
	m_nSheetIndex = nSheetIndex;
	m_bSheetIndexCached = true;
}

bool CParticleSystemDefinition::IsSheetIndexCached() const
{
	return m_bSheetIndexCached;
}

void CParticleSystemDefinition::InvalidateSheetIndex()
{
	m_bSheetIndexCached = false;
}


//----------------------------------------------------------------------------------
// Returns the particle system fallback
//----------------------------------------------------------------------------------
CParticleSystemDefinition *CParticleSystemDefinition::GetFallbackReplacementDefinition() const
{
	if ( HasFallback() )
	{
		if ( !m_pFallback() )
		{
			const_cast< CParticleSystemDefinition* >( this )->m_pFallback.Set( g_pParticleSystemMgr->FindParticleSystem( m_FallbackReplacementName.c_str() ) );
		}
		return m_pFallback();
	}
	return nullptr;
}


//----------------------------------------------------------------------------------
// Does the particle system use the power of two frame buffer texture (refraction?)
//----------------------------------------------------------------------------------
bool CParticleSystemDefinition::UsesPowerOfTwoFrameBufferTexture()
{
	// NOTE: This has to be this way to ensure we don't load every freaking material @ startup
	contract_assert( IsPrecached() );
	return g_pMaterialSystem && m_Material && m_Material->NeedsPowerOfTwoFrameBufferTexture( false ); // The false checks if it will ever need the frame buffer, not just this frame
}

//----------------------------------------------------------------------------------
// Does the particle system use the power of two frame buffer texture (refraction?)
//----------------------------------------------------------------------------------
bool CParticleSystemDefinition::UsesFullFrameBufferTexture()
{
	// NOTE: This has to be this way to ensure we don't load every freaking material @ startup
	contract_assert( IsPrecached() );
	return g_pMaterialSystem && m_Material && m_Material->NeedsFullFrameBufferTexture( false ); // The false checks if it will ever need the frame buffer, not just this frame
}

//-----------------------------------------------------------------------------
// Helper methods to write particle systems
//-----------------------------------------------------------------------------
void CParticleSystemDefinition::WriteOperators( CDmxElement *pElement, 
	const char *pOpKeyName, const ParticleOperatorList_t &inList )
{
	CDmxElementModifyScope modify( pElement );
	CDmxAttribute* pAttribute = pElement->AddAttribute( pOpKeyName );
	std::vector< CDmxElement* >& ops = pAttribute->GetArrayForEdit<CDmxElement*>( );

	int nCount = std::ssize( inList );
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxElement *pOperator = CreateDmxElement( "DmeParticleOperator" );
		ops.push_back( pOperator );

		const IParticleOperatorDefinition *pDef = inList[i]->GetDefinition();
		pOperator->SetValue( "name", pDef->GetName() );
		pOperator->SetValue( "functionName", pDef->GetName() );

		const DmxElementUnpackStructure_t *pUnpack = pDef->GetUnpackStructure();
		if ( pUnpack )
		{
			pOperator->AddAttributesFromStructure( inList[i].get(), pUnpack );
		}
	}
}

void CParticleSystemDefinition::WriteChildren( CDmxElement *pElement )
{
	CDmxElementModifyScope modify( pElement );
	CDmxAttribute* pAttribute = pElement->AddAttribute( "children" );
	std::vector< CDmxElement* >& children = pAttribute->GetArrayForEdit<CDmxElement*>( );
	int nCount = std::ssize( m_Children );
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxElement *pChildRef = CreateDmxElement( "DmeParticleChild" );
		children.push_back( pChildRef );
		children[i]->AddAttributesFromStructure( &m_Children[i], s_ChildrenInfoUnpack );
		CDmxElement *pChildParticleSystem;
		if ( m_Children[i].m_bUseNameBasedLookup )
		{
			pChildParticleSystem = g_pParticleSystemMgr->CreateParticleDmxElement( 
				g_pParticleSystemMgr->GetParticleSystemNameFromIndex( m_Children[i].m_Name ) );
		}
		else
		{
			pChildParticleSystem = g_pParticleSystemMgr->CreateParticleDmxElement( m_Children[i].m_Id );
		}
		pChildRef->SetValue( "name", pChildParticleSystem->GetName() );
		pChildRef->SetValue( "child", pChildParticleSystem );
	}
}

CDmxElement *CParticleSystemDefinition::Write()
{
	const char *pName = GetName();

	CDmxElement *pElement = CreateDmxElement( "DmeParticleSystemDefinition" );
	pElement->SetValue( "name", pName );
	pElement->AddAttributesFromStructure( this, s_pParticleSystemDefinitionUnpack );
	WriteOperators( pElement, "renderers",m_Renderers );
	WriteOperators( pElement, "operators", m_Operators );
	WriteOperators( pElement, "initializers", m_Initializers );
	WriteOperators( pElement, "emitters", m_Emitters );
	WriteChildren( pElement );
	WriteOperators( pElement, "forces", m_ForceGenerators );
	WriteOperators( pElement, "constraints", m_Constraints );

	return pElement;
}

void CParticleSystemDefinition::SetupContextData( void )
{
	// calculate sizes and offsets for context data
	ParticleOperatorList_t *olists[] = {
		&m_Operators, &m_Renderers, &m_Initializers, &m_Emitters, &m_ForceGenerators,
		&m_Constraints
	};
	std::vector< std::size_t > *offsetLists[] = {
		&m_nOperatorsCtxOffsets, &m_nRenderersCtxOffsets,
		&m_nInitializersCtxOffsets, &m_nEmittersCtxOffsets,
		&m_nForceGeneratorsCtxOffsets, &m_nConstraintsCtxOffsets,
	};

	// loop through all operators, fill in offset entries, and calulate total data needed
	m_nContextDataSize = 0;
	for( int i = 0; i < NELEMS( olists ); i++ )
	{
		int nCount = std::ssize( *olists[i] );
		for( int j = 0; j < nCount; j++ )
		{
			offsetLists[i]->push_back( m_nContextDataSize );
			m_nContextDataSize += (*olists[i])[j].get()->GetRequiredContextBytes();
			// align context data
			m_nContextDataSize = (m_nContextDataSize + 15) & (~0xf );
		}
	}
}


//-----------------------------------------------------------------------------
// Finds an operator by id
//-----------------------------------------------------------------------------
ParticleOperatorList_t *CParticleSystemDefinition::GetOperatorList( ParticleFunctionType_t type )
{
	switch( type )
	{
	case FUNCTION_EMITTER:
		return &m_Emitters;
	case FUNCTION_RENDERER:
		return &m_Renderers;
	case FUNCTION_INITIALIZER:
		return &m_Initializers;
	case FUNCTION_OPERATOR:
		return &m_Operators;
	case FUNCTION_FORCEGENERATOR:
		return &m_ForceGenerators;
	case FUNCTION_CONSTRAINT:
		return &m_Constraints;
	default:
		contract_assert( 0 );
		return nullptr;
	}
}


//-----------------------------------------------------------------------------
// Finds an operator by id
//-----------------------------------------------------------------------------
CParticleOperatorInstance *CParticleSystemDefinition::FindOperatorById( ParticleFunctionType_t type, const DmObjectId_t &id )
{
	ParticleOperatorList_t *pVec = GetOperatorList( type );
	if ( !pVec )
		return nullptr;

	int nCount = std::ssize( *pVec );
	for ( int i = 0; i < nCount; ++i )
	{
		if ( IsUniqueIdEqual( id, (*pVec)[ i ]->GetId() ) )
			return (*pVec)[ i ].get();
	}
	return nullptr;
}


//-----------------------------------------------------------------------------
// Finds an operator by name (slow!)
//-----------------------------------------------------------------------------
CParticleOperatorInstance *CParticleSystemDefinition::FindOperatorByName( const char *pOperatorName )
{
	for ( int i = 0; i < PARTICLE_FUNCTION_COUNT; i++ )
	{
		ParticleOperatorList_t *pVec = ( i == FUNCTION_CHILDREN ) ? nullptr : GetOperatorList( (ParticleFunctionType_t)i );
		if ( !pVec )
			continue;

		int nCount = std::ssize( *pVec );
		for ( int j = 0; j < nCount; ++j )
		{
			if ( !V_stricmp( pOperatorName, (*pVec)[ j ]->GetDefinition()->GetName() ) )
				return (*pVec)[ j ].get();
		}
	}
	return nullptr;
}


//-----------------------------------------------------------------------------
// Operator dispatch: the group (source file) of the operator's kind picks the switch that knows
// its class (see particle_operator_dispatch.inl).
//-----------------------------------------------------------------------------
#define KS_PARTICLE_OP_DEFINE_ENTRY( Ret, Name, Const, Params, Args ) \
	Ret ParticleOps::Name( KS_PARTICLE_OP_SIGNATURE( Const, KS_PARTICLE_OP_UNPAREN Params ) ) \
	{ \
		switch ( pOp->m_Group ) \
		{ \
		case ParticleOperatorGroup::Constraints: return Name##_Constraints( KS_PARTICLE_OP_ARGS( KS_PARTICLE_OP_UNPAREN Args ) ); \
		case ParticleOperatorGroup::Initializers: return Name##_Initializers( KS_PARTICLE_OP_ARGS( KS_PARTICLE_OP_UNPAREN Args ) ); \
		case ParticleOperatorGroup::Emitters: return Name##_Emitters( KS_PARTICLE_OP_ARGS( KS_PARTICLE_OP_UNPAREN Args ) ); \
		case ParticleOperatorGroup::Forces: return Name##_Forces( KS_PARTICLE_OP_ARGS( KS_PARTICLE_OP_UNPAREN Args ) ); \
		case ParticleOperatorGroup::Operators: return Name##_Operators( KS_PARTICLE_OP_ARGS( KS_PARTICLE_OP_UNPAREN Args ) ); \
		case ParticleOperatorGroup::Renderers: return Name##_Renderers( KS_PARTICLE_OP_ARGS( KS_PARTICLE_OP_UNPAREN Args ) ); \
		} \
		std::unreachable(); \
	}
KS_PARTICLE_OP_ENTRIES( KS_PARTICLE_OP_DEFINE_ENTRY )
#undef KS_PARTICLE_OP_DEFINE_ENTRY

CParticleOperatorInstance *ParticleOps::Create( ParticleOperatorKind kind )
{
	switch ( ParticleOperatorGroupOf( kind ) )
	{
	case ParticleOperatorGroup::Constraints: return Create_Constraints( kind );
	case ParticleOperatorGroup::Initializers: return Create_Initializers( kind );
	case ParticleOperatorGroup::Emitters: return Create_Emitters( kind );
	case ParticleOperatorGroup::Forces: return Create_Forces( kind );
	case ParticleOperatorGroup::Operators: return Create_Operators( kind );
	case ParticleOperatorGroup::Renderers: return Create_Renderers( kind );
	}
	std::unreachable();
}

void ParticleOps::Destroy( CParticleOperatorInstance *pOp )
{
	if ( !pOp )
		return;
	switch ( pOp->m_Group )
	{
	case ParticleOperatorGroup::Constraints: return Destroy_Constraints( pOp );
	case ParticleOperatorGroup::Initializers: return Destroy_Initializers( pOp );
	case ParticleOperatorGroup::Emitters: return Destroy_Emitters( pOp );
	case ParticleOperatorGroup::Forces: return Destroy_Forces( pOp );
	case ParticleOperatorGroup::Operators: return Destroy_Operators( pOp );
	case ParticleOperatorGroup::Renderers: return Destroy_Renderers( pOp );
	}
	std::unreachable();
}

CParticleOperatorInstance *IParticleOperatorDefinition::CreateInstance( const DmObjectId_t &id ) const
{
	contract_assert( m_bCreatable );
	CParticleOperatorInstance *pOp = ParticleOps::Create( m_Kind );
	pOp->SetDefinition( this, id );
	return pOp;
}


//-----------------------------------------------------------------------------
//
// CParticleOperatorInstance
//
//-----------------------------------------------------------------------------
void CParticleOperatorInstance::InitNewParticles( CParticleCollection *pParticles,
												  int nFirstParticle, int nParticleCount,
												  int nAttributeWriteMask, void *pContext ) const
{
	if ( !nParticleCount )
		return;

	if ( nParticleCount < 16 )								// don't bother with vectorizing
															// unless enough particles to bother
	{
		InitNewParticlesScalar( pParticles, nFirstParticle, nParticleCount, nAttributeWriteMask, pContext );
		return;
	}
	
	int nHead = nFirstParticle & 3;
	if ( nHead )
	{
		// need to init up to 3 particles before we are block aligned
		int nHeadCount = std::min( nParticleCount, 4 - nHead );
		InitNewParticlesScalar( pParticles, nFirstParticle, nHeadCount, nAttributeWriteMask, pContext );
		nParticleCount -= nHeadCount;
		nFirstParticle += nHeadCount;
	}

	// now, we are aligned
	int nBlockCount = nParticleCount / 4;
	if ( nBlockCount )
	{
		InitNewParticlesBlock( pParticles, nFirstParticle / 4, nBlockCount, nAttributeWriteMask, pContext );
		nParticleCount -= 4 * nBlockCount;
		nFirstParticle += 4 * nBlockCount;
	}

	// do tail
	if ( nParticleCount )
	{
		InitNewParticlesScalar( pParticles, nFirstParticle, nParticleCount, nAttributeWriteMask, pContext );
	}
}


//-----------------------------------------------------------------------------
//
// CParticleCollection
//
//-----------------------------------------------------------------------------

//------------------------------------------------------------------------------
// need custom new/delete for alignment for simd
//------------------------------------------------------------------------------
void *CParticleCollection::operator new( std::size_t nSize )
{
	return MemAlloc_AllocAlignedUnattributed( nSize, 16 );
}

void* CParticleCollection::operator new( std::size_t nSize, int nBlockUse, const char *pFileName, int nLine )
{
	return MemAlloc_AllocAlignedFileLine( nSize, 16, pFileName, nLine );
}

void CParticleCollection::operator delete(void *pData)
{
	if ( pData )
	{
		MemAlloc_FreeAligned( pData );
	}
}

void CParticleCollection::operator delete( void* pData, int nBlockUse, const char *pFileName, int nLine )
{
	if ( pData )
	{
		MemAlloc_FreeAligned( pData );
	}
}

void *CWorldCollideContextData::operator new( std::size_t nSize )
{
	return MemAlloc_AllocAlignedUnattributed( nSize, 16 );
}

void* CWorldCollideContextData::operator new( std::size_t nSize, int nBlockUse, const char *pFileName, int nLine )
{
	return MemAlloc_AllocAlignedFileLine( nSize, 16, pFileName, nLine );
}

void CWorldCollideContextData::operator delete(void *pData)
{
	if ( pData )
	{
		MemAlloc_FreeAligned( pData );
	}
}

void CWorldCollideContextData::operator delete( void* pData, int nBlockUse, const char *pFileName, int nLine )
{
	if ( pData )
	{
		MemAlloc_FreeAligned( pData );
	}
}


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CParticleCollection::CParticleCollection( )
{
	static_assert( ( MAX_RANDOM_FLOATS & ( MAX_RANDOM_FLOATS - 1 ) ) == 0 );
	static_assert( sizeof( s_pRandomFloats ) / sizeof( float ) >= MAX_RANDOM_FLOATS );

	std::memset( this, 0, sizeof(CParticleCollection) );
	m_flOOMaxDistSqr = 1.0f;


	m_flPreviousDt = 0.05f;
	m_nParticleFlags = PCFLAGS_FIRST_FRAME;
	m_LocalLighting = Color(255, 255, 255, 255);
	m_LocalLightingCP = -1;
	m_bPendingRestart = false;
	m_flTargetDrawTime = 0;
	m_bQueuedStartEmission = false;
	m_bFrozen = false;
	m_pCPInfo = nullptr;
	m_pCachedParticleBatches = nullptr;
	m_bTriedLoadingSheet = false;
}

CParticleCollection::~CParticleCollection( void )
{
	UnlinkFromDefList();

	m_Children.delete_all();

	if ( m_pParticleMemory )
	{
		MemAlloc_FreeAligned( m_pParticleMemory );
		m_pParticleMemory = nullptr;
	}

	if ( m_pPreviousAttributeMemory )
	{
		MemAlloc_FreeAligned( m_pPreviousAttributeMemory );
		m_pPreviousAttributeMemory = nullptr;
	}

	if ( m_pParticleInitialMemory )
	{
		MemAlloc_FreeAligned( m_pParticleInitialMemory );
		m_pParticleInitialMemory = nullptr;
	}
	if ( m_pConstantMemory )
	{
		MemAlloc_FreeAligned( m_pConstantMemory );
		m_pConstantMemory = nullptr;
	}
	if ( m_pOperatorContextData )
	{
		MemAlloc_FreeAligned( m_pOperatorContextData );
		m_pOperatorContextData = nullptr;
	}

	for( int i = 0 ; i < std::size( m_pCollisionCacheData ) ; i++ )
	{
		if ( m_pCollisionCacheData[i] )
		{
			delete m_pCollisionCacheData[i];
			m_pCollisionCacheData[i] = nullptr;
		}
	}
	if ( m_pCPInfo )
	{
		delete[] m_pCPInfo;
		m_pCPInfo = nullptr;
	}

	if ( m_pCachedParticleBatches )
	{
		delete m_pCachedParticleBatches;
		m_pCachedParticleBatches = nullptr;
	}
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
void CParticleCollection::Init( CParticleSystemDefinition *pDef, float flDelay, int nRandomSeed )
{
	m_pDef = pDef;
	
	// Link into def list
	LinkIntoDefList();

	m_pRenderable = nullptr;

	InitStorage( pDef );
	
	// Initialize sheet data
	m_Sheet.Set( g_pParticleSystemMgr->FindOrLoadSheet( pDef ) );
	m_bTriedLoadingSheet = false;

	// FIXME: This seed needs to be recorded per instance!
	m_bIsScrubbable = ( nRandomSeed != 0 );
	if ( m_bIsScrubbable )
	{
		m_nRandomSeed = nRandomSeed;
	}
	else
	{
		m_nRandomSeed = (int)(intp)this;
#ifndef _DEBUG
		m_nRandomSeed += Plat_MSTime();
#endif
	}

	SetAttributeToConstant( PARTICLE_ATTRIBUTE_XYZ, 0.0f, 0.0f, 0.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_PREV_XYZ, 0.0f, 0.0f, 0.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_LIFE_DURATION, 1.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_RADIUS, pDef->m_flConstantRadius );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_ROTATION, pDef->m_flConstantRotation );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_ROTATION_SPEED, pDef->m_flConstantRotationSpeed );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_TINT_RGB,
		pDef->m_ConstantColor.r() / 255.0f, pDef->m_ConstantColor.g() / 255.0f,
		pDef->m_ConstantColor.b() / 255.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_ALPHA, pDef->m_ConstantColor.a() / 255.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_CREATION_TIME, 0.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER, pDef->m_nConstantSequenceNumber );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER1, pDef->m_nConstantSequenceNumber1 );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_TRAIL_LENGTH, 0.1f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_PARTICLE_ID, 0 );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_YAW, 0.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_ALPHA2, 1.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_PITCH, 0.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_NORMAL, 
		pDef->m_ConstantNormal.x, pDef->m_ConstantNormal.y, pDef->m_ConstantNormal.z );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_GLOW_RGB, 1.0f, 1.0f, 1.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_GLOW_ALPHA, 1.0f );

	// Offset the child in time
	m_flCurTime = -flDelay;
	m_fl4CurTime = ReplicateX4( m_flCurTime );
	if ( m_pDef->m_nContextDataSize )
	{
		m_pOperatorContextData = reinterpret_cast<uint8 *> 
			( MemAlloc_AllocAlignedUnattributed( m_pDef->m_nContextDataSize, 16 ) );
	}
	
	m_flNextSleepTime = g_pParticleSystemMgr->GetLastSimulationTime() + pDef->m_flNoDrawTimeToGoToSleep;

	m_nControlPointReadMask = pDef->m_nControlPointReadMask;
	m_nControlPointNonPositionalMask = pDef->m_nControlPointNonPositionalMask;

	// Instance child particle systems
	int nChildCount = std::ssize( pDef->m_Children );
	for ( int i = 0; i < nChildCount; ++i )
	{
		if ( nRandomSeed != 0 )
		{
			nRandomSeed += 129;
		}

		CParticleCollection *pChild;
		if ( pDef->m_Children[i].m_bUseNameBasedLookup )
		{
			pChild = g_pParticleSystemMgr->CreateParticleCollection( pDef->m_Children[i].m_Name, -m_flCurTime + pDef->m_Children[i].m_flDelay, nRandomSeed );
		}
		else
		{
			pChild = g_pParticleSystemMgr->CreateParticleCollection( pDef->m_Children[i].m_Id, -m_flCurTime + pDef->m_Children[i].m_flDelay, nRandomSeed );
		}
		if ( pChild )
		{
			pChild->m_pParent = this;
			m_Children.push_back( pChild );
			m_nControlPointReadMask |= pChild->m_nControlPointReadMask;
			m_nControlPointNonPositionalMask |= pChild->m_nControlPointNonPositionalMask;
			if ( pDef->m_Children[i].m_bEndCap )
			{
				pChild->m_flCurTime = -std::numeric_limits<float>::max();
			}
		}
	}

	if ( !IsValid() )
		return;

	m_bIsTranslucent = ComputeIsTranslucent();
	m_bIsTwoPass = ComputeIsTwoPass();
	m_bIsBatchable = ComputeIsBatchable();
	m_bIsOrderImportant = ComputeIsOrderImportant();
	m_bRunForParentApplyKillList = ComputeRunForParentApplyKillList();
	LabelTextureUsage();
	m_bAnyUsesPowerOfTwoFrameBufferTexture = ComputeUsesPowerOfTwoFrameBufferTexture();
	m_bAnyUsesFullFrameBufferTexture = ComputeUsesFullFrameBufferTexture();

	m_bInEndCap = false;

	// now, allocate the control point data.
	// we will always allocate on extra, so that if someone sets a control point that the particle system doesn't
	// use, then there will be a dummy one to write to.
	int nHighest;
	for( nHighest = 63; nHighest > 0 ; nHighest-- )
	{
		if ( m_nControlPointReadMask & ( 1ll << nHighest ) )
		{
			break;
		}
	}
	m_nNumControlPointsAllocated = std::min( MAX_PARTICLE_CONTROL_POINTS, 2 + nHighest );
	//Warning( " save %d bytes by only allocating %d cp's\n", ( 64 - m_nNumControlPointsAllocated ) * sizeof( CParticleCPInfo ), m_nNumControlPointsAllocated );
	m_pCPInfo = new CParticleCPInfo[ m_nNumControlPointsAllocated];

	// align all control point orientations with the global world
	std::memset( m_pCPInfo, 0, sizeof( CParticleCPInfo ) * m_nNumControlPointsAllocated );
	for( int i=0; i < m_nNumControlPointsAllocated; i++ )
	{
		ControlPoint(i).m_ForwardVector.y = 1.0f;
		ControlPoint(i).m_UpVector.z = 1.0f;
		ControlPoint(i).m_RightVector.x = 1.0f;
	}
	// now, init context data
	ParticleOperatorList_t *olists[] =
	{
		&(m_pDef->m_Operators), &(m_pDef->m_Renderers), 
		&(m_pDef->m_Initializers), &(m_pDef->m_Emitters),
		&(m_pDef->m_ForceGenerators),
		&(m_pDef->m_Constraints),
	};
	std::vector< std::size_t > *offsetlists[]=
	{
		&(m_pDef->m_nOperatorsCtxOffsets), &(m_pDef->m_nRenderersCtxOffsets),
		&(m_pDef->m_nInitializersCtxOffsets), &(m_pDef->m_nEmittersCtxOffsets),
		&(m_pDef->m_nForceGeneratorsCtxOffsets),
		&(m_pDef->m_nConstraintsCtxOffsets),

	};

	for( int i=0; i<NELEMS( olists ); i++ )
	{
		int nOperatorCount = std::ssize( *olists[i] );
		for( int j=0; j < nOperatorCount; j++ )
		{
			(*olists[i])[j].get()->InitializeContextData( this, m_pOperatorContextData+ (*offsetlists)[i][j] );
		}
	}


}

int CParticleCollection::GetCurrentParticleDefCount( CParticleSystemDefinition* pDef )
{
	int nDefCount = pDef->m_nFallbackCurrentCount;
	CParticleSystemDefinition *pFallback = pDef;
	while ( pFallback && pFallback->HasFallback() )
	{
		pFallback = pFallback->GetFallbackReplacementDefinition();
		if ( !pFallback )
		{
			break;
		}
		nDefCount += pFallback->m_nFallbackCurrentCount;
	}
	return nDefCount;
}

bool CParticleCollection::Init( CParticleSystemDefinition *pDef )
{
	if ( pDef->GetMinCPULevel() > g_pParticleSystemMgr->GetParticleCPULevel() || pDef->GetMinGPULevel() > g_pParticleSystemMgr->GetParticleGPULevel() )
	{
		pDef = nullptr;
		return false;
	}

	float flFallbackMultiplier = g_pParticleSystemMgr->GetFallbackMultiplier();
	float flFallbackBase = g_pParticleSystemMgr->GetFallbackBase();
	float flThresholdSimMS = g_pParticleSystemMgr->GetSimFallbackThresholdMs();

	int nFallbackCount = GetCurrentParticleDefCount( pDef );

	// Determine if we've gone past our maximum allowable time for simulating particles.
	// NOTE: If particle simulation starts overlapping client operations, then we'll need to 
	//       make setting and querying of sim duration threadsafe.
	float flPreviousSimMS = g_pParticleSystemMgr->GetLastSimulationDuration() * 1000.0f;
	if ( flPreviousSimMS > flThresholdSimMS )
	{
		float flMSOver = flPreviousSimMS - flThresholdSimMS;
		float flSimFallbackBaseMultiplier = g_pParticleSystemMgr->GetSimFallbackBaseMultiplier();

		// Increase the fallback base by a factor of r_particle_sim_fallback_base_multiplier 
		// for each millisecond we're over the threshold
		flFallbackBase += flMSOver * flSimFallbackBaseMultiplier;

		// Uncomment to spew when we're trying to fall back because sim time took too long
		//Warning( "Particle sim took too long: %f, threshold %f\n", flPreviousSimMS, flThresholdSimMS );
	}

	// If our maximum number of simultaneous definitions has been exceeded fallback to the appropriate def
	while ( pDef && pDef->HasFallback() && 
		( ( nFallbackCount * flFallbackMultiplier ) + flFallbackBase ) >= pDef->m_nFallbackMaxCount )
	{
		nFallbackCount -= pDef->m_nFallbackCurrentCount;
		pDef = pDef->GetFallbackReplacementDefinition();
	}

	if ( !pDef ) // || !pDef->IsPrecached() )
	{
		Warning( "Particlelib: Missing precache for particle system type \"%s\"!\n", pDef ? pDef->GetName() : "unknown" );
		CParticleSystemDefinition *pErrorDef = g_pParticleSystemMgr->FindParticleSystem( "error" );
		if ( pErrorDef )
		{
			pDef = pErrorDef;
		}
	}
	Init( pDef, 0.0f, 0 );
	return IsValid();
}

bool CParticleCollection::Init( const char *pParticleSystemName )
{
	if ( !pParticleSystemName )
		return false;

	CParticleSystemDefinition *pDef = g_pParticleSystemMgr->FindParticleSystem( pParticleSystemName );
	if ( !pDef )
	{
		Warning( "Attempted to create unknown particle system type \"%s\"!\n", pParticleSystemName );
		return false;
	}

	return Init( pDef );
}


bool CParticleCollection::IsFullyValid( void ) const
{
	if ( m_pDef.GetObject() == nullptr )
		return false;

	for( CParticleCollection *p = m_Children.head(); p; p = p->m_pNext )
	{
		if ( !p->IsFullyValid() )
			return false;
	}

	return true;
}

bool CParticleCollection::DependsOnSystem( const char *pName ) const
{
	if ( m_pDef.GetObject() == nullptr )
		return false;

	if ( m_pDef->m_Name == pName )
		return true;

	for( CParticleCollection *p = m_Children.head(); p; p = p->m_pNext )
	{
		if ( p->DependsOnSystem(pName) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// List management for collections sharing the same particle definition
//-----------------------------------------------------------------------------
void CParticleCollection::LinkIntoDefList( )
{
	contract_assert( !m_pPrevDef && !m_pNextDef );

	++( m_pDef->m_nFallbackCurrentCount );
	m_pPrevDef = nullptr;
	m_pNextDef = m_pDef->m_pFirstCollection;
	m_pDef->m_pFirstCollection = this;
	if ( m_pNextDef )
	{
		m_pNextDef->m_pPrevDef = this;
	}

#ifdef _DEBUG
	CParticleCollection *pCollection = m_pDef->FirstCollection();
	while ( pCollection )
	{
		contract_assert( pCollection->m_pDef == m_pDef );
		pCollection = pCollection->GetNextCollectionUsingSameDef();
	}
#endif
}

void CParticleCollection::UnlinkFromDefList( )
{
	if ( !m_pDef )
		return;

	--( m_pDef->m_nFallbackCurrentCount );
	if ( m_pDef->m_pFirstCollection == this )
	{
		m_pDef->m_pFirstCollection = m_pNextDef;
		contract_assert( !m_pPrevDef );
	}
	else 
	{
		contract_assert( m_pPrevDef );
		m_pPrevDef->m_pNextDef = m_pNextDef;
	}

	if ( m_pNextDef )
	{
		m_pNextDef->m_pPrevDef = m_pPrevDef;
	}

	m_pNextDef = m_pPrevDef = nullptr;

#ifdef _DEBUG
	CParticleCollection *pCollection = m_pDef->FirstCollection();
	while ( pCollection )
	{
		contract_assert( pCollection->m_pDef == m_pDef );
		pCollection = pCollection->GetNextCollectionUsingSameDef();
	}
#endif

	m_pDef = nullptr;
}


//-----------------------------------------------------------------------------
// Reset the particle cache if the frame has changed
//-----------------------------------------------------------------------------
void CParticleCollection::ResetParticleCache()
{
	if ( m_pCachedParticleBatches )
	{

		uint32 nCurrentFrame = g_pMaterialSystem->GetCurrentFrameCount();
		if ( m_pCachedParticleBatches->m_nLastValidParticleCacheFrame != nCurrentFrame )
		{
			m_pCachedParticleBatches->ClearBatches();
			m_pCachedParticleBatches->m_nLastValidParticleCacheFrame = nCurrentFrame;
		}
	}
}

//-----------------------------------------------------------------------------
// Get the cached particle batches for this particular particle collection
//-----------------------------------------------------------------------------
CCachedParticleBatches *CParticleCollection::GetCachedParticleBatches()
{
	if ( !m_pCachedParticleBatches )
	{
		m_pCachedParticleBatches = new CCachedParticleBatches();
	}

	return m_pCachedParticleBatches;
}

//-----------------------------------------------------------------------------
// Particle memory initialization
//-----------------------------------------------------------------------------
void CParticleCollection::InitStorage( CParticleSystemDefinition *pDef )
{
	contract_assert( pDef->m_nMaxParticles < 65536 );

	m_nMaxAllowedParticles = std::min( MAX_PARTICLES_IN_A_SYSTEM, pDef->m_nMaxParticles );
	m_nAllocatedParticles = 4 + 4 * ( ( m_nMaxAllowedParticles + 3 ) / 4 );						    

	int nConstantMemorySize = 3 * 4 * MAX_PARTICLE_ATTRIBUTES * sizeof(float) + 16;
						 
	// Align allocation for constant attributes to 16 byte boundaries
	m_pConstantMemory =	(uint8 * ) MemAlloc_AllocAlignedUnattributed( nConstantMemorySize, 16 );
	m_pConstantAttributes = ( float * ) m_pConstantMemory;

	m_nPerParticleInitializedAttributeMask = pDef->m_nPerParticleInitializedAttributeMask;
	m_nPerParticleUpdatedAttributeMask = pDef->m_nPerParticleUpdatedAttributeMask;

	// Only worry about initial attributes that are per-particle *and* are updated at a later time
	// If they aren't updated at a later time, then we can just point the initial + current pointers at the same memory
	m_nPerParticleReadInitialAttributeMask = pDef->m_nInitialAttributeReadMask & 
		( pDef->m_nPerParticleInitializedAttributeMask & pDef->m_nPerParticleUpdatedAttributeMask );

	// This is the mask of attributes which are initialized per-particle, but never updated
	// *and* where operators want to read initial particle state
	int nPerParticleReadConstantAttributeMask = pDef->m_nInitialAttributeReadMask & 
		( pDef->m_nPerParticleInitializedAttributeMask & ( ~pDef->m_nPerParticleUpdatedAttributeMask ) );

	int sz = 0;
	int nInitialAttributeSize = 0;
	int nPerParticleAttributeMask = m_nPerParticleInitializedAttributeMask | m_nPerParticleUpdatedAttributeMask;
	for( int bit = 0; bit < MAX_PARTICLE_ATTRIBUTES; bit++ )
	{
		int nAttrSize = ( ( 1 << bit ) & ATTRIBUTES_WHICH_ARE_VEC3S_MASK ) ? 3 : 1;
		if ( nPerParticleAttributeMask & ( 1 << bit ) )
		{ 
			sz += nAttrSize;
		}
		if ( m_nPerParticleReadInitialAttributeMask & ( 1 << bit ) )
		{
			nInitialAttributeSize += nAttrSize;
		}
	}

	// Gotta allocate a couple extra floats to account for padding
	int nAllocationSize = m_nAllocatedParticles * sz * sizeof(float) + sizeof( FourVectors );
	m_pParticleMemory = ( uint8 * ) MemAlloc_AllocAlignedUnattributed( nAllocationSize, 16 );
	m_nAttributeMemorySize = nAllocationSize;
	std::memset( m_pParticleMemory, 0, nAllocationSize );

	// Allocate space for the initial attributes
	if ( nInitialAttributeSize != 0 )
	{
		int nInitialAllocationSize = m_nAllocatedParticles * nInitialAttributeSize * sizeof(float) + sizeof( FourVectors );
		m_pParticleInitialMemory = ( uint8 * ) MemAlloc_AllocAlignedUnattributed( nInitialAllocationSize, 16 );
		memset( m_pParticleInitialMemory, 0, nInitialAllocationSize );
	}

	// Align allocation to 16-byte boundaries
	float *pMem = ( float* ) m_pParticleMemory;
	float *pInitialMem = ( float* )( m_pParticleInitialMemory );

	// Point each attribute to memory associated with that attribute
	for( int bit = 0; bit < MAX_PARTICLE_ATTRIBUTES; bit++ )
	{
		int nAttrSize = ( ( 1 << bit ) & ATTRIBUTES_WHICH_ARE_VEC3S_MASK ) ? 3 : 1;

		if ( nPerParticleAttributeMask & ( 1 << bit ) )
		{ 
			m_ParticleAttributes.m_pAttributes[ bit ] = pMem;
			m_ParticleAttributes.m_nFloatStrides[ bit ] = nAttrSize * 4;
			pMem += nAttrSize * m_nAllocatedParticles;
		}
		else
		{
			m_ParticleAttributes.m_pAttributes[ bit ] = GetConstantAttributeMemory( bit );
			m_ParticleAttributes.m_nFloatStrides[ bit ] = 0;
		}

		// Are we reading
		if ( pDef->m_nInitialAttributeReadMask & ( 1 << bit ) )
		{
			if ( m_nPerParticleReadInitialAttributeMask & ( 1 << bit ) )
			{
				contract_assert( pInitialMem );
				m_ParticleInitialAttributes.m_pAttributes[ bit ] = pInitialMem;
				m_ParticleInitialAttributes.m_nFloatStrides[ bit ] = nAttrSize * 4;
				pInitialMem += nAttrSize * m_nAllocatedParticles;
			}
			else if ( nPerParticleReadConstantAttributeMask & ( 1 << bit ) )
			{
				m_ParticleInitialAttributes.m_pAttributes[ bit ] = m_ParticleAttributes.m_pAttributes[ bit ];
				m_ParticleInitialAttributes.m_nFloatStrides[ bit ] = m_ParticleAttributes.m_nFloatStrides[ bit ];
			}
			else
			{
				m_ParticleInitialAttributes.m_pAttributes[ bit ] = GetConstantAttributeMemory( bit );
				m_ParticleInitialAttributes.m_nFloatStrides[ bit ] = 0;
			}
		}
		else
		{
			// Catch errors where code is reading data it didn't request
			m_ParticleInitialAttributes.m_pAttributes[ bit ] = nullptr;
			m_ParticleInitialAttributes.m_nFloatStrides[ bit ] = 0;
		}
	}
}


//-----------------------------------------------------------------------------
// Returns the particle collection name
//-----------------------------------------------------------------------------
const char *CParticleCollection::GetName() const
{
	return m_pDef ? m_pDef->GetName() : "";
}


//-----------------------------------------------------------------------------
// Does the particle system use the frame buffer texture (refraction?)
//-----------------------------------------------------------------------------
bool CParticleCollection::UsesPowerOfTwoFrameBufferTexture( bool bThisFrame ) const
{
	if ( ! m_bAnyUsesPowerOfTwoFrameBufferTexture )			// quick out if neither us or our children ever use
	{
		return false;
	}
	if ( bThisFrame )
	{
		return SystemContainsParticlesWithBoolSet( &CParticleCollection::m_bUsesPowerOfTwoFrameBufferTexture );
	}
	return true;
}
//-----------------------------------------------------------------------------
// Does the particle system use the full frame buffer texture (soft particles)
//-----------------------------------------------------------------------------
bool CParticleCollection::UsesFullFrameBufferTexture( bool bThisFrame ) const
{
	if ( ! m_bAnyUsesFullFrameBufferTexture )				// quick out if neither us or our children ever use
	{
		return false;
	}
	if ( bThisFrame )
	{
		return SystemContainsParticlesWithBoolSet( &CParticleCollection::m_bUsesFullFrameBufferTexture );
	}
	return true;
}

bool CParticleCollection::SystemContainsParticlesWithBoolSet( bool CParticleCollection::*pField ) const
{
	if ( m_nActiveParticles && ( this->*pField ) )
		return true;
	for( CParticleCollection *p = m_Children.head(); p; p = p->m_pNext )
	{
		if ( p->SystemContainsParticlesWithBoolSet( pField ) )
			return true;
	}
	return false;

}

void CParticleCollection::LabelTextureUsage( void )
{
	if ( m_pDef )
	{
		m_bUsesPowerOfTwoFrameBufferTexture = m_pDef->UsesPowerOfTwoFrameBufferTexture();
		m_bUsesFullFrameBufferTexture = m_pDef->UsesFullFrameBufferTexture();
	}

	for( CParticleCollection *p = m_Children.head(); p; p = p->m_pNext )
	{
		p->LabelTextureUsage();
	}
}

bool CParticleCollection::ComputeUsesPowerOfTwoFrameBufferTexture()
{
	if ( !m_pDef )
		return false;

	if ( m_pDef->UsesPowerOfTwoFrameBufferTexture() )
		return true;

	for( CParticleCollection *p = m_Children.head(); p; p = p->m_pNext )
	{
		if ( p->UsesPowerOfTwoFrameBufferTexture( false ) )
			return true;
	}

	return false;
}

bool CParticleCollection::ComputeUsesFullFrameBufferTexture()
{
	if ( !m_pDef )
		return false;

	if ( m_pDef->UsesFullFrameBufferTexture() )
		return true;

	for( CParticleCollection *p = m_Children.head(); p; p = p->m_pNext )
	{
		if ( p->UsesFullFrameBufferTexture( false ) )
			return true;
	}

	return false;
}
//-----------------------------------------------------------------------------
// Is the particle system two-pass?
//-----------------------------------------------------------------------------
bool CParticleCollection::ContainsOpaqueCollections()
{
	if ( !m_pDef )
		return false;

	IMaterial *pMaterial = m_pDef->GetMaterial();

	if ( pMaterial && ( !m_pDef->GetMaterial()->IsTranslucent() ) )
		return true;
	
	for( CParticleCollection *p = m_Children.head(); p; p = p->m_pNext )
	{
		if ( p->ContainsOpaqueCollections( ) )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Is the particle system two-pass?
//-----------------------------------------------------------------------------
bool CParticleCollection::IsTwoPass() const
{
	return m_bIsTwoPass;
}

bool CParticleCollection::ComputeIsTwoPass()
{
	if ( !ComputeIsTranslucent() )
		return false;

	return ContainsOpaqueCollections();
}


//-----------------------------------------------------------------------------
// Is the particle system translucent
//-----------------------------------------------------------------------------
bool CParticleCollection::IsTranslucent() const
{
	return m_bIsTranslucent;
}

bool CParticleCollection::ComputeIsTranslucent()
{
	if ( !m_pDef )
		return false;

	IMaterial *pMaterial = m_pDef->GetMaterial();

	if ( pMaterial && ( m_pDef->GetMaterial()->IsTranslucent() ) )
		return true;

	for( CParticleCollection *p = m_Children.head(); p; p = p->m_pNext )
	{
		if ( p->IsTranslucent( ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Is the particle system batchable
//-----------------------------------------------------------------------------
bool CParticleCollection::IsBatchable() const
{
	return m_bIsBatchable;
}

bool CParticleCollection::ComputeIsBatchable()
{
	int nRendererCount = GetRendererCount();
	for( int i = 0; i < nRendererCount; i++ )
	{
		if ( !GetRenderer( i )->IsBatchable() )
			return false;
	}

	for( CParticleCollection *p = m_Children.head(); p; p = p->m_pNext )
	{
		if ( !p->IsBatchable() )
			return false;
	}

	return true;
}



//-----------------------------------------------------------------------------
// Is the order of the particles important
//-----------------------------------------------------------------------------
bool CParticleCollection::IsOrderImportant() const
{
	return m_bIsOrderImportant;
}

bool CParticleCollection::ComputeIsOrderImportant()
{
	int nRendererCount = GetRendererCount();
	for( int i = 0; i < nRendererCount; i++ )
	{
		if ( GetRenderer( i )->IsOrderImportant() )
			return true;
	}

	for( CParticleCollection *p = m_Children.head(); p; p = p->m_pNext )
	{
		if ( p->IsOrderImportant() )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Does this system want to run inside its parent's ApplyKillList?
//-----------------------------------------------------------------------------
bool CParticleCollection::ShouldRunForParentApplyKillList() const
{
	return m_bRunForParentApplyKillList;
}

bool CParticleCollection::ComputeRunForParentApplyKillList()
{
	// Only run the system during ApplyKillList if an emitter operator wants to run then
	// (initializers may then run subsequently, but they won't without an emitter!)
	bool bApplyingKillList = true;
	int nEmitterCount = std::ssize( m_pDef->m_Emitters );
	for( int i = 0; i < nEmitterCount; i++ )
	{
		if ( m_pDef->m_Emitters[i].get()->ShouldRun( bApplyingKillList ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Renderer iteration
//-----------------------------------------------------------------------------
int CParticleCollection::GetRendererCount() const
{
	return IsValid() ? std::ssize( m_pDef->m_Renderers ) : 0;
}

CParticleOperatorInstance *CParticleCollection::GetRenderer( int i )
{
	return IsValid() ? m_pDef->m_Renderers[i].get() : nullptr;
}

void *CParticleCollection::GetRendererContext( int i )
{
	return IsValid() ? m_pOperatorContextData + m_pDef->m_nRenderersCtxOffsets[i] : nullptr;
}


//-----------------------------------------------------------------------------
// Visualize operators (for editing/debugging)
//-----------------------------------------------------------------------------
void CParticleCollection::VisualizeOperator( const DmObjectId_t *pOpId )
{
	m_pRenderOp = nullptr;
	if ( !pOpId || !m_pDef )
		return;

	m_pRenderOp = m_pDef->FindOperatorById( FUNCTION_EMITTER, *pOpId );
	if ( !m_pRenderOp )
	{
		m_pRenderOp = m_pDef->FindOperatorById( FUNCTION_INITIALIZER, *pOpId );
		if ( !m_pRenderOp )
		{
			m_pRenderOp = m_pDef->FindOperatorById( FUNCTION_OPERATOR, *pOpId );
		}
	}
}


float FadeInOut( float flFadeInStart, float flFadeInEnd, float flFadeOutStart, float flFadeOutEnd, float flCurTime )
{
	if ( flFadeInStart > flCurTime )						// started yet?
		return 0.0;

	if ( ( flFadeOutEnd > 0. ) && ( flFadeOutEnd < flCurTime ) ) // timed out?
		return 0.;

	// handle out of order cases
	flFadeInEnd = std::max( flFadeInEnd, flFadeInStart );
	flFadeOutStart = std::max( flFadeOutStart, flFadeInEnd );
	flFadeOutEnd = std::max( flFadeOutEnd, flFadeOutStart );

	float flStrength = 1.0;
	if (
		( flFadeInEnd > flCurTime ) &&
		( flFadeInEnd > flFadeInStart ) )
		flStrength = std::min( flStrength, FLerp( 0, 1, flFadeInStart, flFadeInEnd, flCurTime ) );

	if ( ( flCurTime > flFadeOutStart) &&
		 ( flFadeOutEnd > flFadeOutStart) )
		flStrength = std::min( flStrength, FLerp( 0, 1, flFadeOutEnd, flFadeOutStart, flCurTime ) );

	return flStrength;

}

bool CParticleCollection::CheckIfOperatorShouldRun( 
	CParticleOperatorInstance const * pOp ,
	float *pflCurStrength,
	bool bApplyingParentKillList )
{
	if ( !pOp->ShouldRun( bApplyingParentKillList ) )
		return false;

	if ( pOp->m_bStrengthFastPath )
	{
		*pflCurStrength = 1.0;
		return true;
	}
	if ( pOp->m_nOpEndCapState != -1 )
	{
		if ( m_bInEndCap != ( pOp->m_nOpEndCapState == 1 ) )
			return false;
	}
	float flTime=m_flCurTime;
	if ( pOp->m_nOpTimeOffsetSeed )							// allow per-instance-of-particle-system random phase control for operator strength.
	{
		float flOffset = RandomFloat( pOp->m_nOpTimeOffsetSeed, pOp->m_flOpTimeOffsetMin, pOp->m_flOpTimeOffsetMax );
		flTime += flOffset;
		flTime = std::max<std::common_type_t<double, float>>( 0.0, flTime );
	}
	if ( pOp->m_nOpTimeScaleSeed && ( flTime > pOp->m_flOpStartFadeInTime ) )
	{
		float flTimeScalar = 1.0 / std::max<std::common_type_t<double, float>>( .0001, RandomFloat( pOp->m_nOpTimeScaleSeed, pOp->m_flOpTimeScaleMin, pOp->m_flOpTimeScaleMax ) );
		flTime = pOp->m_flOpStartFadeInTime + flTimeScalar * ( flTime - pOp->m_flOpStartFadeInTime );
	}
	if ( pOp->m_flOpFadeOscillatePeriod > 0.0 )
	{
		flTime = std::fmod( m_flCurTime*( 1.0/pOp->m_flOpFadeOscillatePeriod ), 1.0 );
	}

	float flStrength = FadeInOut( pOp->m_flOpStartFadeInTime, pOp->m_flOpEndFadeInTime,
								  pOp->m_flOpStartFadeOutTime, pOp->m_flOpEndFadeOutTime,
								  flTime );
	if ( pOp->m_nOpStrengthScaleSeed )
	{
		float flStrengthMultiplier = RandomFloat( pOp->m_nOpStrengthScaleSeed, pOp->m_flOpStrengthMinScale, pOp->m_flOpStrengthMaxScale );
		flStrength *= std::max<std::common_type_t<double, float>>( 0., flStrength * flStrengthMultiplier );
	}
	*pflCurStrength = flStrength;
	return ( flStrength > 0.0 );
}


void CParticleOperatorInstance::CheckForFastPath( void )
{
	// store away whether this operator has any of the operator modulation params set (most ops dont)
	if (
		( m_flOpStartFadeInTime == 0. ) &&
		( m_flOpEndFadeOutTime == 0. ) &&
		( m_flOpStartFadeOutTime == 0. ) &&
		( m_flOpEndFadeOutTime == 0. ) &&
		( m_flOpTimeOffsetMin == 0 ) &&
		( m_flOpTimeOffsetMax == 0 ) &&
		( m_flOpTimeScaleMin == 1 ) &&
		( m_flOpTimeScaleMax == 1 ) &&
		( m_flOpStrengthMaxScale == 1 ) &&
		( m_flOpStrengthMinScale == 1 ) &&
		( m_nOpEndCapState == -1 ) )
	{
		m_bStrengthFastPath = true;
	}
	else
	{
		m_bStrengthFastPath = false;
	}

}

bool CParticleOperatorInstance::HasAttribute( CParticleCollection *pParticles, int nAttribute ) const
{
	return ( pParticles->m_ParticleAttributes.Stride( nAttribute ) > 0 );
}

KillListItem_t *CParticleOperatorInstance::GetParentKillList( CParticleCollection *pParticles, int &nNumParticlesToKill ) const
{
	if ( pParticles->m_pParent )
	{
		nNumParticlesToKill = pParticles->m_pParent->m_nNumParticlesToKill;
		return pParticles->m_pParent->m_pParticleKillList;
	}
	nNumParticlesToKill = 0;
	return nullptr;
}

#define CHECKSYSTEM( p ) 0

void CParticleCollection::RunRestartedEmitters( void )
{
	// run all emitters once that want to respond to a restart
	if ( m_nParticleFlags & PCFLAGS_FIRST_FRAME )			// in case we aggregated twice before _any_ sim
	{
		SimulateFirstFrame();
		m_nParticleFlags &= ~PCFLAGS_FIRST_FRAME;
	}
	else
	{
		UpdatePrevControlPoints( m_flPreviousDt );				// make sure control points are virgin
	}

	int nEmitterCount = std::ssize( m_pDef->m_Emitters );
	for( int i=0; i < nEmitterCount; i++ )
	{
		int nOldParticleCount = m_nActiveParticles;
		float flEmitStrength = 0;
		if ( CheckIfOperatorShouldRun( m_pDef->m_Emitters[i].get(), &flEmitStrength ) )
		{
			uint32 nInittedMask = m_pDef->m_Emitters[i].get()->Emit( 
				this, flEmitStrength, 
				m_pOperatorContextData + m_pDef->m_nEmittersCtxOffsets[i] );
			if ( nOldParticleCount != m_nActiveParticles )
			{
				// init newly emitted particles
				InitializeNewParticles( nOldParticleCount, m_nActiveParticles - nOldParticleCount, nInittedMask );
				CHECKSYSTEM( this );
			}
		}
	}
	for( CParticleCollection *pChild = m_Children.head(); pChild != nullptr; pChild = pChild->m_pNext )
		pChild->RunRestartedEmitters();
	
}

// rj: this may not be the correct thing to do, to set all of the children to the same renderable.  particles_new may need to set the renderable to themselves instead.
void CParticleCollection::SetRenderable( void *pRenderable )
{
	m_pRenderable = pRenderable;

	for( CParticleCollection *p = m_Children.head(); p; p = p->m_pNext )
	{
		p->SetRenderable( pRenderable );
	}
}

//-----------------------------------------------------------------------------
// Restarts a particle system
//-----------------------------------------------------------------------------
void CParticleCollection::Restart( EParticleRestartMode_t eMode )
{
	// Always reset the framecount for tick rates - this needs to be reset so that systems which are framerate dependent get updated frame counts
	m_nDrawnFrames = 0;
	// if we already have a pending restart, process it now
	if ( m_bPendingRestart )
	{
		RunRestartedEmitters();
		m_bPendingRestart = false;
	}
	if ( eMode == RESTART_RESET_AND_MAKE_SURE_EMITS_HAPPEN )
	{
		m_bPendingRestart = true;
		m_nParticleFlags &= ~PCFLAGS_PREV_CONTROL_POINTS_INITIALIZED;
	}
	int nEmitterCount = std::ssize( m_pDef->m_Emitters );
	for( int i = 0; i < nEmitterCount; i++ )
	{
		m_pDef->m_Emitters[i].get()->Restart( this, m_pOperatorContextData + m_pDef->m_nEmittersCtxOffsets[i] );
	}

	int nInitializerCount = std::ssize( m_pDef->m_Initializers );
	for( int i = 0; i < nInitializerCount; i++ )
	{
		m_pDef->m_Initializers[i].get()->Restart( this, m_pOperatorContextData + m_pDef->m_nInitializersCtxOffsets[i] );
	}

	// Update all children
	for( CParticleCollection *pChild = m_Children.head(); pChild != nullptr; pChild = pChild->m_pNext )
	{
		// Remove any delays from the time (otherwise we're offset by it oddly)
		pChild->Restart( eMode );
	}

}


//-----------------------------------------------------------------------------
// Main entry point for rendering
//-----------------------------------------------------------------------------
void CParticleCollection::Render( int nViewRecursionLevel, IMatRenderContext *pRenderContext, const Vector4D &vecDiffuseModulation, bool bTranslucentOnly, void *pCameraObject )
{
	if ( !IsValid() )
		return;

	if ( !m_Sheet() && !m_bTriedLoadingSheet )
	{
		m_bTriedLoadingSheet = true;
		m_Sheet.Set( g_pParticleSystemMgr->FindOrLoadSheet( m_pDef, true ) );
	}

	m_flNextSleepTime = std::max( m_flNextSleepTime, ( g_pParticleSystemMgr->GetLastSimulationTime() + m_pDef->m_flNoDrawTimeToGoToSleep ));

	if ( m_nActiveParticles != 0 )
	{
		Vector4D vecActualModulation;
		Vector4DMultiply( vecDiffuseModulation, m_pDef->m_vecMaterialModulation, vecActualModulation );
		IMaterial *pMaterial = m_pDef->GetMaterial();

		if ( pMaterial &&
			 ( !bTranslucentOnly || m_pDef->GetMaterial()->IsTranslucent() || ( vecActualModulation[3] != 1.0f ) ) )
		{

			int nCount = std::ssize( m_pDef->m_Renderers );
			for( int i = 0; i < nCount; i++ )
			{
				float flStrength;
				if ( !CheckIfOperatorShouldRun( m_pDef->m_Renderers[i].get(), &flStrength ) )
					continue;

				if ( m_pDef->IsScreenSpaceEffect() )
				{
					pRenderContext->MatrixMode( MATERIAL_VIEW );
					pRenderContext->PushMatrix();
					pRenderContext->LoadIdentity();
					pRenderContext->MatrixMode( MATERIAL_PROJECTION );
					pRenderContext->PushMatrix();
					pRenderContext->LoadIdentity();
					pRenderContext->Ortho( -100, -100, 100, 100, -100, 100 );
					m_pDef->m_Renderers[i].get()->Render( 
						pRenderContext, this, vecActualModulation, m_pOperatorContextData + m_pDef->m_nRenderersCtxOffsets[i], nViewRecursionLevel );
					pRenderContext->MatrixMode( MATERIAL_VIEW );
					pRenderContext->PopMatrix();
					pRenderContext->MatrixMode( MATERIAL_PROJECTION );
					pRenderContext->PopMatrix();
				}
				else
				{
					m_pDef->m_Renderers[i].get()->Render( 
						pRenderContext, this, vecActualModulation, m_pOperatorContextData + m_pDef->m_nRenderersCtxOffsets[i], nViewRecursionLevel );
				}
			}

		}
	}
	
	// let children render
	for( CParticleCollection *p = m_Children.head(); p; p = p->m_pNext )
	{
		p->Render( nViewRecursionLevel, pRenderContext, vecDiffuseModulation, bTranslucentOnly, pCameraObject );
	}

	// Visualize specific ops for debugging/editing
	if ( m_pRenderOp )
	{
		m_pRenderOp->RenderDebug( this );
	}
}

void CParticleCollection::UpdatePrevControlPoints( float dt )
{
	m_flPreviousDt = dt;
	for(int i=0; i <= m_nHighestCP; i++ )
		ControlPoint( i ).m_PrevPosition = ControlPoint( i ).m_Position;
	m_nParticleFlags |= PCFLAGS_PREV_CONTROL_POINTS_INITIALIZED;
}

#define START_OP
#define END_OP

void CParticleCollection::InitializeNewParticles( int nFirstParticle, int nParticleCount, uint32 nInittedMask, bool bApplyingParentKillList )
{
	CVProfScope _( "CParticleCollection::InitializeNewParticles", 0, VPROF_BUDGETGROUP_PARTICLE_SIMULATION, false, BUDGETFLAG_OTHER );

#ifdef _DEBUG
	m_bIsRunningInitializers = true;
#endif

	// now, initialize the attributes of all the new particles
	int nPerParticleAttributeMask = m_nPerParticleInitializedAttributeMask | m_nPerParticleUpdatedAttributeMask;
	int nAttrsLeftToInit = nPerParticleAttributeMask & ~nInittedMask;
	int nInitializerCount = std::ssize( m_pDef->m_Initializers );
	for ( int i = 0; i < nInitializerCount; i++ )
	{
		CParticleOperatorInstance *pOp = m_pDef->m_Initializers[i].get();
		int nInitializerAttrMask = pOp->GetWrittenAttributes();
		if ( ( ( nInitializerAttrMask & nAttrsLeftToInit ) == 0 ) || pOp->InitMultipleOverride() )
			continue;
		if ( !pOp->ShouldRun( bApplyingParentKillList ) )
			continue;

		void *pContext = m_pOperatorContextData + m_pDef->m_nInitializersCtxOffsets[i];
		START_OP;
		if ( m_bIsScrubbable && !pOp->IsScrubSafe() )
		{
			for ( int j = 0; j < nParticleCount; ++j )
			{
				pOp->InitNewParticles( this, nFirstParticle + j, 1, nAttrsLeftToInit, pContext );
			}
		}
		else
		{
			pOp->InitNewParticles( this, nFirstParticle, nParticleCount, nAttrsLeftToInit, pContext );
		}
		END_OP;
		nAttrsLeftToInit &= ~nInitializerAttrMask;
	}

	// always run second tier initializers (modifiers) after first tier - this ensures they don't get stomped.
	for ( int i = 0; i < nInitializerCount; i++ )
	{
		int nInitializerAttrMask = m_pDef->m_Initializers[i].get()->GetWrittenAttributes();
		CParticleOperatorInstance *pOp = m_pDef->m_Initializers[i].get();
		if ( !pOp->InitMultipleOverride() )
			continue;
		if ( !pOp->ShouldRun( bApplyingParentKillList ) )
			continue;

		void *pContext = m_pOperatorContextData + m_pDef->m_nInitializersCtxOffsets[i];
		START_OP;
		if ( m_bIsScrubbable && !pOp->IsScrubSafe() )
		{
			for ( int j = 0; j < nParticleCount; ++j )
			{
				pOp->InitNewParticles( this, nFirstParticle + j, 1, nAttrsLeftToInit, pContext );
			}
		}
		else
		{
			pOp->InitNewParticles( this, nFirstParticle, nParticleCount, nAttrsLeftToInit, pContext );
		}
		END_OP;
		nAttrsLeftToInit &= ~nInitializerAttrMask;
	}

#ifdef _DEBUG
	m_bIsRunningInitializers = false;
#endif

	InitParticleAttributes( nFirstParticle, nParticleCount, nAttrsLeftToInit );

	CopyInitialAttributeValues( nFirstParticle, nParticleCount );
}

void CParticleCollection::SkipToTime( float t )
{
	if ( t > m_flCurTime )
	{
		UpdatePrevControlPoints( t - m_flCurTime );
		m_flCurTime = t;	
		m_fl4CurTime = ReplicateX4( t );
		m_nParticleFlags &= ~PCFLAGS_FIRST_FRAME;
		
		// FIXME: In future, we may have to tell operators, initializers about this too
		int nEmitterCount = std::ssize( m_pDef->m_Emitters );
		int i;
		for( i = 0; i < nEmitterCount; i++ )
		{
			m_pDef->m_Emitters[i].get()->SkipToTime( t, this, m_pOperatorContextData + m_pDef->m_nEmittersCtxOffsets[i] );
		}

		CParticleCollection *pChild;

		// Update all children
		for( i = 0, pChild = m_Children.head(); pChild != nullptr; pChild = pChild->m_pNext, i++ )
		{
			// Remove any delays from the time (otherwise we're offset by it oddly)
			pChild->SkipToTime( t - m_pDef->m_Children[i].m_flDelay );
		}
	}
}


void CParticleCollection::SimulateFirstFrame( )
{
	m_flPrevSimTime = 1.0e23;
	m_flDt = 0.0f;
	m_nDrawnFrames = 0;

	// For the first frame, copy over the initial control points
	if ( ( m_nParticleFlags & PCFLAGS_PREV_CONTROL_POINTS_INITIALIZED ) == 0 )
	{
		UpdatePrevControlPoints( 0.05f );
	}

	m_nOperatorRandomSampleOffset = 0;
	int nCount = std::ssize( m_pDef->m_Operators );
	for( int i = 0; i < nCount; i++ )
	{
		float flStrength = 0;
		CParticleOperatorInstance *pOp = m_pDef->m_Operators[i].get();
		if ( pOp->ShouldRunBeforeEmitters() &&
			 CheckIfOperatorShouldRun( pOp, &flStrength ) )
		{
			pOp->Operate( this, flStrength, m_pOperatorContextData + m_pDef->m_nOperatorsCtxOffsets[i] );
			CHECKSYSTEM( this );
			UpdatePrevControlPoints( 0.05f );
		}
		m_nOperatorRandomSampleOffset += 17;
	}
	
	// first, create initial particles
	int nNumToCreate = std::min( m_pDef->m_nInitialParticles, m_nMaxAllowedParticles );
	if ( nNumToCreate > 0 )
	{
		SetNActiveParticles( nNumToCreate );
		InitializeNewParticles( 0, nNumToCreate, 0 );
		CHECKSYSTEM( this );
	}
}

void CParticleCollection::EmitAndInit( CParticleCollection *pCollection, bool bApplyingParentKillList ) // static
{
	if ( bApplyingParentKillList && !pCollection->ShouldRunForParentApplyKillList() )
		return;

	int nEmitterCount = std::ssize( pCollection->m_pDef->m_Emitters );
	for( int i = 0; i < nEmitterCount; i++ )
	{
		int nOldParticleCount = pCollection->m_nActiveParticles;
		float flEmitStrength = 0;
		if ( pCollection->CheckIfOperatorShouldRun( pCollection->m_pDef->m_Emitters[i].get(), &flEmitStrength, bApplyingParentKillList ) )
		{
			uint32 nInittedMask = pCollection->m_pDef->m_Emitters[i].get()->Emit( 
				pCollection, flEmitStrength, pCollection->m_pOperatorContextData + pCollection->m_pDef->m_nEmittersCtxOffsets[i] );
			if ( nOldParticleCount != pCollection->m_nActiveParticles )
			{
				// init newly emitted particles
				pCollection->InitializeNewParticles( nOldParticleCount, pCollection->m_nActiveParticles - nOldParticleCount, nInittedMask, bApplyingParentKillList );
				CHECKSYSTEM( pCollection );
			}
		}
	}
}


void CParticleCollection::Simulate( float dt )
{
	CVProfScope _( "CParticleCollection::Simulate", 0, VPROF_BUDGETGROUP_PARTICLE_SIMULATION, false, BUDGETFLAG_OTHER );
	if ( ( dt < 0.0f ) || ( m_bFrozen ) )
		return;

	if ( !m_pDef )
		return;

	// Don't do anything until we've hit t == 0
	// This is used for delayed children
	if ( m_flCurTime < 0.0f )
	{
		if ( dt >= 1.0e-22 )
		{
			m_flCurTime += dt;
			m_fl4CurTime = ReplicateX4( m_flCurTime );
			UpdatePrevControlPoints( dt );
		}
		return;
	}

	// run initializers if necessary (once we hit t == 0)
	if ( m_nParticleFlags & PCFLAGS_FIRST_FRAME )
	{
		SimulateFirstFrame();
		m_nParticleFlags &= ~PCFLAGS_FIRST_FRAME;
	}

	else
	{
		// if the system has been Reset, we need to copy the control points to prev control points
		if ( ! ( m_nParticleFlags & PCFLAGS_PREV_CONTROL_POINTS_INITIALIZED ) )
			UpdatePrevControlPoints( m_flPreviousDt );
	}

	if ( dt < 1.0e-22 )
		return;

	m_bPendingRestart = false;


	bool bAttachedKillList = false;
	
	if ( ! HasAttachedKillList() )
	{
		g_pParticleSystemMgr->AttachKillList( this );
		bAttachedKillList = true;
	}
	
	float flRemainingDt = dt;
	float flMaxDT = 0.1;									// default
	if ( m_pDef->m_flMaximumTimeStep > 0.0 )
		flMaxDT = m_pDef->m_flMaximumTimeStep;

	// Limit timestep if needed (prevents short lived particles from being created and destroyed before being rendered.
	//if ( m_pDef->m_flMaximumSimTime != 0.0 && !m_bHasDrawnOnce )
	if ( m_pDef->m_flMaximumSimTime != 0.0 && ( m_nDrawnFrames <= m_pDef->m_nMinimumFrames ) )
	{
		if ( ( flRemainingDt + m_flCurTime ) > m_pDef->m_flMaximumSimTime )
		{
			//if delta+current > checkpoint then delta = checkpoint - current
			flRemainingDt = m_pDef->m_flMaximumSimTime - m_flCurTime;
			flRemainingDt = std::max( m_pDef->m_flMinimumSimTime, flRemainingDt );
		}
		m_nDrawnFrames += 1;
	}

	flRemainingDt = std::min( flRemainingDt, 10 * flMaxDT );	// no more than 10 passes ever

	m_flTargetDrawTime += flRemainingDt;

	if ( ( m_flTargetDrawTime >= m_flPrevSimTime ) && ( m_flTargetDrawTime < m_flCurTime ) )
	{
		// we can skip simulation
		flRemainingDt = 0;
	}

	float flMinTime = m_pDef->m_flMinimumTimeStep;
	bool bSaveOldValuesForInterpolation = false;

	while( flRemainingDt > 0.0 )
	{
		float flDT_ThisStep = flRemainingDt;
		if ( flDT_ThisStep > flMaxDT )
		{
			flDT_ThisStep = flMaxDT;
		}
		else
		{
			if ( flMinTime > flDT_ThisStep )				// can't do lerping if its going to take multiple steps?
			{
				flDT_ThisStep = flMinTime;
				bSaveOldValuesForInterpolation = true;
			}
		}
		flRemainingDt -= flDT_ThisStep;
		if ( m_flDt )
			m_flPreviousDt = m_flDt;
		m_flDt = flDT_ThisStep;
		m_flPrevSimTime = m_flCurTime;
		m_flCurTime += flDT_ThisStep;
		m_fl4CurTime = ReplicateX4( m_flCurTime );
		
		// now, if we are oging to interpolate, copy the current values of all attributes away
		if ( bSaveOldValuesForInterpolation )
		{
			// !! speed - we could copy just the active region.
			if ( ! m_pPreviousAttributeMemory )
			{
				m_pPreviousAttributeMemory = ( uint8 * ) MemAlloc_AllocAlignedUnattributed( m_nAttributeMemorySize, 16 );
				std::memset( m_pPreviousAttributeMemory, 0, m_nAttributeMemorySize );
				// set up the pointers
				m_PreviousFrameAttributes = m_ParticleAttributes;
				for( int i = 0; i < MAX_PARTICLE_ATTRIBUTES; i++ )
				{
					if ( m_ParticleAttributes.Stride( i ) )
					{
						m_PreviousFrameAttributes.m_pAttributes[i] = ( float * ) 
							( GetPrevAttributeMemory() + 
							  ( m_ParticleAttributes.ByteAddress( i ) - GetAttributeMemory() ) );
					}
				}
			}
			CopyParticleAttributesToPreviousAttributes();
		}

#ifdef _DEBUG
		m_bIsRunningOperators = true;
#endif
		
		m_nOperatorRandomSampleOffset = 0;
		int nCount = std::ssize( m_pDef->m_Operators );
		for( int i = 0; i < nCount; i++ )
		{
			float flStrength;
			CParticleOperatorInstance *pOp = m_pDef->m_Operators[i].get();
			if ( pOp->ShouldRunBeforeEmitters() &&
				 CheckIfOperatorShouldRun( pOp, &flStrength ) )
			{
				START_OP;
				pOp->Operate( this, flStrength, m_pOperatorContextData + m_pDef->m_nOperatorsCtxOffsets[i] );
				END_OP;
				CHECKSYSTEM( this );
				if ( m_nNumParticlesToKill )
				{
					ApplyKillList();
				}
				m_nOperatorRandomSampleOffset += 17;
			}
		}
#ifdef _DEBUG
		m_bIsRunningOperators = false;
#endif

		// Run emitters and initializers:
		EmitAndInit( this );

#ifdef _DEBUG
		m_bIsRunningOperators = true;
#endif
		
		m_nOperatorRandomSampleOffset = 0;
		nCount = std::ssize( m_pDef->m_Operators );
		if ( m_nActiveParticles )
			for( int i = 0; i < nCount; i++ )
			{
				float flStrength;
				CParticleOperatorInstance *pOp = m_pDef->m_Operators[i].get();
				if ( (!  pOp->ShouldRunBeforeEmitters() ) &&
					 CheckIfOperatorShouldRun( pOp, &flStrength ) )
				{
					START_OP;
					pOp->Operate( this, flStrength, m_pOperatorContextData + m_pDef->m_nOperatorsCtxOffsets[i] );
					END_OP;
					CHECKSYSTEM( this );
					if ( m_nNumParticlesToKill )
					{
						ApplyKillList();
						if ( ! m_nActiveParticles )
							break;								// don't run any more operators
					}
					m_nOperatorRandomSampleOffset += 17;
				}
			}
#ifdef _DEBUG
		m_bIsRunningOperators = false;
#endif

		nCount = std::ssize( m_pDef->m_Renderers );
		for( int i = 0; i < nCount; i++ )
		{
			CParticleOperatorInstance *pOp = m_pDef->m_Renderers[i].get();
			START_OP;
			pOp->PostSimulate( this, m_pOperatorContextData + m_pDef->m_nRenderersCtxOffsets[i] );
			END_OP;
		}
	}

	// let children simulate
	for( CParticleCollection *i = m_Children.head(); i; i = i->m_pNext )
	{
		LoanKillListTo( i );								// re-use the allocated kill list for the children
		i->Simulate( dt );
		i->m_pParticleKillList = nullptr;
	}
	if ( bAttachedKillList )
		g_pParticleSystemMgr->DetachKillList( this );
	UpdatePrevControlPoints( dt );

	// Bloat the bounding box by bounds around the control point
	BloatBoundsUsingControlPoint();

	// check for freezing
	if ( m_pDef->m_flStopSimulationAfterTime < m_flCurTime )
	{
		m_bFrozen = true;
	}


// FIXME: Is there a way of doing this iteratively?
//	RecomputeBounds();
}


//-----------------------------------------------------------------------------
// Copies the constant attributes into the per-particle attributes
//-----------------------------------------------------------------------------
void CParticleCollection::InitParticleAttributes( int nStartParticle, int nNumParticles, int nAttrsLeftToInit )
{
	if ( nAttrsLeftToInit == 0 )
		return;

	// !! speed!! do sse init here
	for( int i = nStartParticle; i < nStartParticle + nNumParticles; i++ )
	{
		for ( int nAttr = 0; nAttr < MAX_PARTICLE_ATTRIBUTES; ++nAttr )
		{
			if ( ( nAttrsLeftToInit & ( 1 << nAttr ) ) == 0 )
				continue;

			float *pAttrData = GetFloatAttributePtrForWrite( nAttr, i );

			// Special case for particle id
			if ( nAttr == PARTICLE_ATTRIBUTE_PARTICLE_ID )
			{
				*( (int*)pAttrData ) = ( m_nRandomSeed + m_nUniqueParticleId ) & RANDOM_FLOAT_MASK;
				m_nUniqueParticleId++;
				continue;
			}

			// Special case for the creation time mask
			if ( nAttr == PARTICLE_ATTRIBUTE_CREATION_TIME )
			{
				*pAttrData = m_flCurTime;
				continue;
			}

			// If this assertion fails, it means we're writing into constant memory, which is a nono
			contract_assert( m_ParticleAttributes.Stride( nAttr ) != 0 );
			float *pConstantAttr = GetConstantAttributeMemory( nAttr );
			*pAttrData = *pConstantAttr;
			if ( m_ParticleAttributes.Stride( nAttr ) == 12 )
			{
				pAttrData[4] = pConstantAttr[4];
				pAttrData[8] = pConstantAttr[8];
			}
		}
	}
}

void CParticleCollection::CopyInitialAttributeValues( int nStartParticle, int nNumParticles )
{
	// if doinginterpolated sim, update the previous values to be the current ones, otherwise we may interpolate between
	// old values based upon a previous particle that was in this slot.

	if ( m_nPerParticleReadInitialAttributeMask == 0 )
		return;

	// FIXME: Do SSE copy here
	for( int i = nStartParticle; i < nStartParticle + nNumParticles; i++ )
	{
		for ( int nAttr = 0; nAttr < MAX_PARTICLE_ATTRIBUTES; ++nAttr )
		{
			if ( m_nPerParticleReadInitialAttributeMask & (1 << nAttr) )
			{
				const float *pSrcAttribute = GetFloatAttributePtr( nAttr, i );
				float *pDestAttribute = GetInitialFloatAttributePtrForWrite( nAttr, i );
				contract_assert( m_ParticleInitialAttributes.Stride( nAttr ) != 0 );
				contract_assert( m_ParticleAttributes.Stride( nAttr ) == m_ParticleInitialAttributes.Stride( nAttr ) );
				*pDestAttribute = *pSrcAttribute;
				if ( m_ParticleAttributes.Stride( nAttr ) == 12 )
				{
					pDestAttribute[4] = pSrcAttribute[4];
					pDestAttribute[8] = pSrcAttribute[8];
				}
			}
		}
	}
}

void CParticleCollection::CopyParticleAttributesToPreviousAttributes( void ) const
{
	for( int i = 0; i < MAX_PARTICLE_ATTRIBUTES; i++ )
	{
		if ( m_PreviousFrameAttributes.Stride( i ) )
		{
			int nSz = m_nPaddedActiveParticles * 4 * m_PreviousFrameAttributes.Stride( i );
			std::memcpy( m_PreviousFrameAttributes.Address( i ), m_ParticleAttributes.Address( i ), nSz );
		}
	}
}

//-----------------------------------------------------------------------------e
// Computes a random vector inside a sphere
//-----------------------------------------------------------------------------
float CParticleCollection::RandomVectorInUnitSphere( int nRandomSampleId, Vector *pVector )
{
	// Guarantee uniform random distribution within a sphere
	// Graphics gems III contains this algorithm ("Nonuniform random point sets via warping")
	float u = RandomFloat( nRandomSampleId, 0.0001f, 1.0f );
	float v = RandomFloat( nRandomSampleId+1, 0.0001f, 1.0f );
	float w = RandomFloat( nRandomSampleId+2, 0.0001f, 1.0f );

	float flPhi = std::acos( 1 - 2 * u );
	float flTheta = 2 * std::numbers::pi * v;
	float flRadius = std::powf( w, 1.0f / 3.0f );

	float flSinPhi, flCosPhi;
	float flSinTheta, flCosTheta;
	SinCos( flPhi, &flSinPhi, &flCosPhi );
	SinCos( flTheta, &flSinTheta, &flCosTheta );

	pVector->x = flRadius * flSinPhi * flCosTheta;
	pVector->y = flRadius * flSinPhi * flSinTheta;
	pVector->z = flRadius * flCosPhi;
	return flRadius;
}



//-----------------------------------------------------------------------------
// Used to retrieve the position of a control point
// somewhere between m_flCurTime and m_flCurTime - m_fPreviousDT
//-----------------------------------------------------------------------------
void CParticleCollection::GetControlPointAtTime( int nControlPoint, float flTime, Vector *pControlPoint )
{
	contract_assert( m_pDef->ReadsControlPoint( nControlPoint ) );
// 	if ( nControlPoint > GetHighestControlPoint() )
// 	{
// 		DevWarning(2, "Warning : Particle system (%s) using unassigned ControlPoint %d!\n", GetName(), nControlPoint );
// 	}

	float flPrevTime = m_flCurTime - m_flDt;

	// While this assert is valid, the below if statement is a good enough band-aid to make it so that 
	// particles aren't appearing a weird locations.
	//	Assert( flTime + 0.5f >= flPrevTime && flTime <= m_flCurTime );

	if ( flTime < flPrevTime )
	{
		flTime = flPrevTime;
	}

	float deltaTime = ( flTime - flPrevTime );

	if ( m_flDt == 0.0f || ( deltaTime == 0.0f ) )
	{
		VectorCopy( ControlPoint( nControlPoint ).m_Position, *pControlPoint );
		return;
	}

	float t = deltaTime / m_flDt;
	VectorLerp( ControlPoint( nControlPoint ).m_PrevPosition, ControlPoint( nControlPoint ).m_Position, t, *pControlPoint );
	contract_assert( IsFinite(pControlPoint->x) && IsFinite(pControlPoint->y) && IsFinite(pControlPoint->z) );
}

//-----------------------------------------------------------------------------
// Used to retrieve the previous position of a control point
// 
//-----------------------------------------------------------------------------
void CParticleCollection::GetControlPointAtPrevTime( int nControlPoint, Vector *pControlPoint )
{
	contract_assert( m_pDef->ReadsControlPoint( nControlPoint ) );
	*pControlPoint = ControlPoint( nControlPoint ).m_PrevPosition;
}

void CParticleCollection::GetControlPointTransformAtCurrentTime( int nControlPoint, matrix3x4_t *pMat )
{
	contract_assert( m_pDef->ReadsControlPoint( nControlPoint ) );
	const Vector &vecControlPoint = GetControlPointAtCurrentTime( nControlPoint );

	// FIXME: Use quaternion lerp to get control point transform at time
	Vector left;
	VectorMultiply( ControlPoint( nControlPoint ).m_RightVector, -1.0f, left );
	pMat->Init( ControlPoint( nControlPoint ).m_ForwardVector, left, ControlPoint( nControlPoint ).m_UpVector, vecControlPoint );
}

void CParticleCollection::GetControlPointTransformAtCurrentTime( int nControlPoint, VMatrix *pMat )
{
	GetControlPointTransformAtCurrentTime( nControlPoint, &pMat->As3x4() );
	pMat->m[3][0] = pMat->m[3][1] = pMat->m[3][2] = 0.0f; pMat->m[3][3] = 1.0f;
}

void CParticleCollection::GetControlPointOrientationAtTime( int nControlPoint, float flTime, Vector *pForward, Vector *pRight, Vector *pUp )
{
	contract_assert( m_pDef->ReadsControlPoint( nControlPoint ) );

	// FIXME: Use quaternion lerp to get control point transform at time
	*pForward = ControlPoint( nControlPoint ).m_ForwardVector;
	*pRight = ControlPoint( nControlPoint ).m_RightVector;
	*pUp = ControlPoint( nControlPoint ).m_UpVector;
}

void CParticleCollection::GetControlPointTransformAtTime( int nControlPoint, float flTime, matrix3x4_t *pMat )
{
	contract_assert( m_pDef->ReadsControlPoint( nControlPoint ) );
	Vector vecControlPoint;
	GetControlPointAtTime( nControlPoint, flTime, &vecControlPoint );

	// FIXME: Use quaternion lerp to get control point transform at time
	Vector left;
	VectorMultiply( ControlPoint(nControlPoint).m_RightVector, -1.0f, left );
	pMat->Init( ControlPoint( nControlPoint ).m_ForwardVector, left, ControlPoint( nControlPoint ).m_UpVector, vecControlPoint );
}

void CParticleCollection::GetControlPointTransformAtTime( int nControlPoint, float flTime, VMatrix *pMat )
{
	GetControlPointTransformAtTime( nControlPoint, flTime, &pMat->As3x4() );
	pMat->m[3][0] = pMat->m[3][1] = pMat->m[3][2] = 0.0f; pMat->m[3][3] = 1.0f;
}

void CParticleCollection::GetControlPointTransformAtTime( int nControlPoint, float flTime, CParticleSIMDTransformation *pXForm )
{
	contract_assert( m_pDef->ReadsControlPoint( nControlPoint ) );
	Vector vecControlPoint;
	GetControlPointAtTime( nControlPoint, flTime, &vecControlPoint );

	pXForm->m_v4Origin.DuplicateVector( vecControlPoint );
	pXForm->m_v4Fwd.DuplicateVector( ControlPoint( nControlPoint ).m_ForwardVector );
	pXForm->m_v4Up.DuplicateVector( ControlPoint( nControlPoint ).m_UpVector );
	//Vector left;
	//VectorMultiply( ControlPoint(nControlPoint).m_RightVector, -1.0f, left );
	pXForm->m_v4Right.DuplicateVector( ControlPoint( nControlPoint ).m_RightVector );

}

int CParticleCollection::GetHighestControlPoint( void ) const
{
	return m_nHighestCP;
}

//-----------------------------------------------------------------------------
// Returns the render bounds
//-----------------------------------------------------------------------------
void CParticleCollection::GetBounds( Vector *pMin, Vector *pMax )
{
	*pMin = m_MinBounds;
	*pMax = m_MaxBounds;
}

//-----------------------------------------------------------------------------
// Bloat the bounding box by bounds around the control point
//-----------------------------------------------------------------------------
void CParticleCollection::BloatBoundsUsingControlPoint()
{
	// more specifically, some particle systems were using "start" as an input, so it got set as control point 1,
	// so other particle systems had an extra point in their bounding box, that generally remained at the world origin
	RecomputeBounds();

	// Deal with children
	// NOTE: Bounds have been recomputed for children prior to this call in Simulate
	bool bIsValid = m_bBoundsValid;
	Vector vecMins, vecMaxs;
	for( CParticleCollection *i = m_Children.head(); i; i = i->m_pNext )
	{
		// mdonofrio - skip screen space effects, they have CPs at the origin
		if( (i->m_nActiveParticles > 0 ) &&
			(!i->m_pDef->IsScreenSpaceEffect())  )
		{
			i->GetBounds( &vecMins, &vecMaxs );
			VectorMin( m_MinBounds, vecMins, m_MinBounds );
			VectorMax( m_MaxBounds, vecMaxs, m_MaxBounds );
			bIsValid = ( bIsValid || i->m_bBoundsValid );
		}
	}

	m_bBoundsValid = bIsValid;
}


//-----------------------------------------------------------------------------
// Recomputes the bounds
//-----------------------------------------------------------------------------
inline void UpdateBounds( fltx4 &min_, fltx4 &max_, fltx4 &sum_, fltx4 val )
{
	min_ = MinSIMD( min_, val );
	max_ = MaxSIMD( max_, val );
	sum_ = AddSIMD( sum_, val );
}

inline void UpdateBounds( float &min_, float &max_, float &sum_, float val )
{
	min_ = std::min( min_, val );
	max_ = std::max( max_, val );
	sum_ = sum_ + val;
}

void CParticleCollection::RecomputeBounds( void )
{
	// mdonofrio - skip screen space effects (as they have CPs at the origin) as well as those with 0 active particles 
	if( ( m_nActiveParticles == 0 ) ||
		m_pDef->IsScreenSpaceEffect() )
	{
		m_bBoundsValid = false;
		m_MinBounds.Init( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
		m_MaxBounds.Init( -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() );
		m_Center.Init();
		return;
	}

	fltx4 min_x = ReplicateX4(1.0e23);
	fltx4 min_y = min_x;
	fltx4 min_z = min_x;
	fltx4 max_x = ReplicateX4(-1.0e23);
	fltx4 max_y = max_x;
	fltx4 max_z = max_x;

	fltx4 sum_x = Four_Zeros;
	fltx4 sum_y = Four_Zeros;
	fltx4 sum_z = Four_Zeros;

	float flMaxTail = m_pDef->GetMaxTailLength();
	float flOODt = ( m_flDt != 0.0f ) ? ( 1.0f / m_flDt ) : 1.0f;
	fltx4 maxtail = ReplicateX4( flMaxTail );
	fltx4 oodt = ReplicateX4( flOODt );

	std::size_t xyz_stride, prev_stride, trail_stride;
	const fltx4 *xyz   = GetM128AttributePtr( PARTICLE_ATTRIBUTE_XYZ,			&xyz_stride );
	const fltx4 *prev  = GetM128AttributePtr( PARTICLE_ATTRIBUTE_PREV_XYZ,		&prev_stride );
	const fltx4 *trail = GetM128AttributePtr( PARTICLE_ATTRIBUTE_TRAIL_LENGTH,	&trail_stride );

	int ctr = m_nActiveParticles/4;
	bool bHasTail = ( flMaxTail > 0.0f );
	if ( bHasTail )
	{
		while ( ctr-- )
		{
			UpdateBounds( min_x, max_x, sum_x, xyz[0] );
			UpdateBounds( min_y, max_y, sum_y, xyz[1] );
			UpdateBounds( min_z, max_z, sum_z, xyz[2] );

			fltx4 delta_x = SubSIMD( prev[0], xyz[0] );
			fltx4 delta_y = SubSIMD( prev[1], xyz[1] );
			fltx4 delta_z = SubSIMD( prev[2], xyz[2] );

			fltx4 d2_x = MulSIMD( delta_x, delta_x );
			fltx4 d2_y = MulSIMD( delta_y, delta_y );
			fltx4 d2_z = MulSIMD( delta_z, delta_z );

			fltx4 lensq = AddSIMD( d2_z, AddSIMD( d2_y, d2_x ) );
			fltx4 len = MaxSIMD( ReplicateX4( 0.001f ), SqrtSIMD( lensq ) );
			fltx4 invlen = ReciprocalSIMD( len );

			delta_x = MulSIMD( delta_x, invlen );
			delta_y = MulSIMD( delta_y, invlen );
			delta_z = MulSIMD( delta_z, invlen );

			len = MulSIMD( len, trail[0] );
			len = MulSIMD( len, oodt );
			len = MinSIMD( len, maxtail );

			delta_x = MulSIMD( delta_x, len );
			delta_y = MulSIMD( delta_y, len );
			delta_z = MulSIMD( delta_z, len );

			fltx4 tail_x = AddSIMD( xyz[0], delta_x );
			fltx4 tail_y = AddSIMD( xyz[1], delta_y );
			fltx4 tail_z = AddSIMD( xyz[2], delta_z );

			UpdateBounds( min_x, max_x, sum_x, tail_x );
			UpdateBounds( min_y, max_y, sum_y, tail_y );
			UpdateBounds( min_z, max_z, sum_z, tail_z );

			xyz += xyz_stride;
			prev += prev_stride;
			trail += trail_stride;
		}
	}
	else
	{
		while ( ctr-- )
		{
			UpdateBounds( min_x, max_x, sum_x, xyz[0] );
			UpdateBounds( min_y, max_y, sum_y, xyz[1] );
			UpdateBounds( min_z, max_z, sum_z, xyz[2] );

			xyz += xyz_stride;
		}
	}

	m_bBoundsValid = true;
	m_MinBounds.x = std::min( std::min( SubFloat( min_x, 0 ), SubFloat( min_x, 1 ) ), std::min( SubFloat( min_x, 2 ), SubFloat( min_x, 3 ) ) );
	m_MinBounds.y = std::min( std::min( SubFloat( min_y, 0 ), SubFloat( min_y, 1 ) ), std::min( SubFloat( min_y, 2 ), SubFloat( min_y, 3 ) ) );
	m_MinBounds.z = std::min( std::min( SubFloat( min_z, 0 ), SubFloat( min_z, 1 ) ), std::min( SubFloat( min_z, 2 ), SubFloat( min_z, 3 ) ) );
							  
	m_MaxBounds.x = std::max( std::max( SubFloat( max_x, 0 ), SubFloat( max_x, 1 ) ), std::max( SubFloat( max_x, 2 ), SubFloat( max_x, 3 ) ) );
	m_MaxBounds.y = std::max( std::max( SubFloat( max_y, 0 ), SubFloat( max_y, 1 ) ), std::max( SubFloat( max_y, 2 ), SubFloat( max_y, 3 ) ) );
	m_MaxBounds.z = std::max( std::max( SubFloat( max_z, 0 ), SubFloat( max_z, 1 ) ), std::max( SubFloat( max_z, 2 ), SubFloat( max_z, 3 ) ) );

	float fsum_x = SubFloat( sum_x, 0 ) + SubFloat( sum_x, 1 ) + SubFloat( sum_x, 2 ) + SubFloat( sum_x, 3 );
	float fsum_y = SubFloat( sum_y, 0 ) + SubFloat( sum_y, 1 ) + SubFloat( sum_y, 2 ) + SubFloat( sum_y, 3 );
	float fsum_z = SubFloat( sum_z, 0 ) + SubFloat( sum_z, 1 ) + SubFloat( sum_z, 2 ) + SubFloat( sum_z, 3 );

	// now, handle "tail" in a non-sse manner
	for( int i=0; i < ( m_nActiveParticles & 3 ); i++)
	{
		Vector pos( SubFloat( xyz[0], i ), SubFloat( xyz[1], i ), SubFloat( xyz[2], i ) );

		UpdateBounds( m_MinBounds.x, m_MaxBounds.x, fsum_x, pos.x );
		UpdateBounds( m_MinBounds.y, m_MaxBounds.y, fsum_y, pos.y );
		UpdateBounds( m_MinBounds.z, m_MaxBounds.z, fsum_z, pos.z );
		if ( bHasTail )

		if ( flMaxTail > 0.0f )
		{
			Vector pos_prev( SubFloat( prev[0], i ), SubFloat( prev[1], i ), SubFloat( prev[2], i ) );
			Vector dir = pos_prev - pos;
			float len = VectorNormalize( dir );
			len = std::min( std::max( len, 0.001f ) * SubFloat( trail[0], i ) * flOODt, flMaxTail );
			Vector tail = pos + dir * len;

			UpdateBounds( m_MinBounds.x, m_MaxBounds.x, fsum_x, tail.x );
			UpdateBounds( m_MinBounds.y, m_MaxBounds.y, fsum_y, tail.y );
			UpdateBounds( m_MinBounds.z, m_MaxBounds.z, fsum_z, tail.z );
		}
	}

	VectorAdd( m_MinBounds, m_pDef->m_BoundingBoxMin, m_MinBounds );
	VectorAdd( m_MaxBounds, m_pDef->m_BoundingBoxMax, m_MaxBounds );

	// calculate center
	float flOONumParticles = 1.0 / ( ( flMaxTail > 0.0f ) ? 2 * m_nActiveParticles : m_nActiveParticles );
	m_Center.x = flOONumParticles * fsum_x;
	m_Center.y = flOONumParticles * fsum_y;
	m_Center.z = flOONumParticles * fsum_z;
}


//-----------------------------------------------------------------------------
// Is the particle system finished emitting + all its particles are dead?
//-----------------------------------------------------------------------------
bool CParticleCollection::IsFinished( void ) const
{
	if ( !m_pDef )
		return true;
	if ( m_nParticleFlags & PCFLAGS_FIRST_FRAME )
		return false;
	if ( m_nActiveParticles )
		return false;
	if ( m_bDormant ) 
		return false;

	// no particles. See if any emmitters intead to create more particles
	int nEmitterCount = std::ssize( m_pDef->m_Emitters );
	for( int i=0; i < nEmitterCount; i++ )
	{
		if ( m_pDef->m_Emitters[i].get()->MayCreateMoreParticles( this, m_pOperatorContextData+m_pDef->m_nEmittersCtxOffsets[i] ) )
			return false;
	}


	// make sure all children are finished
	CParticleCollection *pChild = m_Children.head();

	for( int i = 0; pChild != nullptr; pChild = pChild->m_pNext, i++ )
	{
		if ( !pChild->IsFinished() && !m_pDef->m_Children[i].m_bEndCap )
			return false;
		// return false if we're currently playing our endcap effect and not finished with it
		if ( m_pDef->m_Children[i].m_bEndCap && !pChild->IsFinished() && m_bInEndCap )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Stop emitting particles
//-----------------------------------------------------------------------------
void CParticleCollection::StopEmission( bool bInfiniteOnly, bool bRemoveAllParticles, bool bWakeOnStop, bool bPlayEndCap )
{
	if ( !m_pDef )
		return;

	// Whenever we call stop emission, we clear out our dormancy. This ensures we
	// get deleted if we're told to stop emission while dormant. SetDormant() ensures
	// dormancy is set to true after stopping out emission.
	m_bDormant = false;
	
	if ( bWakeOnStop )
	{
		// Set next sleep time - an additional fudge factor is added over the normal time
		// so that existing particles have a chance to go away.
		m_flNextSleepTime = std::max( m_flNextSleepTime, ( g_pParticleSystemMgr->GetLastSimulationTime() + 10 ));
	}
		 
	m_bEmissionStopped = true;

	for( int i=0; i < std::ssize( m_pDef->m_Emitters ); i++ )
	{
		m_pDef->m_Emitters[i].get()->StopEmission( this, m_pOperatorContextData + m_pDef->m_nEmittersCtxOffsets[i], bInfiniteOnly );
	}

	if ( bRemoveAllParticles )
	{
		SetNActiveParticles( 0 );
	}

	// Stop our children as well
	if ( bPlayEndCap )
	{
		CParticleCollection *pChild;
		int i;
		m_bInEndCap = true;
		for( i = 0, pChild = m_Children.head(); pChild != nullptr; pChild = pChild->m_pNext, i++ )
		{
			pChild->m_bInEndCap = true;
			if ( m_pDef->m_Children[i].m_bEndCap )
			{
				pChild->m_flCurTime = 0.0f;
				pChild->StartEmission( bInfiniteOnly );
			}
			else
				pChild->StopEmission( bInfiniteOnly, bRemoveAllParticles, bWakeOnStop, bPlayEndCap );
		}
	}
	else
	{
		for( CParticleCollection *p = m_Children.head(); p; p = p->m_pNext )
		{
			p->StopEmission( bInfiniteOnly, bRemoveAllParticles );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Stop emitting particles
//-----------------------------------------------------------------------------
void CParticleCollection::StartEmission( bool bInfiniteOnly )
{
	if ( !m_pDef )
		return;
	
	m_bEmissionStopped = false;

	for( int i=0; i < std::ssize( m_pDef->m_Emitters ); i++ )
	{
		m_pDef->m_Emitters[i].get()->StartEmission( this, m_pOperatorContextData + m_pDef->m_nEmittersCtxOffsets[i], bInfiniteOnly );
	}

	// Start our children as well
	CParticleCollection *pChild = m_Children.head();
	for( int i = 0; pChild != nullptr; pChild = pChild->m_pNext, i++ )
	{
		// Don't start End Cap Effects - these only play when stopping emission.
		if ( !m_pDef->m_Children[i].m_bEndCap )
		{
			pChild->StartEmission( bInfiniteOnly );
		}
	}

	// Set our sleep time to some time in the future so we update again
	m_flNextSleepTime = g_pParticleSystemMgr->GetLastSimulationTime() + m_pDef->m_flNoDrawTimeToGoToSleep;
}

//-----------------------------------------------------------------------------
// Purpose: Dormant particle systems simulate their particles, but don't emit 
//			new ones. Unlike particle systems that have StopEmission() called
//			dormant particle systems don't remove themselves when they're
//			out of particles, assuming they'll return at some point.
//-----------------------------------------------------------------------------
void CParticleCollection::SetDormant( bool bDormant )
{
	// Don't stop or start emission if we are not changing dormancy state
	if ( bDormant == m_bDormant )
		return;

	// If emissions have already been stopped, don't go dormant, we're supposed to be dying.
	if ( m_bEmissionStopped && bDormant )
		return;

	if ( bDormant )
	{
		StopEmission();
		m_bQueuedStartEmission = false;
	}
	else
	{
		//StartEmission();
		m_bQueuedStartEmission = true;						// start emission during next sim step
		// Set our sleep time to some time in the future so we update again
		m_flNextSleepTime = g_pParticleSystemMgr->GetLastSimulationTime() + m_pDef->m_flNoDrawTimeToGoToSleep;
	}

	m_bDormant = bDormant;
}

bool CParticleCollection::IsEmitting() const
{
	return !m_bEmissionStopped;
}

void CParticleAttributeAddressTable::CopyParticleAttributes( int nSrcIndex, int nDestIndex ) const
{
	for( int p = 0; p < std::size( m_pAttributes ); p++ )
	{
		switch( m_nFloatStrides[p] )
		{
			case 4:										// move a float
				m_pAttributes[p][nDestIndex] = m_pAttributes[p][nSrcIndex];
				break;

			case 12:										// move a vec3
			{
				// sse weirdness
				int oldidxsse = 12 * ( nDestIndex >> 2 );
				int oldofs = oldidxsse + ( nDestIndex & 3 );
				int lastidxsse = 12 * ( nSrcIndex >> 2 );
				int lastofs = lastidxsse + ( nSrcIndex & 3 );
				
				m_pAttributes[p][oldofs] = m_pAttributes[p][lastofs];
				m_pAttributes[p][4 + oldofs] = m_pAttributes[p][4 + lastofs];
				m_pAttributes[p][8 + oldofs] = m_pAttributes[p][8 + lastofs];
				break;
			}
		}
	}
}

void CParticleCollection::MoveParticle( int nInitialIndex, int nNewIndex )
{
	// Copy the per-particle attributes
	m_ParticleAttributes.CopyParticleAttributes( nInitialIndex, nNewIndex );
	m_ParticleInitialAttributes.CopyParticleAttributes( nInitialIndex, nNewIndex );
	if ( m_pPreviousAttributeMemory )
	{
		m_PreviousFrameAttributes.CopyParticleAttributes( nInitialIndex, nNewIndex );
	}
}


//-----------------------------------------------------------------------------
// Kill List processing.
//-----------------------------------------------------------------------------


#define MAX_SIMULTANEOUS_KILL_LISTS 16
static std::atomic<int> g_nKillBufferInUse[MAX_SIMULTANEOUS_KILL_LISTS];
static KillListItem_t *g_pKillBuffers[MAX_SIMULTANEOUS_KILL_LISTS];

void CParticleSystemMgr::DetachKillList( CParticleCollection *pParticles )
{
	if ( pParticles->m_pParticleKillList )
	{
		// find which it is
		for(int i=0; i < NELEMS( g_pKillBuffers ); i++)
		{
			if ( g_pKillBuffers[i] == pParticles->m_pParticleKillList )
			{
				pParticles->m_pParticleKillList = nullptr;
				g_nKillBufferInUse[i] = 0;					// no need to interlock
				return;
			}
		}
		contract_assert( 0 );										// how did we get here?
	}
}

void CParticleSystemMgr::AttachKillList( CParticleCollection *pParticles )
{
	// look for a free slot
	for(;;)
	{
		for(int i=0; i < NELEMS( g_nKillBufferInUse ); i++)
		{
			if ( ! g_nKillBufferInUse[i] )					// available?
			{
				// try to take it!
				int nExpected = 0;
				if ( g_nKillBufferInUse[i].compare_exchange_strong( nExpected, 1 ) )
				{
					if ( ! g_pKillBuffers[i] )
					{
						g_pKillBuffers[i] = new KillListItem_t[MAX_PARTICLES_IN_A_SYSTEM];
					}
					pParticles->m_pParticleKillList = g_pKillBuffers[i];
					return;									// done!
				}

			}
		}
		contract_assert( 0 );											// why don't we have enough buffers?
		ThreadSleep();
	}
}


void CParticleCollection::ApplyKillList( void )
{
	// first, kill particles past bounds
	const KillListItem_t *pCurKillListSlot = m_pParticleKillList;
	while( m_nNumParticlesToKill && pCurKillListSlot[ m_nNumParticlesToKill-1 ].nIndex >= (uint)m_nActiveParticles )
	{
		m_nNumParticlesToKill--;
	}
	contract_assert( m_nNumParticlesToKill <= m_nActiveParticles );

	if ( m_nNumParticlesToKill == 0 )
		return;

	// next, run any child system emitter/initializer operators which request the parent's kill list:
	bool bApplyingParentKillList = true;
	for( CParticleCollection *pChild = m_Children.head(); pChild != nullptr; pChild = pChild->m_pNext )
	{
		// TODO: make this more general (there's a bunch of "first frame" and "frame-to-frame" setup that happens in Simulate() which is skipped here)
		EmitAndInit( pChild, bApplyingParentKillList );
	}

	// now, execute kill list (NOTE: this algorithm assumes the particles listed
	// in the kill list are in ascending order - this is checked in KillParticle)
	unsigned int nParticlesActiveNow = m_nActiveParticles;
	int nLeftInKillList = m_nNumParticlesToKill;
	if ( nLeftInKillList == m_nActiveParticles )
	{
		nParticlesActiveNow = 0; // Simply discard all particles
	}
	// TODO: check KILL_LIST_FLAG_DONT_KILL here (take no action for those kill list entries)
	else if ( IsOrderImportant() )
	{
		// shift
		m_pParticleKillList[ nLeftInKillList ].nIndex = m_nActiveParticles;
		for ( int nKilled = 0; nKilled < nLeftInKillList; )
		{
			int nWriteIndex      = m_pParticleKillList[ nKilled ].nIndex - nKilled;
			nKilled++;
			int nNextIndexToKill = m_pParticleKillList[ nKilled ].nIndex - nKilled;
			for ( nWriteIndex; nWriteIndex < nNextIndexToKill; nWriteIndex++ )
			{
				MoveParticle( ( nWriteIndex + nKilled ), nWriteIndex );
			}
		}
		nParticlesActiveNow -= nLeftInKillList;
	}
	else
	{
		while( nLeftInKillList )
		{
			unsigned int nKillIndex = (pCurKillListSlot++)->nIndex;
			nLeftInKillList--;

			// now, we will move a particle from the end to where we are
			// first, we have to find the last particle (which is not in the kill list)
			while ( nLeftInKillList &&
				( pCurKillListSlot[ nLeftInKillList-1 ].nIndex == nParticlesActiveNow-1 ))
			{
				nLeftInKillList--;
				nParticlesActiveNow--;
			}

			// we might be killing the last particle
			if ( nKillIndex == nParticlesActiveNow-1 )
			{
				// killing last one
				nParticlesActiveNow--;
				break;											// we are done
			}

			// move the last particle to this one and chop off the end of the list
			MoveParticle( nParticlesActiveNow-1, nKillIndex );
			nParticlesActiveNow--;
		}
	}

	// set count in system and wipe kill list
	SetNActiveParticles( nParticlesActiveNow );
	m_nNumParticlesToKill = 0;
}

void CParticleCollection::CalculatePathValues( CPathParameters const &PathIn,
											   float flTimeStamp,
											   Vector *pStartPnt,
											   Vector *pMidPnt,
											   Vector *pEndPnt
											   )
{
	Vector StartPnt;
	GetControlPointAtTime( PathIn.m_nStartControlPointNumber, flTimeStamp, &StartPnt );
	Vector EndPnt;
	GetControlPointAtTime( PathIn.m_nEndControlPointNumber, flTimeStamp, &EndPnt );
	
	Vector MidP;
	VectorLerp(StartPnt, EndPnt, PathIn.m_flMidPoint, MidP);

	if ( PathIn.m_nBulgeControl )
	{
		Vector vTarget=(EndPnt-StartPnt);
		float flBulgeScale = 0.0;
		int nCP=PathIn.m_nStartControlPointNumber;
		if ( PathIn.m_nBulgeControl == 2)
			nCP = PathIn.m_nEndControlPointNumber;
		Vector Fwd = ControlPoint( nCP ).m_ForwardVector;
		float len=VectorLength( vTarget);
		if ( len > 1.0e-6 )
		{
			vTarget *= (1.0/len);						// normalize
			flBulgeScale = 1.0-fabs( DotProduct( vTarget, Fwd )); // bulge inversely scaled
		}
		Vector Potential_MidP=Fwd;
		float flOffsetDist = VectorLength( Potential_MidP );
		if ( flOffsetDist > 1.0e-6 )
		{
			Potential_MidP *= (PathIn.m_flBulge*len*flBulgeScale)/flOffsetDist;
			MidP += Potential_MidP;
		}
	}
	else
	{
		Vector RndVector;
		RandomVector( 0, -PathIn.m_flBulge, PathIn.m_flBulge, &RndVector);
		MidP+=RndVector;
	}
	
	*pStartPnt = StartPnt;
	*pMidPnt = MidP;
	*pEndPnt = EndPnt;
}


//-----------------------------------------------------------------------------
//
// Default impelemtation of the query
//
//-----------------------------------------------------------------------------

static IParticleSystemQuery s_DefaultParticleSystemQuery;


//-----------------------------------------------------------------------------
//
// Particle system manager
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CParticleSystemMgr::CParticleSystemMgr()
{
	m_pQuery = &s_DefaultParticleSystemQuery;
	m_bDidInit = false;
	m_bUsingDefaultQuery = true;
	m_bShouldLoadSheets = true;
	m_bAllowPrecache = true;
	m_pParticleSystemDictionary = nullptr;
	m_nNumFramesMeasured = 0;
	m_flLastSimulationTime = 0.0f;
	m_flLastSimulationDuration = 0.0f;
	m_pShadowDepthMaterial = nullptr;

	// Init the attribute table
	InitAttributeTable();
}

CParticleSystemMgr::~CParticleSystemMgr()
{
	FlushAllSheets();
	if ( m_pParticleSystemDictionary )
	{
		delete m_pParticleSystemDictionary;
		m_pParticleSystemDictionary = nullptr;
	}
}


//-----------------------------------------------------------------------------
// Initialize the particle system
//-----------------------------------------------------------------------------
bool CParticleSystemMgr::Init( IParticleSystemQuery *pQuery, bool bAllowPrecache )
{
	if ( g_pMaterialSystem && ( !g_pMaterialSystem->QueryInterface( MATERIAL_SYSTEM_INTERFACE_VERSION ) ) )
	{
		Msg( "CParticleSystemMgr compiled using an old IMaterialSystem\n" );
		return false;
	}

	if ( m_bUsingDefaultQuery && pQuery )
	{
		m_pQuery = pQuery;
		m_bUsingDefaultQuery = false;
	}

	if ( !m_bDidInit )
	{
		m_pParticleSystemDictionary = new CParticleSystemDictionary;
		// NOTE: This is for the editor only
		AddParticleOperator( FUNCTION_CHILDREN, &s_ChildOperatorDefinition );

		if ( g_pMaterialSystem )
		{
			KeyValues *pVMTKeyValues = new KeyValues( "DepthWrite" );
			pVMTKeyValues->SetInt( "$no_fullbright", 1 );
			pVMTKeyValues->SetInt( "$model", 0 );
			pVMTKeyValues->SetInt( "$alphatest", 0 );
			m_pShadowDepthMaterial = g_pMaterialSystem->CreateMaterial( "__particlesDepthWrite", pVMTKeyValues );
		}
		SeedRandSIMD( 12345678 );
		m_bDidInit = true;
	}

	m_bAllowPrecache = bAllowPrecache;

	return true;
}

//-----------------------------------------------------------------------------
void CParticleSystemMgr::Shutdown()
{
	if ( m_pShadowDepthMaterial )
	{
		m_pShadowDepthMaterial->Release();
		m_pShadowDepthMaterial = nullptr;
	}
}

//-----------------------------------------------------------------------------
// Init the attribute table
//-----------------------------------------------------------------------------
void CParticleSystemMgr::InitAttributeTable( void )
{
	// Init the attribute table
#define INITPARTICLE_ATTRIBUTE( name )												\
	{																				\
		int bit = PARTICLE_ATTRIBUTE_##name;										\
		s_AttributeTable[ bit ].nDataType = PARTICLE_ATTRIBUTE_##name##_DATATYPE;	\
		s_AttributeTable[ bit ].pName = #name;										\
	}

	std::memset( s_AttributeTable, 0, sizeof( s_AttributeTable ) );

	INITPARTICLE_ATTRIBUTE( XYZ );
	INITPARTICLE_ATTRIBUTE( LIFE_DURATION );
	INITPARTICLE_ATTRIBUTE( PREV_XYZ );
	INITPARTICLE_ATTRIBUTE( RADIUS );
	INITPARTICLE_ATTRIBUTE( ROTATION );
	INITPARTICLE_ATTRIBUTE( ROTATION_SPEED );
	INITPARTICLE_ATTRIBUTE( TINT_RGB );
	INITPARTICLE_ATTRIBUTE( ALPHA );
	INITPARTICLE_ATTRIBUTE( CREATION_TIME );
	INITPARTICLE_ATTRIBUTE( SEQUENCE_NUMBER );
	INITPARTICLE_ATTRIBUTE( TRAIL_LENGTH );
	INITPARTICLE_ATTRIBUTE( PARTICLE_ID );
	INITPARTICLE_ATTRIBUTE( YAW );
	INITPARTICLE_ATTRIBUTE( SEQUENCE_NUMBER1 );
	INITPARTICLE_ATTRIBUTE( HITBOX_INDEX );
	INITPARTICLE_ATTRIBUTE( HITBOX_RELATIVE_XYZ );
	INITPARTICLE_ATTRIBUTE( ALPHA2 );
	INITPARTICLE_ATTRIBUTE( SCRATCH_VEC );
	INITPARTICLE_ATTRIBUTE( SCRATCH_FLOAT );
	INITPARTICLE_ATTRIBUTE( UNUSED );
	INITPARTICLE_ATTRIBUTE( PITCH );
	INITPARTICLE_ATTRIBUTE( NORMAL );
	INITPARTICLE_ATTRIBUTE( GLOW_RGB );
	INITPARTICLE_ATTRIBUTE( GLOW_ALPHA );

	for ( int i = 0; i < MAX_PARTICLE_ATTRIBUTES; i++ )
	{
		if ( !s_AttributeTable[ i ].pName )
		{
			// The above list of initializers needs updating!
			Warning( "CParticleSystemMgr::InitAttributeTable has an out-of-date attribute list! (element %d not set up)\n", i );
			contract_assert( 0 );
		}
	}
}

//----------------------------------------------------------------------------------
// String -> Attribute mapping
//----------------------------------------------------------------------------------
int CParticleSystemMgr::GetParticleAttributeByName( const char *pName ) const
{
	// TODO: OPTIMIZATION: use Chris's CUtlStringToken class here to speed this up
	for ( int i = 0; i < MAX_PARTICLE_ATTRIBUTES; i++ )
	{
		if ( !V_stricmp( pName, s_AttributeTable[ i ].pName ) )
			return i;
	}
	return -1;
}

//----------------------------------------------------------------------------------
// Attribute -> String mapping
//----------------------------------------------------------------------------------
const char *CParticleSystemMgr::GetParticleAttributeName( int nAttribute ) const
{
	if ( ( nAttribute < 0 ) || ( nAttribute >= MAX_PARTICLE_ATTRIBUTES ) )
	{
		contract_assert( 0 );
		return "unknown";
	}
	return s_AttributeTable[ nAttribute ].pName;
}

//----------------------------------------------------------------------------------
// Get the data type of a given attribute
//----------------------------------------------------------------------------------
EAttributeDataType CParticleSystemMgr::GetParticleAttributeDataType( int nAttribute ) const
{
	contract_assert( nAttribute >= 0 );
	contract_assert( nAttribute < MAX_PARTICLE_ATTRIBUTES );
	return s_AttributeTable[ nAttribute ].nDataType;
}

//----------------------------------------------------------------------------------
// Cache/uncache materials used by particle systems
//----------------------------------------------------------------------------------
void CParticleSystemMgr::PrecacheParticleSystem( int nStringNumber, const char *pName )
{
	if ( !pName || !pName[0] )
	{
		return;
	}

	ParticleSystemHandle_t hParticleSystem = GetParticleSystemIndex( pName );

	// Used to display an error system if the requested one isn't known from the manifest
	if ( hParticleSystem == INVALID_PARTICLE_SYSTEM_HANDLE )
	{
		Warning( "Attempted to precache unknown particle system \"%s\"!\n", pName );
		hParticleSystem = GetParticleSystemIndex( "error" );
	}

	CParticleSystemDefinition* pDef = FindParticleSystem( hParticleSystem );

	std::vector< ParticleSystemHandle_t > &lookup = ( nStringNumber >= 0 ) ? m_PrecacheLookup : m_ClientPrecacheLookup;
	if ( nStringNumber < 0 )
	{
		nStringNumber = - nStringNumber - 1;
	}
	int nCountToAdd = nStringNumber + 1 - std::ssize( lookup );
	for ( int i = 0; i < nCountToAdd; ++i )
	{
		lookup.push_back( INVALID_PARTICLE_SYSTEM_HANDLE );
	}
	lookup[ nStringNumber ] = hParticleSystem;

	if ( !pDef )
	{
		Warning( "Attempted to precache unknown particle system \"%s\"!\n", pName );
		return;
	}

	pDef->Precache();
}

void CParticleSystemMgr::LevelShutdown( void )
{
//	InvalidateGlobalCollisionCache();						// keep the collision cahce out of the server binary for now
}

void CParticleSystemMgr::UncacheAllParticleSystems()
{
	m_PrecacheLookup.clear();
	m_ClientPrecacheLookup.clear();

	if ( m_pParticleSystemDictionary )
	{
		int nCount = m_pParticleSystemDictionary->Count();
		for ( int i = 0; i < nCount; ++i )
		{
			CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->GetParticleSystem( i );
			if ( pDef )
			{
				pDef->Uncache();
			}
		}

		nCount = m_pParticleSystemDictionary->NameCount();
		for ( ParticleSystemHandle_t h = 0; h < nCount; ++h )
		{
			CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( h );
			if ( pDef )
			{
				pDef->Uncache();
			}
		}
	}

	// Flush sheets, as they can accumulate several MB of memory per map
	FlushAllSheets();
}


//-----------------------------------------------------------------------------
// return the particle field name
//-----------------------------------------------------------------------------
static const char *s_pParticleFieldNames[MAX_PARTICLE_ATTRIBUTES] = 
{
	"Position",			// XYZ, 0
	"Life Duration",	// LIFE_DURATION, 1 );
	"Position Previous",// PREV_XYZ 
	"Radius",			// RADIUS, 3 );

	"Roll",				// ROTATION, 4 );
	"Roll Speed",		// ROTATION_SPEED, 5 );
	"Color",			// TINT_RGB, 6 );
	"Alpha",			// ALPHA, 7 );

	"Creation Time",	// CREATION_TIME, 8 );
	"Sequence Number",	// SEQUENCE_NUMBER, 9 );
	"Trail Length",		// TRAIL_LENGTH, 10 );
	"Particle ID",		// PARTICLE_ID, 11 ); 

	"Yaw",				// YAW, 12 );
	"Sequence Number 1",// SEQUENCE_NUMBER1, 13 );
	"Hitbox Index",		// HITBOX_INDEX, 14
	"Hitbox Offset Position",	// HITBOX_XYZ_RELATIVE 15

	"Alpha Alternate",	// ALPHA2, 16
	"Scratch Vector",	// SCRATCH_VEC 17
	"Scratch Float",	// SCRATCH_FLOAT 18
	nullptr,

	"Pitch",			// PITCH, 20
	"Normal",			// NORMAL, 21
	"Glow RGB",			//GLOW_RGB,22
	"Glow Alpha",		//GLOW_ALPHA,23
};

const char* CParticleSystemMgr::GetParticleFieldName( int nParticleField ) const
{
	return s_pParticleFieldNames[nParticleField];
}

//-----------------------------------------------------------------------------
// Returns the available particle operators
//-----------------------------------------------------------------------------
void CParticleSystemMgr::AddParticleOperator( ParticleFunctionType_t nOpType,
											 IParticleOperatorDefinition *pOpFactory )
{
	m_ParticleOperators[nOpType].push_back( pOpFactory );
}


static const char *s_pFilterNames[ ] = 
{
	"All",
	"Position and Velocity",
	"Life Duration",
	"Parameter Remapping",
	"Rotation",
	"Size",
	"Color and Opacity",
	"Animation Sequence",
	"Hitbox",
	"Normal",
	"Control Points"
};

const char *CParticleSystemMgr::GetFilterName( ParticleFilterType_t nFilterType ) const
{
	static_assert( std::size( s_pFilterNames ) == FILTER_COUNT );
	return s_pFilterNames[nFilterType];
}

std::vector< IParticleOperatorDefinition * > &CParticleSystemMgr::GetAvailableParticleOperatorList( ParticleFunctionType_t nWhichList )
{
	return m_ParticleOperators[nWhichList];
}

const DmxElementUnpackStructure_t *CParticleSystemMgr::GetParticleSystemDefinitionUnpackStructure()
{
	return s_pParticleSystemDefinitionUnpack;
}


//------------------------------------------------------------------------------
// custom allocators for operators so simd aligned
//------------------------------------------------------------------------------
void *CParticleOperatorInstance::operator new( std::size_t nSize )
{
	return MemAlloc_AllocAlignedUnattributed( nSize, 16 );
}

void* CParticleOperatorInstance::operator new( std::size_t nSize, int nBlockUse, const char *pFileName, int nLine )
{
	return MemAlloc_AllocAlignedFileLine( nSize, 16, pFileName, nLine );
}

void CParticleOperatorInstance::operator delete(void *pData)
{
	if ( pData )
	{
		MemAlloc_FreeAligned( pData );
	}
}

void CParticleOperatorInstance::operator delete( void* pData, int nBlockUse, const char *pFileName, int nLine )
{
	if ( pData )
	{
		MemAlloc_FreeAligned( pData );
	}
}


//-----------------------------------------------------------------------------
// Load a PCF file and list the particle systems in it
//-----------------------------------------------------------------------------
void CParticleSystemMgr::GetParticleSystemsInFile( const char *pFileName, std::vector< std::string > *pOutSystemNameList )
{
	if( pOutSystemNameList == nullptr ) 
		return;

	pOutSystemNameList->clear();

	std::optional< std::vector< std::byte > > data = g_pFullFileSystem->ReadFile( pFileName, "GAME" );
	if ( !data )
	{
		return;
	}

	ks::byte_reader buf( *data );
	GetParticleSystemsInBuffer( buf, pOutSystemNameList );
}

void CParticleSystemMgr::GetParticleSystemsInBuffer( ks::byte_reader &buf, std::vector< std::string > *pOutSystemNameList )
{
	if( pOutSystemNameList == nullptr ) 
		return;

	pOutSystemNameList->clear();

	DECLARE_DMX_CONTEXT( );

	CDmxElement *pRoot;
	if ( !UnserializeDMX( buf, &pRoot ) )
	{
		return;
	}

	if ( !V_stricmp( pRoot->GetTypeString(), "DmeParticleSystemDefinition" ) )
	{
		pOutSystemNameList->push_back( pRoot->GetName() );
		CleanupDMX( pRoot );
		return;
	}

	const CDmxAttribute *pDefinitions = pRoot->GetAttribute( "particleSystemDefinitions" );
	if ( !pDefinitions || pDefinitions->GetType() != AT_ELEMENT_ARRAY )
	{
		CleanupDMX( pRoot );
		return;
	}

	const std::vector< CDmxElement* >& definitions = pDefinitions->GetArray<CDmxElement*>( );
	int nCount = std::ssize( definitions );
	for ( int i = 0; i < nCount; ++i )
	{
		pOutSystemNameList->push_back( definitions[i]->GetName() );
	}

	CleanupDMX( pRoot );
}


//-----------------------------------------------------------------------------
// Read the particle config file from a byte reader
//-----------------------------------------------------------------------------
bool CParticleSystemMgr::ReadParticleDefinitions( ks::byte_reader &buf, const char *pFileName, bool bPrecache, bool bDecommitTempMemory )
{
	DECLARE_DMX_CONTEXT_DECOMMIT( bDecommitTempMemory );

	CDmxElement *pRoot;
	if ( !UnserializeDMX( buf, &pRoot, pFileName ) )
	{
		Warning( "Unable to read particle definition %s! It is probably not a binary DMX file!\n", pFileName );
		return false;
	}

	if ( !V_stricmp( pRoot->GetTypeString(), "DmeParticleSystemDefinition" ) )
	{
		CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->AddParticleSystem( pRoot );
		if ( pDef && bPrecache )
		{
			pDef->m_bAlwaysPrecache = true;
			if ( m_bAllowPrecache )
			{
				pDef->Precache();
			}
		}
		CleanupDMX( pRoot );
		return true;
	}

	const CDmxAttribute *pDefinitions = pRoot->GetAttribute( "particleSystemDefinitions" );
	if ( !pDefinitions || pDefinitions->GetType() != AT_ELEMENT_ARRAY )
	{
		CleanupDMX( pRoot );
		return false;
	}

	const std::vector< CDmxElement* >& definitions = pDefinitions->GetArray<CDmxElement*>( );
	int nCount = std::ssize( definitions );
	for ( int i = 0; i < nCount; ++i )
	{
		CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->AddParticleSystem( definitions[i] );
		if ( pDef && bPrecache )
		{
			pDef->m_bAlwaysPrecache = true;
			if ( m_bAllowPrecache )
			{
				pDef->Precache();
			}
		}

	}

	CleanupDMX( pRoot );
	return true;
}


//-----------------------------------------------------------------------------
// Decommits temporary memory
//-----------------------------------------------------------------------------
void CParticleSystemMgr::DecommitTempMemory()
{
	DecommitDMXMemory();
}


//-----------------------------------------------------------------------------
// Sets the last simulation time, used for particle system sleeping logic
//-----------------------------------------------------------------------------
void CParticleSystemMgr::SetLastSimulationTime( float flTime )
{
	m_flLastSimulationTime = flTime;
}

float CParticleSystemMgr::GetLastSimulationTime() const
{
	return m_flLastSimulationTime;
}

//-----------------------------------------------------------------------------
// Sets the last simulation duration ( the amount of time we spent simulating particle ) last frame
// Used to fallback to cheaper particle systems under load
//-----------------------------------------------------------------------------
void CParticleSystemMgr::SetLastSimulationDuration( float flDuration )
{
	m_flLastSimulationDuration = flDuration;
}

float CParticleSystemMgr::GetLastSimulationDuration() const
{
	return m_flLastSimulationDuration;
}

//-----------------------------------------------------------------------------
// GPU/CPU Level
//-----------------------------------------------------------------------------
void CParticleSystemMgr::SetSystemLevel( int nCPULevel, int nGPULevel )
{
	m_nCPULevel = nCPULevel;
	m_nGPULevel = nGPULevel;
}

int CParticleSystemMgr::GetParticleCPULevel() const
{
	return m_nCPULevel;
}

int CParticleSystemMgr::GetParticleGPULevel() const
{
	return m_nGPULevel;
}


//-----------------------------------------------------------------------------
// Fallback paramters
//-----------------------------------------------------------------------------
void CParticleSystemMgr::SetFallbackParameters( float flBase, float flMultiplier, float flSimFallbackBaseMultiplier, float flSimThresholdMs )
{
	m_flFallbackBase = flBase;
	m_flFallbackMultiplier = flMultiplier;
	m_flSimFallbackBaseMultiplier = flSimFallbackBaseMultiplier;
	m_flSimThresholdMs = flSimThresholdMs;
}

float CParticleSystemMgr::GetFallbackBase() const
{
	return m_flFallbackBase;
}

float CParticleSystemMgr::GetFallbackMultiplier() const
{
	return m_flFallbackMultiplier;
}

float CParticleSystemMgr::GetSimFallbackThresholdMs() const
{
	return m_flSimThresholdMs;
}

float CParticleSystemMgr::GetSimFallbackBaseMultiplier() const
{
	return m_flSimFallbackBaseMultiplier;
}

//-----------------------------------------------------------------------------
// Unserialization-related methods
//-----------------------------------------------------------------------------
void CParticleSystemMgr::AddParticleSystem( CDmxElement *pParticleSystem )
{
	m_pParticleSystemDictionary->AddParticleSystem( pParticleSystem );
}

CParticleSystemDefinition* CParticleSystemMgr::FindParticleSystem( const char *pName )
{
	return m_pParticleSystemDictionary->FindParticleSystem( pName );
}

CParticleSystemDefinition* CParticleSystemMgr::FindParticleSystem( const DmObjectId_t& id )
{
	return m_pParticleSystemDictionary->FindParticleSystem( id );
}

CParticleSystemDefinition* CParticleSystemMgr::FindParticleSystem( ParticleSystemHandle_t hParticleSystem )
{
	return m_pParticleSystemDictionary->FindParticleSystem( hParticleSystem );
}

CParticleSystemDefinition* CParticleSystemMgr::FindPrecachedParticleSystem( int nPrecacheIndex )
{
	std::vector< ParticleSystemHandle_t > &lookup = ( nPrecacheIndex >= 0 ) ? m_PrecacheLookup : m_ClientPrecacheLookup;
	if ( nPrecacheIndex < 0 )
	{
		nPrecacheIndex = - nPrecacheIndex - 1;
	}

	if ( nPrecacheIndex >= std::ssize( lookup ) )
		return nullptr;
	return FindParticleSystem( lookup[nPrecacheIndex] );
}

//-----------------------------------------------------------------------------
// Read the particle config file from a byte reader
//-----------------------------------------------------------------------------
bool CParticleSystemMgr::ReadParticleConfigFile( ks::byte_reader &buf, bool bPrecache, bool bDecommitTempMemory, const char *pFileName )
{
	return ReadParticleDefinitions( buf, pFileName, bPrecache, bDecommitTempMemory );
}


//-----------------------------------------------------------------------------
// Read the particle config file from a byte reader
//-----------------------------------------------------------------------------
bool CParticleSystemMgr::ReadParticleConfigFile( const char *pFileName, bool bPrecache, bool bDecommitTempMemory )
{
	// Names starting with a '!' are always precached.
	if ( pFileName[0] == '!' )
	{
		bPrecache = true;
		++pFileName;
	}

	if ( PLATFORM_EXT[0] )
	{
		char szTargetName[MAX_PATH];
		CreatePlatformFilename( pFileName, szTargetName, sizeof( szTargetName ) );

		std::optional< std::vector< std::byte > > fileData = g_pFullFileSystem->ReadFile( szTargetName, "GAME" );
		if ( fileData )
		{
			ks::byte_reader fileBuffer( *fileData );
			return ReadParticleConfigFile( fileBuffer, bPrecache, bDecommitTempMemory, szTargetName );
		}
		else
		{
			// 360/PS3 version should have been there, 360/PS3 zips can only have binary particles
			Warning( "Particles: Missing '%s'\n", szTargetName );
			return false;
		}
	}

//	char pFallbackBuf[MAX_PATH];
	// Look for fallback particle systems
	char pTemp[MAX_PATH];
	V_StripExtension( pFileName, pTemp, sizeof(pTemp) );
	const char *pExt = V_GetFileExtension( pFileName );
	if ( !pExt )
	{
		pExt = "pcf";
	}
	
	/*
	// FIXME: Hook GPU level and/or CPU level into fallbacks instead of dxsupport level
	if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 90 )
	{
		V_snprintf( pFallbackBuf, sizeof(pFallbackBuf), "%s_dx80.%s", pTemp, pExt );
		if ( g_pFullFileSystem->FileExists( pFallbackBuf ) )
		{
			pFileName = pFallbackBuf;
		}
	}
	else if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() == 90 &&  g_pMaterialSystemHardwareConfig->PreferReducedFillrate() )
	{
		V_snprintf( pFallbackBuf, sizeof(pFallbackBuf), "%s_dx90_slow.%s", pTemp, pExt );
		if ( g_pFullFileSystem->FileExists( pFallbackBuf ) )
		{
			pFileName = pFallbackBuf;
		}
	}
	*/

	std::optional< std::vector< std::byte > > data = g_pFullFileSystem->ReadFile( pFileName, "GAME" );
	if ( data )
	{
		ks::byte_reader buf( *data );
		return ReadParticleConfigFile( buf, bPrecache, bDecommitTempMemory, pFileName );
	}

	Warning( "Particles: Missing '%s'\n", pFileName );
	return false;
}


//-----------------------------------------------------------------------------
// Write a specific particle config to a byte writer
//-----------------------------------------------------------------------------
bool CParticleSystemMgr::WriteParticleConfigFile( const char *pParticleSystemName, ks::byte_writer &buf, bool bPreventNameBasedLookup )
{
	DECLARE_DMX_CONTEXT();
	// Create DMX elements representing the particle system definition
	CDmxElement *pParticleSystem = CreateParticleDmxElement( pParticleSystemName );
	return WriteParticleConfigFile( pParticleSystem, buf, bPreventNameBasedLookup );
}

bool CParticleSystemMgr::WriteParticleConfigFile( const DmObjectId_t& id, ks::byte_writer &buf, bool bPreventNameBasedLookup )
{
	DECLARE_DMX_CONTEXT();
	// Create DMX elements representing the particle system definition
	CDmxElement *pParticleSystem = CreateParticleDmxElement( id );
	return WriteParticleConfigFile( pParticleSystem, buf, bPreventNameBasedLookup );
}

bool CParticleSystemMgr::WriteParticleConfigFile( CDmxElement *pParticleSystem, ks::byte_writer &buf, bool bPreventNameBasedLookup )
{
	pParticleSystem->SetValue( "preventNameBasedLookup", bPreventNameBasedLookup );

	CDmxAttribute* pAttribute = pParticleSystem->GetAttribute( "children" );
	const std::vector< CDmxElement* >& children = pAttribute->GetArray<CDmxElement*>( );
	int nCount = std::ssize( children );
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxElement *pChildRef = children[ i ];
		CDmxElement *pChild = pChildRef->GetValue<CDmxElement*>( "child" );
		pChild->SetValue( "preventNameBasedLookup", bPreventNameBasedLookup );
	}

	// Now write the DMX elements out
	bool bOk = SerializeDMX( buf, pParticleSystem );
	CleanupDMX( pParticleSystem );
	return bOk;
}

ParticleSystemHandle_t CParticleSystemMgr::GetParticleSystemIndex( const char *pParticleSystemName )
{
	if ( !pParticleSystemName )
		return INVALID_PARTICLE_SYSTEM_HANDLE;

	return m_pParticleSystemDictionary->FindParticleSystemHandle( pParticleSystemName );
}

ParticleSystemHandle_t CParticleSystemMgr::FindOrAddParticleSystemIndex( const char *pParticleSystemName )
{
	if ( !pParticleSystemName )
		return INVALID_PARTICLE_SYSTEM_HANDLE;

	return m_pParticleSystemDictionary->FindOrAddParticleSystemHandle( pParticleSystemName );
}


const char *CParticleSystemMgr::GetParticleSystemNameFromIndex( ParticleSystemHandle_t iIndex )
{
	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( iIndex );
	return pDef ? pDef->GetName() : "Unknown";
}

int CParticleSystemMgr::GetParticleSystemCount( void )
{
	return m_pParticleSystemDictionary->NameCount();
}


//-----------------------------------------------------------------------------
// Factory method for creating particle collections
//-----------------------------------------------------------------------------
CParticleCollection *CParticleSystemMgr::CreateParticleCollection( const char *pParticleSystemName, float flDelay, int nRandomSeed )
{
	if ( !pParticleSystemName )
		return nullptr;

	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( pParticleSystemName );
	if ( !pDef )
	{
		Warning( "Attempted to create unknown particle system type %s\n", pParticleSystemName );
		return nullptr;
	}
	CParticleCollection *pParticleCollection = new CParticleCollection;
	pParticleCollection->Init( pDef, flDelay, nRandomSeed );
	return pParticleCollection;
}

CParticleCollection *CParticleSystemMgr::CreateParticleCollection( ParticleSystemHandle_t particleSystemName, float flDelay, int nRandomSeed )
{
	if ( particleSystemName == INVALID_PARTICLE_SYSTEM_HANDLE )
		return nullptr;

	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( particleSystemName );
	if ( !pDef )
	{
		Warning( "Attempted to create unknown particle system with unknown symbol\n" );
		return nullptr;
	}
	CParticleCollection *pParticleCollection = new CParticleCollection;
	pParticleCollection->Init( pDef, flDelay, nRandomSeed );
	return pParticleCollection;
}

CParticleCollection *CParticleSystemMgr::CreateParticleCollection( const DmObjectId_t &id, float flDelay, int nRandomSeed )
{
	if ( !IsUniqueIdValid( id ) )
		return nullptr;

	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( id );
	if ( !pDef )
	{
		char pBuf[256];
		UniqueIdToString( id, pBuf, sizeof(pBuf) );
		Warning( "Attempted to create unknown particle system id %s\n", pBuf );
		return nullptr;
	}
	CParticleCollection *pParticleCollection = new CParticleCollection;
	pParticleCollection->Init( pDef, flDelay, nRandomSeed );
	return pParticleCollection;
}


//--------------------------------------------------------------------------------
// Is a particular particle system defined?
//--------------------------------------------------------------------------------
bool CParticleSystemMgr::IsParticleSystemDefined( const DmObjectId_t &id )
{
	if ( !IsUniqueIdValid( id ) )
		return false;

	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( id );
	return ( pDef != nullptr );
}


//--------------------------------------------------------------------------------
// Is a particular particle system defined?
//--------------------------------------------------------------------------------
bool CParticleSystemMgr::IsParticleSystemDefined( const char *pName )
{
	if ( !pName || !pName[0] )
		return false;

	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( pName );
	return ( pDef != nullptr );
}


//--------------------------------------------------------------------------------
// Particle kill list
//--------------------------------------------------------------------------------


//--------------------------------------------------------------------------------
// Serialization-related methods
//--------------------------------------------------------------------------------
CDmxElement *CParticleSystemMgr::CreateParticleDmxElement( const DmObjectId_t &id )
{
	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( id );

	// Create DMX elements representing the particle system definition
	return pDef->Write( );
}

CDmxElement *CParticleSystemMgr::CreateParticleDmxElement( const char *pParticleSystemName )
{
	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( pParticleSystemName );

	// Create DMX elements representing the particle system definition
	return pDef->Write( );
}


//--------------------------------------------------------------------------------
// Particle sheets
//--------------------------------------------------------------------------------
static unsigned int s_nBaseTextureVarCache = 0;

CSheet *CParticleSystemMgr::FindOrLoadSheet( CParticleSystemDefinition *pDef, bool bTryReloading )
{
	if ( !m_bShouldLoadSheets )
		return nullptr;

	if ( !bTryReloading )
	{
		if ( pDef->IsSheetIndexCached() )
		{
			if ( pDef->GetSheetIndex() < 0 )
				return nullptr;
			return m_SheetList[ pDef->GetSheetIndex() ].get();
		}

		pDef->CacheSheetIndex( -1 );
	}

	IMaterial *pMaterial = pDef->GetMaterial();
	if ( !pMaterial )
		return nullptr;

	IMaterialVar *pVar = pMaterial->FindVarFast( "$basetexture", &s_nBaseTextureVarCache );
	if ( !pVar || !pVar->IsDefined() )
		return nullptr;

	ITexture *pTex = pVar->GetTextureValue();
	if ( !pTex || pTex->IsError() )
		return nullptr;

	CSheet *pNewSheet = nullptr;
	auto [itSheet, bNewSheet] = m_SheetIndex.try_emplace( pTex->GetName(), (int)m_SheetList.size() );
	if ( bNewSheet )
	{
		m_SheetList.emplace_back();
	}
	int sheetName = itSheet->second;
	if ( !bNewSheet && ( !bTryReloading ) )
	{
		// Means the string was already known
		pNewSheet = m_SheetList[ sheetName ].get();
	}
	else
	{
		// get compact sheet representation held by texture
		std::size_t numBytes;
		void const *pSheetData = pTex->GetResourceData( VTF_RSRC_SHEET, &numBytes );
		if ( pSheetData )
		{
			// expand compact sheet into fatter runtime form
			ks::byte_reader bufLoad( pSheetData, numBytes );
			pNewSheet = new CSheet( bufLoad );
		}
		// A reloaded sheet's predecessor may still be in use by live collections: leave it be.
		(void)m_SheetList[ sheetName ].release();
		m_SheetList[ sheetName ].reset( pNewSheet );
	}

	pDef->CacheSheetIndex( sheetName );
	return pNewSheet;
}

void CParticleSystemMgr::FlushAllSheets( void )
{
	m_SheetList.clear();
	m_SheetIndex.clear();
	if ( !m_pParticleSystemDictionary )
		return;

	int nCount = m_pParticleSystemDictionary->Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CParticleSystemDefinition* pDef = m_pParticleSystemDictionary->GetParticleSystem( i );
		pDef->InvalidateSheetIndex();
	}

	nCount = m_pParticleSystemDictionary->NameCount();
	for ( ParticleSystemHandle_t h = 0; h < nCount; ++h )
	{
		CParticleSystemDefinition* pDef = m_pParticleSystemDictionary->FindParticleSystem( h );
		pDef->InvalidateSheetIndex();
	}
}

void CParticleSystemMgr::ShouldLoadSheets( bool bLoadSheets )
{
	// Client loads sheets for rendering, server doesn't need to.
	m_bShouldLoadSheets = bLoadSheets;
}

//-----------------------------------------------------------------------------
// Render cache
//-----------------------------------------------------------------------------
void CParticleSystemMgr::ResetRenderCache( void )
{
	int nCount = std::ssize( m_RenderCache );
	for ( int i = 0; i < nCount; ++i )
	{
		m_RenderCache[i].m_ParticleCollections.clear();
	}
}

void CParticleSystemMgr::AddToRenderCache( CParticleCollection *pParticles )
{
	if ( !pParticles->IsValid() )
		return;

	IMaterial *pMaterial = pParticles->m_pDef->GetMaterial();

	if ( ( !pMaterial) || pMaterial->IsTranslucent() )
		return;

	pParticles->m_flNextSleepTime = std::max( pParticles->m_flNextSleepTime, ( g_pParticleSystemMgr->GetLastSimulationTime() + pParticles->m_pDef->m_flNoDrawTimeToGoToSleep ));
	// Find the current rope list.
	int iRenderCache = 0;
	int nRenderCacheCount = std::ssize( m_RenderCache );
	for ( ; iRenderCache < nRenderCacheCount; ++iRenderCache )
	{
		if ( ( pParticles->m_pDef->GetMaterial() == m_RenderCache[iRenderCache].m_pMaterial ) )
			break;
	}

	// A full rope list should have been generate in CreateRenderCache
	// If we didn't find one, then allocate the mofo.
	if ( iRenderCache == nRenderCacheCount )
	{
		iRenderCache = std::ssize( m_RenderCache ); m_RenderCache.emplace_back();
		m_RenderCache[iRenderCache].m_pMaterial = pParticles->m_pDef->GetMaterial();
	}

	m_RenderCache[iRenderCache].m_ParticleCollections.push_back( pParticles );

	for( CParticleCollection *p = pParticles->m_Children.head(); p; p = p->m_pNext )
	{
		AddToRenderCache( p );
	}
}


void CParticleSystemMgr::BuildBatchList( int iRenderCache, IMatRenderContext *pRenderContext, std::vector< Batch_t >& batches )
{
	batches.clear();

	IMaterial *pMaterial = m_RenderCache[iRenderCache].m_pMaterial;
	int nMaxVertices = pRenderContext->GetMaxVerticesToRender( pMaterial );
	int nMaxIndices = pRenderContext->GetMaxIndicesToRender();

	int nRemainingVertices = nMaxVertices;
	int nRemainingIndices = nMaxIndices;

	int i = std::ssize( batches ); batches.emplace_back();
	Batch_t* pBatch = &batches[i];
	pBatch->m_nVertCount = 0;
	pBatch->m_nIndexCount = 0;

	// Ask each renderer about the # of verts + ints it will draw
	int nCacheCount = std::ssize( m_RenderCache[iRenderCache].m_ParticleCollections );
	for ( int iCache = 0; iCache < nCacheCount; ++iCache )
	{
		CParticleCollection *pParticles = m_RenderCache[iRenderCache].m_ParticleCollections[iCache];
		if ( !pParticles->IsValid() )
			continue;

		int nRenderCount = pParticles->GetRendererCount();
		for ( int j = 0; j < nRenderCount; ++j )
		{
			int nFirstParticle = 0;
			while ( nFirstParticle < pParticles->m_nActiveParticles )
			{
				int i;

				BatchStep_t step;
				step.m_pParticles = pParticles;
				step.m_pRenderer = pParticles->GetRenderer( j );
				step.m_pContext = pParticles->GetRendererContext( j ); 
				step.m_nFirstParticle = nFirstParticle;
				step.m_nParticleCount = step.m_pRenderer->GetParticlesToRender( pParticles, 
					step.m_pContext, nFirstParticle, nRemainingVertices, nRemainingIndices, &step.m_nVertCount, &i );
				nFirstParticle += step.m_nParticleCount;

				if ( step.m_nParticleCount > 0 )
				{
					pBatch->m_nVertCount += step.m_nVertCount;
					pBatch->m_nIndexCount += i;
					pBatch->m_BatchStep.push_back( step );
					contract_assert( pBatch->m_nVertCount <= nMaxVertices && pBatch->m_nIndexCount <= nMaxIndices );
				}
				else
				{
					if ( pBatch->m_nVertCount == 0 )
						break;

					// Not enough room
					contract_assert( pBatch->m_nVertCount > 0 && pBatch->m_nIndexCount > 0 ); 
					int j = std::ssize( batches ); batches.emplace_back();
					pBatch = &batches[j];
					pBatch->m_nVertCount = 0;
					pBatch->m_nIndexCount = 0;
					nRemainingVertices = nMaxVertices;
					nRemainingIndices = nMaxIndices;
				}
			}
		}
	}

	if ( pBatch->m_nVertCount <= 0 || pBatch->m_nIndexCount <= 0 ) 
	{
		ks::swap_remove( batches, std::ssize( batches ) - 1 );
	}
}

void CParticleSystemMgr::DumpParticleList( const char *pNameSubstring /* = NULL */ )
{
	if ( pNameSubstring )
	{
		DevMsg( "New Particle Systems Matching '%s':\n", pNameSubstring );
	}
	else
	{
		DevMsg( "All Particle Systems:\n" );
	}

	for ( int i = 0; i < m_pParticleSystemDictionary->NameCount(); i++ )
	{
		CParticleSystemDefinition *p = ( *m_pParticleSystemDictionary )[ i ];

		if ( !pNameSubstring || V_stristr(p->GetName(),pNameSubstring) )
		{
			for ( CParticleCollection *c = p->FirstCollection(); c; c = c->GetNextCollectionUsingSameDef() )
			{
				Vector min,max,center;
				c->GetBounds( &min, &max );
				center = (min+max)*0.5f;
				DevMsg( "%40s: Age: %6.2f, NumActive: %3d, Bounds Center: (%.2f,%.2f,%.2f) (0x%p)\n", p->GetName(), c->m_flCurTime, c->m_nActiveParticles, center.x, center.y, center.z, c );
			}
		}
	}
}

void CParticleSystemMgr::DumpProfileInformation( void )
{
}

void CParticleSystemMgr::CommitProfileInformation( bool bCommit )
{
}


void CParticleSystemMgr::DrawRenderCache( IMatRenderContext *pRenderContext, bool bShadowDepth )
{
	int nRenderCacheCount = std::ssize( m_RenderCache );
	if ( nRenderCacheCount == 0 )
		return;

	CVProfScope _( "CParticleSystemMgr::DrawRenderCache", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	std::vector< Batch_t > batches; batches.reserve( 8 );

	for ( int iRenderCache = 0; iRenderCache < nRenderCacheCount; ++iRenderCache )
	{
		int nCacheCount = std::ssize( m_RenderCache[iRenderCache].m_ParticleCollections );
		if ( nCacheCount == 0 )
			continue;

		// FIXME: When rendering shadow depth, do it all in 1 batch
		IMaterial *pMaterial = bShadowDepth ? m_pShadowDepthMaterial : m_RenderCache[iRenderCache].m_pMaterial;		

		BuildBatchList( iRenderCache, pRenderContext, batches );
		int nBatchCount = std::ssize( batches );
		if ( nBatchCount == 0 )
			continue;

		pRenderContext->Bind( pMaterial );
		CMeshBuilder meshBuilder;
		IMesh* pMesh = pRenderContext->GetDynamicMesh( );

		for ( int i = 0; i < nBatchCount; ++i )
		{
			const Batch_t& batch = batches[i];
			contract_assert( batch.m_nVertCount > 0 && batch.m_nIndexCount > 0 );
			meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, batch.m_nVertCount, batch.m_nIndexCount );

			int nVertexOffset = 0;
			int nBatchStepCount = std::ssize( batch.m_BatchStep );
			for ( int j = 0; j < nBatchStepCount; ++j )
			{
				const BatchStep_t &step = batch.m_BatchStep[j];
				// FIXME: this will break if it ever calls into C_OP_RenderSprites::Render[TwoSequence]SpriteCard()
				//        (need to protect against that and/or split the meshBuilder batch to support that path here)
				step.m_pRenderer->RenderUnsorted( step.m_pParticles, step.m_pContext, pRenderContext, 
					meshBuilder, nVertexOffset, step.m_nFirstParticle, step.m_nParticleCount );
				nVertexOffset += step.m_nVertCount;
			}

			meshBuilder.End();
			pMesh->Draw();
		}
	}

	ResetRenderCache( );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
}




void CParticleCollection::UpdateHitBoxInfo( int nControlPointNumber, const char *pszHitboxSetName )
{
	CModelHitBoxesInfo &hb = ControlPointHitBox( nControlPointNumber );

	if ( hb.m_flLastUpdateTime == m_flCurTime )
		return;												// up to date

	hb.m_flLastUpdateTime = m_flCurTime;

	// make sure space allocated
	if ( ! hb.m_pHitBoxes )
		hb.m_pHitBoxes = new ModelHitBoxInfo_t[ MAXSTUDIOBONES ];
	if ( ! hb.m_pPrevBoxes )
		hb.m_pPrevBoxes = new ModelHitBoxInfo_t[ MAXSTUDIOBONES ];

	// save current into prev
	hb.m_nNumPrevHitBoxes = hb.m_nNumHitBoxes;
	hb.m_flPrevLastUpdateTime = hb.m_flLastUpdateTime;
	V_swap( hb.m_pHitBoxes, hb.m_pPrevBoxes );

	// issue hitbox query
	hb.m_nNumHitBoxes = g_pParticleSystemMgr->Query()->GetControllingObjectHitBoxInfo(
		this, nControlPointNumber, MAXSTUDIOBONES, hb.m_pHitBoxes, pszHitboxSetName );

}

void GetParticleManifest( std::vector< std::string >& list )
{
	GetParticleManifest( list, "particles/particles_manifest.txt" );
}

void GetParticleManifest( std::vector< std::string >& list, const char *pFile )
{
	// Open the manifest file, and read the particles specified inside it
	KeyValues *manifest = new KeyValues( pFile );
	if ( manifest->LoadFromFile( g_pFullFileSystem, pFile, "GAME" ) )
	{
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != nullptr; sub = sub->GetNextKey() )
		{
			if ( !V_stricmp( sub->GetName(), "file" ) )
			{
				list.push_back( sub->GetString() );
				continue;
			}

			Warning( "CParticleMgr::Init:  Manifest '%s' with bogus file type '%s', expecting 'file'\n", pFile, sub->GetName() );
		}
	}
	else
	{
		Warning( "PARTICLE SYSTEM: Unable to load manifest file '%s'\n", pFile );
	}

	manifest->deleteThis();
}
