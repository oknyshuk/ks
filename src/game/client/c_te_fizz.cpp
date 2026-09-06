//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_basetempentity.h"
#include "c_te_legacytempents.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Fizz TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEFizz" } ]]
      C_TEFizz : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEFizz, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TEFizz( void );
	virtual			~C_TEFizz( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] int				m_nEntity;
	[[= ks::reflect::Net{} ]] int				m_nModelIndex;
	[[= ks::reflect::Net{} ]] int				m_nDensity;
	[[= ks::reflect::Net{} ]] int				m_nCurrent;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEFizz::C_TEFizz( void )
{
	m_nEntity		= 0;
	m_nModelIndex	= 0;
	m_nDensity		= 0;
	m_nCurrent		= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEFizz::~C_TEFizz( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_TEFizz::PostDataUpdate( DataUpdateType_t updateType )
{
	VPROF( "C_TEFizz::PostDataUpdate" );

	C_BaseEntity *pEnt = cl_entitylist->GetEnt( m_nEntity );
	if (pEnt != NULL)
	{
		tempents->FizzEffect(pEnt, m_nModelIndex, m_nDensity, m_nCurrent );
	}
}

void TE_Fizz( IRecipientFilter& filter, float delay,
	const C_BaseEntity *ed, int modelindex, int density, int current )
{
	C_BaseEntity *pEnt = (C_BaseEntity *)ed;
	if (pEnt != NULL)
	{
		tempents->FizzEffect(pEnt, modelindex, density, current );
	}
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TEFizz, DT_TEFizz, CTEFizz )


