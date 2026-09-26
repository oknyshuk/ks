//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include <set>
#include "reflect_datamap.h"
#include "reflect_annotations.h"

#include "ai_goalentity.h"

//-----------------------------------------------------------------------------
//
// CAI_GoalEntity implementation
//

IMPLEMENT_REFLECT_DATAMAP( CAI_GoalEntity )


//-------------------------------------

void CAI_GoalEntity::Spawn()
{
	SetContextThink( &CAI_GoalEntity::DelayedRefresh, gpGlobals->curtime + 0.1f, "Refresh" );
}


//-------------------------------------

void CAI_GoalEntity::OnRestore()
{
	BaseClass::OnRestore();

	ExitDormant();

	if ( ( m_flags & ACTIVE ) )
		gEntList.AddListenerEntity( this );
}

//-------------------------------------

void CAI_GoalEntity::DelayedRefresh()
{
	inputdata_t ignored;
	if ( m_fStartActive )
	{
		Assert( !(m_flags & ACTIVE) );
		InputActivate( ignored );
		m_fStartActive = false;
	}
	else
		InputUpdateActors( ignored );
	
	SetContextThink( nullptr, 0, "Refresh" );
}

//-------------------------------------

void CAI_GoalEntity::PruneActors()
{
	for ( int i = std::ssize( m_actors ) - 1; i >= 0; i-- )
	{
		if ( m_actors[i] == nullptr || m_actors[i]->IsMarkedForDeletion() || m_actors[i]->GetState() == NPC_STATE_DEAD )
			ks::swap_remove( m_actors, i );
	}
}

//-------------------------------------

void CAI_GoalEntity::ResolveNames()
{
	m_actors.assign( 0, {} );
	
	CBaseEntity *pEntity = nullptr;
	for (;;)
	{
		switch ( m_SearchType )
		{
			case ST_ENTNAME:
			{
				pEntity = gEntList.FindEntityByName( pEntity, m_iszActor );
				break;
			}
			
			case ST_CLASSNAME:
			{
				pEntity = gEntList.FindEntityByClassname( pEntity, STRING( m_iszActor ) );
				break;
			}
		}
		
		if ( !pEntity )
			break;
			
		CAI_BaseNPC *pActor = pEntity->MyNPCPointer();
		
		if ( pActor  && pActor->GetState() != NPC_STATE_DEAD )
		{
			AIHANDLE temp;
			temp = pActor;
			m_actors.push_back( temp );
		}
	}
		
	m_hGoalEntity = gEntList.FindEntityByName( nullptr, m_iszGoal );
}

//-------------------------------------

void CAI_GoalEntity::InputActivate( inputdata_t &inputdata )
{
	if ( !( m_flags & ACTIVE ) )
	{
		OnActivate();

		gEntList.AddListenerEntity( this );
		
		UpdateActors();
		m_flags |= ACTIVE;
		
		for ( int i = 0; i < std::ssize( m_actors ); i++ )
		{
			EnableGoal( m_actors[i] );
		}
	}
}

//-------------------------------------

void CAI_GoalEntity::InputUpdateActors( inputdata_t &inputdata )
{
	int i;
	// multiset: m_actors may hold the same NPC more than once
	std::multiset<CAI_BaseNPC *> prevActors;
	
	PruneActors();
	
	for ( i = 0; i < std::ssize( m_actors ); i++ )
	{
		prevActors.insert( m_actors[i] );
	}
	
	ResolveNames();
	
	for ( i = 0; i < std::ssize( m_actors ); i++ )
	{
		auto index = prevActors.find( m_actors[i] );
		if ( index == prevActors.end() )
		{
			if ( m_flags & ACTIVE )
				EnableGoal( m_actors[i] );
		}
		else
			prevActors.erase( index );
	}
	
	for ( CAI_BaseNPC *pPrevActor : prevActors )
	{
		if ( m_flags & ACTIVE )
			DisableGoal( pPrevActor );
	}
}

//-------------------------------------

void CAI_GoalEntity::InputDeactivate( inputdata_t &inputdata ) 	
{
	if ( m_flags & ACTIVE )
	{
		OnDeactivate();

		gEntList.RemoveListenerEntity( this );
		UpdateActors();
		m_flags &= ~ACTIVE;

		for ( int i = 0; i < std::ssize( m_actors ); i++ )
		{
			DisableGoal( m_actors[i] );
		}		
	}
}

//-------------------------------------

void CAI_GoalEntity::EnterDormant( void )
{
	if ( m_flags & ACTIVE )
	{
		m_flags |= DORMANT;
		for ( int i = 0; i < std::ssize( m_actors ); i++ )
		{
			DisableGoal( m_actors[i] );
		}
	}
}

//-------------------------------------

void CAI_GoalEntity::ExitDormant( void )
{
	if ( m_flags & DORMANT )
	{
		m_flags &= ~DORMANT;

		inputdata_t ignored;
		InputUpdateActors( ignored );
	}
}

//-------------------------------------

void CAI_GoalEntity::UpdateOnRemove()
{
	if ( m_flags & ACTIVE )
	{
		inputdata_t inputdata;
		InputDeactivate( inputdata );
	}
	BaseClass::UpdateOnRemove();
}

//-------------------------------------

void CAI_GoalEntity::OnEntityCreated( CBaseEntity *pEntity )
{
	Assert( m_flags & ACTIVE );
	
	if ( pEntity->MyNPCPointer() )
	{
		SetContextThink( &CAI_GoalEntity::DelayedRefresh, gpGlobals->curtime + 0.1f, "Refresh" );
	}
	
}

//-------------------------------------

void CAI_GoalEntity::OnEntityDeleted( CBaseEntity *pEntity )
{
	Assert( pEntity != this );
}

//-----------------------------------------------------------------------------

int CAI_GoalEntity::DrawDebugTextOverlays()
{
	char tempstr[512];
	int offset = BaseClass::DrawDebugTextOverlays();

	Q_snprintf( tempstr, sizeof(tempstr), "Active: %s", IsActive() ? "yes" : "no" );
	EntityText( offset, tempstr, 0 );
	offset++;
		
	return offset;
}


