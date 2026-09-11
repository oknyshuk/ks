//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SOUNDSCAPE_H
#define SOUNDSCAPE_H

#include "reflect_annotations.h"

class CEnvSoundscape;

struct ss_update_t
{
	CBasePlayer *pPlayer;
	CEnvSoundscape	*pCurrentSoundscape;
	Vector		playerPosition;
	float		currentDistance;
	int			traceCount;
	bool		bInRange;
};

class CEnvSoundscape : public CServerOnlyEntity
{
public:
	DECLARE_CLASS( CEnvSoundscape, CServerOnlyEntity );
	DECLARE_DATADESC();

	CEnvSoundscape();
	~CEnvSoundscape();

	bool KeyValue( const char *szKeyName, const char *szValue );
	void Spawn( void );
	void Precache( void );
	void UpdateForPlayer( ss_update_t &update );
	void WriteAudioParamsTo( audioparams_t &audio );
	bool InRangeOfPlayer( CBasePlayer *pPlayer );
	void DrawDebugGeometryOverlays( void );

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ToggleEnabled", .type = FIELD_VOID } ]] void InputToggleEnabled( inputdata_t &inputdata );

	string_t GetSoundscapeName() const {return m_soundscapeName;}


private:

	bool IsEnabled( void ) const;
	void Disable( void );
	void Enable( void );


public:
	[[= ks::reflect::Key{ .name = "OnPlay" } ]] COutputEvent	m_OnPlay;
	[[= ks::reflect::Key{ .name = "radius" } ]] float	m_flRadius;
	string_t m_soundscapeName;
	int		m_soundscapeIndex;
	int		m_soundscapeEntityId;
	[[= ks::reflect::Key{ .name = "position7", .index = 7 } ]] [[= ks::reflect::Key{ .name = "position6", .index = 6 } ]] [[= ks::reflect::Key{ .name = "position5", .index = 5 } ]] [[= ks::reflect::Key{ .name = "position4", .index = 4 } ]] [[= ks::reflect::Key{ .name = "position3", .index = 3 } ]] [[= ks::reflect::Key{ .name = "position2", .index = 2 } ]] [[= ks::reflect::Key{ .name = "position1", .index = 1 } ]] [[= ks::reflect::Key{ .name = "position0", .index = 0 } ]] string_t m_positionNames[NUM_AUDIO_LOCAL_SOUNDS];
	
	// If this is set, then this soundscape ignores all its parameters and uses
	// those of this soundscape.
	CHandle<CEnvSoundscape> m_hProxySoundscape;


private:

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool	m_bDisabled;
};


class CEnvSoundscapeProxy : public CEnvSoundscape
{
public:
	DECLARE_CLASS( CEnvSoundscapeProxy, CEnvSoundscape );
	DECLARE_DATADESC();

	CEnvSoundscapeProxy();
	virtual void Activate();

	// Here just to stop it falling back to CEnvSoundscape's, and
	// printing bogus errors about missing soundscapes.
	virtual void Precache() { return; }

private:
	[[= ks::reflect::Key{ .name = "MainSoundscapeName" } ]] string_t m_MainSoundscapeName;
};


class CEnvSoundscapeTriggerable : public CEnvSoundscape
{
friend class CTriggerSoundscape;

public:
	DECLARE_CLASS( CEnvSoundscapeTriggerable, CEnvSoundscape );

	CEnvSoundscapeTriggerable();
	
	// Overrides the base class's think and prevents it from running at all.
	virtual void Think();


private:

	// Passed through from CTriggerSoundscape.
	void DelegateStartTouch( CBaseEntity *pEnt );
	void DelegateEndTouch( CBaseEntity *pEnt );
};


#endif // SOUNDSCAPE_H
