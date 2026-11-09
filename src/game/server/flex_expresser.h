#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: A CBaseFlex, with an Expresser so that it can

#include "reflect_annotations.h"

#include "flexcycler.h"
#include "ai_speech.h"

#define FCYCLER_NOTSOLID 1

class CFlexExpresserShim : public CFlexCycler
{
public:
	inline CAI_Expresser *GetExpresser( void ) { return m_pExpresser; }
	inline const CAI_Expresser *GetMultiplayerExpresser( void ) const { return m_pExpresser; }

protected:
	CAI_Expresser *m_pExpresser;
};


class CFlexExpresser : public CAI_ExpresserHost<CFlexExpresserShim>
{
	DECLARE_CLASS( CFlexExpresser, CAI_ExpresserHost<CFlexExpresserShim> );

public:
	DECLARE_DATADESC();
	CFlexExpresser();
	~CFlexExpresser();

	inline CAI_Expresser *GetExpresser( void ) { return m_pExpresser; }
	inline const CAI_Expresser *GetMultiplayerExpresser( void ) const { return m_pExpresser; }
	IResponseSystem *GetResponseSystem_Impl( void );
	void Spawn_Impl();
	void Think_Impl( void );
	int OnTakeDamage_Impl( const CTakeDamageInfo &info ); //< stubbed out, does nothing, flexexpressers are immortal
	[[= ks::reflect::Input{ .name = "SpeakResponseConcept", .type = FIELD_STRING } ]] void InputSpeakResponseConcept( inputdata_t &inputdata );

	/// given a pointer to a CBaseEntity, return a CFlexExpresser * if that base entity is a prop_talker, NULL otherwise
	/// (hopefully faster than dynamic_cast). Passing NULL is safe.
	static CFlexExpresser *AsFlexExpresser( CBaseEntity *pEntity );

	[[= ks::reflect::Key{ .name = "maxThenAnyDispatchDist" } ]] float m_flThenAnyMaxDist; //< if nonzero, override the maximum dispatch distance for a THEN ANY followup.
protected:
	CAI_Expresser *CreateExpresser( void );

};
