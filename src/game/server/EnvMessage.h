#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"

#include "baseentity.h"
#include "entityoutput.h"

inline constexpr auto SF_MESSAGE_ONCE = 0x0001; // Fade in, not out
inline constexpr auto SF_MESSAGE_ALL = 0x0002; // Send to all clients

class CMessage : public CPointEntity
{
public:
	DECLARE_CLASS( CMessage, CPointEntity );

	void	Spawn_Impl( void );
	void	Precache_Impl( void );

	inline void SetMessage( string_t iszMessage ) { m_iszMessage = iszMessage; }

	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

private:

	[[= ks::reflect::Input{ .name = "ShowMessage", .type = FIELD_VOID } ]] void InputShowMessage( inputdata_t &inputdata );

	[[= ks::reflect::Key{ .name = "message" } ]] string_t m_iszMessage;		// Message to display.
	[[= ks::reflect::Key{ .name = "messagevolume" } ]] float m_MessageVolume;
	[[= ks::reflect::Key{ .name = "messageattenuation" } ]] int m_MessageAttenuation;
	float m_Radius;

	DECLARE_DATADESC();

	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "messagesound" } ]] string_t m_sNoise;
	[[= ks::reflect::Key{ .name = "OnShowMessage" } ]] COutputEvent m_OnShowMessage;
};
