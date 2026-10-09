// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
import std;
import ks.core;
#if defined(CLIENT_DLL)
import ks.tier1;
#include "const.h"
#include "predictable_entity.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "debugoverlay_shared.h"
#elif defined(GAME_DLL)
import ks.math;
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "iserverentity.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "base_transmit_proxy.h"
#include "networkvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "player.h"
#include "ndebugoverlay.h"
#endif
#include "ammodef.h"


//-----------------------------------------------------------------------------
// Purpose: Switches to the best weapon that is also better than the given weapon.
// Input  : pCurrent - The current weapon used by the player.
// Output : Returns true if the weapon was switched, false if there was no better
//			weapon to switch to.
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::SwitchToNextBestWeapon(CBaseCombatWeapon *pCurrent)
{
	CBaseCombatWeapon *pNewWeapon = g_pGameRules->GetNextBestWeapon(this, pCurrent);
	
	if ( ( pNewWeapon != nullptr ) && ( pNewWeapon != pCurrent ) )
	{
		return Weapon_Switch( pNewWeapon );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Switches to the given weapon (providing it has ammo)
// Input  :
// Output : true is switch succeeded
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex /*=0*/ ) 
{
	if ( pWeapon == nullptr )
		return false;

	// Already have it out?
	if ( m_hActiveWeapon.Get() == pWeapon )
	{
		if ( !m_hActiveWeapon->IsWeaponVisible() || m_hActiveWeapon->IsHolstered() )
			return m_hActiveWeapon->Deploy( );
		return false;
	}

	if (!Weapon_CanSwitchTo(pWeapon))
	{
		return false;
	}

	if ( m_hActiveWeapon )
	{
		if ( !m_hActiveWeapon->IsAlwaysActive() )
		{
			if ( !m_hActiveWeapon->Holster( pWeapon ) )
			{
				return false;
			}
		}
	}

	m_hActiveWeapon = pWeapon;
	return pWeapon->Deploy( );
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not we can switch to the given weapon.
// Input  : pWeapon - 
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::Weapon_CanSwitchTo( CBaseCombatWeapon *pWeapon )
{
	if (IsPlayer())
	{
		CBasePlayer *pPlayer = (CBasePlayer *)this;
#if !defined( CLIENT_DLL )
		IServerVehicle *pVehicle = pPlayer->GetVehicle();
#else
		IClientVehicle *pVehicle = pPlayer->GetVehicle();
#endif
		if (pVehicle && !pPlayer->UsingStandardWeaponsInVehicle())
			return false;
	}

	if ( !pWeapon->HasAnyAmmo() && !GetAmmoCount( pWeapon->m_iPrimaryAmmoType ) )
		return false;

	if ( !pWeapon->CanDeploy() )
		return false;
	
	if ( m_hActiveWeapon )
	{
		if ( !m_hActiveWeapon->CanHolster() )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBaseCombatWeapon
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseCombatCharacter::GetActiveWeapon() const
{
	return m_hActiveWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : i - 
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseCombatCharacter::GetWeapon( int i ) const
{
	contract_assert( (i >= 0) && (i < MAX_WEAPONS) );
	return m_hMyWeapons[i].Get();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iCount - 
//			iAmmoIndex - 
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::RemoveAmmo( int iCount, int iAmmoIndex )
{
	if (iCount <= 0)
		return;

	if ( iAmmoIndex < 0 )
		return;

	// Infinite ammo?
	if ( GetAmmoDef()->CanCarryInfiniteAmmo( iAmmoIndex ) )
		return;

	extern ConVar sv_infinite_ammo;
	if ( sv_infinite_ammo.GetInt() == 2 ) // infinite total ammo but magazine reloads are still required.
		return;

	// Ammo pickup sound
	m_iAmmo.Set( iAmmoIndex, std::max( m_iAmmo[iAmmoIndex] - iCount, 0 ) );
}

void CBaseCombatCharacter::RemoveAmmo( int iCount, const char *szName )
{
	RemoveAmmo( iCount, GetAmmoDef()->Index(szName) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::RemoveAllAmmo( )
{
	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		m_iAmmo.Set( i, 0 );
	}
}

//-----------------------------------------------------------------------------
// FIXME: This is a sort of hack back-door only used by physgun!
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::SetAmmoCount( int iCount, int iAmmoIndex )
{
	// NOTE: No sound, no max check! Seems pretty bogus to me!
	m_iAmmo.Set( iAmmoIndex, iCount );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the amount of ammunition of a particular type owned
//			owned by the character
// Input  :	Ammo Index
// Output :	The amount of ammo
//-----------------------------------------------------------------------------
int CBaseCombatCharacter::GetAmmoCount( int iAmmoIndex ) const
{

	// DEPRECATED - Ido says please use GetReserveAmmoCount

	if ( iAmmoIndex == -1 )
		return 0;

	// Infinite ammo?
	if ( GetAmmoDef()->CanCarryInfiniteAmmo( iAmmoIndex ) )
		return 999;

	return m_iAmmo[ iAmmoIndex ];
}

//-----------------------------------------------------------------------------
// Purpose: Returns the amount of ammunition of the specified type the character's carrying
//-----------------------------------------------------------------------------
int	CBaseCombatCharacter::GetAmmoCount( char *szName ) const
{
	return GetAmmoCount( GetAmmoDef()->Index(szName) );
}

//-----------------------------------------------------------------------------
// Purpose: Returns weapon if already owns a weapon of this class
//-----------------------------------------------------------------------------
CBaseCombatWeapon* CBaseCombatCharacter::Weapon_OwnsThisType( const char *pszWeapon, int iSubType ) const
{
	for ( int i = 0; i < MAX_WEAPONS; i++ ) 
	{
		if ( m_hMyWeapons[i].Get() && FClassnameIs( m_hMyWeapons[i], pszWeapon ) )
		{
			// Make sure it matches the subtype
			if ( m_hMyWeapons[i]->GetSubType() == iSubType )
			{
				return m_hMyWeapons[i];
			}
		}
	}

	return nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the weapon (if any) in the requested slot
// Input  : slot - which slot to poll
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseCombatCharacter::Weapon_GetSlot( int slot ) const
{
	int	targetSlot = slot;

	// Check for that slot being occupied already
	for ( int i=0; i < MAX_WEAPONS; i++ )
	{
		if ( m_hMyWeapons[i].Get() != nullptr )
		{
			// If the slots match, it's already occupied
			if ( m_hMyWeapons[i]->GetSlot() == targetSlot )
				return m_hMyWeapons[i];
		}
	}

	return nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the weapon (if any) in the requested loadout position
// Input  : position - which slot to poll
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseCombatCharacter::Weapon_GetPosition( int position ) const
{
	// Check for that slot being occupied already
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		if ( m_hMyWeapons[ i ].Get() != nullptr )
		{
			CEconItemView * pItem = m_hMyWeapons[ i ]->GetEconItemView() ;
			if ( !pItem )
				continue;

			loadout_positions_t unItemPos = ( loadout_positions_t )pItem->GetItemDefinition()->GetLoadoutSlot( GetTeamNumber() );

			// If the slots match, it's already occupied
			if ( unItemPos == position )
				return m_hMyWeapons[ i ];
		}
	}

	return nullptr;
}




int CBaseCombatCharacter::BloodColor()
{
	return m_bloodColor;
}


//-----------------------------------------------------------------------------
// Blood color (see BLOOD_COLOR_* macros in baseentity.h)
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::SetBloodColor( int nBloodColor )
{
	m_bloodColor = nBloodColor;
}

//-----------------------------------------------------------------------------
/**
The main visibility check.  Checks all the entity specific reasons that could 
make IsVisible fail.  Then checks points in space to get environmental reasons.
This is LOS, plus invisibility and fog and smoke and such.
*/

enum VisCacheResult_t
{
	VISCACHE_UNKNOWN = 0,
	VISCACHE_IS_VISIBLE,
	VISCACHE_IS_NOT_VISIBLE,
};

enum
{
	VIS_CACHE_MAX_ENTRIES = 0xFFFF
};

constexpr auto VIS_CACHE_ENTRY_LIFE = .090f;

class CCombatCharVisCache : public CAutoGameSystemPerFrame
{
public:
	virtual void FrameUpdatePreEntityThink();
	virtual void LevelShutdownPreEntity();

private:
	struct VisCacheKey_t
	{
		CHandle< CBaseCombatCharacter >	m_hEntity1;
		CHandle< CBaseCombatCharacter >	m_hEntity2;
	};

	struct CVisCacheKeyLess
	{
		bool operator()( const VisCacheKey_t &lhs, const VisCacheKey_t &rhs ) const
		{
			return ( std::memcmp( &lhs, &rhs, sizeof( VisCacheKey_t ) ) < 0 );
		}
	};

	struct VisCacheEntry_t
	{
		float							m_flTime;
		bool							m_bEntity1CanSeeEntity2;
		bool							m_bEntity2CanSeeEntity1;
	};

	using VisCacheMap_t = std::map< VisCacheKey_t, VisCacheEntry_t, CVisCacheKeyLess >;

public:
	// An entry, and whether the caller's pair is stored the other way round.
	struct VisCacheRef_t
	{
		VisCacheMap_t::iterator	m_it;
		bool					m_bValid;
		bool					m_bReverse;
	};

	VisCacheRef_t LookupVisibility( const CBaseCombatCharacter *pChar1, CBaseCombatCharacter *pChar2 );
	VisCacheResult_t HasVisibility( const VisCacheRef_t &ref ) const;
	void RegisterVisibility( const VisCacheRef_t &ref, bool bChar1SeesChar2, bool bChar2SeesChar1 );

private:
	VisCacheMap_t m_VisCache;

	mutable int m_nTestCount;
	mutable int m_nHitCount;
};

void CCombatCharVisCache::FrameUpdatePreEntityThink()
{
	//	Msg( "test: %d/%d\n", m_nHitCount, m_nTestCount );

	// Lazy retirement of vis cache
	// NOTE: 256 was chosen heuristically based on a playthrough where 200
	// was the max # in the viscache where nothing could be retired.
	if ( m_VisCache.size() < 256 )
		return;

	for ( int i = 0; i < 8; ++i )
	{
		auto it = std::next( m_VisCache.begin(), RandomInt( 0, (int)m_VisCache.size() - 1 ) );
		const VisCacheKey_t &key = it->first;
		if ( !key.m_hEntity1.IsValid() || !key.m_hEntity2.IsValid() || ( gpGlobals->curtime - it->second.m_flTime > 10.0f ) )
		{
			m_VisCache.erase( it );
		}
	}
}

void CCombatCharVisCache::LevelShutdownPreEntity()
{
	m_VisCache.clear();
}

CCombatCharVisCache::VisCacheRef_t CCombatCharVisCache::LookupVisibility( const CBaseCombatCharacter *pChar1, CBaseCombatCharacter *pChar2 )
{
	VisCacheKey_t cacheKey;
	if ( pChar1 < pChar2 )
	{
		cacheKey.m_hEntity1 = pChar1;
		cacheKey.m_hEntity2 = pChar2;
	}
	else
	{
		cacheKey.m_hEntity1 = pChar2;
		cacheKey.m_hEntity2 = pChar1;
	}

	auto it = m_VisCache.find( cacheKey );
	if ( it == m_VisCache.end() )
	{
		if ( m_VisCache.size() >= VIS_CACHE_MAX_ENTRIES )
			return { m_VisCache.end(), false, false };

		it = m_VisCache.emplace( cacheKey, VisCacheEntry_t{} ).first;
		it->second.m_flTime = gpGlobals->curtime - 2.0f * VIS_CACHE_ENTRY_LIFE;
	}

	return { it, true, !( pChar1 < pChar2 ) };
}

VisCacheResult_t CCombatCharVisCache::HasVisibility( const VisCacheRef_t &ref ) const
{
	if ( !ref.m_bValid )
		return VISCACHE_UNKNOWN;

	m_nTestCount++;

	bool bReverse = ref.m_bReverse;
	const VisCacheEntry_t &entry = ref.m_it->second;
	if ( gpGlobals->curtime - entry.m_flTime > VIS_CACHE_ENTRY_LIFE )
		return VISCACHE_UNKNOWN;

	m_nHitCount++;

	bool bIsVisible = !bReverse ? entry.m_bEntity1CanSeeEntity2 : entry.m_bEntity2CanSeeEntity1;
	return bIsVisible ? VISCACHE_IS_VISIBLE : VISCACHE_IS_NOT_VISIBLE;
}

void CCombatCharVisCache::RegisterVisibility( const VisCacheRef_t &ref, bool bEntity1CanSeeEntity2, bool bEntity2CanSeeEntity1 )
{
	if ( !ref.m_bValid )
		return;

	bool bReverse = ref.m_bReverse;
	VisCacheEntry_t &entry = ref.m_it->second;
	entry.m_flTime = gpGlobals->curtime;
	if ( !bReverse )
	{
		entry.m_bEntity1CanSeeEntity2 = bEntity1CanSeeEntity2;
		entry.m_bEntity2CanSeeEntity1 = bEntity2CanSeeEntity1;
	}
	else
	{
		entry.m_bEntity1CanSeeEntity2 = bEntity2CanSeeEntity1;
		entry.m_bEntity2CanSeeEntity1 = bEntity1CanSeeEntity2;
	}
}

static CCombatCharVisCache s_CombatCharVisCache;


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::IsAbleToSee( const CBaseEntity *pEntity, FieldOfViewCheckType checkFOV )
{
	CBaseCombatCharacter *pBCC = const_cast<CBaseEntity *>( pEntity )->MyCombatCharacterPointer();
	if ( pBCC )
		return IsAbleToSee( pBCC, checkFOV );

	// Test this every time; it's cheap.
	Vector vecEyePosition = EyePosition();
	Vector vecTargetPosition = pEntity->WorldSpaceCenter();

#ifdef GAME_DLL
	Vector vecEyeToTarget;
	VectorSubtract( vecTargetPosition, vecEyePosition, vecEyeToTarget );
	float flDistToOther = VectorNormalize( vecEyeToTarget ); 

	// We can't see because they are too far in the fog
	if ( IsHiddenByFog( flDistToOther ) )
		return false;
#endif

	if ( !ComputeLOS( vecEyePosition, vecTargetPosition ) )
		return false;


	return ( checkFOV != USE_FOV || IsInFieldOfView( vecTargetPosition ) );
}

static void ComputeSeeTestPosition( Vector *pEyePosition, CBaseCombatCharacter *pBCC )
{
	{
		*pEyePosition = pBCC->EyePosition();
	}	
}

bool CBaseCombatCharacter::IsAbleToSee( CBaseCombatCharacter *pBCC, FieldOfViewCheckType checkFOV )
{
	Vector vecEyePosition, vecOtherEyePosition;
	ComputeSeeTestPosition( &vecEyePosition, this );
	ComputeSeeTestPosition( &vecOtherEyePosition, pBCC );

#ifdef GAME_DLL
	Vector vecEyeToTarget;
	VectorSubtract( vecOtherEyePosition, vecEyePosition, vecEyeToTarget );
	float flDistToOther = VectorNormalize( vecEyeToTarget ); 

	// Test this every time; it's cheap.
	// We can't see because they are too far in the fog
	if ( IsHiddenByFog( flDistToOther ) )
		return false;

#endif

	// Check if we have a cached-off visibility
	CCombatCharVisCache::VisCacheRef_t cacheRef = s_CombatCharVisCache.LookupVisibility( this, pBCC );
	VisCacheResult_t nResult = s_CombatCharVisCache.HasVisibility( cacheRef );

	// Compute symmetric visibility
	if ( nResult == VISCACHE_UNKNOWN )
	{
		bool bThisCanSeeOther = false, bOtherCanSeeThis = false;
		if ( ComputeLOS( vecEyePosition, vecOtherEyePosition ) )
		{
			bThisCanSeeOther = true, bOtherCanSeeThis = true;
		}

		s_CombatCharVisCache.RegisterVisibility( cacheRef, bThisCanSeeOther, bOtherCanSeeThis );
		nResult = bThisCanSeeOther ? VISCACHE_IS_VISIBLE : VISCACHE_IS_NOT_VISIBLE;
	}

	if ( nResult == VISCACHE_IS_VISIBLE )
		return ( checkFOV != USE_FOV || IsInFieldOfView( pBCC ) );

	return false;
}

class CTraceFilterNoCombatCharacters : public CTraceFilterSimple
{
public:
	CTraceFilterNoCombatCharacters( const IHandleEntity *passentity = nullptr, int collisionGroup = COLLISION_GROUP_NONE )
		: CTraceFilterSimple( passentity, collisionGroup )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		if ( CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask ) )
		{
			CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
			if ( !pEntity )
				return false;

			if ( pEntity->MyCombatCharacterPointer() || pEntity->MyCombatWeaponPointer() )
				return false;

			// Honor BlockLOS - this lets us see through partially-broken doors, etc
			if ( !pEntity->BlocksLOS() )
				return false;

			return true;
		}

		return false;
	}
};

bool CBaseCombatCharacter::ComputeLOS( const Vector &vecEyePosition, const Vector &vecTarget ) const
{
	// We simply can't see because the world is in the way.
	trace_t result;
	CTraceFilterNoCombatCharacters traceFilter( nullptr, COLLISION_GROUP_NONE );
	UTIL_TraceLine( vecEyePosition, vecTarget, MASK_OPAQUE | CONTENTS_IGNORE_NODRAW_OPAQUE | CONTENTS_MONSTER, &traceFilter, &result );
	return ( result.fraction == 1.0f );
}



//-----------------------------------------------------------------------------
/**
Return true if our view direction is pointing at the given target, 
within the cosine of the angular tolerance. LINE OF SIGHT IS NOT CHECKED.
*/
bool CBaseCombatCharacter::IsLookingTowards( const CBaseEntity *target, float cosTolerance ) const
{
	return IsLookingTowards( target->WorldSpaceCenter(), cosTolerance ) || IsLookingTowards( target->EyePosition(), cosTolerance ) || IsLookingTowards( target->GetAbsOrigin(), cosTolerance );
}


//-----------------------------------------------------------------------------
/**
Return true if our view direction is pointing at the given target, 
within the cosine of the angular tolerance. LINE OF SIGHT IS NOT CHECKED.
*/
bool CBaseCombatCharacter::IsLookingTowards( const Vector &target, float cosTolerance ) const
{
	Vector toTarget = target - EyePosition();
	toTarget.NormalizeInPlace();

	Vector forward;
	AngleVectors( EyeAngles(), &forward );

	return ( DotProduct( forward, toTarget ) >= cosTolerance );
}


//-----------------------------------------------------------------------------
/**
Returns true if we are looking towards something within a tolerence determined 
by our field of view
*/
bool CBaseCombatCharacter::IsInFieldOfView( CBaseEntity *entity ) const
{
	CBasePlayer *pPlayer = ToBasePlayer( const_cast< CBaseCombatCharacter* >( this ) );
	float flTolerance = pPlayer ? std::cos( DEG2RAD( pPlayer->GetFOV() * 0.5f ) ) : BCC_DEFAULT_LOOK_TOWARDS_TOLERANCE;

	Vector vecForward;
	Vector vecEyePosition = EyePosition();
	AngleVectors( EyeAngles(), &vecForward );

	// FIXME: Use a faster check than this!

	// Check 3 spots, or else when standing right next to someone looking at their eyes, 
	// the angle will be too great to see their center.
	Vector vecToTarget = entity->GetAbsOrigin() - vecEyePosition;
	vecToTarget.NormalizeInPlace();
	if ( DotProduct( vecForward, vecToTarget ) >= flTolerance )
		return true;

	vecToTarget = entity->WorldSpaceCenter() - vecEyePosition;
	vecToTarget.NormalizeInPlace();
	if ( DotProduct( vecForward, vecToTarget ) >= flTolerance )
		return true;

	vecToTarget = entity->EyePosition() - vecEyePosition;
	vecToTarget.NormalizeInPlace();
	return ( DotProduct( vecForward, vecToTarget ) >= flTolerance );
}

//-----------------------------------------------------------------------------
/**
Returns true if we are looking towards something within a tolerence determined 
by our field of view
*/
bool CBaseCombatCharacter::IsInFieldOfView( const Vector &pos ) const
{
	CBasePlayer *pPlayer = ToBasePlayer( const_cast< CBaseCombatCharacter* >( this ) );

	if ( pPlayer )
		return IsLookingTowards( pos, std::cos( DEG2RAD( pPlayer->GetFOV() * 0.5f ) ) );

	return IsLookingTowards( pos );
}

//-----------------------------------------------------------------------------
/**
Strictly checks Line of Sight only.
*/

bool CBaseCombatCharacter::IsLineOfSightClear( CBaseEntity *entity, LineOfSightCheckType checkType ) const
{
#ifdef CLIENT_DLL
	if ( entity->MyCombatCharacterPointer() )
		return IsLineOfSightClear( entity->EyePosition(), checkType, entity );
	return IsLineOfSightClear( entity->WorldSpaceCenter(), checkType, entity );
#else
	// FIXME: Should we do the same check here as the client does?
	return IsLineOfSightClear( entity->WorldSpaceCenter(), checkType, entity ) || IsLineOfSightClear( entity->EyePosition(), checkType, entity ) || IsLineOfSightClear( entity->GetAbsOrigin(), checkType, entity );
#endif
}

//-----------------------------------------------------------------------------
/**
Strictly checks Line of Sight only.
*/
static bool TraceFilterNoCombatCharacters( IHandleEntity *pServerEntity, int contentsMask )
{
	// Honor BlockLOS also to allow seeing through partially-broken doors
	CBaseEntity *entity = EntityFromEntityHandle( pServerEntity );
	return ( entity->MyCombatCharacterPointer() == nullptr && !entity->MyCombatWeaponPointer() && entity->BlocksLOS() );
}

bool CBaseCombatCharacter::IsLineOfSightClear( const Vector &pos, LineOfSightCheckType checkType, CBaseEntity *entityToIgnore ) const
{
	if( checkType == IGNORE_ACTORS )
	{

		// use the query cache unless it causes problems
		trace_t trace;
		CTraceFilterNoCombatCharacters traceFilter( entityToIgnore, COLLISION_GROUP_NONE );
		UTIL_TraceLine( EyePosition(), pos, MASK_OPAQUE | CONTENTS_IGNORE_NODRAW_OPAQUE | CONTENTS_MONSTER, &traceFilter, &trace );

		return trace.fraction == 1.0f;
	}
	else
	{
		trace_t trace;
		CTraceFilterSkipTwoEntities traceFilter( this, entityToIgnore, COLLISION_GROUP_NONE );
		UTIL_TraceLine( EyePosition(), pos, MASK_OPAQUE | CONTENTS_IGNORE_NODRAW_OPAQUE, &traceFilter, &trace );

		return trace.fraction == 1.0f;
	}
}
