//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef C_TRIGGERS_H
#define C_TRIGGERS_H

#include "reflect_annotations.h"
#ifdef _WIN32
#pragma once
#endif

#include "c_basetoggle.h"
#include "triggers_shared.h"

class [[= ks::reflect::NetTable{ .name = "DT_BaseTrigger" } ]]
      [[= ks::reflect::From<"m_spawnflags", ks::reflect::Net{}>{} ]]
      C_BaseTrigger : public C_BaseToggle
{
	DECLARE_CLASS( C_BaseTrigger, C_BaseToggle );
	DECLARE_CLIENTCLASS();

public:

	[[= ks::reflect::Net{} ]] bool	m_bClientSidePredicted;
};

class [[= ks::reflect::NetTable{ .name = "DT_BaseVPhysicsTrigger" } ]]
      C_BaseVPhysicsTrigger : public C_BaseEntity
{
	DECLARE_CLASS( C_BaseVPhysicsTrigger , C_BaseEntity );
	DECLARE_CLIENTCLASS();

public:

	//virtual bool PassesTriggerFilters(C_BaseEntity *pOther);

protected:
	bool						m_bDisabled;
	string_t					m_iFilterName;
	//CHandle<class C_BaseFilter>	m_hFilter; //CBaseFilter is not networked yet. Only really care about m_bDisabled for this first pass.
};

#endif // C_TRIGGERS_H
