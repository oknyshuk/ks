//--------------------------------------------------------------------------------------------------------
// Copyright (c) 2007 Turtle Rock Studios, Inc. - All Rights Reserved

#ifndef FOG_VOLUME_H
#define FOG_VOLUME_H

#include "reflect_annotations.h"

#ifdef _WIN32
#pragma once
#endif


class CFogController;
class CPostProcessController;
class CColorCorrection;


//--------------------------------------------------------------------------------------------------------
// Fog volume entity
class CFogVolume : public CServerOnlyEntity
{
	DECLARE_CLASS( CFogVolume, CServerOnlyEntity );
	DECLARE_DATADESC();

public:
	CFogVolume();
	virtual ~CFogVolume();
	virtual void Spawn( void );
	virtual void Activate();

	static CFogVolume *FindFogVolumeForPosition( const Vector &position );

	const char *GetFogControllerName() const 
	{
		return STRING( m_fogName );
	}

	CFogController* GetFogController( ) const
	{
		return m_hFogController.Get();
	}

	CPostProcessController* GetPostProcessController( ) const
	{
		return m_hPostProcessController.Get();
	}

	CColorCorrection* GetColorCorrectionController( ) const
	{
		return m_hColorCorrectionController.Get();
	}

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &data );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &data );

private:
	[[= ks::reflect::Key{ .name = "FogName" } ]] string_t m_fogName;
	[[= ks::reflect::Key{ .name = "PostProcessName" } ]] string_t m_postProcessName;
	[[= ks::reflect::Key{ .name = "ColorCorrectionName" } ]] string_t m_colorCorrectionName;

	CHandle< CFogController > m_hFogController;
	CHandle< CPostProcessController > m_hPostProcessController;
	CHandle< CColorCorrection > m_hColorCorrectionController;

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool m_bDisabled;
	bool m_bInFogVolumesList;

	void AddToGlobalList();
	void RemoveFromGlobalList();
};

extern CUtlVector< CFogVolume * > TheFogVolumes;


#endif // FOG_VOLUME_H
