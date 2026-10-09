// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "bone_accessor.h"

bool CBoneAccessor::isBoneAvailableForRead( int iBone ) const
{
	if ( m_pAnimating )
	{
		CStudioHdr *pHdr = m_pAnimating->GetModelPtr();
		if ( pHdr )
		{
			return ( pHdr->boneFlags( iBone ) & m_ReadableBones ) != 0;
		}
	}

	return false;
}

bool CBoneAccessor::isBoneAvailableForWrite( int iBone ) const
{
	if ( m_pAnimating )
	{
		CStudioHdr *pHdr = m_pAnimating->GetModelPtr();
		if ( pHdr )
		{
			// double check consistency
			// !!! DbgAssert( pHdr->pBone( iBone )->flags == pHdr->boneFlags( iBone ) );
			return ( pHdr->boneFlags( iBone ) & m_WritableBones ) != 0;
		}
	}

	return false;
}


#if defined( CLIENT_DLL ) && defined( _DEBUG )

	void CBoneAccessor::SanityCheckBone( int iBone, bool bReadable ) const
	{
		if ( !m_pAnimating )
		{
			return;
		}

		if ( bReadable )
		{
			contract_assert( isBoneAvailableForRead( iBone ) );
		}
		else
		{
			contract_assert( isBoneAvailableForWrite( iBone ) );
		}
	}

#endif
