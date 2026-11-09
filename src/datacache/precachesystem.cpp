// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
#include "tier2/tier2.h"
#include "appframework/apptiers.h"
#include "datacache/iprecachesystem.h"
#include "datacache/iresourceaccesscontrol.h"

//-----------------------------------------------------------------------------
// Implementation class
//-----------------------------------------------------------------------------
class CPrecacheSystem : public IPrecacheSystem
{
	// App system protocol (a tier 2 system)
public:
	bool Connect( CreateInterfaceFn factory ) { return AppSystemTier2Connect( factory ); }
	void Disconnect() { AppSystemTier2Disconnect(); }
	void *QueryInterface( const char *pInterfaceName ) { return nullptr; }
	InitReturnVal_t Init() { return AppSystemTier1Init(); }
	void Shutdown() { AppSystemTier1Shutdown(); }

	// Inherited from IResourceAccessControl
public:
	void Register( CBaseResourcePrecacher *pResourcePrecacherFirst, PrecacheSystem_t nSystem );

	// Precaches/uncaches all resources used by a particular system
	void Cache( PrecacheHandler_t pPrecacheHandler, PrecacheSystem_t nSystem, const char *pName, bool bPrecache, ResourceList_t hResourceList, bool bBuildResourceList );

	void UncacheAll( PrecacheHandler_t pPrecacheHandler );

	// Limits resource access to only resources used by this particular system
	// Use GLOBAL system, and NULL name to disable limited resource access
	void LimitResourceAccess( PrecacheSystem_t nSystem, const char *pName );
	void EndLimitedResourceAccess();

private:
	CBaseResourcePrecacher *m_pFirstPrecacher[PRECACHE_SYSTEM_COUNT];
	ks::idict< ResourceList_t > m_ResourceList[ PRECACHE_SYSTEM_COUNT ];
};

//-----------------------------------------------------------------------------
// String names corresponding to resource types
//-----------------------------------------------------------------------------
static const char *s_pResourceSystemName[] =
{
	"global client resource",
	"global server resource",
	"vgui panel",		
	"dispatch effect",	
	"shared system",
};

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
CPrecacheSystem g_PrecacheSystem;

static InterfaceReg s_Reg_CPrecacheSystem_IPrecacheSystem( []() -> void * { return static_cast<IPrecacheSystem *>( &g_PrecacheSystem ); }, PRECACHE_SYSTEM_INTERFACE_VERSION );;

//-----------------------------------------------------------------------------
// Precaches/uncaches all resources used by a particular system
//-----------------------------------------------------------------------------
void CPrecacheSystem::Cache( PrecacheHandler_t pPrecacheHandler, PrecacheSystem_t nSystem, const char *pName, bool bPrecache, ResourceList_t hResourceList, bool bBuildResourceList )
{	
	static_assert( std::size( s_pResourceSystemName ) == PRECACHE_SYSTEM_COUNT );

	CBaseResourcePrecacher *pPrecacher = m_pFirstPrecacher[nSystem];
	for( ; pPrecacher; pPrecacher = pPrecacher->GetNext() )
	{
		if ( pName && V_stricmp( pName, pPrecacher->GetName() ) )
			continue;
		
		if ( bBuildResourceList && g_pResourceAccessControl )
		{
			contract_assert( hResourceList == RESOURCE_LIST_INVALID );
			// a null name is never stored, so it gets a fresh list each time
			ResourceList_t *pFound = pName ? ks::find_ptr( m_ResourceList[ nSystem ], pName ) : nullptr;
			if ( pFound )
			{
				hResourceList = *pFound;
			}
			else
			{
				char pDebugName[256];
				V_snprintf( pDebugName, sizeof(pDebugName), "%s \"%s\"", s_pResourceSystemName[nSystem], pName );
				hResourceList = g_pResourceAccessControl->CreateResourceList( pDebugName );
				if ( pName )
				{
					m_ResourceList[ nSystem ][ pName ] = hResourceList;
				}
			}
		}
		pPrecacher->Cache( pPrecacheHandler, bPrecache, hResourceList, false );

		if ( !bPrecache && pName )
		{
			m_ResourceList[ nSystem ][ pName ] = nullptr;
		}
	}
}

//-----------------------------------------------------------------------------
// Uncaches everything
//-----------------------------------------------------------------------------
void CPrecacheSystem::UncacheAll( PrecacheHandler_t pPrecacheHandler )
{
	int nSystem;
	for ( nSystem = 0; nSystem < PRECACHE_SYSTEM_COUNT; nSystem ++ )
	{
		CBaseResourcePrecacher *pPrecacher = m_pFirstPrecacher[nSystem];
		for( ; pPrecacher; pPrecacher = pPrecacher->GetNext() )
		{
			pPrecacher->Cache( pPrecacheHandler, false, RESOURCE_LIST_INVALID, false );
		}

		m_ResourceList[nSystem].clear();
	}
}

//-----------------------------------------------------------------------------
// Called to register a list of resource precachers for a given system
//-----------------------------------------------------------------------------
void CPrecacheSystem::Register( CBaseResourcePrecacher *pResourcePrecacherFirst, PrecacheSystem_t nSystem )
{
	// do we already have any precachers registered for this system?
	CBaseResourcePrecacher *pCur = m_pFirstPrecacher[nSystem];

	if ( pCur )
	{
		while ( pCur->GetNext() != nullptr )
		{
			pCur = pCur->GetNext();
		}
		// add the head of the new list to the tail of the existing list
		pCur->SetNext( pResourcePrecacherFirst );
	}
	else
	{
		m_pFirstPrecacher[nSystem] = pResourcePrecacherFirst;
	}	
}

//-----------------------------------------------------------------------------
// Limits resource access to only resources used by this particular system
// Use GLOBAL system, and NULL name to disable limited resource access
//-----------------------------------------------------------------------------
void CPrecacheSystem::LimitResourceAccess( PrecacheSystem_t nSystem, const char *pName )
{
	if ( g_pResourceAccessControl )
	{
		ResourceList_t *pFound = ( pName != nullptr ) ? ks::find_ptr( m_ResourceList[nSystem], pName ) : nullptr;
		if ( pFound )
		{
			g_pResourceAccessControl->LimitAccess( *pFound );
		}
		else
		{
			g_pResourceAccessControl->LimitAccess( RESOURCE_LIST_INVALID );
		}
	}
}


void CPrecacheSystem::EndLimitedResourceAccess()
{
	if ( g_pResourceAccessControl )
	{
		g_pResourceAccessControl->LimitAccess( RESOURCE_LIST_INVALID );
	}
}

//-----------------------------------------------------------------------------
// The IPrecacheSystem members forward to CPrecacheSystem
//-----------------------------------------------------------------------------
#include "facade/iprecachesystem.inl"
