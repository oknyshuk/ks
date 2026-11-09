#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"

#include "shareddefs.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "entityinput.h"
#include "entityoutput.h"
#include "eventqueue.h"

class CLogicRelay : public CLogicalEntity
{
public:
	DECLARE_CLASS( CLogicRelay, CLogicalEntity );

	CLogicRelay();

	void Activate_Impl();
	void Think_Impl();

	// Input handlers
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableRefire", .type = FIELD_VOID } ]] void InputEnableRefire( inputdata_t &inputdata );  // Private input handler, not in FGD
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Trigger", .type = FIELD_VOID } ]] void InputTrigger( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "CancelPending", .type = FIELD_VOID } ]] void InputCancelPending( inputdata_t &inputdata );

	DECLARE_DATADESC();

	// Outputs
	[[= ks::reflect::Key{ .name = "OnTrigger" } ]] COutputEvent m_OnTrigger;
	[[= ks::reflect::Key{ .name = "OnSpawn" } ]] COutputEvent m_OnSpawn;
	
private:

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool m_bDisabled;
	bool m_bWaitForRefire;			// Set to disallow a refire while we are waiting for our outputs to finish firing.
};
