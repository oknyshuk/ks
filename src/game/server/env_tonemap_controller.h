#pragma once
//--------------------------------------------------------------------------------------------------------
// Copyright 2007 Turtle Rock Studios, Inc.


#include "reflect_annotations.h"

#include "triggers.h"
#include "igamesystem.h"

//--------------------------------------------------------------------------------------------------------
class CTonemapTrigger : public CBaseTrigger
{
public:
	DECLARE_CLASS( CTonemapTrigger, CBaseTrigger );
	DECLARE_DATADESC();

	void Spawn_Impl( void );
	void StartTouch_Impl( CBaseEntity *other );
	void EndTouch_Impl( CBaseEntity *other );

	CBaseEntity *GetTonemapController( void ) const;

private:
	[[= ks::reflect::Key{ .name = "TonemapName" } ]] string_t m_tonemapControllerName;
	EHANDLE m_hTonemapController;
};


//--------------------------------------------------------------------------------------------------------
inline CBaseEntity *CTonemapTrigger::GetTonemapController( void ) const
{
	return m_hTonemapController.Get();
}


//--------------------------------------------------------------------------------------------------------
// Tonemap Controller System.
class CTonemapSystem : public CAutoGameSystem
{
public:

	// Creation/Init.
	CTonemapSystem( char const *name ) : CAutoGameSystem( name ) 
	{
		m_hMasterController = nullptr;
	}

	~CTonemapSystem()
	{
		m_hMasterController = nullptr;
	}

	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity();
	CBaseEntity *GetMasterTonemapController( void ) const;

private:

	EHANDLE m_hMasterController;
};


//--------------------------------------------------------------------------------------------------------
inline CBaseEntity *CTonemapSystem::GetMasterTonemapController( void ) const
{
	return m_hMasterController.Get();
}

//--------------------------------------------------------------------------------------------------------
CTonemapSystem *TheTonemapSystem( void );


