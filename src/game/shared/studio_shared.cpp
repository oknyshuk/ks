// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
#include "cdll_client_int.h"
#elif defined(GAME_DLL)
#include "shareddefs.h"
#include "util.h"
#endif
#include "studio.h"
#include "engine/ivmodelinfo.h"

////////////////////////////////////////////////////////////////////////
const studiohdr_t *studiohdr_t::FindModel( void **cache, char const *modelname ) const
{
	return modelinfo->FindModel( this, cache, modelname );
}

virtualmodel_t *studiohdr_t::GetVirtualModel( void ) const
{
	if ( numincludemodels == 0 )
		return nullptr;
	return modelinfo->GetVirtualModel( this );
}

const studiohdr_t *virtualgroup_t::GetStudioHdr( ) const
{
	return modelinfo->FindModel( this->cache );
}


byte *studiohdr_t::GetAnimBlock( int iBlock, bool preloadIfMissing ) const
{
	return modelinfo->GetAnimBlock( this, iBlock, preloadIfMissing );
}

bool studiohdr_t::hasAnimBlockBeenPreloaded( int i ) const
{
	return modelinfo->HasAnimBlockBeenPreloaded( this, i );
}

int	studiohdr_t::GetAutoplayList( unsigned short **pOut ) const
{
	return modelinfo->GetAutoplayList( this, pOut );
}
