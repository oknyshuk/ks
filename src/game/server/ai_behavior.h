#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "ai_component.h"
#include "ai_basenpc.h"
#include "ai_default.h"
#include "ai_criteria.h"
#include "networkvar.h"
#include "generic_classmap.h"

#ifdef DEBUG
import std;
#endif

//-----------------------------------------------------------------------------
// CAI_Behavior...
//
// Purpose:	The core component that defines a behavior in an NPC by selecting
//			schedules and running tasks
//
//			Intended to be used as an organizational tool as well as a way
//			for various NPCs to share behaviors without sharing an inheritance
//			relationship, and without cramming those behaviors into the base
//			NPC class.
//-----------------------------------------------------------------------------

struct AIChannelScheduleState_t
{
	AIChannelScheduleState_t() { std::memset( this, 0, sizeof( *this ) ); }

	bool				 bActive;
	CAI_Schedule *		 pSchedule;
	int					 idealSchedule;
	int					 failSchedule;
	int					 iCurTask;
	TaskStatus_e		 fTaskStatus;
	float				 timeStarted;
	float				 timeCurTaskStarted;
	AI_TaskFailureCode_t taskFailureCode;
	bool				 bScheduleWasInterrupted;

	DECLARE_SIMPLE_DATADESC();
};


//-----------------------------------------------------------------------------
// Purpose: Base class defines interface to behaviors and provides bridging
//			methods
//-----------------------------------------------------------------------------

class CAI_BehaviorBase : public CAI_Component, public IAI_BehaviorBridge
{
	DECLARE_CLASS( CAI_BehaviorBase, CAI_Component )
public:
	CAI_BehaviorBase(CAI_BaseNPC *pOuter = nullptr)
	 : 	CAI_Component(pOuter),
	 	m_pBackBridge(nullptr)
	{
		m_bAllocated = false;
	}

	void	SetAllocated( ) { m_bAllocated = true; }
	bool	IsAllocated( ) { return m_bAllocated; }

	// Bridges from the host back into this behavior
	void BehaviorBridge_PrescheduleThink() { PrescheduleThink(); }
	void BehaviorBridge_OnScheduleChange() { OnScheduleChange(); }
	bool BehaviorBridge_IsCrouching() { return IsCrouching(); }
	bool BehaviorBridge_IsValidEnemy( CBaseEntity * a1 ) { return IsValidEnemy( a1 ); }
	CBaseEntity * BehaviorBridge_BestEnemy() { return BestEnemy(); }
	bool BehaviorBridge_IsCoverPosition( const Vector & a1, const Vector & a2 ) { return IsCoverPosition( a1, a2 ); }
	bool BehaviorBridge_IsValidCover( const Vector & a1, CAI_Hint const * a2 ) { return IsValidCover( a1, a2 ); }
	bool BehaviorBridge_IsValidShootPosition( const Vector & a1, CAI_Node * a2, CAI_Hint const * a3 ) { return IsValidShootPosition( a1, a2, a3 ); }
	bool BehaviorBridge_WeaponLOSCondition( const Vector & a1, const Vector & a2, bool a3 ) { return WeaponLOSCondition( a1, a2, a3 ); }
	float BehaviorBridge_GetMaxTacticalLateralMovement() { return GetMaxTacticalLateralMovement(); }
	bool BehaviorBridge_ShouldIgnoreSound( CSound * a1 ) { return ShouldIgnoreSound( a1 ); }
	void BehaviorBridge_OnSeeEntity( CBaseEntity * a1 ) { OnSeeEntity( a1 ); }
	void BehaviorBridge_OnFriendDamaged( CBaseCombatCharacter * a1, CBaseEntity * a2 ) { OnFriendDamaged( a1, a2 ); }
	bool BehaviorBridge_IsInterruptable() { return IsInterruptable(); }
	bool BehaviorBridge_IsNavigationUrgent() { return IsNavigationUrgent(); }
	bool BehaviorBridge_ShouldPlayerAvoid() { return ShouldPlayerAvoid(); }
	int BehaviorBridge_OnTakeDamage_Alive( const CTakeDamageInfo & a1 ) { return OnTakeDamage_Alive( a1 ); }
	float BehaviorBridge_GetDefaultNavGoalTolerance() { return GetDefaultNavGoalTolerance(); }
	float BehaviorBridge_GetReasonableFacingDist() { return GetReasonableFacingDist(); }
	bool BehaviorBridge_CanFlinch() { return CanFlinch(); }
	bool BehaviorBridge_IsCrouchedActivity( Activity a1 ) { return IsCrouchedActivity( a1 ); }
	bool BehaviorBridge_QueryHearSound( CSound * a1 ) { return QueryHearSound( a1 ); }
	bool BehaviorBridge_CanRunAScriptedNPCInteraction( bool a1 ) { return CanRunAScriptedNPCInteraction( a1 ); }
	Activity BehaviorBridge_GetFlinchActivity( bool a1, bool a2 ) { return GetFlinchActivity( a1, a2 ); }
	bool BehaviorBridge_OnCalcBaseMove( AILocalMoveGoal_t * a1, float a2, AIMoveResult_t * a3 ) { return OnCalcBaseMove( a1, a2, a3 ); }
	void BehaviorBridge_ModifyOrAppendCriteria( AI_CriteriaSet& a1 ) { ModifyOrAppendCriteria( a1 ); }
	void BehaviorBridge_Teleport( const Vector * a1, const QAngle * a2, const Vector * a3, bool a4 ) { Teleport( a1, a2, a3, a4 ); }
	void BehaviorBridge_HandleAnimEvent( animevent_t * a1 ) { HandleAnimEvent( a1 ); }
	bool BehaviorBridge_FValidateHintType( CAI_Hint * a1 ) { return FValidateHintType( a1 ); }
	bool BehaviorBridge_ShouldAlwaysThink() { return ShouldAlwaysThink(); }
	bool BehaviorBridge_IsCurTaskContinuousMove() { return IsCurTaskContinuousMove(); }
	void BehaviorBridge_AimGun() { AimGun(); }
	Activity BehaviorBridge_NPC_TranslateActivity( Activity a1 ) { return NPC_TranslateActivity( a1 ); }
	void BehaviorBridge_OnMovementFailed() { OnMovementFailed(); }
	void BehaviorBridge_OnMovementComplete() { OnMovementComplete(); }

	// Behavior methods that forward to the host through the bridge
	virtual void PrescheduleThink() { m_pBackBridge->BehaviorBridge_PrescheduleThink(); }
	virtual void OnScheduleChange() { m_pBackBridge->BehaviorBridge_OnScheduleChange(); }
	virtual bool IsCrouching() { return m_pBackBridge->BehaviorBridge_IsCrouching(); }
	virtual bool IsValidEnemy( CBaseEntity * a1 ) { return m_pBackBridge->BehaviorBridge_IsValidEnemy( a1 ); }
	virtual CBaseEntity * BestEnemy() { return m_pBackBridge->BehaviorBridge_BestEnemy(); }
	virtual bool IsCoverPosition( const Vector & a1, const Vector & a2 ) { return m_pBackBridge->BehaviorBridge_IsCoverPosition( a1, a2 ); }
	virtual bool IsValidCover( const Vector & a1, CAI_Hint const * a2 ) { return m_pBackBridge->BehaviorBridge_IsValidCover( a1, a2 ); }
	virtual bool IsValidShootPosition( const Vector & a1, CAI_Node * a2, CAI_Hint const * a3 ) { return m_pBackBridge->BehaviorBridge_IsValidShootPosition( a1, a2, a3 ); }
	virtual bool WeaponLOSCondition( const Vector & a1, const Vector & a2, bool a3 ) { return m_pBackBridge->BehaviorBridge_WeaponLOSCondition( a1, a2, a3 ); }
	virtual float GetMaxTacticalLateralMovement() { return m_pBackBridge->BehaviorBridge_GetMaxTacticalLateralMovement(); }
	virtual bool ShouldIgnoreSound( CSound * a1 ) { return m_pBackBridge->BehaviorBridge_ShouldIgnoreSound( a1 ); }
	virtual void OnSeeEntity( CBaseEntity * a1 ) { m_pBackBridge->BehaviorBridge_OnSeeEntity( a1 ); }
	virtual void OnFriendDamaged( CBaseCombatCharacter * a1, CBaseEntity * a2 ) { m_pBackBridge->BehaviorBridge_OnFriendDamaged( a1, a2 ); }
	virtual bool IsInterruptable() { return m_pBackBridge->BehaviorBridge_IsInterruptable(); }
	virtual bool IsNavigationUrgent() { return m_pBackBridge->BehaviorBridge_IsNavigationUrgent(); }
	virtual bool ShouldPlayerAvoid() { return m_pBackBridge->BehaviorBridge_ShouldPlayerAvoid(); }
	virtual int OnTakeDamage_Alive( const CTakeDamageInfo & a1 ) { return m_pBackBridge->BehaviorBridge_OnTakeDamage_Alive( a1 ); }
	virtual float GetDefaultNavGoalTolerance() { return m_pBackBridge->BehaviorBridge_GetDefaultNavGoalTolerance(); }
	virtual float GetReasonableFacingDist() { return m_pBackBridge->BehaviorBridge_GetReasonableFacingDist(); }
	virtual bool CanFlinch() { return m_pBackBridge->BehaviorBridge_CanFlinch(); }
	virtual bool IsCrouchedActivity( Activity a1 ) { return m_pBackBridge->BehaviorBridge_IsCrouchedActivity( a1 ); }
	virtual bool QueryHearSound( CSound * a1 ) { return m_pBackBridge->BehaviorBridge_QueryHearSound( a1 ); }
	virtual bool CanRunAScriptedNPCInteraction( bool a1 ) { return m_pBackBridge->BehaviorBridge_CanRunAScriptedNPCInteraction( a1 ); }
	virtual Activity GetFlinchActivity( bool a1, bool a2 ) { return m_pBackBridge->BehaviorBridge_GetFlinchActivity( a1, a2 ); }
	virtual bool OnCalcBaseMove( AILocalMoveGoal_t * a1, float a2, AIMoveResult_t * a3 ) { return m_pBackBridge->BehaviorBridge_OnCalcBaseMove( a1, a2, a3 ); }
	virtual void ModifyOrAppendCriteria( AI_CriteriaSet& a1 ) { m_pBackBridge->BehaviorBridge_ModifyOrAppendCriteria( a1 ); }
	virtual void Teleport( const Vector * a1, const QAngle * a2, const Vector * a3, bool a4 ) { m_pBackBridge->BehaviorBridge_Teleport( a1, a2, a3, a4 ); }
	virtual void HandleAnimEvent( animevent_t * a1 ) { m_pBackBridge->BehaviorBridge_HandleAnimEvent( a1 ); }
	virtual bool FValidateHintType( CAI_Hint * a1 ) { return m_pBackBridge->BehaviorBridge_FValidateHintType( a1 ); }
	virtual bool ShouldAlwaysThink() { return m_pBackBridge->BehaviorBridge_ShouldAlwaysThink(); }
	virtual bool IsCurTaskContinuousMove() { return m_pBackBridge->BehaviorBridge_IsCurTaskContinuousMove(); }
	virtual void AimGun() { m_pBackBridge->BehaviorBridge_AimGun(); }
	virtual Activity NPC_TranslateActivity( Activity a1 ) { return m_pBackBridge->BehaviorBridge_NPC_TranslateActivity( a1 ); }
	virtual void OnMovementFailed() { m_pBackBridge->BehaviorBridge_OnMovementFailed(); }
	virtual void OnMovementComplete() { m_pBackBridge->BehaviorBridge_OnMovementComplete(); }

	virtual const char *GetClassNameV() { return ""; }
	virtual const char *GetName() = 0;

	virtual bool DeleteOnHostDestroy() { return m_bAllocated; } // @QUESTION: should switch to reference count?

	virtual bool KeyValue( const char *szKeyName, const char *szValue ) 
	{
		return false;
	}
	
	bool IsRunning()								{ contract_assert( GetOuter() ); return ( GetOuter()->GetPrimaryBehavior() == this ); }
	virtual bool CanSelectSchedule()				{ return true; }
	virtual void BeginScheduleSelection() 			{}
	virtual void EndScheduleSelection() 			{}

	void SetBackBridge( IAI_BehaviorBridge *pBackBridge )
	{
		contract_assert( m_pBackBridge == nullptr || pBackBridge == nullptr );
		m_pBackBridge = pBackBridge;
	}

	virtual void Precache()										{}
	virtual void Spawn()										{}
	virtual void UpdateOnRemove()								{}
	virtual void Event_Killed( const CTakeDamageInfo &info )	{}
	virtual void CleanupOnDeath( CBaseEntity *pCulprit, bool bFireDeathOutput ) {}

	virtual void OnChangeHintGroup( string_t oldGroup, string_t newGroup ) {}

	void BridgeOnStartSchedule( int scheduleType );

	int  BridgeSelectSchedule();
	bool BridgeSelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode, int *pResult );
	bool BridgeStartTask( const Task_t *pTask );
	bool BridgeRunTask( const Task_t *pTask);

	int BridgeTranslateSchedule( int scheduleType );
	bool BridgeGetSchedule( int localScheduleID, CAI_Schedule **ppResult );
	bool BridgeTaskName(int taskID, const char **);

	virtual void BuildScheduleTestBits() {}
	virtual void BuildScheduleTestBitsNotActive() {}

	virtual void GatherConditions();
	virtual void GatherConditionsNotActive() { return; } // Override this and your behavior will call this in place of GatherConditions() when your behavior is NOT the active one.
	virtual void OnUpdateShotRegulator() {}

	virtual float GetJumpGravity() const;
	virtual bool IsJumpLegal( const Vector &startPos, const Vector &apex, const Vector &endPos, float maxUp, float maxDown, float maxDist ) const;
	virtual bool MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost );

	virtual void OnChangeActiveWeapon( CBaseCombatWeapon *pOldWeapon, CBaseCombatWeapon *pNewWeapon ) {};

	virtual CAI_ClassScheduleIdSpace *GetClassScheduleIdSpace();

	virtual int  DrawDebugTextOverlays( int text_offset );

	virtual void OnRestore() {}

public:
	//
	// Secondary schedule channel support
	//
	void StartChannel( int channel );
	void StopChannel( int channel );

	void MaintainChannelSchedules();
	void MaintainSchedule( int channel );

	void SetSchedule( int channel, CAI_Schedule *pNewSchedule );
	bool SetSchedule( int channel, int localScheduleID );

	void ClearSchedule( int channel, const char *szReason );

	CAI_Schedule *GetCurSchedule( int channel );
	bool IsCurSchedule( int channel, int schedId, bool fIdeal = true );
	virtual void OnScheduleChange( int channel );

	virtual void OnStartSchedule( int channel, int scheduleType );

	virtual int SelectSchedule( int channel );
	virtual int SelectFailSchedule(  int channel, int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	virtual int TranslateSchedule( int channel, int scheduleType ) { return scheduleType; }

	virtual void StartTask( int channel, const Task_t *pTask );
	virtual void RunTask( int channel, const Task_t *pTask );

	const Task_t *GetCurTask( void ) { return BaseClass::GetCurTask(); }
	const Task_t *GetCurTask( int channel );

	bool TaskIsComplete( int channel )	{ return ( m_ScheduleChannels[channel].fTaskStatus == TASKSTATUS_COMPLETE ); }
	int TaskIsComplete()	{ return BaseClass::TaskIsComplete(); }

	virtual void TaskFail( AI_TaskFailureCode_t code ) { BaseClass::TaskFail( code ) ; } 
	void TaskFail( const char *pszGeneralFailText )	{ BaseClass::TaskFail( pszGeneralFailText ); }
	void TaskComplete( bool fIgnoreSetFailedCondition = false ) { BaseClass::TaskComplete( fIgnoreSetFailedCondition ); }

	virtual void TaskFail( int channel, AI_TaskFailureCode_t code );
	void TaskFail( int channel, const char *pszGeneralFailText )	{ TaskFail( channel, MakeFailCode( pszGeneralFailText ) ); }
	void TaskComplete( int channel, bool fIgnoreSetFailedCondition = false );

private:
	bool IsScheduleValid( AIChannelScheduleState_t *pScheduleState );
	CAI_Schedule *GetNewSchedule( int channel );
	CAI_Schedule *GetFailSchedule( AIChannelScheduleState_t *pScheduleState );
	const Task_t *GetTask( AIChannelScheduleState_t *pScheduleState );


	std::vector< AIChannelScheduleState_t > m_ScheduleChannels;

protected:
	int GetNpcState() { return GetOuter()->m_NPCState; }

	virtual void OnStartSchedule( int scheduleType );

	virtual int SelectSchedule();
	virtual int	SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	virtual void StartTask( const Task_t *pTask );
	virtual void RunTask( const Task_t *pTask );
	virtual int TranslateSchedule( int scheduleType );
	virtual CAI_Schedule *GetSchedule(int schedule);
	virtual const char *GetSchedulingErrorName();
	bool IsCurSchedule( int schedId, bool fIdeal = true );


	CAI_Hint *		GetHintNode()							{ return GetOuter()->GetHintNode(); }
	const CAI_Hint *GetHintNode() const						{ return GetOuter()->GetHintNode(); }
	void			SetHintNode( CAI_Hint *pHintNode )		{ GetOuter()->SetHintNode( pHintNode ); }
	void			ClearHintNode( float reuseDelay = 0.0 )	{ GetOuter()->ClearHintNode( reuseDelay ); }
	string_t		GetHintGroup()						{ return GetOuter()->GetHintGroup();	}
	void			ClearHintGroup()					{ GetOuter()->ClearHintGroup();			}
	void			SetHintGroup( string_t name )		{ GetOuter()->SetHintGroup( name );		}


	// For now, only support simple behavior stack:
	void BehaviorBridge_GatherConditions()	{ (m_pBackBridge)->BehaviorBridge_GatherConditions(); };
	int BehaviorBridge_SelectSchedule()	{ return (m_pBackBridge)->BehaviorBridge_SelectSchedule(); };
	int BehaviorBridge_TranslateSchedule( int a1 )	{ return (m_pBackBridge)->BehaviorBridge_TranslateSchedule( a1 ); };


protected:
	// Used by derived classes to chain a task to a task that might not be the 
	// one they are currently handling:
	void	ChainStartTask( int task, float taskData = 0 );
	void	ChainRunTask( int task, float taskData = 0 );

protected:



	bool NotifyChangeBehaviorStatus( bool fCanFinishSchedule = false );

	bool HaveSequenceForActivity( Activity activity )		{ return GetOuter()->HaveSequenceForActivity( activity ); }
	
	//---------------------------------

	//
	// These allow derived classes to implement custom schedules
	//
	static CAI_GlobalScheduleNamespace *GetSchedulingSymbols()		{ return CAI_BaseNPC::GetSchedulingSymbols(); }
	static bool				LoadSchedules()							{ return true; }
	virtual bool			IsBehaviorSchedule( int scheduleType )	{ return false; }

	CAI_Navigator *			GetNavigator() 							{ return GetOuter()->GetNavigator(); 		}
	CAI_Motor *				GetMotor() 								{ return GetOuter()->GetMotor(); 			}
	CAI_TacticalServices *	GetTacticalServices()					{ return GetOuter()->GetTacticalServices();	}

	bool 				 m_fOverrode;
	IAI_BehaviorBridge *m_pBackBridge;

	bool				m_bAllocated;

public:
	static CGenericClassmap< CAI_BehaviorBase > *GetBehaviorClasses();
private:
	static CGenericClassmap< CAI_BehaviorBase > *GetBehaviorClassesInternal();

private:
	
	DECLARE_DATADESC();
};

#define LINK_BEHAVIOR_TO_CLASS( localName, className )													\
	static CAI_BehaviorBase *C##className##Factory( void )												\
	{																									\
		return static_cast< CAI_BehaviorBase * >( new className );										\
	};																									\
	class C##localName##Foo																				\
	{																									\
	public:																								\
		C##localName##Foo( void )																		\
		{																								\
			CAI_BehaviorBase::GetBehaviorClasses()->Add( #localName, #className,							\
				sizeof( className ),&C##className##Factory );											\
		}																								\
	};																									\
	static C##localName##Foo g_C##localName##Foo;

#define LINK_BEHAVIOR_TO_CLASSNAME( className )															\
	static CAI_BehaviorBase *C##className##Factory( void )												\
	{																									\
		return static_cast< CAI_BehaviorBase * >( new className );										\
	};																									\
	class C##className##Foo																				\
	{																									\
	public:																								\
		C##className##Foo( void )																		\
		{																								\
		CAI_BehaviorBase::GetBehaviorClasses()->Add( className::GetClassName(), #className,				\
				sizeof( className ),&C##className##Factory );											\
		}																								\
	};																									\
	static C##className##Foo g_C##className##Foo;

//-----------------------------------------------------------------------------
// Purpose: Template provides provides back bridge to owning class and 
//			establishes namespace settings
//-----------------------------------------------------------------------------

template <class NPC_CLASS = CAI_BaseNPC, const int ID_SPACE_OFFSET = 100000>
class CAI_Behavior : public CAI_ComponentWithOuter<NPC_CLASS, CAI_BehaviorBase>
{
public:
	DECLARE_CLASS_NOFRIEND( CAI_Behavior, NPC_CLASS );
	
	enum
	{
		NEXT_TASK 			= ID_SPACE_OFFSET,
		NEXT_SCHEDULE 		= ID_SPACE_OFFSET,
		NEXT_CONDITION 		= ID_SPACE_OFFSET,
		NEXT_CHANNEL		= ID_SPACE_OFFSET,
	};

	void SetCondition( int condition )
	{
		if ( condition >= ID_SPACE_OFFSET && condition < ID_SPACE_OFFSET + 10000 ) // it's local to us
			condition = GetClassScheduleIdSpace()->ConditionLocalToGlobal( condition );
		this->GetOuter()->SetCondition( condition );
	}

	bool HasCondition( int condition )
	{
		if ( condition >= ID_SPACE_OFFSET && condition < ID_SPACE_OFFSET + 10000 ) // it's local to us
			condition = GetClassScheduleIdSpace()->ConditionLocalToGlobal( condition );
		return this->GetOuter()->HasCondition( condition );
	}

	bool HasInterruptCondition( int condition )
	{
		if ( condition >= ID_SPACE_OFFSET && condition < ID_SPACE_OFFSET + 10000 ) // it's local to us
			condition = GetClassScheduleIdSpace()->ConditionLocalToGlobal( condition );
		return this->GetOuter()->HasInterruptCondition( condition );
	}

	void ClearCondition( int condition )
	{
		if ( condition >= ID_SPACE_OFFSET && condition < ID_SPACE_OFFSET + 10000 ) // it's local to us
			condition = GetClassScheduleIdSpace()->ConditionLocalToGlobal( condition );
		this->GetOuter()->ClearCondition( condition );
	}

protected:
	CAI_Behavior(NPC_CLASS *pOuter = nullptr)
	 : CAI_ComponentWithOuter<NPC_CLASS, CAI_BehaviorBase>(pOuter)
	{
	}

	static CAI_GlobalScheduleNamespace *GetSchedulingSymbols()
	{
		return NPC_CLASS::GetSchedulingSymbols();
	}
	virtual CAI_ClassScheduleIdSpace *GetClassScheduleIdSpace()
	{
		return this->GetOuter()->GetClassScheduleIdSpace();
	}

	static CAI_ClassScheduleIdSpace &AccessClassScheduleIdSpaceDirect()
	{
		return NPC_CLASS::AccessClassScheduleIdSpaceDirect();
	}

private:
	virtual bool IsBehaviorSchedule( int scheduleType ) { return ( scheduleType >= ID_SPACE_OFFSET && scheduleType < ID_SPACE_OFFSET + 10000 ); }
};


//-----------------------------------------------------------------------------
// Purpose: The common instantiation of the above template
//-----------------------------------------------------------------------------

typedef CAI_Behavior<> CAI_SimpleBehavior;

//-----------------------------------------------------------------------------
// Purpose: Base class for AIs that want to act as a host for CAI_Behaviors
//			NPCs aren't required to use this, but probably want to.
//-----------------------------------------------------------------------------

template <class BASE_NPC>
class CAI_BehaviorHostBase : public BASE_NPC
{
	DECLARE_CLASS( CAI_BehaviorHostBase, BASE_NPC );

protected:
	CAI_BehaviorHostBase()
	{
	}

};

template <class BASE_NPC>
class CAI_BehaviorHost : public CAI_BehaviorHostBase<BASE_NPC>
{
	DECLARE_CLASS( CAI_BehaviorHost, CAI_BehaviorHostBase<BASE_NPC> );
public:

	CAI_BehaviorHost()
	{
	}

	// Bridges from the behavior back to the host's base class
	void BehaviorBridge_PrescheduleThink() { BaseClass::PrescheduleThink(); }
	void BehaviorBridge_OnScheduleChange() { BaseClass::OnScheduleChange(); }
	bool BehaviorBridge_IsCrouching() { return BaseClass::IsCrouching(); }
	bool BehaviorBridge_IsValidEnemy( CBaseEntity * a1 ) { return BaseClass::IsValidEnemy( a1 ); }
	CBaseEntity * BehaviorBridge_BestEnemy() { return BaseClass::BestEnemy(); }
	bool BehaviorBridge_IsCoverPosition( const Vector & a1, const Vector & a2 ) { return BaseClass::IsCoverPosition( a1, a2 ); }
	bool BehaviorBridge_IsValidCover( const Vector & a1, CAI_Hint const * a2 ) { return BaseClass::IsValidCover( a1, a2 ); }
	bool BehaviorBridge_IsValidShootPosition( const Vector & a1, CAI_Node * a2, CAI_Hint const * a3 ) { return BaseClass::IsValidShootPosition( a1, a2, a3 ); }
	bool BehaviorBridge_WeaponLOSCondition( const Vector & a1, const Vector & a2, bool a3 ) { return BaseClass::WeaponLOSCondition( a1, a2, a3 ); }
	float BehaviorBridge_GetMaxTacticalLateralMovement() { return BaseClass::GetMaxTacticalLateralMovement(); }
	bool BehaviorBridge_ShouldIgnoreSound( CSound * a1 ) { return BaseClass::ShouldIgnoreSound( a1 ); }
	void BehaviorBridge_OnSeeEntity( CBaseEntity * a1 ) { BaseClass::OnSeeEntity( a1 ); }
	void BehaviorBridge_OnFriendDamaged( CBaseCombatCharacter * a1, CBaseEntity * a2 ) { BaseClass::OnFriendDamaged( a1, a2 ); }
	bool BehaviorBridge_IsInterruptable() { return BaseClass::IsInterruptable(); }
	bool BehaviorBridge_IsNavigationUrgent() { return BaseClass::IsNavigationUrgent(); }
	bool BehaviorBridge_ShouldPlayerAvoid() { return BaseClass::ShouldPlayerAvoid(); }
	int BehaviorBridge_OnTakeDamage_Alive( const CTakeDamageInfo & a1 ) { return BaseClass::OnTakeDamage_Alive( a1 ); }
	float BehaviorBridge_GetDefaultNavGoalTolerance() { return BaseClass::GetDefaultNavGoalTolerance(); }
	float BehaviorBridge_GetReasonableFacingDist() { return BaseClass::GetReasonableFacingDist(); }
	bool BehaviorBridge_CanFlinch() { return BaseClass::CanFlinch(); }
	bool BehaviorBridge_IsCrouchedActivity( Activity a1 ) { return BaseClass::IsCrouchedActivity( a1 ); }
	bool BehaviorBridge_QueryHearSound( CSound * a1 ) { return BaseClass::QueryHearSound( a1 ); }
	bool BehaviorBridge_CanRunAScriptedNPCInteraction( bool a1 ) { return BaseClass::CanRunAScriptedNPCInteraction( a1 ); }
	Activity BehaviorBridge_GetFlinchActivity( bool a1, bool a2 ) { return BaseClass::GetFlinchActivity( a1, a2 ); }
	bool BehaviorBridge_OnCalcBaseMove( AILocalMoveGoal_t * a1, float a2, AIMoveResult_t * a3 ) { return BaseClass::OnCalcBaseMove( a1, a2, a3 ); }
	void BehaviorBridge_ModifyOrAppendCriteria( AI_CriteriaSet& a1 ) { BaseClass::ModifyOrAppendCriteria( a1 ); }
	void BehaviorBridge_Teleport( const Vector * a1, const QAngle * a2, const Vector * a3, bool a4 ) { BaseClass::Teleport( a1, a2, a3, a4 ); }
	void BehaviorBridge_HandleAnimEvent( animevent_t * a1 ) { BaseClass::HandleAnimEvent( a1 ); }
	bool BehaviorBridge_FValidateHintType( CAI_Hint * a1 ) { return BaseClass::FValidateHintType( a1 ); }
	bool BehaviorBridge_ShouldAlwaysThink() { return BaseClass::ShouldAlwaysThink(); }
	bool BehaviorBridge_IsCurTaskContinuousMove() { return BaseClass::IsCurTaskContinuousMove(); }
	void BehaviorBridge_AimGun() { BaseClass::AimGun(); }
	Activity BehaviorBridge_NPC_TranslateActivity( Activity a1 ) { return BaseClass::NPC_TranslateActivity( a1 ); }
	void BehaviorBridge_OnMovementFailed() { BaseClass::OnMovementFailed(); }
	void BehaviorBridge_OnMovementComplete() { BaseClass::OnMovementComplete(); }

	// Host methods that forward to the primary behavior when one is active
	void PrescheduleThink() { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->PrescheduleThink(); else BaseClass::PrescheduleThink(); }
	void OnScheduleChange() { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->OnScheduleChange(); else BaseClass::OnScheduleChange(); }
	bool IsCrouching() { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->IsCrouching(); else return BaseClass::IsCrouching(); }
	bool IsValidEnemy( CBaseEntity * a1 ) { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->IsValidEnemy( a1 ); else return BaseClass::IsValidEnemy( a1 ); }
	CBaseEntity * BestEnemy() { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->BestEnemy(); else return BaseClass::BestEnemy(); }
	bool IsCoverPosition( const Vector & a1, const Vector & a2 ) { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->IsCoverPosition( a1, a2 ); else return BaseClass::IsCoverPosition( a1, a2 ); }
	bool IsValidCover( const Vector & a1, CAI_Hint const * a2 ) { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->IsValidCover( a1, a2 ); else return BaseClass::IsValidCover( a1, a2 ); }
	bool IsValidShootPosition( const Vector & a1, CAI_Node * a2, CAI_Hint const * a3 ) { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->IsValidShootPosition( a1, a2, a3 ); else return BaseClass::IsValidShootPosition( a1, a2, a3 ); }
	bool WeaponLOSCondition( const Vector & a1, const Vector & a2, bool a3 ) { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->WeaponLOSCondition( a1, a2, a3 ); else return BaseClass::WeaponLOSCondition( a1, a2, a3 ); }
	float GetMaxTacticalLateralMovement() { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->GetMaxTacticalLateralMovement(); else return BaseClass::GetMaxTacticalLateralMovement(); }
	bool ShouldIgnoreSound( CSound * a1 ) { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->ShouldIgnoreSound( a1 ); else return BaseClass::ShouldIgnoreSound( a1 ); }
	void OnSeeEntity( CBaseEntity * a1 ) { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->OnSeeEntity( a1 ); else BaseClass::OnSeeEntity( a1 ); }
	void OnFriendDamaged( CBaseCombatCharacter * a1, CBaseEntity * a2 ) { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->OnFriendDamaged( a1, a2 ); else BaseClass::OnFriendDamaged( a1, a2 ); }
	bool IsInterruptable() { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->IsInterruptable(); else return BaseClass::IsInterruptable(); }
	bool IsNavigationUrgent() { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->IsNavigationUrgent(); else return BaseClass::IsNavigationUrgent(); }
	bool ShouldPlayerAvoid() { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->ShouldPlayerAvoid(); else return BaseClass::ShouldPlayerAvoid(); }
	int OnTakeDamage_Alive( const CTakeDamageInfo & a1 ) { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->OnTakeDamage_Alive( a1 ); else return BaseClass::OnTakeDamage_Alive( a1 ); }
	float GetDefaultNavGoalTolerance() { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->GetDefaultNavGoalTolerance(); else return BaseClass::GetDefaultNavGoalTolerance(); }
	float GetReasonableFacingDist() { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->GetReasonableFacingDist(); else return BaseClass::GetReasonableFacingDist(); }
	bool CanFlinch() { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->CanFlinch(); else return BaseClass::CanFlinch(); }
	bool IsCrouchedActivity( Activity a1 ) { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->IsCrouchedActivity( a1 ); else return BaseClass::IsCrouchedActivity( a1 ); }
	bool QueryHearSound( CSound * a1 ) { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->QueryHearSound( a1 ); else return BaseClass::QueryHearSound( a1 ); }
	bool CanRunAScriptedNPCInteraction( bool a1 ) { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->CanRunAScriptedNPCInteraction( a1 ); else return BaseClass::CanRunAScriptedNPCInteraction( a1 ); }
	Activity GetFlinchActivity( bool a1, bool a2 ) { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->GetFlinchActivity( a1, a2 ); else return BaseClass::GetFlinchActivity( a1, a2 ); }
	bool OnCalcBaseMove( AILocalMoveGoal_t * a1, float a2, AIMoveResult_t * a3 ) { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->OnCalcBaseMove( a1, a2, a3 ); else return BaseClass::OnCalcBaseMove( a1, a2, a3 ); }
	void ModifyOrAppendCriteria( AI_CriteriaSet& a1 ) { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->ModifyOrAppendCriteria( a1 ); else BaseClass::ModifyOrAppendCriteria( a1 ); }
	void Teleport( const Vector * a1, const QAngle * a2, const Vector * a3, bool a4 ) { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->Teleport( a1, a2, a3, a4 ); else BaseClass::Teleport( a1, a2, a3, a4 ); }
	void HandleAnimEvent( animevent_t * a1 ) { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->HandleAnimEvent( a1 ); else BaseClass::HandleAnimEvent( a1 ); }
	bool FValidateHintType( CAI_Hint * a1 ) { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FValidateHintType( a1 ); else return BaseClass::FValidateHintType( a1 ); }
	bool ShouldAlwaysThink() { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->ShouldAlwaysThink(); else return BaseClass::ShouldAlwaysThink(); }
	bool IsCurTaskContinuousMove() { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->IsCurTaskContinuousMove(); else return BaseClass::IsCurTaskContinuousMove(); }
	void AimGun() { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->AimGun(); else BaseClass::AimGun(); }
	Activity NPC_TranslateActivity( Activity a1 ) { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->NPC_TranslateActivity( a1 ); else return BaseClass::NPC_TranslateActivity( a1 ); }
	void OnMovementFailed() { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->OnMovementFailed(); else BaseClass::OnMovementFailed(); }
	void OnMovementComplete() { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->OnMovementComplete(); else BaseClass::OnMovementComplete(); }

	void CleanupOnDeath( CBaseEntity *pCulprit = nullptr, bool bFireDeathOutput = true );


	// Bridges
	void			Precache();
	void			UpdateOnRemove();
	void			Event_Killed( const CTakeDamageInfo &info );
	void 			GatherConditions();
	int 			SelectSchedule();
	void			KeepRunningBehavior();
	int				SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	void			OnStartSchedule( int scheduleType );
	int 			TranslateSchedule( int scheduleType );
	void 			StartTask( const Task_t *pTask );
	void 			RunTask( const Task_t *pTask );
	CAI_Schedule *	GetSchedule(int localScheduleID);
	const char *	TaskName(int taskID);
	void			BuildScheduleTestBits();
	void			BuildScheduleTestBitsNotActive();

	void			OnChangeHintGroup( string_t oldGroup, string_t newGroup );
	
	void			OnChangeActiveWeapon( CBaseCombatWeapon *pOldWeapon, CBaseCombatWeapon *pNewWeapon );

	void			OnRestore();

	float			GetJumpGravity() const;
	bool			IsJumpLegal( const Vector &startPos, const Vector &apex, const Vector &endPos, float maxUp, float maxDown, float maxDist ) const;
	bool			MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost );

	//---------------------------------

protected:

	CAI_Schedule *	GetNewSchedule();
	CAI_Schedule *	GetFailSchedule();
private:
	void 			BehaviorBridge_GatherConditions();
	int				BehaviorBridge_SelectSchedule();
	int				BehaviorBridge_TranslateSchedule( int scheduleType );
	float			BehaviorBridge_GetJumpGravity() const;
	bool			BehaviorBridge_IsJumpLegal( const Vector &startPos, const Vector &apex, const Vector &endPos, float maxUp, float maxDown, float maxDist ) const;
	bool			BehaviorBridge_MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost );


	bool			m_bCalledBehaviorSelectSchedule;
	
};

//-----------------------------------------------------------------------------

// The first frame a behavior begins schedule selection, it won't have had it's GatherConditions()
// called. To fix this, BeginScheduleSelection() manually calls the new behavior's GatherConditions(),
// but sets this global so that the baseclass GatherConditions() isn't called as well.
extern bool g_bBehaviorHost_PreventBaseClassGatherConditions;

//-----------------------------------------------------------------------------

inline void CAI_BehaviorBase::BridgeOnStartSchedule( int scheduleType )
{
	int localId = AI_IdIsGlobal( scheduleType ) ? GetClassScheduleIdSpace()->ScheduleGlobalToLocal( scheduleType ) : scheduleType;
	OnStartSchedule( localId );
}

//-------------------------------------

inline int CAI_BehaviorBase::BridgeSelectSchedule()
{
	int result = SelectSchedule();

	if ( IsBehaviorSchedule( result ) )
		return GetClassScheduleIdSpace()->ScheduleLocalToGlobal( result );

	return result;
}

//-------------------------------------

inline bool CAI_BehaviorBase::BridgeSelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode, int *pResult )
{
	m_fOverrode = true;
	int result = SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
	if ( m_fOverrode )
	{
		if ( result != SCHED_NONE )
		{
			if ( IsBehaviorSchedule( result ) )
				*pResult = GetClassScheduleIdSpace()->ScheduleLocalToGlobal( result );
			else
				*pResult = result;
			return true;
		}
		Warning( "An AI behavior is in control but has no recommended schedule\n" );
	}
	return false;
}

//-------------------------------------

inline bool CAI_BehaviorBase::BridgeStartTask( const Task_t *pTask )
{
	m_fOverrode = true;
	StartTask( pTask );
	return m_fOverrode;
}

//-------------------------------------

inline bool CAI_BehaviorBase::BridgeRunTask( const Task_t *pTask)
{
	m_fOverrode = true;
	RunTask( pTask );
	return m_fOverrode;
}

//-------------------------------------

inline void CAI_BehaviorBase::ChainStartTask( int task, float taskData )
{
	Task_t tempTask = { task, taskData }; 

	bool fPrevOverride = m_fOverrode;
	this->GetOuter()->StartTask( (const Task_t *)&tempTask );
	m_fOverrode = fPrevOverride;;
}

//-------------------------------------

inline void CAI_BehaviorBase::ChainRunTask( int task, float taskData )
{ 
	Task_t tempTask = { task, taskData }; 
	bool fPrevOverride = m_fOverrode;
	this->GetOuter()->RunTask( (const Task_t *)	&tempTask );
	m_fOverrode = fPrevOverride;;
}

//-------------------------------------

inline int CAI_BehaviorBase::BridgeTranslateSchedule( int scheduleType )
{
	int localId = AI_IdIsGlobal( scheduleType ) ? GetClassScheduleIdSpace()->ScheduleGlobalToLocal( scheduleType ) : scheduleType;
	int result = TranslateSchedule( localId );
	
	return result;
}

//-------------------------------------

inline bool CAI_BehaviorBase::BridgeGetSchedule( int localScheduleID, CAI_Schedule **ppResult )
{
	*ppResult = GetSchedule( localScheduleID );
	return (*ppResult != nullptr );
}

//-------------------------------------

inline bool CAI_BehaviorBase::BridgeTaskName( int taskID, const char **ppResult )
{
	if ( AI_IdIsLocal( taskID ) )
	{
		*ppResult = GetSchedulingSymbols()->TaskIdToSymbol( GetClassScheduleIdSpace()->TaskLocalToGlobal( taskID ) );
		return (*ppResult != nullptr );
	}
	return false;
}

//-----------------------------------------------------------------------------

template <class BASE_NPC>
inline void CAI_BehaviorHost<BASE_NPC>::CleanupOnDeath( CBaseEntity *pCulprit, bool bFireDeathOutput )
{
	this->DeferSchedulingToBehavior( nullptr );
	for( int i = 0; i < std::ssize( this->m_Behaviors ); i++ )
	{
		this->m_Behaviors[i]->CleanupOnDeath( pCulprit, bFireDeathOutput );
	}
	BaseClass::CleanupOnDeath( pCulprit, bFireDeathOutput );
}

//-------------------------------------

template <class BASE_NPC>
inline void CAI_BehaviorHost<BASE_NPC>::GatherConditions()					
{ 
	// Iterate over behaviors and call GatherConditionsNotActive() on each behavior
	// not currently active.
	for( int i = 0; i < std::ssize( this->m_Behaviors ); i++ )
	{
		if( this->m_Behaviors[i] != this->m_pPrimaryBehavior )
		{
			this->m_Behaviors[i]->GatherConditionsNotActive();
		}
	}

	if ( this->m_pPrimaryBehavior )
		this->m_pPrimaryBehavior->GatherConditions(); 
	else
		BaseClass::GatherConditions();
}

//-------------------------------------

template <class BASE_NPC>
inline void CAI_BehaviorHost<BASE_NPC>::BehaviorBridge_GatherConditions()
{
	if ( g_bBehaviorHost_PreventBaseClassGatherConditions )
		return;

	BaseClass::GatherConditions();
}

//-------------------------------------

template <class BASE_NPC>
inline void CAI_BehaviorHost<BASE_NPC>::OnStartSchedule( int scheduleType )
{
	if ( this->m_pPrimaryBehavior )
		this->m_pPrimaryBehavior->BridgeOnStartSchedule( scheduleType ); 
	BaseClass::OnStartSchedule( scheduleType );
}

//-------------------------------------

template <class BASE_NPC>
inline int CAI_BehaviorHost<BASE_NPC>::BehaviorBridge_SelectSchedule() 
{
	return BaseClass::SelectSchedule();
}

//-------------------------------------

template <class BASE_NPC>
inline CAI_Schedule *CAI_BehaviorHost<BASE_NPC>::GetNewSchedule()
{
	m_bCalledBehaviorSelectSchedule = false;
	CAI_Schedule *pResult = BaseClass::GetNewSchedule();
	if ( !m_bCalledBehaviorSelectSchedule && this->m_pPrimaryBehavior )
		this->DeferSchedulingToBehavior( nullptr );
	return pResult;
}

//-------------------------------------

template <class BASE_NPC>
inline CAI_Schedule *CAI_BehaviorHost<BASE_NPC>::GetFailSchedule()
{
	m_bCalledBehaviorSelectSchedule = false;
	CAI_Schedule *pResult = BaseClass::GetFailSchedule();
	if ( !m_bCalledBehaviorSelectSchedule && this->m_pPrimaryBehavior )
		this->DeferSchedulingToBehavior( nullptr );
	return pResult;
}

//-------------------------------------

template <class BASE_NPC>
inline int CAI_BehaviorHost<BASE_NPC>::BehaviorBridge_TranslateSchedule( int scheduleType ) 
{
	return BaseClass::TranslateSchedule( scheduleType );
}

//-------------------------------------

template <class BASE_NPC>
inline int CAI_BehaviorHost<BASE_NPC>::TranslateSchedule( int scheduleType ) 
{
	if ( this->m_pPrimaryBehavior )
	{
		return this->m_pPrimaryBehavior->BridgeTranslateSchedule( scheduleType );
	}
	return BaseClass::TranslateSchedule( scheduleType );
}

//-------------------------------------

template <class BASE_NPC>
inline int CAI_BehaviorHost<BASE_NPC>::SelectSchedule()
{
	m_bCalledBehaviorSelectSchedule = true;
	if ( this->m_pPrimaryBehavior )
	{
		return this->m_pPrimaryBehavior->BridgeSelectSchedule();
	}

	return BaseClass::SelectSchedule();
}

//-------------------------------------

template <class BASE_NPC>
inline void CAI_BehaviorHost<BASE_NPC>::KeepRunningBehavior()
{
	if ( this->m_pPrimaryBehavior )
		m_bCalledBehaviorSelectSchedule = true;
}

//-------------------------------------

template <class BASE_NPC>
inline int CAI_BehaviorHost<BASE_NPC>::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	m_bCalledBehaviorSelectSchedule = true;
	int result = 0;
	if ( this->m_pPrimaryBehavior && this->m_pPrimaryBehavior->BridgeSelectFailSchedule( failedSchedule, failedTask, taskFailCode, &result ) )
		return result;
	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

//-------------------------------------

template <class BASE_NPC>
inline void CAI_BehaviorHost<BASE_NPC>::StartTask( const Task_t *pTask )
{
	if ( this->m_pPrimaryBehavior && this->m_pPrimaryBehavior->BridgeStartTask( pTask ) )
		return;
	BaseClass::StartTask( pTask );
}

//-------------------------------------

template <class BASE_NPC>
inline void CAI_BehaviorHost<BASE_NPC>::RunTask( const Task_t *pTask )
{
	if ( this->m_pPrimaryBehavior && this->m_pPrimaryBehavior->BridgeRunTask( pTask ) )
		return;
	BaseClass::RunTask( pTask );
}

//-------------------------------------

template <class BASE_NPC>
inline CAI_Schedule *CAI_BehaviorHost<BASE_NPC>::GetSchedule(int localScheduleID)
{
	CAI_Schedule *pResult;
	if ( this->m_pPrimaryBehavior && this->m_pPrimaryBehavior->BridgeGetSchedule( localScheduleID, &pResult ) )
		return pResult;
	return BaseClass::GetSchedule( localScheduleID );
}

//-------------------------------------

template <class BASE_NPC>
inline const char *CAI_BehaviorHost<BASE_NPC>::TaskName(int taskID)
{
	const char *pszResult = nullptr;
	if ( this->m_pPrimaryBehavior && this->m_pPrimaryBehavior->BridgeTaskName( taskID, &pszResult ) )
		return pszResult;
	return BaseClass::TaskName( taskID );
}

//-------------------------------------

template <class BASE_NPC>
inline void CAI_BehaviorHost<BASE_NPC>::BuildScheduleTestBits()
{
	// Iterate over behaviors and call BuildScheduleTestBitsNotActive() on each behavior
	// not currently active.
	for( int i = 0; i < std::ssize( this->m_Behaviors ); i++ )
	{
		if( this->m_Behaviors[i] != this->m_pPrimaryBehavior )
		{
			this->m_Behaviors[i]->BuildScheduleTestBitsNotActive();
		}
	}

	if ( this->m_pPrimaryBehavior )
		this->m_pPrimaryBehavior->BuildScheduleTestBits();

	BaseClass::BuildScheduleTestBits();
}

//-------------------------------------

template <class BASE_NPC>
inline void CAI_BehaviorHost<BASE_NPC>::OnChangeHintGroup( string_t oldGroup, string_t newGroup )
{
	for( int i = 0; i < std::ssize( this->m_Behaviors ); i++ )
	{
		this->m_Behaviors[i]->OnChangeHintGroup( oldGroup, newGroup );
	}
	BaseClass::OnChangeHintGroup( oldGroup, newGroup );
}

//-------------------------------------

template <class BASE_NPC>
inline float CAI_BehaviorHost<BASE_NPC>::BehaviorBridge_GetJumpGravity() const
{
	return BaseClass::GetJumpGravity();
}

//-------------------------------------

template <class BASE_NPC>
inline bool CAI_BehaviorHost<BASE_NPC>::BehaviorBridge_IsJumpLegal( const Vector &startPos, const Vector &apex, const Vector &endPos, float maxUp, float maxDown, float maxDist ) const
{
	return BaseClass::IsJumpLegal( startPos, apex, endPos, maxUp, maxDown, maxDist );
}

//-------------------------------------

template <class BASE_NPC>
inline bool CAI_BehaviorHost<BASE_NPC>::BehaviorBridge_MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost )
{
	return BaseClass::MovementCost( moveType, vecStart, vecEnd, pCost );
}

//-------------------------------------

template <class BASE_NPC>
inline void CAI_BehaviorHost<BASE_NPC>::OnChangeActiveWeapon( CBaseCombatWeapon *pOldWeapon, CBaseCombatWeapon *pNewWeapon )
{
	for( int i = 0; i < std::ssize( this->m_Behaviors ); i++ )
	{
		this->m_Behaviors[i]->OnChangeActiveWeapon( pOldWeapon, pNewWeapon );
	}
	BaseClass::OnChangeActiveWeapon( pOldWeapon, pNewWeapon );
}

//-------------------------------------

template <class BASE_NPC>
inline void CAI_BehaviorHost<BASE_NPC>::OnRestore()
{
	for( int i = 0; i < std::ssize( this->m_Behaviors ); i++ )
	{
		this->m_Behaviors[i]->OnRestore();
	}
	BaseClass::OnRestore();
}

//-------------------------------------

template <class BASE_NPC>
inline void CAI_BehaviorHost<BASE_NPC>::Precache()
{
	BaseClass::Precache();
	for( int i = 0; i < std::ssize( this->m_Behaviors ); i++ )
	{
		this->m_Behaviors[i]->Precache();
	}
}

//-------------------------------------

template <class BASE_NPC>
inline void CAI_BehaviorHost<BASE_NPC>::UpdateOnRemove()
{
	for( int i = 0; i < std::ssize( this->m_Behaviors ); i++ )
	{
		this->m_Behaviors[i]->UpdateOnRemove();
	}
	BaseClass::UpdateOnRemove();
}

//-------------------------------------

template <class BASE_NPC>
inline void CAI_BehaviorHost<BASE_NPC>::Event_Killed( const CTakeDamageInfo &info )
{
	for( int i = 0; i < std::ssize( this->m_Behaviors ); i++ )
	{
		this->m_Behaviors[i]->Event_Killed( info );
	}
	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------

template <class BASE_NPC>
inline float CAI_BehaviorHost<BASE_NPC>::GetJumpGravity() const
{
	// @HACKHACK
	float base = BaseClass::GetJumpGravity();
	for( int i = 0; i < std::ssize( this->m_Behaviors ); i++ )
	{
		float current  = this->m_Behaviors[i]->GetJumpGravity();
		if ( current != base )
		{
			return current;
		}
	}

	return BaseClass::GetJumpGravity();
}

//-------------------------------------

template <class BASE_NPC>
inline bool CAI_BehaviorHost<BASE_NPC>::IsJumpLegal( const Vector &startPos, const Vector &apex, const Vector &endPos, float maxUp, float maxDown, float maxDist ) const
{
	// @HACKHACK
	bool base = BaseClass::IsJumpLegal( startPos, apex, endPos, maxUp, maxDown, maxDist );
	for( int i = 0; i < std::ssize( this->m_Behaviors ); i++ )
	{
		bool current  = this->m_Behaviors[i]->IsJumpLegal( startPos, apex, endPos, maxUp, maxDown, maxDist );
		if ( current != base )
		{
			return current;
		}
	}

	return base;
}

template <class BASE_NPC>
inline bool CAI_BehaviorHost<BASE_NPC>::MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost )
{
	// @HACKHACK
	bool base = BaseClass::MovementCost( moveType, vecStart, vecEnd, pCost );

	for( int i = 0; i < std::ssize( this->m_Behaviors ); i++ )
	{
		bool current  = this->m_Behaviors[i]->MovementCost( moveType, vecStart, vecEnd, pCost );
		if ( current != base )
		{
			return current;
		}
	}

	return base;
}

//-----------------------------------------------------------------------------
