// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: When triggered, will attempt to fire off each of its outputs.  Each output
// has its own chance of firing.

#include "edict.h"
#include "iserverentity.h"
#include "shareddefs.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "baseanimating.h"
#include "reflect_datamap.h"
#include "entityoutput.h"
#include "eventqueue.h"
#include "soundent.h"
#include "logic_random_outputs.h"

const int SF_REMOVE_ON_FIRE				= 0x001;	// Relay will remove itself after being triggered.
const int SF_ALLOW_FAST_RETRIGGER		= 0x002;	// Unless set, entity will disable itself until the last output is sent.

LINK_ENTITY_TO_CLASS(logic_random_outputs, CLogicRandomOutputs);


IMPLEMENT_REFLECT_DATAMAP( CLogicRandomOutputs )



//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CLogicRandomOutputs::CLogicRandomOutputs(void)
{
}

//-----------------------------------------------------------------------------
// Purpose: Read in the chance of firing each output
//-----------------------------------------------------------------------------
bool CLogicRandomOutputs::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if ( szValue && szValue[0] )
	{
		for ( int i=0; i < NUM_RANDOM_OUTPUTS; i++ )
		{
			if ( FStrEq( szKeyName, UTIL_VarArgs( "OnTriggerChance%d", i ) ) )
			{
				m_flOnTriggerChance[i] = std::atof( szValue );
				return true;
			}
		}
	}

	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

//------------------------------------------------------------------------------
// Kickstarts a think if we have OnSpawn connections.
//------------------------------------------------------------------------------
void CLogicRandomOutputs::Activate_Impl()
{
	BaseClass::Activate_Impl();
	
	if ( m_OnSpawn.NumberOfElements() > 0)
	{
		SetNextThink( gpGlobals->curtime + 0.01 );
	}
}


//-----------------------------------------------------------------------------
// If we have OnSpawn connections, this is called shortly after spawning to
// fire the OnSpawn output.
//-----------------------------------------------------------------------------
void CLogicRandomOutputs::Think_Impl()
{
	// Fire an output when we spawn. This is used for self-starting an entity
	// template -- since the logic_random_outputs is inside the template, it gets all the
	// name and I/O connection fixup, so can target other entities in the template.
	m_OnSpawn.FireOutput( this, this );

	// We only get here if we had OnSpawn connections, so this is safe.
	if ( m_spawnflags & SF_REMOVE_ON_FIRE )
	{
		UTIL_Remove(this);
	}
}


//------------------------------------------------------------------------------
// Purpose: Turns on the entity, allowing it to fire outputs.
//------------------------------------------------------------------------------
void CLogicRandomOutputs::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

//------------------------------------------------------------------------------
// Purpose: Enables us to fire again. This input is only posted from our Trigger
//			function to prevent rapid refire.
//------------------------------------------------------------------------------
void CLogicRandomOutputs::InputEnableRefire( inputdata_t &inputdata )
{ 
	Msg(" now enabling refire\n" );
	m_bWaitForRefire = false;
}


//------------------------------------------------------------------------------
// Purpose: Cancels any I/O events in the queue that were fired by us.
//------------------------------------------------------------------------------
void CLogicRandomOutputs::InputCancelPending( inputdata_t &inputdata )
{ 
	g_EventQueue.CancelEvents( this );

	// Stop waiting; allow another Trigger.
	m_bWaitForRefire = false;
}


//------------------------------------------------------------------------------
// Purpose: Turns off the entity, preventing it from firing outputs.
//------------------------------------------------------------------------------
void CLogicRandomOutputs::InputDisable( inputdata_t &inputdata )
{ 
	m_bDisabled = true;
}


//------------------------------------------------------------------------------
// Purpose: Toggles the enabled/disabled state of the entity.
//------------------------------------------------------------------------------
void CLogicRandomOutputs::InputToggle( inputdata_t &inputdata )
{ 
	m_bDisabled = !m_bDisabled;
}


//-----------------------------------------------------------------------------
// Purpose: Input handler that triggers the logic_random_outputs.
//-----------------------------------------------------------------------------
void CLogicRandomOutputs::InputTrigger( inputdata_t &inputdata )
{
	if ((!m_bDisabled) && (!m_bWaitForRefire))
	{
		for ( int i=0 ; i < NUM_RANDOM_OUTPUTS ; i++ )
		{
			if ( RandomFloat() <= m_flOnTriggerChance[i] )
			{
				m_Output[i].FireOutput( inputdata.pActivator, this );
			}
		}
		
		if (m_spawnflags & SF_REMOVE_ON_FIRE)
		{
			UTIL_Remove(this);
		}
		else if (!(m_spawnflags & SF_ALLOW_FAST_RETRIGGER))
		{
			// find the max delay from all our outputs
			float fMaxDelay = 0;
			for ( int i=0 ; i < NUM_RANDOM_OUTPUTS ; i++ )
			{
				fMaxDelay = std::max( fMaxDelay, m_Output[i].GetMaxDelay() );
			}
			if ( fMaxDelay > 0 )
			{
				// Disable the relay so that it cannot be refired until after the last output
				// has been fired and post an input to re-enable ourselves.
				m_bWaitForRefire = true;
				g_EventQueue.AddEvent(this, "EnableRefire", fMaxDelay + 0.001, this, this);
			}
		}
	}
}
