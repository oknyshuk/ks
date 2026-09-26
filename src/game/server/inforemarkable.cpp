//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: Definition of the info_remarkable entity.
//			This entity is a quick and dirty hack to provide writers 
//			with some kind of object that characters can remark upon.
//			It is not performant, because it relies upon each character
//			polling over each of these for visibility.  A better approach
//			will be an object that is notified by the engine when it is within
//			a character's view.
//
// $NoKeywords: $
//=============================================================================//


#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "inforemarkable.h"
#include "tier1/vector_ops.h"


/// A global list of entities that can be remarked upon.
/// @todo This is an awful, brute force solution. Fix it.
class CRemarkableEntityList : public CAutoGameSystem
{
public:
	CRemarkableEntityList( char const *name ) : CAutoGameSystem( name )
	{}

	virtual void LevelShutdownPostEntity()  
	{ 
		m_list = {};
	}

	void AddEntity( CInfoRemarkable *pItem )
	{
		m_list.push_back( pItem );
	}

	void RemoveEntity( CInfoRemarkable *pItem )
	{
		ks::erase_first( m_list, pItem );
	}
	CInfoRemarkable::tRemarkableList m_list;
};

CRemarkableEntityList g_RemarkableList( "CRemarkableEntityList" );


CInfoRemarkable::tRemarkableList *CInfoRemarkable::GetListOfAllThatIsRemarkable( void )
{
	return &g_RemarkableList.m_list;
}

CInfoRemarkable::~CInfoRemarkable()
{
	g_RemarkableList.RemoveEntity( this );
}

void CInfoRemarkable::Spawn( void )
{
	g_RemarkableList.AddEntity(this);
	m_iTimesRemarkedUpon = 0;
}



//--------------------------------------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( info_remarkable, CInfoRemarkable );


//--------------------------------------------------------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CInfoRemarkable )
