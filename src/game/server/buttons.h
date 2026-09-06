//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ====
//
// Purpose: 
//
//=============================================================================

#ifndef BUTTONS_H
#define BUTTONS_H

#include "reflect_annotations.h"
#ifdef _WIN32
#pragma once
#endif


class [[= ks::reflect::NetTable{ .name = "DT_BaseButton" } ]]
      CBaseButton : public CBaseToggle
{
public:

	DECLARE_CLASS( CBaseButton, CBaseToggle );
	DECLARE_SERVERCLASS();

	void Spawn( void );
	virtual void Precache( void );
	bool CreateVPhysics();
	void RotSpawn( void );
	bool KeyValue( const char *szKeyName, const char *szValue );
	int DrawDebugTextOverlays();

protected:

	void ButtonActivate( );
	void SparkSoundCache( void );

	void ButtonTouch( ::CBaseEntity *pOther );
	void ButtonSpark ( void );
	void TriggerAndWait( void );
	void ButtonReturn( void );
	void ButtonBackHome( void );
	void ButtonUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	bool OnUseLocked( CBaseEntity *pActivator );

	virtual void Lock();
	virtual void Unlock();

	// Input handlers
	[[= ks::reflect::Input{ .name = "Lock", .type = FIELD_VOID } ]] void InputLock( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Unlock", .type = FIELD_VOID } ]] void InputUnlock( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Press", .type = FIELD_VOID } ]] void InputPress( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "PressIn", .type = FIELD_VOID } ]] void InputPressIn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "PressOut", .type = FIELD_VOID } ]] void InputPressOut( inputdata_t &inputdata );

	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	
	enum BUTTON_CODE { BUTTON_NOTHING, BUTTON_ACTIVATE, BUTTON_RETURN, BUTTON_PRESS };

	BUTTON_CODE	ButtonResponseToTouch( void );
	void Press( CBaseEntity *pActivator, BUTTON_CODE eCode );
	
	DECLARE_DATADESC();

	virtual int	ObjectCaps(void);

	[[= ks::reflect::Key{ .name = "movedir" } ]] Vector m_vecMoveDir;

	bool	m_fStayPushed;		// button stays pushed in until touched again?
	bool	m_fRotating;		// a rotating button?  default is a sliding button.

	locksound_t m_ls;			// door lock sounds
	
	byte	m_bLockedSound;		// ordinals from entity selection
	byte	m_bLockedSentence;	
	byte	m_bUnlockedSound;	
	byte	m_bUnlockedSentence;
	bool	m_bLocked;
	[[= ks::reflect::Key{ .name = "sounds" } ]] int		m_sounds;
	float	m_flUseLockedTime;		// Controls how often we fire the OnUseLocked output.

	bool	m_bSolidBsp;

	string_t	m_sNoise;			// The actual WAV file name of the sound.

	[[= ks::reflect::Key{ .name = "OnDamaged" } ]] COutputEvent m_OnDamaged;
	[[= ks::reflect::Key{ .name = "OnPressed" } ]] COutputEvent m_OnPressed;
	[[= ks::reflect::Key{ .name = "OnUseLocked" } ]] COutputEvent m_OnUseLocked;
	[[= ks::reflect::Key{ .name = "OnIn" } ]] COutputEvent m_OnIn;
	[[= ks::reflect::Key{ .name = "OnOut" } ]] COutputEvent m_OnOut;

	int		m_nState;
};


//
// Rotating button (aka "lever")
//
class CRotButton : public CBaseButton
{
public:
	DECLARE_CLASS( CRotButton, CBaseButton );

	void Spawn( void );
	bool CreateVPhysics( void );

};


class [[= ks::reflect::KeyFrom<"m_bSolidBsp", ks::reflect::Key{ .name = "solidbsp" } >{} ]]
      CMomentaryRotButton : public CRotButton
{
	DECLARE_CLASS( CMomentaryRotButton, CRotButton );

public:
	void	Spawn ( void );
	bool	CreateVPhysics( void );
	virtual int	ObjectCaps( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	UseMoveDone( void );
	void	ReturnMoveDone( void );
	void	OutputMovementComplete(void);
	void	SetPositionMoveDone(void);
	void	UpdateSelf( float value, bool bPlaySound );

	void	PlaySound( void );
	void	UpdateTarget( float value, CBaseEntity *pActivator );

	int		DrawDebugTextOverlays(void);

	static CMomentaryRotButton *Instance( edict_t *pent ) { return (CMomentaryRotButton *)GetContainingEntity(pent); }

	float GetPos(const QAngle &vecAngles);

	DECLARE_DATADESC();

	virtual void Lock();
	virtual void Unlock();

	// Input handlers
	[[= ks::reflect::Input{ .name = "SetPosition", .type = FIELD_FLOAT } ]] void InputSetPosition( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetPositionImmediately", .type = FIELD_FLOAT } ]] void InputSetPositionImmediately( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "_DisableUpdateTarget", .type = FIELD_VOID } ]] void InputDisableUpdateTarget( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "_EnableUpdateTarget", .type = FIELD_VOID } ]] void InputEnableUpdateTarget( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

	virtual void Enable( void );
	virtual void Disable( void );

	bool	m_bDisabled;

	[[= ks::reflect::Key{ .name = "Position" } ]] COutputFloat m_Position;
	[[= ks::reflect::Key{ .name = "OnUnpressed" } ]] COutputEvent m_OnUnpressed;
	[[= ks::reflect::Key{ .name = "OnFullyOpen" } ]] COutputEvent m_OnFullyOpen;
	[[= ks::reflect::Key{ .name = "OnFullyClosed" } ]] COutputEvent m_OnFullyClosed;
	[[= ks::reflect::Key{ .name = "OnReachedPosition" } ]] COutputEvent m_OnReachedPosition;

	int			m_lastUsed;
	QAngle		m_start;
	QAngle		m_end;
	float		m_IdealYaw;
	string_t	m_sNoise;

	bool		m_bUpdateTarget;		// Used when jiggling so that we don't jiggle the target (door, etc)

	[[= ks::reflect::Key{ .name = "StartDirection" } ]] int			m_direction;
	[[= ks::reflect::Key{ .name = "returnspeed" } ]] float		m_returnSpeed;
	[[= ks::reflect::Key{ .name = "StartPosition" } ]] float		m_flStartPosition;

protected:

	void UpdateThink( void );
};



//--------------------------------------------------------------------------------------------------------
//
//	CButtonTimed - func_button_timed
//
//--------------------------------------------------------------------------------------------------------

class CButtonTimed : public CBaseButton
{
public:
	DECLARE_CLASS( CButtonTimed, CBaseButton );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CButtonTimed();

	void UseTimed( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void UseThink( void );

	virtual void Spawn( void );
	virtual int	ObjectCaps( void )
	{
		const int base_flags = BaseClass::ObjectCaps();
		return (base_flags | FCAP_CONTINUOUS_USE | FCAP_USE_IN_RADIUS);
	}

	void StopUse( void );

	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );
	void Enable( void );
	void Disable( void );

private:
	CNetworkVar( string_t, m_sUseString);
	CNetworkVar( string_t, m_sUseSubString);

	int m_nUseTime;
	bool m_bAutoDisable;

	bool m_bInUse;
	float m_flUseStartTime;
	float m_flLastUseTime;

	COutputEvent m_OnUnpressed;
	COutputEvent m_OnTimeUp;
};


#endif // BUTTONS_H
