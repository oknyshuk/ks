//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: stores map postprocess params
//
//=============================================================================
#include "cbase.h"
#include "c_postprocesscontroller.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CPostProcessController
#undef CPostProcessController
#endif

IMPLEMENT_REFLECT_CLIENTCLASS( C_PostProcessController, DT_PostProcessController, CPostProcessController )

C_PostProcessController* C_PostProcessController::ms_pMasterController = NULL;

//-----------------------------------------------------------------------------
C_PostProcessController::C_PostProcessController( void )
: 	m_bMaster( false )
{
	if ( ms_pMasterController == NULL )
	{
		ms_pMasterController = this;
	}
}

//-----------------------------------------------------------------------------
C_PostProcessController::~C_PostProcessController( void )
{
	if ( ms_pMasterController == this )
	{
		ms_pMasterController = NULL;
	}
}

void C_PostProcessController::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	if ( m_bMaster )
	{
		ms_pMasterController = this;
	}
}
