// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

module;
#include "client_class.h"
#include "declare_class.h"
#include "reflect_annotations.h"
module ks.client:triggers;

import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import :baseentity;
import :basetoggle;
import :particlemgr;
import ks.core.types;

class CBaseEntity;

using namespace ::ks::reflect;

class CBaseTrigger : public CBaseToggle
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseTrigger" } ]]
	[[= ks::reflect::From<"m_spawnflags", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBaseTrigger, CBaseToggle );
	DECLARE_CLIENTCLASS();

public:

	[[= ks::reflect::Net{} ]] bool	m_bClientSidePredicted;
};

class CBaseVPhysicsTrigger : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseVPhysicsTrigger" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBaseVPhysicsTrigger , CBaseEntity );
	DECLARE_CLIENTCLASS();

public:

	//virtual bool PassesTriggerFilters(CBaseEntity *pOther);

protected:
	bool						m_bDisabled;
	string_t					m_iFilterName;
	//CHandle<class C_BaseFilter>	m_hFilter; //CBaseFilter is not networked yet. Only really care about m_bDisabled for this first pass.
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCenterPrint
{
public:
						CCenterPrint( void );

	virtual void		Destroy( void );
	
	virtual void		SetTextColor( int r, int g, int b, int a );
	virtual void		Print( const char *text );
	virtual void		Print( const wchar_t *text );
	virtual void		ColorPrint( int r, int g, int b, int a, char *text );
	virtual void		ColorPrint( int r, int g, int b, int a, wchar_t *text );
	virtual void		Clear( void );
};

extern CCenterPrint *GetCenterPrint();
