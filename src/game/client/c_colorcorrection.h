//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Color correction entity with simple radial falloff
//
// $NoKeywords: $
//===========================================================================//

#ifndef C_COLORCORRECTION_H
#define C_COLORCORRECTION_H

#include "reflect_annotations.h"
#ifdef _WIN32
#pragma once
#endif

#include "colorcorrectionmgr.h"

//------------------------------------------------------------------------------
// Purpose : Color correction entity with radial falloff
//------------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_ColorCorrection" } ]]
      C_ColorCorrection : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_ColorCorrection, C_BaseEntity );

	DECLARE_CLIENTCLASS();

	C_ColorCorrection();
	virtual ~C_ColorCorrection();

	void OnDataChanged(DataUpdateType_t updateType);
	bool ShouldDraw();

	virtual void Update(C_BasePlayer *pPlayer, float ccScale);
	
	bool IsMaster() const { return m_bMaster; }
	bool IsClientSide() const;
	bool IsExclusive() const { return m_bExclusive; }

	void EnableOnClient( bool bEnable, bool bSkipFade = false );

	Vector GetOrigin();
	float  GetMinFalloff();
	float  GetMaxFalloff();

	void   SetWeight( float fWeight );

protected:
	void StartFade( int nSplitScreenSlot, float flDuration );
	float GetFadeRatio( int nSplitScreenSlot ) const;
	bool IsFadeTimeElapsed( int nSplitScreenSlot ) const;

	[[= ks::reflect::Net{} ]] Vector	m_vecOrigin;

	[[= ks::reflect::Net{} ]] float	m_minFalloff;
	[[= ks::reflect::Net{} ]] float	m_maxFalloff;
	[[= ks::reflect::Net{} ]] float	m_flFadeInDuration;
	[[= ks::reflect::Net{} ]] float	m_flFadeOutDuration;
	[[= ks::reflect::Net{} ]] float	m_flMaxWeight;
	[[= ks::reflect::Net{} ]] float	m_flCurWeight;		// networked from server
	[[= ks::reflect::Net{} ]] char	m_netLookupFilename[MAX_PATH];

	[[= ks::reflect::Net{} ]] bool	m_bEnabled;			// networked from server
	[[= ks::reflect::Net{} ]] bool	m_bMaster;
	[[= ks::reflect::Net{} ]] bool	m_bClientSide;
	[[= ks::reflect::Net{} ]] bool	m_bExclusive;

	bool	m_bEnabledOnClient[MAX_SPLITSCREEN_PLAYERS];
	float	m_flCurWeightOnClient[MAX_SPLITSCREEN_PLAYERS];
	bool	m_bFadingIn[MAX_SPLITSCREEN_PLAYERS];
	float	m_flFadeStartWeight[MAX_SPLITSCREEN_PLAYERS];
	float	m_flFadeStartTime[MAX_SPLITSCREEN_PLAYERS];
	float	m_flFadeDuration[MAX_SPLITSCREEN_PLAYERS];

	ClientCCHandle_t m_CCHandle;
};

#endif
