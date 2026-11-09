// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

// Purpose: Definition of the info_remarkable entity.
//			This entity is a quick and dirty hack to provide writers
//			with some kind of object that characters can remark upon.
//			It is not performant, because it relies upon each character
//			polling over each of these for visibility.  A better approach
//			will be an object that is notified by the engine when it is within
//			a character's view.

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
#include "player.h"
#include "recipientfilter.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "inforemarkable.h"

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

void CInfoRemarkable::Spawn_Impl( void )
{
	g_RemarkableList.AddEntity(this);
	m_iTimesRemarkedUpon = 0;
}



//--------------------------------------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( info_remarkable, CInfoRemarkable );


//--------------------------------------------------------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CInfoRemarkable )
