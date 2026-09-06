//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ====
//
// Purpose: Implements many of the entities that control logic flow within a map.
//
//=============================================================================

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "entityinput.h"
#include "entityoutput.h"
#include "eventqueue.h"
#include "mathlib/mathlib.h"
#include "globalstate.h"
#include "ndebugoverlay.h"
#include "vstdlib/random.h"
#include "gameinterface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern CServerGameDLL g_ServerGameDLL;

//-----------------------------------------------------------------------------
// Purpose: An entity that acts as a container for game scripts.
//-----------------------------------------------------------------------------

#define MAX_SCRIPT_GROUP 16

class CLogicScript : public CPointEntity
{
public:
	DECLARE_CLASS( CLogicScript, CPointEntity );
	DECLARE_DATADESC();

	void RunVScripts()
	{
		/*
			EntityGroup <- [];
			function __AppendToScriptGroup( name ) 
			{
				if ( name.len() == 0 ) 
				{ 
					EntityGroup.append( null ); 
				} 
				else
				{ 
					local ent = Entities.FindByName( null, name );
					EntityGroup.append( ent );
					if ( ent != 0 )
					{
						ent.ValidateScriptScope();
						ent.GetScriptScope().EntityGroup <- EntityGroup;
					}
				}
			}
		*/

 		static const char szAddCode[] =
		{
			0x45,0x6e,0x74,0x69,0x74,0x79,0x47,0x72,0x6f,0x75,0x70,0x20,0x3c,0x2d,0x20,0x5b,0x5d,0x3b,0x0d,0x0a,
			0x66,0x75,0x6e,0x63,0x74,0x69,0x6f,0x6e,0x20,0x5f,0x5f,0x41,0x70,0x70,0x65,0x6e,0x64,0x54,0x6f,0x53,
			0x63,0x72,0x69,0x70,0x74,0x47,0x72,0x6f,0x75,0x70,0x28,0x20,0x6e,0x61,0x6d,0x65,0x20,0x29,0x20,0x0d,
			0x0a,0x7b,0x0d,0x0a,0x09,0x69,0x66,0x20,0x28,0x20,0x6e,0x61,0x6d,0x65,0x2e,0x6c,0x65,0x6e,0x28,0x29,
			0x20,0x3d,0x3d,0x20,0x30,0x20,0x29,0x20,0x0d,0x0a,0x09,0x7b,0x20,0x0d,0x0a,0x09,0x09,0x45,0x6e,0x74,
			0x69,0x74,0x79,0x47,0x72,0x6f,0x75,0x70,0x2e,0x61,0x70,0x70,0x65,0x6e,0x64,0x28,0x20,0x6e,0x75,0x6c,
			0x6c,0x20,0x29,0x3b,0x20,0x0d,0x0a,0x09,0x7d,0x20,0x0d,0x0a,0x09,0x65,0x6c,0x73,0x65,0x0d,0x0a,0x09,
			0x7b,0x20,0x0d,0x0a,0x09,0x09,0x6c,0x6f,0x63,0x61,0x6c,0x20,0x65,0x6e,0x74,0x20,0x3d,0x20,0x45,0x6e,
			0x74,0x69,0x74,0x69,0x65,0x73,0x2e,0x46,0x69,0x6e,0x64,0x42,0x79,0x4e,0x61,0x6d,0x65,0x28,0x20,0x6e,
			0x75,0x6c,0x6c,0x2c,0x20,0x6e,0x61,0x6d,0x65,0x20,0x29,0x3b,0x0d,0x0a,0x09,0x09,0x45,0x6e,0x74,0x69,
			0x74,0x79,0x47,0x72,0x6f,0x75,0x70,0x2e,0x61,0x70,0x70,0x65,0x6e,0x64,0x28,0x20,0x65,0x6e,0x74,0x20,
			0x29,0x3b,0x0d,0x0a,0x09,0x09,0x69,0x66,0x20,0x28,0x20,0x65,0x6e,0x74,0x20,0x21,0x3d,0x20,0x6e,0x75,
			0x6c,0x6c,0x20,0x29,0x0d,0x0a,0x09,0x09,0x7b,0x0d,0x0a,0x09,0x09,0x09,0x65,0x6e,0x74,0x2e,0x56,0x61,
			0x6c,0x69,0x64,0x61,0x74,0x65,0x53,0x63,0x72,0x69,0x70,0x74,0x53,0x63,0x6f,0x70,0x65,0x28,0x29,0x3b,
			0x0d,0x0a,0x09,0x09,0x09,0x65,0x6e,0x74,0x2e,0x47,0x65,0x74,0x53,0x63,0x72,0x69,0x70,0x74,0x53,0x63,
			0x6f,0x70,0x65,0x28,0x29,0x2e,0x45,0x6e,0x74,0x69,0x74,0x79,0x47,0x72,0x6f,0x75,0x70,0x20,0x3c,0x2d,
			0x20,0x45,0x6e,0x74,0x69,0x74,0x79,0x47,0x72,0x6f,0x75,0x70,0x3b,0x0d,0x0a,0x09,0x09,0x7d,0x0d,0x0a,
			0x09,0x7d,0x0d,0x0a,0x7d,0x0d,0x0a,0x00
		};

		int iLastMember;
		for ( iLastMember = MAX_SCRIPT_GROUP - 1; iLastMember >= 0; iLastMember-- )
		{
			if ( m_iszGroupMembers[iLastMember] != NULL_STRING )
			{
				break;
			}
		}

		if ( iLastMember >= 0 )
		{
			HSCRIPT hAddScript = g_pScriptVM->CompileScript( szAddCode );
			if ( hAddScript )
			{
				ValidateScriptScope();
				m_ScriptScope.Run( hAddScript );
				HSCRIPT hAddFunc = m_ScriptScope.LookupFunction( "__AppendToScriptGroup" );
				if ( hAddFunc )
				{
					for ( int i = 0; i <= iLastMember; i++ )
					{
						m_ScriptScope.Call( hAddFunc, NULL, STRING(m_iszGroupMembers[i]) );
					}
					g_pScriptVM->ReleaseFunction( hAddFunc );
					m_ScriptScope.ClearValue( "__AppendToScriptGroup" );
				}

				g_pScriptVM->ReleaseScript( hAddScript );
			}
		}
		BaseClass::RunVScripts();
	}

	[[= ks::reflect::Key{ .name = "Group16", .index = 15 } ]] [[= ks::reflect::Key{ .name = "Group14", .index = 14 } ]] [[= ks::reflect::Key{ .name = "Group13", .index = 13 } ]] [[= ks::reflect::Key{ .name = "Group12", .index = 12 } ]] [[= ks::reflect::Key{ .name = "Group11", .index = 11 } ]] [[= ks::reflect::Key{ .name = "Group10", .index = 10 } ]] [[= ks::reflect::Key{ .name = "Group09", .index = 9 } ]] [[= ks::reflect::Key{ .name = "Group08", .index = 8 } ]] [[= ks::reflect::Key{ .name = "Group07", .index = 7 } ]] [[= ks::reflect::Key{ .name = "Group06", .index = 6 } ]] [[= ks::reflect::Key{ .name = "Group05", .index = 5 } ]] [[= ks::reflect::Key{ .name = "Group04", .index = 4 } ]] [[= ks::reflect::Key{ .name = "Group03", .index = 3 } ]] [[= ks::reflect::Key{ .name = "Group02", .index = 2 } ]] [[= ks::reflect::Key{ .name = "Group01", .index = 1 } ]] [[= ks::reflect::Key{ .name = "Group00", .index = 0 } ]] string_t m_iszGroupMembers[MAX_SCRIPT_GROUP];

};

LINK_ENTITY_TO_CLASS( logic_script, CLogicScript );

IMPLEMENT_REFLECT_DATAMAP( CLogicScript )


//-----------------------------------------------------------------------------
// Purpose: Compares a set of integer inputs to the one main input
//			Outputs true if they are all equivalant, false otherwise
//-----------------------------------------------------------------------------
class CLogicCompareInteger : public CLogicalEntity
{
public:
	DECLARE_CLASS( CLogicCompareInteger, CLogicalEntity );

	// outputs
	[[= ks::reflect::Key{ .name = "OnEqual" } ]] COutputEvent m_OnEqual;
	[[= ks::reflect::Key{ .name = "OnNotEqual" } ]] COutputEvent m_OnNotEqual;

	// data
	[[= ks::reflect::Key{ .name = "IntegerValue" } ]] int m_iIntegerValue;
	[[= ks::reflect::Key{ .name = "ShouldComparetoValue" } ]] int m_iShouldCompareToValue;

	DECLARE_DATADESC();

	CMultiInputVar m_AllIntCompares;

	// Input handlers
	[[= ks::reflect::Input{ .name = "InputValue", .type = FIELD_INPUT } ]] void InputValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "CompareValues", .type = FIELD_INPUT } ]] void InputCompareValues( inputdata_t &inputdata );
};


LINK_ENTITY_TO_CLASS( logic_multicompare, CLogicCompareInteger );


IMPLEMENT_REFLECT_DATAMAP( CLogicCompareInteger )




//-----------------------------------------------------------------------------
// Purpose: Adds to the list of compared values
//-----------------------------------------------------------------------------
void CLogicCompareInteger::InputValue( inputdata_t &inputdata )
{
	// make sure it's an int, if it can't be converted just throw it away
	if ( !inputdata.value.Convert(FIELD_INTEGER) )
		return;

	// update the value list with the new value
	m_AllIntCompares.AddValue( inputdata.value, inputdata.nOutputID );

	// if we haven't already this frame, send a message to ourself to update and fire
	if ( !m_AllIntCompares.m_bUpdatedThisFrame )
	{
		// TODO: need to add this event with a lower priority, so it gets called after all inputs have arrived
		g_EventQueue.AddEvent( this, "CompareValues", 0, inputdata.pActivator, this, inputdata.nOutputID );
		m_AllIntCompares.m_bUpdatedThisFrame = TRUE;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Forces a recompare
//-----------------------------------------------------------------------------
void CLogicCompareInteger::InputCompareValues( inputdata_t &inputdata )
{
	m_AllIntCompares.m_bUpdatedThisFrame = FALSE;

	// loop through all the values comparing them
	int value = m_iIntegerValue;
	CMultiInputVar::inputitem_t *input = m_AllIntCompares.m_InputList;

	if ( !m_iShouldCompareToValue && input )
	{
		value = input->value.Int();
	}

	while ( input )
	{
		if ( input->value.Int() != value )
		{
			// false
			m_OnNotEqual.FireOutput( inputdata.pActivator, this );
			return;
		}

		input = input->next;
	}

	// true! all values equal
	m_OnEqual.FireOutput( inputdata.pActivator, this );
}

#ifdef PORTAL2
//-----------------------------------------------------------------------------
// Purpose: Manages two sets of values and can fire outputs based on the state of those values
//			
//-----------------------------------------------------------------------------
class CLogicCoopManager : public CLogicalEntity
{
public:
	DECLARE_CLASS( CLogicCoopManager, CLogicalEntity );

	// outputs
	[[= ks::reflect::Key{ .name = "OnChangeToAllTrue" } ]] COutputEvent m_OnChangeToAllTrue;
	[[= ks::reflect::Key{ .name = "OnChangeToAnyTrue" } ]] COutputEvent m_OnChangeToAnyTrue;
	[[= ks::reflect::Key{ .name = "OnChangeToAllFalse" } ]] COutputEvent m_OnChangeToAllFalse;
	[[= ks::reflect::Key{ .name = "OnChangeToAnyFalse" } ]] COutputEvent m_OnChangeToAnyFalse;

	// data
	bool m_bDefaultPlayerStateA;
	bool m_bDefaultPlayerStateB;

	bool m_bPrevPlayerStateA;
	bool m_bPrevPlayerStateB;
	[[= ks::reflect::Key{ .name = "DefaultPlayerStateA" } ]] bool m_bPlayerStateA;
	[[= ks::reflect::Key{ .name = "DefaultPlayerStateB" } ]] bool m_bPlayerStateB;

	DECLARE_DATADESC();

	void CompareValues( void );

	// Input handlers
	[[= ks::reflect::Input{ .name = "SetStateATrue", .type = FIELD_INPUT } ]] void InputSetStateATrue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetStateAFalse", .type = FIELD_INPUT } ]] void InputSetStateAFalse( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ToggleStateA", .type = FIELD_INPUT } ]] void InputToggleStateA( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetStateBTrue", .type = FIELD_INPUT } ]] void InputSetStateBTrue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetStateBFalse", .type = FIELD_INPUT } ]] void InputSetStateBFalse( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ToggleStateB", .type = FIELD_INPUT } ]] void InputToggleStateB( inputdata_t &inputdata );

};


LINK_ENTITY_TO_CLASS( logic_coop_manager, CLogicCoopManager );


IMPLEMENT_REFLECT_DATAMAP( CLogicCoopManager )


//-----------------------------------------------------------------------------
void CLogicCoopManager::InputSetStateATrue( inputdata_t &inputdata )
{
	m_bPrevPlayerStateA = m_bPlayerStateA;
	m_bPrevPlayerStateB = m_bPlayerStateB;
	m_bPlayerStateA = true;
	CompareValues();
}

//-----------------------------------------------------------------------------
void CLogicCoopManager::InputSetStateAFalse( inputdata_t &inputdata )
{
	m_bPrevPlayerStateA = m_bPlayerStateA;
	m_bPrevPlayerStateB = m_bPlayerStateB;
	m_bPlayerStateA = false;
	CompareValues();
}

//-----------------------------------------------------------------------------
void CLogicCoopManager::InputToggleStateA( inputdata_t &inputdata )
{
	m_bPrevPlayerStateA = m_bPlayerStateA;
	m_bPrevPlayerStateB = m_bPlayerStateB;
	m_bPlayerStateA = !m_bPlayerStateA;
	CompareValues();
}

//-----------------------------------------------------------------------------
void CLogicCoopManager::InputSetStateBTrue( inputdata_t &inputdata )
{
	m_bPrevPlayerStateB = m_bPlayerStateB;
	m_bPrevPlayerStateA = m_bPlayerStateA;
	m_bPlayerStateB = true;
	CompareValues();
}

//-----------------------------------------------------------------------------
void CLogicCoopManager::InputSetStateBFalse( inputdata_t &inputdata )
{
	m_bPrevPlayerStateA = m_bPlayerStateA;
	m_bPrevPlayerStateB = m_bPlayerStateB;
	m_bPlayerStateB = false;
	CompareValues();
}

//-----------------------------------------------------------------------------
void CLogicCoopManager::InputToggleStateB( inputdata_t &inputdata )
{
	m_bPrevPlayerStateA = m_bPlayerStateA;
	m_bPrevPlayerStateB = m_bPlayerStateB;
	m_bPlayerStateB = !m_bPlayerStateB;
	CompareValues();
}

//-----------------------------------------------------------------------------
// Purpose: compares the values (currently happens on all inputs)
//-----------------------------------------------------------------------------
void CLogicCoopManager::CompareValues( void )
{
	// check if something has changed
	if (m_bPrevPlayerStateA != m_bPlayerStateA || m_bPrevPlayerStateB != m_bPlayerStateB ) 
	{
		// both values are true and they just changed with this input
		if ( m_bPlayerStateA == m_bPlayerStateB )
		{
			if ( m_bPlayerStateA == true )
				m_OnChangeToAllTrue.FireOutput( this, this );
			else if ( m_bPlayerStateA == false )
				m_OnChangeToAllFalse.FireOutput( this, this );

		}
		else if ( m_bPlayerStateA != m_bPlayerStateB )
		{
			// only fire the output if they become dissimilar for the first time 
			if ( ( m_bPlayerStateA == true && m_bPrevPlayerStateA != true ) || m_bPlayerStateB == true && m_bPrevPlayerStateB != true )
				m_OnChangeToAnyTrue.FireOutput( this, this );
			else if ( ( m_bPlayerStateA == false && m_bPrevPlayerStateA != false ) || ( m_bPlayerStateB == false && m_bPrevPlayerStateB != false ) )
				m_OnChangeToAnyFalse.FireOutput( this, this );
		}
	}
}
#endif // PORTAL2

class CLogicRegisterActivator : public CLogicalEntity
{
public:
	DECLARE_CLASS( CLogicRegisterActivator, CLogicalEntity );

	CLogicRegisterActivator();

	// Input handlers
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "RegisterEntity", .type = FIELD_STRING } ]] void InputRegisterEntity( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "FireRegisteredAsActivator1", .type = FIELD_VOID } ]] void InputFireRegisteredAsActivator1( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FireRegisteredAsActivator2", .type = FIELD_VOID } ]] void InputFireRegisteredAsActivator2( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FireRegisteredAsActivator3", .type = FIELD_VOID } ]] void InputFireRegisteredAsActivator3( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FireRegisteredAsActivator4", .type = FIELD_VOID } ]] void InputFireRegisteredAsActivator4( inputdata_t &inputdata );

	DECLARE_DATADESC();

	// Outputs
	[[= ks::reflect::Key{ .name = "OnRegisteredActivate1" } ]] COutputEvent m_OnRegisteredActivate1;
	[[= ks::reflect::Key{ .name = "OnRegisteredActivate2" } ]] COutputEvent m_OnRegisteredActivate2;
	[[= ks::reflect::Key{ .name = "OnRegisteredActivate3" } ]] COutputEvent m_OnRegisteredActivate3;
	[[= ks::reflect::Key{ .name = "OnRegisteredActivate4" } ]] COutputEvent m_OnRegisteredActivate4;
	
private:

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool m_bDisabled;
	EHANDLE	m_hRegisteredEntity;
};

//-----------------------------------------------------------------------------
// Purpose: Stores an entity and uses it later for certain actions
//-----------------------------------------------------------------------------

LINK_ENTITY_TO_CLASS(logic_register_activator, CLogicRegisterActivator);

IMPLEMENT_REFLECT_DATAMAP( CLogicRegisterActivator )



//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CLogicRegisterActivator::CLogicRegisterActivator(void)
{
}

//------------------------------------------------------------------------------
// Purpose: Register an entity - can use it later as an activator
//------------------------------------------------------------------------------
void CLogicRegisterActivator::InputRegisterEntity( inputdata_t &inputdata )
{
	m_hRegisteredEntity = gEntList.FindEntityByName( NULL, inputdata.value.String(), this, inputdata.pActivator, inputdata.pCaller );
}

//------------------------------------------------------------------------------
// Purpose: Turns entitiy on, allowing it to fire outputs.
//------------------------------------------------------------------------------
void CLogicRegisterActivator::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

//------------------------------------------------------------------------------
// Purpose: Turns entity off, preventing it from firing outputs.
//------------------------------------------------------------------------------
void CLogicRegisterActivator::InputDisable( inputdata_t &inputdata )
{ 
	m_bDisabled = true;
}


//------------------------------------------------------------------------------
// Purpose: Toggles the enabled/disabled state of the entity.
//------------------------------------------------------------------------------
void CLogicRegisterActivator::InputToggle( inputdata_t &inputdata )
{ 
	m_bDisabled = !m_bDisabled;
}


//-----------------------------------------------------------------------------
// Purpose: Input handler that triggers the messages.
//-----------------------------------------------------------------------------
void CLogicRegisterActivator::InputFireRegisteredAsActivator1( inputdata_t &inputdata )
{
	if (!m_bDisabled && m_hRegisteredEntity)
	{
		m_OnRegisteredActivate1.FireOutput( m_hRegisteredEntity.Get(), this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that triggers the messages.
//-----------------------------------------------------------------------------
void CLogicRegisterActivator::InputFireRegisteredAsActivator2( inputdata_t &inputdata )
{
	if (!m_bDisabled && m_hRegisteredEntity)
	{
		m_OnRegisteredActivate2.FireOutput( m_hRegisteredEntity.Get(), this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that triggers the messages.
//-----------------------------------------------------------------------------
void CLogicRegisterActivator::InputFireRegisteredAsActivator3( inputdata_t &inputdata )
{
	if (!m_bDisabled && m_hRegisteredEntity)
	{
		m_OnRegisteredActivate3.FireOutput( m_hRegisteredEntity.Get(), this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that triggers the messages.
//-----------------------------------------------------------------------------
void CLogicRegisterActivator::InputFireRegisteredAsActivator4( inputdata_t &inputdata )
{
	if (!m_bDisabled && m_hRegisteredEntity)
	{
		m_OnRegisteredActivate4.FireOutput( m_hRegisteredEntity.Get(), this );
	}
}



//-----------------------------------------------------------------------------
// Purpose: Timer entity. Fires an output at regular or random intervals.
//-----------------------------------------------------------------------------
//
// Spawnflags and others constants.
//
const int SF_TIMER_UPDOWN = 1;
const float LOGIC_TIMER_MIN_INTERVAL = 0.01;


class CTimerEntity : public CLogicalEntity
{
public:
	DECLARE_CLASS( CTimerEntity, CLogicalEntity );

	void Spawn( void );
	void Think( void );

	void Toggle( void );
	void Enable( void );
	void Disable( void );
	void FireTimer( void );

	int DrawDebugTextOverlays(void);

	// outputs
	[[= ks::reflect::Key{ .name = "OnTimer" } ]] COutputEvent m_OnTimer;
	[[= ks::reflect::Key{ .name = "OnTimerHigh" } ]] COutputEvent m_OnTimerHigh;
	[[= ks::reflect::Key{ .name = "OnTimerLow" } ]] COutputEvent m_OnTimerLow;

	// inputs
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FireTimer", .type = FIELD_VOID } ]] void InputFireTimer( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "RefireTime", .type = FIELD_FLOAT } ]] void InputRefireTime( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ResetTimer", .type = FIELD_VOID } ]] void InputResetTimer( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "AddToTimer", .type = FIELD_FLOAT } ]] void InputAddToTimer( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SubtractFromTimer", .type = FIELD_FLOAT } ]] void InputSubtractFromTimer( inputdata_t &inputdata );

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] int m_iDisabled;
	[[= ks::reflect::Key{ .name = "RefireTime" } ]] float m_flRefireTime;
	bool m_bUpDownState;
	[[= ks::reflect::Key{ .name = "UseRandomTime", .input = true } ]] int m_iUseRandomTime;
	[[= ks::reflect::Key{ .name = "LowerRandomBound", .input = true } ]] float m_flLowerRandomBound;
	[[= ks::reflect::Key{ .name = "UpperRandomBound", .input = true } ]] float m_flUpperRandomBound;

	// methods
	void ResetTimer( void );
	
	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( logic_timer, CTimerEntity );


IMPLEMENT_REFLECT_DATAMAP( CTimerEntity )



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::Spawn( void )
{
	if (!m_iUseRandomTime && (m_flRefireTime < LOGIC_TIMER_MIN_INTERVAL))
	{
		m_flRefireTime = LOGIC_TIMER_MIN_INTERVAL;
	}

	if ( !m_iDisabled && (m_flRefireTime > 0 || m_iUseRandomTime) )
	{
		Enable();
	}
	else
	{
		Disable();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::Think( void )
{
	FireTimer();
}


//-----------------------------------------------------------------------------
// Purpose: Sets the time the timerentity will next fire
//-----------------------------------------------------------------------------
void CTimerEntity::ResetTimer( void )
{
	if ( m_iDisabled )
		return;

	if ( m_iUseRandomTime )
	{
		m_flRefireTime = random->RandomFloat( m_flLowerRandomBound, m_flUpperRandomBound );
	}

	SetNextThink( gpGlobals->curtime + m_flRefireTime );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::Enable( void )
{
	m_iDisabled = FALSE;
	ResetTimer();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::Disable( void )
{
	m_iDisabled = TRUE;
	SetNextThink( TICK_NEVER_THINK );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::Toggle( void )
{
	if ( m_iDisabled )
	{
		Enable();
	}
	else
	{
		Disable();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::FireTimer( void )
{
	if ( !m_iDisabled )
	{
		//
		// Up/down timers alternate between two outputs.
		//
		if (m_spawnflags & SF_TIMER_UPDOWN)
		{
			if (m_bUpDownState)
			{
				m_OnTimerHigh.FireOutput( this, this );
			}
			else
			{
				m_OnTimerLow.FireOutput( this, this );
			}

			m_bUpDownState = !m_bUpDownState;
		}
		//
		// Regular timers only fire a single output.
		//
		else
		{
			m_OnTimer.FireOutput( this, this );
		}

		ResetTimer();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::InputEnable( inputdata_t &inputdata )
{
	Enable();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::InputDisable( inputdata_t &inputdata )
{
	Disable();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::InputToggle( inputdata_t &inputdata )
{
	Toggle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::InputFireTimer( inputdata_t &inputdata )
{
	FireTimer();
}


//-----------------------------------------------------------------------------
// Purpose: Changes the time interval between timer fires
//			Resets the next firing to be time + newRefireTime
// Input  : Float refire frequency in seconds.
//-----------------------------------------------------------------------------
void CTimerEntity::InputRefireTime( inputdata_t &inputdata )
{
	float flRefireInterval = inputdata.value.Float();

	if ( flRefireInterval < LOGIC_TIMER_MIN_INTERVAL)
	{
		flRefireInterval = LOGIC_TIMER_MIN_INTERVAL;
	}

	if (m_flRefireTime != flRefireInterval )
	{
		m_flRefireTime = flRefireInterval;
		ResetTimer();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTimerEntity::InputResetTimer( inputdata_t &inputdata )
{
	// don't reset the timer if it isn't enabled
	if ( m_iDisabled )
		return;

	ResetTimer();
}


//-----------------------------------------------------------------------------
// Purpose: Adds to the time interval if the timer is enabled
// Input  : Float time to add in seconds
//-----------------------------------------------------------------------------
void CTimerEntity::InputAddToTimer( inputdata_t &inputdata )
{
	// don't add time if the timer isn't enabled
	if ( m_iDisabled )
		return;
	
	// Add time to timer
 	float flNextThink = GetNextThink();	
	SetNextThink( flNextThink += inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: Subtract from the time interval if the timer is enabled
// Input  : Float time to subtract in seconds
//-----------------------------------------------------------------------------
void CTimerEntity::InputSubtractFromTimer( inputdata_t &inputdata )
{
	// don't add time if the timer isn't enabled
	if ( m_iDisabled )
		return;

	// Subtract time from the timer but don't let the timer go negative
	float flNextThink = GetNextThink();
	if ( ( flNextThink - gpGlobals->curtime ) <= inputdata.value.Float() )
	{
		SetNextThink( gpGlobals->curtime );
	}
	else
	{
		SetNextThink( flNextThink -= inputdata.value.Float() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CTimerEntity::DrawDebugTextOverlays( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		// print refire time
		Q_snprintf(tempstr,sizeof(tempstr),"refire interval: %.2f sec", m_flRefireTime);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		// print seconds to next fire
		if ( !m_iDisabled )
		{
			float flNextThink = GetNextThink();
			Q_snprintf( tempstr, sizeof( tempstr ), "      firing in: %.2f sec", flNextThink - gpGlobals->curtime );
			EntityText( text_offset, tempstr, 0);
			text_offset++;
		}
	}
	return text_offset;
}


//-----------------------------------------------------------------------------
// Purpose: Computes a line between two entities
//-----------------------------------------------------------------------------
class CLogicLineToEntity : public CLogicalEntity
{
public:
	DECLARE_CLASS( CLogicLineToEntity, CLogicalEntity );

	void Activate(void);
	void Spawn( void );
	void Think( void );

	// outputs
	[[= ks::reflect::Key{ .name = "Line" } ]] COutputVector m_Line;

	DECLARE_DATADESC();

private:
	[[= ks::reflect::Key{ .name = "source" } ]] string_t m_SourceName;
	EHANDLE	m_StartEntity;
	EHANDLE m_EndEntity;
};

LINK_ENTITY_TO_CLASS( logic_lineto, CLogicLineToEntity );


IMPLEMENT_REFLECT_DATAMAP( CLogicLineToEntity )



//-----------------------------------------------------------------------------
// Find the entities
//-----------------------------------------------------------------------------
void CLogicLineToEntity::Activate(void)
{
	BaseClass::Activate();

	if (m_target != NULL_STRING)
	{
		m_EndEntity = gEntList.FindEntityByName( NULL, m_target );

		//
		// If we were given a bad measure target, just measure sound where we are.
		//
		if ((m_EndEntity == NULL) || (m_EndEntity->edict() == NULL))
		{
			Warning( "logic_lineto - Target not found or target with no origin!\n");
			m_EndEntity = this;
		}
	}
	else
	{
		m_EndEntity = this;
	}

	if (m_SourceName != NULL_STRING)
	{
		m_StartEntity = gEntList.FindEntityByName( NULL, m_SourceName );

		//
		// If we were given a bad measure target, just measure sound where we are.
		//
		if ((m_StartEntity == NULL) || (m_StartEntity->edict() == NULL))
		{
			Warning( "logic_lineto - Source not found or source with no origin!\n");
			m_StartEntity = this;
		}
	}
	else
	{
		m_StartEntity = this;
	}
}


//-----------------------------------------------------------------------------
// Find the entities
//-----------------------------------------------------------------------------
void CLogicLineToEntity::Spawn(void)
{
	SetNextThink( gpGlobals->curtime + 0.01f );
}


//-----------------------------------------------------------------------------
// Find the entities
//-----------------------------------------------------------------------------
void CLogicLineToEntity::Think(void)
{
	CBaseEntity* pDest = m_EndEntity.Get();
	CBaseEntity* pSrc = m_StartEntity.Get();
	if (!pDest || !pSrc || !pDest->edict() || !pSrc->edict())
	{
		// Can sleep for a long time, no more lines.
		m_Line.Set( vec3_origin, this, this );
		SetNextThink( gpGlobals->curtime + 10 );
		return;
	}

	Vector delta;
	VectorSubtract( pDest->GetAbsOrigin(), pSrc->GetAbsOrigin(), delta ); 
	m_Line.Set(delta, this, this);

	SetNextThink( gpGlobals->curtime + 0.01f );
}


//-----------------------------------------------------------------------------
// Purpose: Remaps a given input range to an output range.
//-----------------------------------------------------------------------------
const int SF_MATH_REMAP_IGNORE_OUT_OF_RANGE = 1;
const int SF_MATH_REMAP_CLAMP_OUTPUT_TO_RANGE = 2;

class CMathRemap : public CLogicalEntity
{
public:

	DECLARE_CLASS( CMathRemap, CLogicalEntity );

	void Spawn(void);

	// Keys
	[[= ks::reflect::Key{ .name = "in1" } ]] float m_flInMin;
	[[= ks::reflect::Key{ .name = "in2" } ]] float m_flInMax;
	[[= ks::reflect::Key{ .name = "out1" } ]] float m_flOut1;		// Output value when input is m_fInMin
	[[= ks::reflect::Key{ .name = "out2" } ]] float m_flOut2;		// Output value when input is m_fInMax

	bool  m_bEnabled;

	// Inputs
	[[= ks::reflect::Input{ .name = "InValue", .type = FIELD_FLOAT } ]] void InputValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

	// Outputs
	[[= ks::reflect::Key{ .name = "OutValue" } ]] COutputFloat m_OutValue;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(math_remap, CMathRemap);


IMPLEMENT_REFLECT_DATAMAP( CMathRemap )




//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMathRemap::Spawn(void)
{
	//
	// Avoid a divide by zero in ValueChanged.
	//
	if (m_flInMin == m_flInMax)
	{
		m_flInMin = 0;
		m_flInMax = 1;
	}

	//
	// Make sure min and max are set properly relative to one another.
	//
	if (m_flInMin > m_flInMax)
	{
		float flTemp = m_flInMin;
		m_flInMin = m_flInMax;
		m_flInMax = flTemp;
	}

	m_bEnabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMathRemap::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMathRemap::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that is called when the input value changes.
//-----------------------------------------------------------------------------
void CMathRemap::InputValue( inputdata_t &inputdata )
{
	float flValue = inputdata.value.Float();

	//
	// Disallow out-of-range input values to avoid out-of-range output values.
	//
	float flClampValue = clamp(flValue, m_flInMin, m_flInMax);

	if ((flClampValue == flValue) || !FBitSet(m_spawnflags, SF_MATH_REMAP_IGNORE_OUT_OF_RANGE))
	{
		//
		// Remap the input value to the desired output range and update the output.
		//
		float flRemappedValue = m_flOut1 + (((flValue - m_flInMin) * (m_flOut2 - m_flOut1)) / (m_flInMax - m_flInMin));

		if ( FBitSet( m_spawnflags, SF_MATH_REMAP_CLAMP_OUTPUT_TO_RANGE ) )
		{
			flRemappedValue = clamp( flRemappedValue, m_flOut1, m_flOut2 );
		}

		if ( m_bEnabled == true )
		{
			m_OutValue.Set(flRemappedValue, inputdata.pActivator, this);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Remaps a given input range to an output range.
//-----------------------------------------------------------------------------
const int SF_COLOR_BLEND_IGNORE_OUT_OF_RANGE = 1;

class CMathColorBlend : public CLogicalEntity
{
public:

	DECLARE_CLASS( CMathColorBlend, CLogicalEntity );

	void Spawn(void);

	// Keys
	[[= ks::reflect::Key{ .name = "inmin" } ]] float m_flInMin;
	[[= ks::reflect::Key{ .name = "inmax" } ]] float m_flInMax;
	[[= ks::reflect::Key{ .name = "colormin" } ]] color32 m_OutColor1;		// Output color when input is m_fInMin
	[[= ks::reflect::Key{ .name = "colormax" } ]] color32 m_OutColor2;		// Output color when input is m_fInMax

	// Inputs
	[[= ks::reflect::Input{ .name = "InValue", .type = FIELD_FLOAT } ]] void InputValue( inputdata_t &inputdata );

	// Outputs
	[[= ks::reflect::Key{ .name = "OutColor" } ]] COutputColor32 m_OutValue;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(math_colorblend, CMathColorBlend);


IMPLEMENT_REFLECT_DATAMAP( CMathColorBlend )




//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMathColorBlend::Spawn(void)
{
	//
	// Avoid a divide by zero in ValueChanged.
	//
	if (m_flInMin == m_flInMax)
	{
		m_flInMin = 0;
		m_flInMax = 1;
	}

	//
	// Make sure min and max are set properly relative to one another.
	//
	if (m_flInMin > m_flInMax)
	{
		float flTemp = m_flInMin;
		m_flInMin = m_flInMax;
		m_flInMax = flTemp;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Input handler that is called when the input value changes.
//-----------------------------------------------------------------------------
void CMathColorBlend::InputValue( inputdata_t &inputdata )
{
	float flValue = inputdata.value.Float();

	//
	// Disallow out-of-range input values to avoid out-of-range output values.
	//
	float flClampValue = clamp(flValue, m_flInMin, m_flInMax);
	if ((flClampValue == flValue) || !FBitSet(m_spawnflags, SF_COLOR_BLEND_IGNORE_OUT_OF_RANGE))
	{
		//
		// Remap the input value to the desired output color and update the output.
		//
		color32 Color;
		Color.r = m_OutColor1.r + (((flClampValue - m_flInMin) * (m_OutColor2.r - m_OutColor1.r)) / (m_flInMax - m_flInMin));
		Color.g = m_OutColor1.g + (((flClampValue - m_flInMin) * (m_OutColor2.g - m_OutColor1.g)) / (m_flInMax - m_flInMin));
		Color.b = m_OutColor1.b + (((flClampValue - m_flInMin) * (m_OutColor2.b - m_OutColor1.b)) / (m_flInMax - m_flInMin));
		Color.a = m_OutColor1.a + (((flClampValue - m_flInMin) * (m_OutColor2.a - m_OutColor1.a)) / (m_flInMax - m_flInMin));

		m_OutValue.Set(Color, inputdata.pActivator, this);
	}
}


//-----------------------------------------------------------------------------
// Console command to set the state of a global
//-----------------------------------------------------------------------------
void CC_Global_Set( const CCommand &args )
{
	const char *szGlobal = args[1];
	const char *szState = args[2];

	if ( szGlobal == NULL || szState == NULL )
	{
		Msg( "Usage: global_set <globalname> <state>: Sets the state of the given env_global (0 = OFF, 1 = ON, 2 = DEAD).\n" );
		return;
	}

	int nState = atoi( szState );

	int nIndex = GlobalEntity_GetIndex( szGlobal );

	if ( nIndex >= 0 )
	{
		GlobalEntity_SetState( nIndex, ( GLOBALESTATE )nState );
	}
	else
	{
		GlobalEntity_Add( szGlobal, STRING( gpGlobals->mapname ), ( GLOBALESTATE )nState );
	}
}

static ConCommand global_set( "global_set", CC_Global_Set, "global_set <globalname> <state>: Sets the state of the given env_global (0 = OFF, 1 = ON, 2 = DEAD).", FCVAR_CHEAT );


//-----------------------------------------------------------------------------
// Purpose: Holds a global state that can be queried by other entities to change
//			their behavior, such as "predistaster".
//-----------------------------------------------------------------------------
const int SF_GLOBAL_SET = 1;	// Set global state to initial state on spawn

class CEnvGlobal : public CLogicalEntity
{
public:
	DECLARE_CLASS( CEnvGlobal, CLogicalEntity );

	void Spawn( void );

	// Input handlers
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Remove", .type = FIELD_VOID } ]] void InputRemove( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetCounter", .type = FIELD_INTEGER } ]] void InputSetCounter( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "AddToCounter", .type = FIELD_INTEGER } ]] void InputAddToCounter( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "GetCounter", .type = FIELD_VOID } ]] void InputGetCounter( inputdata_t &inputdata );

	int DrawDebugTextOverlays(void);

	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "Counter" } ]] COutputInt m_outCounter;
		
	[[= ks::reflect::Key{ .name = "globalstate" } ]] string_t	m_globalstate;
	int			m_triggermode;
	[[= ks::reflect::Key{ .name = "initialstate" } ]] int			m_initialstate;
	[[= ks::reflect::Key{ .name = "counter" } ]] int			m_counter;			// A counter value associated with this global.
};


IMPLEMENT_REFLECT_DATAMAP( CEnvGlobal )


LINK_ENTITY_TO_CLASS( env_global, CEnvGlobal );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvGlobal::Spawn( void )
{
	if ( !m_globalstate )
	{
		UTIL_Remove( this );
		return;
	}

#ifdef HL2_EPISODIC
	// if we modify the state of the physics cannon, make sure we precache the ragdoll boogie zap sound
	if ( ( m_globalstate != NULL_STRING ) && ( stricmp( STRING( m_globalstate ), "super_phys_gun" ) == 0 ) )
	{
		PrecacheScriptSound( "RagdollBoogie.Zap" );
	}
#endif

	if ( FBitSet( m_spawnflags, SF_GLOBAL_SET ) )
	{
		if ( !GlobalEntity_IsInTable( m_globalstate ) )
		{
			GlobalEntity_Add( m_globalstate, gpGlobals->mapname, (GLOBALESTATE)m_initialstate );
		}
		
		if ( m_counter != 0 )
		{
			GlobalEntity_SetCounter( m_globalstate, m_counter );
		}
	}
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CEnvGlobal::InputTurnOn( inputdata_t &inputdata )
{
	if ( GlobalEntity_IsInTable( m_globalstate ) )
	{
		GlobalEntity_SetState( m_globalstate, GLOBAL_ON );
	}
	else
	{
		GlobalEntity_Add( m_globalstate, gpGlobals->mapname, GLOBAL_ON );
	}
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CEnvGlobal::InputTurnOff( inputdata_t &inputdata )
{
	if ( GlobalEntity_IsInTable( m_globalstate ) )
	{
		GlobalEntity_SetState( m_globalstate, GLOBAL_OFF );
	}
	else
	{
		GlobalEntity_Add( m_globalstate, gpGlobals->mapname, GLOBAL_OFF );
	}
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CEnvGlobal::InputRemove( inputdata_t &inputdata )
{
	if ( GlobalEntity_IsInTable( m_globalstate ) )
	{
		GlobalEntity_SetState( m_globalstate, GLOBAL_DEAD );
	}
	else
	{
		GlobalEntity_Add( m_globalstate, gpGlobals->mapname, GLOBAL_DEAD );
	}
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CEnvGlobal::InputSetCounter( inputdata_t &inputdata )
{
	if ( !GlobalEntity_IsInTable( m_globalstate ) )
	{
		GlobalEntity_Add( m_globalstate, gpGlobals->mapname, GLOBAL_ON );
	}

	GlobalEntity_SetCounter( m_globalstate, inputdata.value.Int() );
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CEnvGlobal::InputAddToCounter( inputdata_t &inputdata )
{
	if ( !GlobalEntity_IsInTable( m_globalstate ) )
	{
		GlobalEntity_Add( m_globalstate, gpGlobals->mapname, GLOBAL_ON );
	}

	GlobalEntity_AddToCounter( m_globalstate, inputdata.value.Int() );
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CEnvGlobal::InputGetCounter( inputdata_t &inputdata )
{
	if ( !GlobalEntity_IsInTable( m_globalstate ) )
	{
		GlobalEntity_Add( m_globalstate, gpGlobals->mapname, GLOBAL_ON );
	}

	m_outCounter.Set( GlobalEntity_GetCounter( m_globalstate ), inputdata.pActivator, this );
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CEnvGlobal::InputToggle( inputdata_t &inputdata )
{
	GLOBALESTATE oldState = GlobalEntity_GetState( m_globalstate );
	GLOBALESTATE newState;

	if ( oldState == GLOBAL_ON )
	{
		newState = GLOBAL_OFF;
	}
	else if ( oldState == GLOBAL_OFF )
	{
		newState = GLOBAL_ON;
	}
	else
	{
		return;
	}

	if ( GlobalEntity_IsInTable( m_globalstate ) )
	{
		GlobalEntity_SetState( m_globalstate, newState );
	}
	else
	{
		GlobalEntity_Add( m_globalstate, gpGlobals->mapname, newState );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Input  :
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CEnvGlobal::DrawDebugTextOverlays(void) 
{
	// Skip AIClass debug overlays
	int text_offset = CBaseEntity::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];
		Q_snprintf(tempstr,sizeof(tempstr),"State: %s",STRING(m_globalstate));
		EntityText(text_offset,tempstr,0);
		text_offset++;

		GLOBALESTATE nState = GlobalEntity_GetState( m_globalstate );

		switch( nState )
		{
		case GLOBAL_OFF:
			Q_strncpy(tempstr,"Value: OFF",sizeof(tempstr));
			break;

		case GLOBAL_ON:
			Q_strncpy(tempstr,"Value: ON",sizeof(tempstr));
			break;

		case GLOBAL_DEAD:
			Q_strncpy(tempstr,"Value: DEAD",sizeof(tempstr));
			break;
		}
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
#define MS_MAX_TARGETS 32

const int SF_MULTI_INIT	= 1;

class CMultiSource : public CLogicalEntity
{
public:
	DECLARE_CLASS( CMultiSource, CLogicalEntity );

	void Spawn( );
	bool KeyValue( const char *szKeyName, const char *szValue );
	void Use( ::CBaseEntity *pActivator, ::CBaseEntity *pCaller, USE_TYPE useType, float value );
	int	ObjectCaps( void ) { return(BaseClass::ObjectCaps() | FCAP_MASTER); }
	bool IsTriggered( ::CBaseEntity *pActivator );
	void Register( void );

	DECLARE_DATADESC();

	EHANDLE		m_rgEntities[MS_MAX_TARGETS];
	int			m_rgTriggered[MS_MAX_TARGETS];

	[[= ks::reflect::Key{ .name = "OnTrigger" } ]] COutputEvent m_OnTrigger;		// Fired when all connections are triggered.

	int			m_iTotal;
	[[= ks::reflect::Key{ .name = "globalstate" } ]] string_t	m_globalstate;
};

IMPLEMENT_REFLECT_DATAMAP( CMultiSource )


LINK_ENTITY_TO_CLASS( multisource, CMultiSource );


//-----------------------------------------------------------------------------
// Purpose: Cache user entity field values until spawn is called.
// Input  : szKeyName - Key to handle.
//			szValue - Value for key.
// Output : Returns true if the key was handled, false if not.
//-----------------------------------------------------------------------------
bool CMultiSource::KeyValue( const char *szKeyName, const char *szValue )
{
	if (	FStrEq(szKeyName, "style") ||
				FStrEq(szKeyName, "height") ||
				FStrEq(szKeyName, "killtarget") ||
				FStrEq(szKeyName, "value1") ||
				FStrEq(szKeyName, "value2") ||
				FStrEq(szKeyName, "value3"))
	{
	}
	else
	{
		return BaseClass::KeyValue( szKeyName, szValue );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiSource::Spawn()
{ 
	SetNextThink( gpGlobals->curtime + 0.1f );
	m_spawnflags |= SF_MULTI_INIT;	// Until it's initialized
	SetThink(&CMultiSource::Register);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pActivator - 
//			pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void CMultiSource::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{ 
	int i = 0;

	// Find the entity in our list
	while (i < m_iTotal)
		if ( m_rgEntities[i++] == pCaller )
			break;

	// if we didn't find it, report error and leave
	if (i > m_iTotal)
	{
		Warning("MultiSrc: Used by non member %s.\n", pCaller->edict() ? pCaller->GetClassname() : "<logical entity>");
		return;	
	}

	// CONSIDER: a Use input to the multisource always toggles.  Could check useType for ON/OFF/TOGGLE

	Assert(i > 0);
	m_rgTriggered[i-1] ^= 1;

	// 
	if ( IsTriggered( pActivator ) )
	{
		DevMsg( 2, "Multisource %s enabled (%d inputs)\n", GetDebugName(), m_iTotal );
		USE_TYPE useType = USE_TOGGLE;
		if ( m_globalstate != NULL_STRING )
			useType = USE_ON;

		m_OnTrigger.FireOutput(pActivator, this);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMultiSource::IsTriggered( CBaseEntity * )
{
	// Is everything triggered?
	int i = 0;

	// Still initializing?
	if ( m_spawnflags & SF_MULTI_INIT )
		return 0;

	while (i < m_iTotal)
	{
		if (m_rgTriggered[i] == 0)
			break;
		i++;
	}

	if (i == m_iTotal)
	{
		if ( !m_globalstate || GlobalEntity_GetState( m_globalstate ) == GLOBAL_ON )
			return 1;
	}
	
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiSource::Register(void)
{ 
	CBaseEntity *pTarget = NULL;

	m_iTotal = 0;
	memset( m_rgEntities, 0, MS_MAX_TARGETS * sizeof(EHANDLE) );

	SetThink(&CMultiSource::SUB_DoNothing);

	// search for all entities which target this multisource (m_iName)
	// dvsents2: port multisource to entity I/O!

	pTarget = gEntList.FindEntityByTarget( NULL, STRING(GetEntityName()) );

	while ( pTarget && (m_iTotal < MS_MAX_TARGETS) )
	{
		if ( pTarget )
			m_rgEntities[m_iTotal++] = pTarget;

		pTarget = gEntList.FindEntityByTarget( pTarget, STRING(GetEntityName()) );
	}

	pTarget = gEntList.FindEntityByClassname( NULL, "multi_manager" );
	while (pTarget && (m_iTotal < MS_MAX_TARGETS))
	{
		if ( pTarget && pTarget->HasTarget(GetEntityName()) )
			m_rgEntities[m_iTotal++] = pTarget;

		pTarget = gEntList.FindEntityByClassname( pTarget, "multi_manager" );
	}

	m_spawnflags &= ~SF_MULTI_INIT;
}


//-----------------------------------------------------------------------------
// Purpose: Holds a value that can be added to and subtracted from.
//-----------------------------------------------------------------------------
class CMathCounter : public CLogicalEntity
{
	DECLARE_CLASS( CMathCounter, CLogicalEntity );
private:
	[[= ks::reflect::Key{ .name = "min" } ]] float m_flMin;		// Minimum clamp value. If min and max are BOTH zero, no clamping is done.
	[[= ks::reflect::Key{ .name = "max" } ]] float m_flMax;		// Maximum clamp value.
	bool m_bHitMin;		// Set when we reach or go below our minimum value, cleared if we go above it again.
	bool m_bHitMax;		// Set when we reach or exceed our maximum value, cleared if we fall below it again.

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool m_bDisabled;

	bool KeyValue(const char *szKeyName, const char *szValue);
	void Spawn(void);

	int DrawDebugTextOverlays(void);

	void UpdateOutValue(CBaseEntity *pActivator, float fNewValue);

	// Inputs
	[[= ks::reflect::Input{ .name = "Add", .type = FIELD_FLOAT } ]] void InputAdd( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Divide", .type = FIELD_FLOAT } ]] void InputDivide( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Multiply", .type = FIELD_FLOAT } ]] void InputMultiply( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetValue", .type = FIELD_FLOAT } ]] void InputSetValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetValueNoFire", .type = FIELD_FLOAT } ]] void InputSetValueNoFire( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetMaxValueNoFire", .type = FIELD_FLOAT } ]] void InputSetMaxValueNoFire( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetMinValueNoFire", .type = FIELD_FLOAT } ]] void InputSetMinValueNoFire( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Subtract", .type = FIELD_FLOAT } ]] void InputSubtract( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetHitMax", .type = FIELD_FLOAT } ]] void InputSetHitMax( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetHitMin", .type = FIELD_FLOAT } ]] void InputSetHitMin( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "GetValue", .type = FIELD_VOID } ]] void InputGetValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

	// Outputs
	[[= ks::reflect::Key{ .name = "OutValue" } ]] COutputFloat m_OutValue;
	[[= ks::reflect::Key{ .name = "OnGetValue" } ]] COutputFloat m_OnGetValue;	// Used for polling the counter value.
	[[= ks::reflect::Key{ .name = "OnHitMin" } ]] COutputEvent m_OnHitMin;
	[[= ks::reflect::Key{ .name = "OnHitMax" } ]] COutputEvent m_OnHitMax;
	[[= ks::reflect::Key{ .name = "OnChangedFromMin" } ]] COutputEvent m_OnChangedFromMin;
	[[= ks::reflect::Key{ .name = "OnChangedFromMax" } ]] COutputEvent m_OnChangedFromMax;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(math_counter, CMathCounter);


IMPLEMENT_REFLECT_DATAMAP( CMathCounter )




//-----------------------------------------------------------------------------
// Purpose: Handles key values from the BSP before spawn is called.
//-----------------------------------------------------------------------------
bool CMathCounter::KeyValue(const char *szKeyName, const char *szValue)
{
	//
	// Set the initial value of the counter.
	//
	if (!stricmp(szKeyName, "startvalue"))
	{
		m_OutValue.Init(atoi(szValue));
		return(true);
	}

	return(BaseClass::KeyValue(szKeyName, szValue));
}


//-----------------------------------------------------------------------------
// Purpose: Called before spawning, after key values have been set.
//-----------------------------------------------------------------------------
void CMathCounter::Spawn( void )
{
	//
	// Make sure max and min are ordered properly or clamp won't work.
	//
	if (m_flMin > m_flMax)
	{
		float flTemp = m_flMax;
		m_flMax = m_flMin;
		m_flMin = flTemp;
	}

	//
	// Clamp initial value to within the valid range.
	//
	if ((m_flMin != 0) || (m_flMax != 0))
	{
		float flStartValue = clamp(m_OutValue.Get(), m_flMin, m_flMax);
		m_OutValue.Init(flStartValue);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Input  :
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CMathCounter::DrawDebugTextOverlays( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		Q_snprintf(tempstr,sizeof(tempstr),"    min value: %f", m_flMin);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		Q_snprintf(tempstr,sizeof(tempstr),"    max value: %f", m_flMax);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		Q_snprintf(tempstr,sizeof(tempstr),"current value: %f", m_OutValue.Get());
		EntityText(text_offset,tempstr,0);
		text_offset++;

		if( m_bDisabled )
		{	
			Q_snprintf(tempstr,sizeof(tempstr),"*DISABLED*");		
		}
		else
		{
			Q_snprintf(tempstr,sizeof(tempstr),"Enabled.");
		}
		EntityText(text_offset,tempstr,0);
		text_offset++;

	}
	return text_offset;
}

//-----------------------------------------------------------------------------
// Change min/max
//-----------------------------------------------------------------------------
void CMathCounter::InputSetHitMax( inputdata_t &inputdata )
{
	m_flMax = inputdata.value.Float();
	if ( m_flMax < m_flMin )
	{
		m_flMin = m_flMax;
	}
	UpdateOutValue( inputdata.pActivator, m_OutValue.Get() );
}

void CMathCounter::InputSetHitMin( inputdata_t &inputdata )
{
	m_flMin = inputdata.value.Float();
	if ( m_flMax < m_flMin )
	{
		m_flMax = m_flMin;
	}
	UpdateOutValue( inputdata.pActivator, m_OutValue.Get() );
}

	
//-----------------------------------------------------------------------------
// Purpose: Input handler for adding to the accumulator value.
// Input  : Float value to add.
//-----------------------------------------------------------------------------
void CMathCounter::InputAdd( inputdata_t &inputdata )
{
	if( m_bDisabled )
	{
		DevMsg("Math Counter %s ignoring ADD because it is disabled\n", GetDebugName() );
		return;
	}

	float fNewValue = m_OutValue.Get() + inputdata.value.Float();
	UpdateOutValue( inputdata.pActivator, fNewValue );
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for multiplying the current value.
// Input  : Float value to multiply the value by.
//-----------------------------------------------------------------------------
void CMathCounter::InputDivide( inputdata_t &inputdata )
{
	if( m_bDisabled )
	{
		DevMsg("Math Counter %s ignoring DIVIDE because it is disabled\n", GetDebugName() );
		return;
	}

	if (inputdata.value.Float() != 0)
	{
		float fNewValue = m_OutValue.Get() / inputdata.value.Float();
		UpdateOutValue( inputdata.pActivator, fNewValue );
	}
	else
	{
		DevMsg( 1, "LEVEL DESIGN ERROR: Divide by zero in math_value\n" );
		UpdateOutValue( inputdata.pActivator, m_OutValue.Get() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for multiplying the current value.
// Input  : Float value to multiply the value by.
//-----------------------------------------------------------------------------
void CMathCounter::InputMultiply( inputdata_t &inputdata )
{
	if( m_bDisabled )
	{
		DevMsg("Math Counter %s ignoring MULTIPLY because it is disabled\n", GetDebugName() );
		return;
	}

	float fNewValue = m_OutValue.Get() * inputdata.value.Float();
	UpdateOutValue( inputdata.pActivator, fNewValue );
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for updating the value.
// Input  : Float value to set.
//-----------------------------------------------------------------------------
void CMathCounter::InputSetValue( inputdata_t &inputdata )
{
	if( m_bDisabled )
	{
		DevMsg("Math Counter %s ignoring SETVALUE because it is disabled\n", GetDebugName() );
		return;
	}

	UpdateOutValue( inputdata.pActivator, inputdata.value.Float() );
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for updating the value.
// Input  : Float value to set.
//-----------------------------------------------------------------------------
void CMathCounter::InputSetValueNoFire( inputdata_t &inputdata )
{
	if( m_bDisabled )
	{
		DevMsg("Math Counter %s ignoring SETVALUENOFIRE because it is disabled\n", GetDebugName() );
		return;
	}

	float flNewValue = inputdata.value.Float();
	if (( m_flMin != 0 ) || (m_flMax != 0 ))
	{
		flNewValue = clamp(flNewValue, m_flMin, m_flMax);
	}

	m_OutValue.Init( flNewValue );

}

//-----------------------------------------------------------------------------
// Purpose: Input handler for updating the MAX value.
// Input  : Float value to set.
//-----------------------------------------------------------------------------
void CMathCounter::InputSetMaxValueNoFire( inputdata_t &inputdata )
{
	if( m_bDisabled )
	{
		DevMsg("Math Counter %s ignoring SETMAXVALUENOFIRE because it is disabled\n", GetDebugName() );
		return;
	}

	float flNewValue = inputdata.value.Float();
	if ( flNewValue < m_flMin )
	{
		DevMsg("Math Counter %s ignoring SETMAXVALUENOFIRE because the value is less than the Minimum Value!\n", GetDebugName() );
		return;
	}

	m_flMax = flNewValue;

	float flCurValue = m_OutValue.Get();
	if (( m_flMin != 0 ) || (m_flMax != 0 ))
	{
		flCurValue = clamp(flCurValue, m_flMin, m_flMax);
	}

	m_OutValue.Init( flCurValue );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for updating the MAX value.
// Input  : Float value to set.
//-----------------------------------------------------------------------------
void CMathCounter::InputSetMinValueNoFire( inputdata_t &inputdata )
{
	if( m_bDisabled )
	{
		DevMsg("Math Counter %s ignoring SETMINVALUENOFIRE because it is disabled\n", GetDebugName() );
		return;
	}

	float flNewValue = inputdata.value.Float();
	if ( flNewValue > m_flMax )
	{
		DevMsg("Math Counter %s ignoring SETMINVALUENOFIRE because the value is greater than the Maximum Value!\n", GetDebugName() );
		return;
	}

	m_flMin = flNewValue;

	float flCurValue = m_OutValue.Get();
	if (( m_flMin != 0 ) || (m_flMax != 0 ))
	{
		flCurValue = clamp(flCurValue, m_flMin, m_flMax);
	}

	m_OutValue.Init( flCurValue );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for subtracting from the current value.
// Input  : Float value to subtract.
//-----------------------------------------------------------------------------
void CMathCounter::InputSubtract( inputdata_t &inputdata )
{
	if( m_bDisabled )
	{
		DevMsg("Math Counter %s ignoring SUBTRACT because it is disabled\n", GetDebugName() );
		return;
	}

	float fNewValue = m_OutValue.Get() - inputdata.value.Float();
	UpdateOutValue( inputdata.pActivator, fNewValue );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMathCounter::InputGetValue( inputdata_t &inputdata )
{
	float flOutValue = m_OutValue.Get();
	m_OnGetValue.Set( flOutValue, inputdata.pActivator, inputdata.pCaller );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMathCounter::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMathCounter::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the value to the new value, clamping and firing the output value.
// Input  : fNewValue - Value to set.
//-----------------------------------------------------------------------------
void CMathCounter::UpdateOutValue(CBaseEntity *pActivator, float fNewValue)
{
	if ((m_flMin != 0) || (m_flMax != 0))
	{
		//
		// Fire an output any time we reach or exceed our maximum value.
		//
		if ( fNewValue >= m_flMax )
		{
			if ( !m_bHitMax )
			{
				m_bHitMax = true;
				m_OnHitMax.FireOutput( pActivator, this );
			}
		}
		else
		{
			// Fire an output if we just changed from the maximum value
			if ( m_OutValue.Get() == m_flMax )
			{
				m_OnChangedFromMax.FireOutput( pActivator, this );
			}
			
			m_bHitMax = false;
		}

		//
		// Fire an output any time we reach or go below our minimum value.
		//
		if ( fNewValue <= m_flMin )
		{
			if ( !m_bHitMin )
			{
				m_bHitMin = true;
				m_OnHitMin.FireOutput( pActivator, this );
			}
		}
		else
		{
			// Fire an output if we just changed from the minimum value
			if ( m_OutValue.Get() == m_flMin )
			{
				m_OnChangedFromMin.FireOutput( pActivator, this );
			}
			m_bHitMin = false;
		}

		fNewValue = clamp(fNewValue, m_flMin, m_flMax);
	}

	m_OutValue.Set(fNewValue, pActivator, this);
}



//-----------------------------------------------------------------------------
// Purpose: Compares a single string input to up to 16 case values, firing an
//			output corresponding to the case value that matched, or a default
//			output if the input value didn't match any of the case values.
//
//			This can also be used to fire a random output from a set of outputs.
//-----------------------------------------------------------------------------
#define MAX_LOGIC_CASES 16

class CLogicCase : public CLogicalEntity
{
	DECLARE_CLASS( CLogicCase, CLogicalEntity );
private:
	[[= ks::reflect::Key{ .name = "Case16", .index = 15 } ]] [[= ks::reflect::Key{ .name = "Case15", .index = 14 } ]] [[= ks::reflect::Key{ .name = "Case14", .index = 13 } ]] [[= ks::reflect::Key{ .name = "Case13", .index = 12 } ]] [[= ks::reflect::Key{ .name = "Case12", .index = 11 } ]] [[= ks::reflect::Key{ .name = "Case11", .index = 10 } ]] [[= ks::reflect::Key{ .name = "Case10", .index = 9 } ]] [[= ks::reflect::Key{ .name = "Case09", .index = 8 } ]] [[= ks::reflect::Key{ .name = "Case08", .index = 7 } ]] [[= ks::reflect::Key{ .name = "Case07", .index = 6 } ]] [[= ks::reflect::Key{ .name = "Case06", .index = 5 } ]] [[= ks::reflect::Key{ .name = "Case05", .index = 4 } ]] [[= ks::reflect::Key{ .name = "Case04", .index = 3 } ]] [[= ks::reflect::Key{ .name = "Case03", .index = 2 } ]] [[= ks::reflect::Key{ .name = "Case02", .index = 1 } ]] [[= ks::reflect::Key{ .name = "Case01", .index = 0 } ]] string_t m_nCase[MAX_LOGIC_CASES];

	int m_nShuffleCases;
	int m_nLastShuffleCase;
	unsigned char m_uchShuffleCaseMap[MAX_LOGIC_CASES];

	void Spawn(void);

	int BuildCaseMap(unsigned char *puchMap);

	// Inputs
	[[= ks::reflect::Input{ .name = "InValue", .type = FIELD_INPUT } ]] void InputValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "PickRandom", .type = FIELD_VOID } ]] void InputPickRandom( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "PickRandomShuffle", .type = FIELD_VOID } ]] void InputPickRandomShuffle( inputdata_t &inputdata );

	// Outputs
	[[= ks::reflect::Key{ .name = "OnCase01", .index = 0 } ]] [[= ks::reflect::Key{ .name = "OnCase02", .index = 1 } ]] [[= ks::reflect::Key{ .name = "OnCase03", .index = 2 } ]] [[= ks::reflect::Key{ .name = "OnCase04", .index = 3 } ]] [[= ks::reflect::Key{ .name = "OnCase05", .index = 4 } ]] [[= ks::reflect::Key{ .name = "OnCase06", .index = 5 } ]] [[= ks::reflect::Key{ .name = "OnCase07", .index = 6 } ]] [[= ks::reflect::Key{ .name = "OnCase08", .index = 7 } ]] [[= ks::reflect::Key{ .name = "OnCase09", .index = 8 } ]] [[= ks::reflect::Key{ .name = "OnCase10", .index = 9 } ]] [[= ks::reflect::Key{ .name = "OnCase11", .index = 10 } ]] [[= ks::reflect::Key{ .name = "OnCase12", .index = 11 } ]] [[= ks::reflect::Key{ .name = "OnCase13", .index = 12 } ]] [[= ks::reflect::Key{ .name = "OnCase14", .index = 13 } ]] [[= ks::reflect::Key{ .name = "OnCase15", .index = 14 } ]] [[= ks::reflect::Key{ .name = "OnCase16", .index = 15 } ]] COutputEvent m_OnCase[MAX_LOGIC_CASES];		// Fired when the input value matches one of the case values.
	[[= ks::reflect::Key{ .name = "OnDefault" } ]] COutputVariant m_OnDefault;					// Fired when no match was found.

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(logic_case, CLogicCase);


IMPLEMENT_REFLECT_DATAMAP( CLogicCase )




//-----------------------------------------------------------------------------
// Purpose: Called before spawning, after key values have been set.
//-----------------------------------------------------------------------------
void CLogicCase::Spawn( void )
{
	m_nLastShuffleCase = -1;
}


//-----------------------------------------------------------------------------
// Purpose: Evaluates the new input value, firing the appropriate OnCaseX output
//			if the input value matches one of the "CaseX" keys.
// Input  : Value - Variant value to compare against the values of the case fields.
//				We use a variant so that we can convert any input type to a string.
//-----------------------------------------------------------------------------
void CLogicCase::InputValue( inputdata_t &inputdata )
{
	const char *pszValue = inputdata.value.String();
	for (int i = 0; i < MAX_LOGIC_CASES; i++)
	{
		if ((m_nCase[i] != NULL_STRING) && !stricmp(STRING(m_nCase[i]), pszValue))
		{
			m_OnCase[i].FireOutput( inputdata.pActivator, this );
			return;
		}
	}
	
	m_OnDefault.Set( inputdata.value, inputdata.pActivator, this );
}


//-----------------------------------------------------------------------------
// Count the number of valid cases, building a packed array
// that maps 0..NumCases to the actual CaseX values.
//
// This allows our zany mappers to set up cases sparsely if they desire.
// NOTE: assumes pnMap points to an array of MAX_LOGIC_CASES
//-----------------------------------------------------------------------------
int CLogicCase::BuildCaseMap(unsigned char *puchCaseMap)
{
	memset(puchCaseMap, 0, sizeof(unsigned char) * MAX_LOGIC_CASES);

	int nNumCases = 0;
	for (int i = 0; i < MAX_LOGIC_CASES; i++)
	{
		if (m_OnCase[i].NumberOfElements() > 0)
		{
			puchCaseMap[nNumCases] = (unsigned char)i;
			nNumCases++;
		}
	}
	
	return nNumCases;
}


//-----------------------------------------------------------------------------
// Purpose: Makes the case statement choose a case at random.
//-----------------------------------------------------------------------------
void CLogicCase::InputPickRandom( inputdata_t &inputdata )
{
	unsigned char uchCaseMap[MAX_LOGIC_CASES];
	int nNumCases = BuildCaseMap( uchCaseMap );

	//
	// Choose a random case from the ones that were set up by the level designer.
	//
	if ( nNumCases > 0 )
	{
		int nRandom = random->RandomInt(0, nNumCases - 1);
		int nCase = (unsigned char)uchCaseMap[nRandom];

		Assert(nCase < MAX_LOGIC_CASES);

		if (nCase < MAX_LOGIC_CASES)
		{
			m_OnCase[nCase].FireOutput( inputdata.pActivator, this );
		}
	}
	else
	{
		DevMsg( 1, "Firing PickRandom input on logic_case %s with no cases set up\n", GetDebugName() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Makes the case statement choose a case at random.
//-----------------------------------------------------------------------------
void CLogicCase::InputPickRandomShuffle( inputdata_t &inputdata )
{
	int nAvoidCase = -1;
	int nCaseCount = m_nShuffleCases;
	
	if ( nCaseCount == 0 )
	{
		// Starting a new shuffle batch.
		nCaseCount = m_nShuffleCases = BuildCaseMap( m_uchShuffleCaseMap );
		
		if ( ( m_nShuffleCases > 1 ) && ( m_nLastShuffleCase != -1 ) )
		{
			// Remove the previously picked case from the case map for this pick only.
			// This avoids repeats across shuffle batch boundaries.		
			nAvoidCase = m_nLastShuffleCase;
			
			for (int i = 0; i < m_nShuffleCases; i++ )
			{
				if ( m_uchShuffleCaseMap[i] == nAvoidCase )
				{
					unsigned char uchSwap = m_uchShuffleCaseMap[i];
					m_uchShuffleCaseMap[i] = m_uchShuffleCaseMap[nCaseCount - 1];
					m_uchShuffleCaseMap[nCaseCount - 1] = uchSwap;
					nCaseCount--;
					break;
				}
			}
		}
	}
	
	//
	// Choose a random case from the ones that were set up by the level designer.
	// Never repeat a case within a shuffle batch, nor consecutively across batches.
	//
	if ( nCaseCount > 0 )
	{
		int nRandom = random->RandomInt( 0, nCaseCount - 1 );

		int nCase = m_uchShuffleCaseMap[nRandom];
		Assert(nCase < MAX_LOGIC_CASES);

		if (nCase < MAX_LOGIC_CASES)
		{
			m_OnCase[nCase].FireOutput( inputdata.pActivator, this );
		}
		
		m_uchShuffleCaseMap[nRandom] = m_uchShuffleCaseMap[m_nShuffleCases - 1];
		m_nShuffleCases--;

		m_nLastShuffleCase = nCase;
	}
	else
	{
		DevMsg( 1, "Firing PickRandom input on logic_case %s with no cases set up\n", GetDebugName() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Compares a floating point input to a predefined value, firing an
//			output to indicate the result of the comparison.
//-----------------------------------------------------------------------------
class CLogicCompare : public CLogicalEntity
{
	DECLARE_CLASS( CLogicCompare, CLogicalEntity );

public:
	int DrawDebugTextOverlays(void);

private:
	// Inputs
	[[= ks::reflect::Input{ .name = "SetValue", .type = FIELD_FLOAT } ]] void InputSetValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetValueCompare", .type = FIELD_FLOAT } ]] void InputSetValueCompare( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetCompareValue", .type = FIELD_FLOAT } ]] void InputSetCompareValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Compare", .type = FIELD_VOID } ]] void InputCompare( inputdata_t &inputdata );

	void DoCompare(CBaseEntity *pActivator, float flInValue);

	[[= ks::reflect::Key{ .name = "InitialValue" } ]] float m_flInValue;					// Place to hold the last input value for a recomparison.
	[[= ks::reflect::Key{ .name = "CompareValue" } ]] float m_flCompareValue;				// The value to compare the input value against.

	// Outputs
	[[= ks::reflect::Key{ .name = "OnLessThan" } ]] COutputFloat m_OnLessThan;			// Fired when the input value is less than the compare value.
	[[= ks::reflect::Key{ .name = "OnEqualTo" } ]] COutputFloat m_OnEqualTo;			// Fired when the input value is equal to the compare value.
	[[= ks::reflect::Key{ .name = "OnNotEqualTo" } ]] COutputFloat m_OnNotEqualTo;		// Fired when the input value is not equal to the compare value.
	[[= ks::reflect::Key{ .name = "OnGreaterThan" } ]] COutputFloat m_OnGreaterThan;		// Fired when the input value is greater than the compare value.

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(logic_compare, CLogicCompare);


IMPLEMENT_REFLECT_DATAMAP( CLogicCompare )




//-----------------------------------------------------------------------------
// Purpose: Input handler for a new input value without performing a comparison.
//-----------------------------------------------------------------------------
void CLogicCompare::InputSetValue( inputdata_t &inputdata )
{
	m_flInValue = inputdata.value.Float();
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for a setting a new value and doing the comparison.
//-----------------------------------------------------------------------------
void CLogicCompare::InputSetValueCompare( inputdata_t &inputdata )
{
	m_flInValue = inputdata.value.Float();
	DoCompare( inputdata.pActivator, m_flInValue );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for a new input value without performing a comparison.
//-----------------------------------------------------------------------------
void CLogicCompare::InputSetCompareValue( inputdata_t &inputdata )
{
	m_flCompareValue = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for forcing a recompare of the last input value.
//-----------------------------------------------------------------------------
void CLogicCompare::InputCompare( inputdata_t &inputdata )
{
	DoCompare( inputdata.pActivator, m_flInValue );
}


//-----------------------------------------------------------------------------
// Purpose: Compares the input value to the compare value, firing the appropriate
//			output(s) based on the comparison result.
// Input  : flInValue - Value to compare against the comparison value.
//-----------------------------------------------------------------------------
void CLogicCompare::DoCompare(CBaseEntity *pActivator, float flInValue)
{
	if (flInValue == m_flCompareValue)
	{
		m_OnEqualTo.Set(flInValue, pActivator, this);
	}
	else
	{
		m_OnNotEqualTo.Set(flInValue, pActivator, this);

		if (flInValue > m_flCompareValue)
		{
			m_OnGreaterThan.Set(flInValue, pActivator, this);
		}
		else
		{
			m_OnLessThan.Set(flInValue, pActivator, this);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CLogicCompare::DrawDebugTextOverlays( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		// print duration
		Q_snprintf(tempstr,sizeof(tempstr),"    Initial Value: %f", m_flInValue);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		// print hold time
		Q_snprintf(tempstr,sizeof(tempstr),"    Compare Value: %f", m_flCompareValue);
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

//-----------------------------------------------------------------------------
// Purpose: Tests a boolean value, firing an output to indicate whether the
//			value was true or false.
//-----------------------------------------------------------------------------
class CLogicBranch : public CLogicalEntity
{
	DECLARE_CLASS( CLogicBranch, CLogicalEntity );
	
public:

	void UpdateOnRemove();

	void AddLogicBranchListener( CBaseEntity *pEntity );
	inline bool GetLogicBranchState();
	virtual int DrawDebugTextOverlays( void );

private:

	enum LogicBranchFire_t
	{
		LOGIC_BRANCH_FIRE,
		LOGIC_BRANCH_NO_FIRE,
	};

	// Inputs
	[[= ks::reflect::Input{ .name = "SetValue", .type = FIELD_BOOLEAN } ]] void InputSetValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetValueTest", .type = FIELD_BOOLEAN } ]] void InputSetValueTest( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ToggleTest", .type = FIELD_VOID } ]] void InputToggleTest( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Test", .type = FIELD_VOID } ]] void InputTest( inputdata_t &inputdata );

	void UpdateValue(bool bNewValue, CBaseEntity *pActivator, LogicBranchFire_t eFire);

	[[= ks::reflect::Key{ .name = "InitialValue" } ]] bool m_bInValue;					// Place to hold the last input value for a future test.
	
	CUtlVector<EHANDLE> m_Listeners;	// A list of logic_branch_listeners that are monitoring us.

	// Outputs
	[[= ks::reflect::Key{ .name = "OnTrue" } ]] COutputEvent m_OnTrue;				// Fired when the value is true.
	[[= ks::reflect::Key{ .name = "OnFalse" } ]] COutputEvent m_OnFalse;				// Fired when the value is false.

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(logic_branch, CLogicBranch);


IMPLEMENT_REFLECT_DATAMAP( CLogicBranch )


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CLogicBranch::UpdateOnRemove()
{
	for ( int i = 0; i < m_Listeners.Count(); i++ )
	{
		CBaseEntity *pEntity = m_Listeners.Element( i ).Get();
		if ( pEntity )
		{
			g_EventQueue.AddEvent( this, "_OnLogicBranchRemoved", 0, this, this );
		}
	}
	
	BaseClass::UpdateOnRemove();
}
	

//-----------------------------------------------------------------------------
// Purpose: Input handler to set a new input value without firing outputs.
// Input  : Boolean value to set.
//-----------------------------------------------------------------------------
void CLogicBranch::InputSetValue( inputdata_t &inputdata )
{
	UpdateValue( inputdata.value.Bool(), inputdata.pActivator, LOGIC_BRANCH_NO_FIRE );
}


//-----------------------------------------------------------------------------
// Purpose: Input handler to set a new input value and fire appropriate outputs.
// Input  : Boolean value to set.
//-----------------------------------------------------------------------------
void CLogicBranch::InputSetValueTest( inputdata_t &inputdata )
{
	UpdateValue( inputdata.value.Bool(), inputdata.pActivator, LOGIC_BRANCH_FIRE );
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for toggling the boolean value without firing outputs.
//-----------------------------------------------------------------------------
void CLogicBranch::InputToggle( inputdata_t &inputdata )
{
	UpdateValue( !m_bInValue, inputdata.pActivator, LOGIC_BRANCH_NO_FIRE );
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for toggling the boolean value and then firing the
//			appropriate output based on the new value.
//-----------------------------------------------------------------------------
void CLogicBranch::InputToggleTest( inputdata_t &inputdata )
{
	UpdateValue( !m_bInValue, inputdata.pActivator, LOGIC_BRANCH_FIRE );
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for forcing a test of the last input value.
//-----------------------------------------------------------------------------
void CLogicBranch::InputTest( inputdata_t &inputdata )
{
	UpdateValue( m_bInValue, inputdata.pActivator, LOGIC_BRANCH_FIRE );
}


//-----------------------------------------------------------------------------
// Purpose: Tests the last input value, firing the appropriate output based on
//			the test result.
// Input  : bInValue - 
//-----------------------------------------------------------------------------
void CLogicBranch::UpdateValue( bool bNewValue, CBaseEntity *pActivator, LogicBranchFire_t eFire )
{
	if ( m_bInValue != bNewValue )
	{
		m_bInValue = bNewValue;

		for ( int i = 0; i < m_Listeners.Count(); i++ )
		{
			CBaseEntity *pEntity = m_Listeners.Element( i ).Get();
			if ( pEntity )
			{
				g_EventQueue.AddEvent( pEntity, "_OnLogicBranchChanged", 0, this, this );
			}
		}
	}

	if ( eFire == LOGIC_BRANCH_FIRE )
	{
		if ( m_bInValue )
		{
			m_OnTrue.FireOutput( pActivator, this );
		}
		else
		{
			m_OnFalse.FireOutput( pActivator, this );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Accessor for logic_branchlist to test the value of the branch on demand.
//-----------------------------------------------------------------------------
bool CLogicBranch::GetLogicBranchState()
{
	return m_bInValue;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CLogicBranch::AddLogicBranchListener( CBaseEntity *pEntity )
{
	if ( m_Listeners.Find( pEntity ) == -1 )
	{
		m_Listeners.AddToTail( pEntity );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CLogicBranch::DrawDebugTextOverlays( void )
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		// print refire time
		Q_snprintf( tempstr, sizeof(tempstr), "Branch value: %s", (m_bInValue) ? "TRUE" : "FALSE" );
		EntityText( text_offset, tempstr, 0 );
		text_offset++;
	}

	return text_offset;
}

//-----------------------------------------------------------------------------
// Purpose: Autosaves when triggered
//-----------------------------------------------------------------------------
class CLogicAutosave : public CLogicalEntity
{
	DECLARE_CLASS( CLogicAutosave, CLogicalEntity );

protected:
	// Inputs
	[[= ks::reflect::Input{ .name = "Save", .type = FIELD_VOID } ]] void InputSave( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SaveDangerous", .type = FIELD_FLOAT } ]] void InputSaveDangerous( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetMinHitpointsThreshold", .type = FIELD_INTEGER } ]] void InputSetMinHitpointsThreshold( inputdata_t &inputdata );

	DECLARE_DATADESC();
	[[= ks::reflect::Key{ .name = "NewLevelUnit" } ]] bool m_bForceNewLevelUnit;
	[[= ks::reflect::Key{ .name = "MinimumHitPoints" } ]] int m_minHitPoints;
	[[= ks::reflect::Key{ .name = "MinHitPointsToCommit" } ]] int m_minHitPointsToCommit;
};

LINK_ENTITY_TO_CLASS(logic_autosave, CLogicAutosave);

IMPLEMENT_REFLECT_DATAMAP( CLogicAutosave )

//-----------------------------------------------------------------------------
// Purpose: Save!
//-----------------------------------------------------------------------------
void CLogicAutosave::InputSave( inputdata_t &inputdata )
{
	if ( m_bForceNewLevelUnit )
	{
		engine->ClearSaveDir();
	}

	engine->ServerCommand( "autosave\n" );
}

//-----------------------------------------------------------------------------
// Purpose: Save safely!
//-----------------------------------------------------------------------------
void CLogicAutosave::InputSaveDangerous( inputdata_t &inputdata )
{
	CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );

	if ( !pPlayer ) 
		return;

	if ( g_ServerGameDLL.m_fAutoSaveDangerousTime != 0.0f && g_ServerGameDLL.m_fAutoSaveDangerousTime >= gpGlobals->curtime )
	{
		// A previous dangerous auto save was waiting to become safe

		if ( pPlayer->GetDeathTime() == 0.0f || pPlayer->GetDeathTime() > gpGlobals->curtime )
		{
			// The player isn't dead, so make the dangerous auto save safe
			engine->ServerCommand( "autosavedangerousissafe\n" );
		}
	}

	if ( m_bForceNewLevelUnit )
	{
		engine->ClearSaveDir();
	}

	if ( pPlayer->GetHealth() >= m_minHitPoints )
	{
		engine->ServerCommand( "autosavedangerous\n" );
		g_ServerGameDLL.m_fAutoSaveDangerousTime = gpGlobals->curtime + inputdata.value.Float();

		// Player must have this much health when we go to commit, or we don't commit.
		g_ServerGameDLL.m_fAutoSaveDangerousMinHealthToCommit = m_minHitPointsToCommit;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Autosaves when triggered
//-----------------------------------------------------------------------------
class CLogicActiveAutosave : public CLogicAutosave
{
	DECLARE_CLASS( CLogicActiveAutosave, CLogicAutosave );

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]]
	void InputEnable( inputdata_t &inputdata )
	{
		m_flStartTime = -1;
		SetThink( &CLogicActiveAutosave::SaveThink );
		SetNextThink( gpGlobals->curtime );
	}

	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]]
	void InputDisable( inputdata_t &inputdata )
	{
		SetThink( NULL );
	}

	void SaveThink()
	{
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		if ( pPlayer )
		{
			if ( m_flStartTime < 0 )
			{
				if ( pPlayer->GetHealth() <= m_minHitPoints )
				{
					m_flStartTime = gpGlobals->curtime;
				}
			}
			else
			{
				if ( pPlayer->GetHealth() >= m_TriggerHitPoints )
				{
					inputdata_t inputdata;
					DevMsg( 2, "logic_active_autosave (%s, %d) triggered\n", STRING( GetEntityName() ), entindex() );
					if ( !m_flDangerousTime )
					{
						InputSave( inputdata );
					}
					else
					{
						inputdata.value.SetFloat( m_flDangerousTime );
						InputSaveDangerous( inputdata );
					}
					m_flStartTime = -1;
				}
				else if ( m_flTimeToTrigger > 0 && gpGlobals->curtime - m_flStartTime > m_flTimeToTrigger )
				{
					m_flStartTime = -1;
				}
			}
		}

		float thinkInterval = ( m_flStartTime < 0 ) ? 1.0 : 0.5;
		SetNextThink( gpGlobals->curtime + thinkInterval );
	}

	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "TriggerHitPoints" } ]] int m_TriggerHitPoints;
	[[= ks::reflect::Key{ .name = "TimeToTrigger" } ]] float m_flTimeToTrigger;
	float m_flStartTime;
	[[= ks::reflect::Key{ .name = "DangerousTime" } ]] float m_flDangerousTime;
};

LINK_ENTITY_TO_CLASS(logic_active_autosave, CLogicActiveAutosave);

IMPLEMENT_REFLECT_DATAMAP( CLogicActiveAutosave )


//-----------------------------------------------------------------------------
// Purpose: Keyfield set func
//-----------------------------------------------------------------------------
void CLogicAutosave::InputSetMinHitpointsThreshold( inputdata_t &inputdata )
{
	int setTo = inputdata.value.Int();
	AssertMsg1(setTo >= 0 && setTo <= 100, "Tried to set autosave MinHitpointsThreshold to %d!\n", setTo);
	m_minHitPoints = setTo;
}

// Finds the named physics object.  If no name, returns the world
// If a name is specified and an object not found - errors are reported
IPhysicsObject *FindPhysicsObjectByNameOrWorld( string_t name, CBaseEntity *pErrorEntity )
{
	if ( !name )
		return g_PhysWorldObject;

	IPhysicsObject *pPhysics = FindPhysicsObjectByName( name.ToCStr(), pErrorEntity );
	if ( !pPhysics )
	{
		DevWarning("%s: can't find %s\n", pErrorEntity->GetClassname(), name.ToCStr());
	}
	return pPhysics;
}

class CLogicCollisionPair : public CLogicalEntity
{
	DECLARE_CLASS( CLogicCollisionPair, CLogicalEntity );
public:

	void EnableCollisions( bool bEnable )
	{
		IPhysicsObject *pPhysics0 = FindPhysicsObjectByNameOrWorld( m_nameAttach1, this );
		IPhysicsObject *pPhysics1 = FindPhysicsObjectByNameOrWorld( m_nameAttach2, this );

		// need two different objects to do anything
		if ( pPhysics0 && pPhysics1 && pPhysics0 != pPhysics1 )
		{
			m_disabled = !bEnable;
			m_succeeded = true;
			if ( bEnable )
			{
				PhysEnableEntityCollisions( pPhysics0, pPhysics1 );
			}
			else
			{
				PhysDisableEntityCollisions( pPhysics0, pPhysics1 );
			}
		}
		else
		{
			m_succeeded = false;
		}
	}

	void Activate( void )
	{
		if ( m_disabled )
		{
			EnableCollisions( false );
		}
		BaseClass::Activate();
	}

	[[= ks::reflect::Input{ .name = "DisableCollisions", .type = FIELD_VOID } ]]
	void InputDisableCollisions( inputdata_t &inputdata )
	{
		if ( m_succeeded && m_disabled )
			return;
		EnableCollisions( false );
	}

	[[= ks::reflect::Input{ .name = "EnableCollisions", .type = FIELD_VOID } ]]
	void InputEnableCollisions( inputdata_t &inputdata )
	{
		if ( m_succeeded && !m_disabled )
			return;
		EnableCollisions( true );
	}
	// If Activate() becomes PostSpawn()
	//void OnRestore() { Activate(); }

	DECLARE_DATADESC();

private:
	[[= ks::reflect::Key{ .name = "attach1" } ]] string_t		m_nameAttach1;
	[[= ks::reflect::Key{ .name = "attach2" } ]] string_t		m_nameAttach2;
	[[= ks::reflect::Key{ .name = "startdisabled" } ]] bool			m_disabled;
	bool			m_succeeded;
};

IMPLEMENT_REFLECT_DATAMAP( CLogicCollisionPair )

LINK_ENTITY_TO_CLASS( logic_collision_pair, CLogicCollisionPair );


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#define MAX_LOGIC_BRANCH_NAMES 16

class CLogicBranchList : public CLogicalEntity
{
	DECLARE_CLASS( CLogicBranchList, CLogicalEntity );

	virtual void Spawn();
	virtual void Activate();
	virtual int DrawDebugTextOverlays( void );

private:

	enum LogicBranchListenerLastState_t
	{
		LOGIC_BRANCH_LISTENER_NOT_INIT = 0,
		LOGIC_BRANCH_LISTENER_ALL_TRUE,
		LOGIC_BRANCH_LISTENER_ALL_FALSE,
		LOGIC_BRANCH_LISTENER_MIXED,
	};

	void DoTest( CBaseEntity *pActivator );

	[[= ks::reflect::Key{ .name = "Branch16", .index = 15 } ]] [[= ks::reflect::Key{ .name = "Branch15", .index = 14 } ]] [[= ks::reflect::Key{ .name = "Branch14", .index = 13 } ]] [[= ks::reflect::Key{ .name = "Branch13", .index = 12 } ]] [[= ks::reflect::Key{ .name = "Branch12", .index = 11 } ]] [[= ks::reflect::Key{ .name = "Branch11", .index = 10 } ]] [[= ks::reflect::Key{ .name = "Branch10", .index = 9 } ]] [[= ks::reflect::Key{ .name = "Branch09", .index = 8 } ]] [[= ks::reflect::Key{ .name = "Branch08", .index = 7 } ]] [[= ks::reflect::Key{ .name = "Branch07", .index = 6 } ]] [[= ks::reflect::Key{ .name = "Branch06", .index = 5 } ]] [[= ks::reflect::Key{ .name = "Branch05", .index = 4 } ]] [[= ks::reflect::Key{ .name = "Branch04", .index = 3 } ]] [[= ks::reflect::Key{ .name = "Branch03", .index = 2 } ]] [[= ks::reflect::Key{ .name = "Branch02", .index = 1 } ]] [[= ks::reflect::Key{ .name = "Branch01", .index = 0 } ]] string_t m_nLogicBranchNames[MAX_LOGIC_BRANCH_NAMES];
	CUtlVector<EHANDLE> m_LogicBranchList;
	LogicBranchListenerLastState_t m_eLastState;

	// Inputs
	[[= ks::reflect::Input{ .name = "_OnLogicBranchRemoved", .type = FIELD_INPUT } ]] void Input_OnLogicBranchRemoved( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "_OnLogicBranchChanged", .type = FIELD_INPUT } ]] void Input_OnLogicBranchChanged( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Test", .type = FIELD_INPUT } ]] void InputTest( inputdata_t &inputdata );

	// Outputs
	[[= ks::reflect::Key{ .name = "OnAllTrue" } ]] COutputEvent m_OnAllTrue;			// Fired when all the registered logic_branches become true.
	[[= ks::reflect::Key{ .name = "OnAllFalse" } ]] COutputEvent m_OnAllFalse;			// Fired when all the registered logic_branches become false.
	[[= ks::reflect::Key{ .name = "OnMixed" } ]] COutputEvent m_OnMixed;				// Fired when one of the registered logic branches changes, but not all are true or false.

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(logic_branch_listener, CLogicBranchList);


IMPLEMENT_REFLECT_DATAMAP( CLogicBranchList )


//-----------------------------------------------------------------------------
// Purpose: Called before spawning, after key values have been set.
//-----------------------------------------------------------------------------
void CLogicBranchList::Spawn( void )
{
}


//-----------------------------------------------------------------------------
// Finds all the logic_branches that we are monitoring and register ourselves with them.
//-----------------------------------------------------------------------------
void CLogicBranchList::Activate( void )
{
	for ( int i = 0; i < MAX_LOGIC_BRANCH_NAMES; i++ )
	{
		CBaseEntity *pEntity = NULL;
		while ( ( pEntity = gEntList.FindEntityGeneric( pEntity, STRING( m_nLogicBranchNames[i] ), this ) ) != NULL )
		{
			if ( FClassnameIs( pEntity, "logic_branch" ) )
			{
				CLogicBranch *pBranch = (CLogicBranch *)pEntity;
				pBranch->AddLogicBranchListener( this );
				m_LogicBranchList.AddToTail( pBranch );
			}
			else
			{
				DevWarning( "logic_branchlist %s refers to entity %s, which is not a logic_branch\n", GetDebugName(), pEntity->GetDebugName() );
			}
		}
	}
	
	BaseClass::Activate();
}


//-----------------------------------------------------------------------------
// Called when a monitored logic branch is deleted from the world, since that
// might affect our final result.
//-----------------------------------------------------------------------------
void CLogicBranchList::Input_OnLogicBranchRemoved( inputdata_t &inputdata )
{
	int nIndex = m_LogicBranchList.Find( inputdata.pActivator );
	if ( nIndex != -1 )
	{
		m_LogicBranchList.FastRemove( nIndex );
	}

	// See if this logic_branch's deletion affects the final result.
	DoTest( inputdata.pActivator );
}


//-----------------------------------------------------------------------------
// Called when the value of a monitored logic branch changes.
//-----------------------------------------------------------------------------
void CLogicBranchList::Input_OnLogicBranchChanged( inputdata_t &inputdata )
{
	DoTest( inputdata.pActivator );
}


//-----------------------------------------------------------------------------
// Input handler to manually test the monitored logic branches and fire the
// appropriate output.
//-----------------------------------------------------------------------------
void CLogicBranchList::InputTest( inputdata_t &inputdata )
{
	// Force an output.
	m_eLastState = LOGIC_BRANCH_LISTENER_NOT_INIT;

	DoTest( inputdata.pActivator );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CLogicBranchList::DoTest( CBaseEntity *pActivator )
{
	bool bOneTrue = false;
	bool bOneFalse = false;
	
	for ( int i = 0; i < m_LogicBranchList.Count(); i++ )
	{
		CLogicBranch *pBranch = (CLogicBranch *)m_LogicBranchList.Element( i ).Get();
		if ( pBranch && pBranch->GetLogicBranchState() )
		{
			bOneTrue = true;
		}
		else
		{
			bOneFalse = true;
		}
	}

	// Only fire the output if the new result differs from the last result.
	if ( bOneTrue && !bOneFalse )
	{
		if ( m_eLastState != LOGIC_BRANCH_LISTENER_ALL_TRUE )
		{
			m_OnAllTrue.FireOutput( pActivator, this );
			m_eLastState = LOGIC_BRANCH_LISTENER_ALL_TRUE;
		}
	}
	else if ( bOneFalse && !bOneTrue )
	{
		if ( m_eLastState != LOGIC_BRANCH_LISTENER_ALL_FALSE )
		{
			m_OnAllFalse.FireOutput( pActivator, this );
			m_eLastState = LOGIC_BRANCH_LISTENER_ALL_FALSE;
		}
	}
	else
	{
		if ( m_eLastState != LOGIC_BRANCH_LISTENER_MIXED )
		{
			m_OnMixed.FireOutput( pActivator, this );
			m_eLastState = LOGIC_BRANCH_LISTENER_MIXED;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CLogicBranchList::DrawDebugTextOverlays( void )
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		for ( int i = 0; i < m_LogicBranchList.Count(); i++ )
		{
			CLogicBranch *pBranch = (CLogicBranch *)m_LogicBranchList.Element( i ).Get();
			if ( pBranch )
			{
				Q_snprintf( tempstr, sizeof(tempstr), "Branch (%s): %s", STRING(pBranch->GetEntityName()), (pBranch->GetLogicBranchState()) ? "TRUE" : "FALSE" );
				EntityText( text_offset, tempstr, 0 );
				text_offset++;
			}
		}
	}

	return text_offset;
}
