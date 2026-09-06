//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_AI_BASENPC_H
#define C_AI_BASENPC_H

#include "reflect_annotations.h"
#ifdef _WIN32
#pragma once
#endif


#include "c_basecombatcharacter.h"

// NOTE: MOved all controller code into c_basestudiomodel
class [[= ks::reflect::NetTable{ .name = "DT_AI_BaseNPC" } ]]
      [[= ks::reflect::From<"m_lifeState", ks::reflect::Net{}>{} ]]
      C_AI_BaseNPC : public C_BaseCombatCharacter
{
	DECLARE_CLASS( C_AI_BaseNPC, C_BaseCombatCharacter );

public:
	DECLARE_CLIENTCLASS();

	C_AI_BaseNPC();
	virtual unsigned int	PhysicsSolidMaskForEntity( void ) const;
	virtual bool			IsNPC( void ) { return true; }
	bool					IsMoving( void ){ return m_bIsMoving; }
	bool					ShouldAvoidObstacle( void ){ return m_bPerformAvoidance; }
	virtual bool			AddRagdollToFadeQueue( void ) { return m_bFadeCorpse; }

	virtual void			GetRagdollInitBoneArrays( matrix3x4a_t *pDeltaBones0, matrix3x4a_t *pDeltaBones1, matrix3x4a_t *pCurrentBones, float boneDt );

	int						GetDeathPose( void ) { return m_iDeathPose; }

	bool					ShouldModifyPlayerSpeed( void ) { return m_bSpeedModActive;	}
	int						GetSpeedModifyRadius( void ) { return m_iSpeedModRadius; }
	int						GetSpeedModifySpeed( void ) { return m_iSpeedModSpeed;	}

	void					ClientThink( void );
	void					OnDataChanged( DataUpdateType_t type );
	bool					ImportantRagdoll( void ) { return m_bImportanRagdoll;	}

private:
	C_AI_BaseNPC( const C_AI_BaseNPC & ); // not defined, not accessible
	[[= ks::reflect::Net{} ]] float m_flTimePingEffect;
	[[= ks::reflect::Net{} ]] int  m_iDeathPose;
	[[= ks::reflect::Net{} ]] int	 m_iDeathFrame;

	[[= ks::reflect::Net{} ]] int m_iSpeedModRadius;
	[[= ks::reflect::Net{} ]] int m_iSpeedModSpeed;

	[[= ks::reflect::Net{} ]] bool m_bPerformAvoidance;
	[[= ks::reflect::Net{} ]] bool m_bIsMoving;
	[[= ks::reflect::Net{} ]] bool m_bFadeCorpse;
	[[= ks::reflect::Net{} ]] bool m_bSpeedModActive;
	[[= ks::reflect::Net{} ]] bool m_bImportanRagdoll;
};


#endif // C_AI_BASENPC_H
