//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_POSTPROCESSCONTROLLER_H
#define C_POSTPROCESSCONTROLLER_H
#ifdef _WIN32
#pragma once
#endif

#include "postprocess_shared.h"
#include "reflect_annotations.h"

//=============================================================================
//
// Class Postprocess Controller:
//
class [[= ks::reflect::NetTable{ .name = "DT_PostProcessController" } ]]
      [[= ks::reflect::From<"m_PostProcessParameters.m_flParameters",
                            ks::reflect::Net{ .wire = "m_flPostProcessParameters" }>{} ]]
      C_PostProcessController : public C_BaseEntity
{
	DECLARE_CLASS( C_PostProcessController, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();

	C_PostProcessController();
	virtual ~C_PostProcessController();

	virtual void PostDataUpdate( DataUpdateType_t updateType );

	static C_PostProcessController* GetMasterController() { return ms_pMasterController; }

	PostProcessParameters_t	m_PostProcessParameters;
	
private:
	[[= ks::reflect::Net{} ]] bool m_bMaster;

	static C_PostProcessController* ms_pMasterController;
};


#endif // C_POSTPROCESSCONTROLLER_H
