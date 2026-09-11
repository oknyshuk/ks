//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef FUNC_AREAPORTALWINDOW_H
#define FUNC_AREAPORTALWINDOW_H

#include "reflect_annotations.h"


#include "baseentity.h"
#include "utllinkedlist.h"
#include "func_areaportalbase.h"


class [[= ks::reflect::NetTable{ .name = "DT_FuncAreaPortalWindow" } ]]
      [[= ks::reflect::KeyFrom<"m_portalNumber", ks::reflect::Key{ .name = "portalnumber" } >{} ]]
      CFuncAreaPortalWindow : public CFuncAreaPortalBase
{
public:
	DECLARE_CLASS( CFuncAreaPortalWindow, CFuncAreaPortalBase );	
	
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

					CFuncAreaPortalWindow();
					~CFuncAreaPortalWindow();


// Overrides.
public:

	virtual void	Spawn();
	virtual void	Activate();


// CFuncAreaPortalBase stuff.
public:

	virtual bool	UpdateVisibility( const CUtlVector< Vector > &vecOrigins, float fovDistanceAdjustFactor, bool &bIsOpenOnClient );


public:
	// Returns false if the viewer is past the fadeout distance.
	bool IsWindowOpen( const CUtlVector< Vector > &vecOrigins, float fovDistanceAdjustFactor );

public:
	
	CNetworkVar( float, m_flFadeStartDist, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "FadeStartDist" } ]] );	// Distance at which it starts fading (when <= this, alpha=m_flTranslucencyLimit).
	CNetworkVar( float, m_flFadeDist, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "FadeDist" } ]] );		// Distance at which it becomes solid.

	// 0-1 value - minimum translucency it's allowed to get to.
	CNetworkVar( float, m_flTranslucencyLimit, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "TranslucencyLimit" } ]] );

	[[= ks::reflect::Key{ .name = "BackgroundBModel" } ]] string_t 		m_iBackgroundBModelName;	// string name of background bmodel
	CNetworkVar( int, m_iBackgroundModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::ENC_MODELINDEX } ]] );

	//Input handlers
	[[= ks::reflect::Input{ .name = "SetFadeStartDistance", .type = FIELD_FLOAT } ]] void InputSetFadeStartDistance( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFadeEndDistance", .type = FIELD_FLOAT } ]] void InputSetFadeEndDistance( inputdata_t &inputdata );
};



#endif // FUNC_AREAPORTALWINDOW_H
