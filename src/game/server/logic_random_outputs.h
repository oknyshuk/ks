//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef LOGICRANDOMOUTPUTS_H
#define LOGICRANDOMOUTPUTS_H

#include "reflect_annotations.h"

#include "cbase.h"
#include "entityinput.h"
#include "entityoutput.h"
#include "eventqueue.h"

#define NUM_RANDOM_OUTPUTS 8

class CLogicRandomOutputs : public CLogicalEntity
{
public:
	DECLARE_CLASS( CLogicRandomOutputs, CLogicalEntity );

	CLogicRandomOutputs();

	void Activate();
	void Think();
	virtual bool KeyValue( const char *szKeyName, const char *szValue );

	// Input handlers
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableRefire", .type = FIELD_VOID } ]] void InputEnableRefire( inputdata_t &inputdata );  // Private input handler, not in FGD
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Trigger", .type = FIELD_VOID } ]] void InputTrigger( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "CancelPending", .type = FIELD_VOID } ]] void InputCancelPending( inputdata_t &inputdata );

	DECLARE_DATADESC();

	// Outputs
	[[= ks::reflect::Key{ .name = "OnTrigger1", .index = 0 } ]] [[= ks::reflect::Key{ .name = "OnTrigger2", .index = 1 } ]] [[= ks::reflect::Key{ .name = "OnTrigger3", .index = 2 } ]] [[= ks::reflect::Key{ .name = "OnTrigger4", .index = 3 } ]] [[= ks::reflect::Key{ .name = "OnTrigger5", .index = 4 } ]] [[= ks::reflect::Key{ .name = "OnTrigger6", .index = 5 } ]] [[= ks::reflect::Key{ .name = "OnTrigger7", .index = 6 } ]] [[= ks::reflect::Key{ .name = "OnTrigger8", .index = 7 } ]] COutputEvent m_Output[ NUM_RANDOM_OUTPUTS ];
	[[= ks::reflect::Key{ .name = "OnSpawn" } ]] COutputEvent m_OnSpawn;

	float m_flOnTriggerChance[ NUM_RANDOM_OUTPUTS ];
	
private:

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool m_bDisabled;
	bool m_bWaitForRefire;			// Set to disallow a refire while we are waiting for our outputs to finish firing.
};

#endif //LOGICRANDOMOUTPUTS_H
