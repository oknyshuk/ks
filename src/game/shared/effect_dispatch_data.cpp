//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#ifdef CLIENT_DLL
#include "reflect_recvtable.h"
#else
#include "reflect_sendtable.h"
#endif
#include "reflect_annotations.h"
#include "effect_dispatch_data.h"
#include "coordsize.h"

#ifdef CLIENT_DLL
#include "cliententitylist.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define SUBINCH_PRECISION	3

// MAX_MODEL_INDEX_BITS moved to effect_dispatch_data.h: an annotation on CEffectData has to be a
// constant expression where the class is declared.

#ifdef CLIENT_DLL

	#include "dt_recv.h"

	// Not static: declared in effect_dispatch_data.h so the class's annotation can name it.
	void RecvProxy_EntIndex( const CRecvProxyData *pData, void *pStruct, void *pOut )
	{
		int nEntIndex = pData->m_Value.m_Int;
		((CEffectData*)pStruct)->m_hEntity = (nEntIndex < 0) ? INVALID_EHANDLE : ClientEntityList().EntIndexToHandle( nEntIndex );
	}

	IMPLEMENT_REFLECT_TABLE( CEffectData, DT_EffectData );

#else

	#include "dt_send.h"

	IMPLEMENT_REFLECT_TABLE( CEffectData, DT_EffectData );
#ifdef HL2_DLL
#else
#endif
#if defined( TF_DLL )
#else
#endif

#endif

#ifdef CLIENT_DLL

IClientRenderable *CEffectData::GetRenderable() const
{
	return ClientEntityList().GetClientRenderableFromHandle( m_hEntity );
}

C_BaseEntity *CEffectData::GetEntity() const
{
	return ClientEntityList().GetBaseEntityFromHandle( m_hEntity );
}

int CEffectData::entindex() const
{
	C_BaseEntity *pEnt = ClientEntityList().GetBaseEntityFromHandle( m_hEntity );
	return pEnt ? pEnt->entindex() : -1;
}

#endif

#ifdef CLIENT_DLL

bool g_bSuppressParticleEffects = false;

bool SuppressingParticleEffects()
{
	return g_bSuppressParticleEffects;
}

void SuppressParticleEffects( bool bSuppress )
{
	g_bSuppressParticleEffects = bSuppress;
}

#endif
