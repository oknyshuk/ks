// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Contains the set of functions for manipulating entity hierarchies.

// UNDONE: Reconcile this with SetParent()

#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "soundflags.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "hierarchy.h"

//-----------------------------------------------------------------------------
// Purpose: Does the linked list work of removing a child object from the hierarchy.
// Input  : pParent - 
//			pChild - 
//-----------------------------------------------------------------------------
void UnlinkChild( CBaseEntity *pParent, CBaseEntity *pChild )
{
	CBaseEntity *pList;
	EHANDLE *pPrev;

	pList = pParent->m_hMoveChild;
	pPrev = &pParent->m_hMoveChild;
	while ( pList )
	{
		CBaseEntity *pNext = pList->m_hMovePeer;
		if ( pList == pChild )
		{
			// patch up the list
			pPrev->Set( pNext );

			// Clear hierarchy bits for this guy
			pList->m_hMoveParent.Set( nullptr );
			pList->m_hMovePeer.Set( nullptr );
			pList->NetworkProp()->SetNetworkParent( INVALID_EHANDLE );
			pList->DispatchUpdateTransmitState();	
			pList->OnEntityEvent( ENTITY_EVENT_PARENT_CHANGED, nullptr );
			
			pParent->RecalcHasPlayerChildBit();
			return;
		}
		else
		{
			pPrev = &pList->m_hMovePeer;
			pList = pNext;
		}
	}

	// This only happens if the child wasn't found in the parent's child list
	contract_assert( 0 );
}

void LinkChild( CBaseEntity *pParent, CBaseEntity *pChild )
{
	EHANDLE hParent;
	hParent.Set( pParent );
	pChild->m_hMovePeer.Set( pParent->FirstMoveChild() );
	pParent->m_hMoveChild.Set( pChild );
	pChild->m_hMoveParent = hParent;
	pChild->NetworkProp()->SetNetworkParent( hParent );
	pChild->DispatchUpdateTransmitState();
	pChild->OnEntityEvent( ENTITY_EVENT_PARENT_CHANGED, nullptr );
	pParent->RecalcHasPlayerChildBit();
}

void TransferChildren( CBaseEntity *pOldParent, CBaseEntity *pNewParent )
{
	CBaseEntity *pChild = pOldParent->FirstMoveChild();
	while ( pChild )
	{
		// NOTE: Have to do this before the unlink to ensure local coords are valid
		Vector vecAbsOrigin = pChild->GetAbsOrigin();
		QAngle angAbsRotation = pChild->GetAbsAngles();
		Vector vecAbsVelocity = pChild->GetAbsVelocity();
//		QAngle vecAbsAngVelocity = pChild->GetAbsAngularVelocity();

		UnlinkChild( pOldParent, pChild );
		LinkChild( pNewParent, pChild );

		// FIXME: This is a hack to guarantee update of the local origin, angles, etc.
		pChild->m_vecAbsOrigin.Init( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
		pChild->m_angAbsRotation.Init( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
		pChild->m_vecAbsVelocity.Init( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );

		pChild->SetAbsOrigin(vecAbsOrigin);
		pChild->SetAbsAngles(angAbsRotation);
		pChild->SetAbsVelocity(vecAbsVelocity);
//		pChild->SetAbsAngularVelocity(vecAbsAngVelocity);

		pChild  = pOldParent->FirstMoveChild();
	}
}

void UnlinkFromParent( CBaseEntity *pRemove )
{
	if ( pRemove->GetMoveParent() )
	{
		// NOTE: Have to do this before the unlink to ensure local coords are valid
		Vector vecAbsOrigin = pRemove->GetAbsOrigin();
		QAngle angAbsRotation = pRemove->GetAbsAngles();
		Vector vecAbsVelocity = pRemove->GetAbsVelocity();
//		QAngle vecAbsAngVelocity = pRemove->GetAbsAngularVelocity();

		UnlinkChild( pRemove->GetMoveParent(), pRemove );

		pRemove->SetLocalOrigin(vecAbsOrigin);
		pRemove->SetLocalAngles(angAbsRotation);
		pRemove->SetLocalVelocity(vecAbsVelocity);

		// objects with physics need to know that they got teleported. Otherwise physics will tell us where to go later.
		IPhysicsObject *pObject = pRemove->VPhysicsGetObject();
		if( pObject )
		{
			pObject->SetPosition( vecAbsOrigin, angAbsRotation, true );
		}

//		pRemove->SetLocalAngularVelocity(vecAbsAngVelocity);
		if ( pRemove->GetMoveType() != MOVETYPE_NONE && pRemove->GetMoveType() != MOVETYPE_VPHYSICS )
		{
			pRemove->UpdateWaterState();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Clears the parent of all the children of the given object.
//-----------------------------------------------------------------------------
void UnlinkAllChildren( CBaseEntity *pParent )
{
	CBaseEntity *pChild = pParent->FirstMoveChild();
	while ( pChild )
	{
		CBaseEntity *pNext = pChild->NextMovePeer();
		UnlinkFromParent( pChild );
		pChild  = pNext;
	}
}

bool EntityIsParentOf( CBaseEntity *pParent, CBaseEntity *pEntity )
{
	while ( pEntity->GetMoveParent() )
	{
		pEntity = pEntity->GetMoveParent();
		if ( pParent == pEntity )
			return true;
	}

	return false;
}

static void GetAllChildren_r( CBaseEntity *pEntity, std::vector< CBaseEntity * > &list )
{
	for ( ; pEntity != nullptr; pEntity = pEntity->NextMovePeer() )
	{
		list.push_back( pEntity );
		GetAllChildren_r( pEntity->FirstMoveChild(), list );
	}
}

int GetAllChildren( CBaseEntity *pParent, std::vector< CBaseEntity * > &list )
{
	if ( !pParent )
		return 0;

	GetAllChildren_r( pParent->FirstMoveChild(), list );
	return std::ssize( list );
}

int	GetAllInHierarchy( CBaseEntity *pParent, std::vector< CBaseEntity * > &list )
{
	if (!pParent)
		return 0;
	list.push_back( pParent );
	return GetAllChildren( pParent, list ) + 1;
}
