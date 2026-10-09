// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation

// Purpose: A single behavior that handles running all of the NPC's add ons

/*

#ifndef 
#ifdef _WIN32
#pragma once
#endif

#include "ai_behavior.h"

class CAI_AddOn;

//=====================================================================================//
//=====================================================================================//
class CAI_AddOnHostBehavior : public CAI_SimpleBehavior
{
	DECLARE_CLASS( CAI_AddOnHostBehavior, CAI_SimpleBehavior );

public:
	CAI_AddOnHostBehavior();
	virtual const char *GetName() {	return "AddOnHost"; }
	virtual void GatherConditions();
	virtual void GatherConditionsNotActive();

private:
	//----------------------------------------------
	// Conditions gathering
	//----------------------------------------------
	void	GatherConditionsCentral();

public:
	//----------------------------------------------
	// AddOn management
	//----------------------------------------------
	void RegisterAddOn( CAI_AddOn *pAddOn );

private:
	std::vector< CHandle<CAI_AddOn> >m_AddOns;

	DECLARE_DATADESC();
};

#endif//
*/
