//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BASEGRENADE_SHARED_H
#define BASEGRENADE_SHARED_H

#include "reflect_annotations.h"
#ifdef CLIENT_DLL
#include "dt_recv.h"
#endif
#include "const.h"

#if defined( CLIENT_DLL )

#define CBaseGrenade C_BaseGrenade

#include "c_basecombatcharacter.h"
#include "glow_outline_effect.h"

#else

#include "basecombatcharacter.h"
#include "player_pickup.h"

#endif

#include "cs_shareddefs.h"

#define BASEGRENADE_EXPLOSION_VOLUME	1024

void SendProxy_CropFlagsToPlayerFlagBitsLength( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

void RecvProxy_LocalVelocity( const CRecvProxyData *pData, void *pStruct, void *pOut );

class CTakeDamageInfo;


#if !defined( CLIENT_DLL )
class [[= ks::reflect::NetTable{ .name = "DT_BaseGrenade" } ]]
      [[= ks::reflect::From<"m_flDamage", ks::reflect::Net{ .bits = 10, .low = 0.0, .high = 256.0f, .flags = SPROP_ROUNDDOWN }>{} ]]
      [[= ks::reflect::From<"m_DmgRadius", ks::reflect::Net{ .bits = 10, .low = 0.0, .high = 1024.0f, .flags = SPROP_ROUNDDOWN }>{} ]]
      [[= ks::reflect::From<"m_bIsLive", ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_hThrower", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_vecVelocity", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::ENC_VECTOR }>{} ]]
      [[= ks::reflect::From<"m_fFlags", ks::reflect::Net{ .bits = PLAYER_FLAG_BITS, .flags = SPROP_UNSIGNED }, SendProxy_CropFlagsToPlayerFlagBitsLength>{} ]]
      [[= ks::reflect::Exclude{ .table = "DT_AnimTimeMustBeFirst", .prop = "m_flAnimTime" } ]]
      CBaseGrenade : public CBaseAnimating, public CDefaultPlayerPickupVPhysics
#else
class [[= ks::reflect::NetTable{ .name = "DT_BaseGrenade" } ]]
      [[= ks::reflect::From<"m_flDamage", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_DmgRadius", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_bIsLive", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_hThrower", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_vecVelocity", ks::reflect::Net{}, RecvProxy_LocalVelocity>{} ]]
      [[= ks::reflect::From<"m_fFlags", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::PredFrom<"m_vecVelocity", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = 0.5f }>{} ]]
      CBaseGrenade : public CBaseAnimating
#endif
{
	DECLARE_CLASS( CBaseGrenade, CBaseAnimating );
public:

	CBaseGrenade(void);
	~CBaseGrenade(void);

	DECLARE_PREDICTABLE();
	DECLARE_NETWORKCLASS();


#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

	virtual void		Precache( void );

	virtual void		Explode( trace_t *pTrace, int bitsDamageType );
	void				Smoke( void );

	void				BounceTouch( CBaseEntity *pOther );
	void				SlideTouch( CBaseEntity *pOther );
	void				ExplodeTouch( CBaseEntity *pOther );
	void				DangerSoundThink( void );
	void				PreDetonate( void );
	virtual void		Detonate( void );
	void				DetonateUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void				TumbleThink( void );

	virtual Vector		GetBlastForce() { return vec3_origin; }

	virtual void		BounceSound( void );
	virtual int			BloodColor( void ) { return DONT_BLEED; }
	virtual void		Event_Killed( const CTakeDamageInfo &info );

	virtual float		GetShakeAmplitude( void ) { return 25.0; }
	virtual float		GetShakeRadius( void ) { return 750.0; }

	virtual const char *GetParticleSystemName( int pointContents, surfacedata_t *pdata = nullptr ) { return nullptr; }

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_EXPLOSIVE; }

	// Damage accessors.
	virtual float GetDamage()
	{
		return m_flDamage;
	}
	virtual float GetDamageRadius()
	{
		return m_DmgRadius;
	}

	virtual void SetDamage(float flDamage)
	{
		m_flDamage = flDamage;
	}

	virtual void SetDamageRadius(float flDamageRadius)
	{
		m_DmgRadius = flDamageRadius;
	}

	// Bounce sound accessors.
	void SetBounceSound( const char *pszBounceSound ) 
	{
		m_iszBounceSound = MAKE_STRING( pszBounceSound );
	}

	CBaseCombatCharacter *GetThrower( void );
	void				  SetThrower( CBaseCombatCharacter *pThrower );
	CBaseEntity *GetOriginalThrower() { return m_hOriginalThrower; }

#if !defined( CLIENT_DLL )
	// Allow +USE pickup
	int ObjectCaps() 
	{ 
		return (BaseClass::ObjectCaps() | FCAP_IMPULSE_USE | FCAP_USE_IN_RADIUS);
	}

	void				Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
#endif

public:
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_vecVelocity );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_fFlags );
	
	bool				m_bHasWarnedAI;				// whether or not this grenade has issued its DANGER sound to the world sound list yet.
	CNetworkVar( bool, m_bIsLive, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );					// Is this grenade live, or can it be picked up?
	CNetworkVar( float, m_DmgRadius, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );				// How far do I do damage?
	CNetworkVar( float, m_flNextAttack, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = TD_MSECTOLERANCE } ]] );
	float				m_flDetonateTime;			// Time at which to detonate.
	float				m_flWarnAITime;				// Time at which to warn the AI

#if defined( CLIENT_DLL )
	CGlowObject			m_GlowObject;
#endif

protected:

	CNetworkVar( float, m_flDamage, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );		// Damage to inflict.
	string_t m_iszBounceSound;	// The sound to make on bouncing.  If not NULL, overrides the BounceSound() function.

private:
	CNetworkHandle( CBaseEntity, m_hThrower, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );					// Who threw this grenade
	EHANDLE			m_hOriginalThrower;							// Who was the original thrower of this grenade

	CBaseGrenade( const CBaseGrenade & ); // not defined, not accessible

};

#endif // BASEGRENADE_SHARED_H
