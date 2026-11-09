#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"

#include "weapon_csbase.h"

#if defined( CLIENT_DLL )

	#define CKnife C_Knife
	#define CKnifeGG C_KnifeGG

#endif

// ----------------------------------------------------------------------------- //
// CKnife class definition.
// ----------------------------------------------------------------------------- //

class [[= ks::reflect::NetTable{ .name = "DT_WeaponKnife" } ]]
      CKnife : public CWeaponCSBase
{
public:
	DECLARE_CLASS( CKnife, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	
	CKnife();

	// We say yes to this so the weapon system lets us switch to it.
	virtual bool HasPrimaryAmmo();
	virtual bool CanBeSelected();
	
	KS_VIRTUAL void Precache_Impl();

	void Spawn_Impl();
	bool SwingOrStab( CSWeaponMode weaponMode );
	void PrimaryAttack();
	void SecondaryAttack();
	void WeaponAnimation( int iAnimation );

	virtual bool Deploy();

	bool CanDrop();

	void WeaponIdle();

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_KNIFE; }

private:
	CKnife( const CKnife & ) {}

#ifndef CLIENT_DLL

	bool m_swingLeft;

#endif

};

class [[= ks::reflect::NetTable{ .name = "DT_WeaponKnifeGG" } ]]
      CKnifeGG : public CKnife
{
public:
	DECLARE_CLASS( CKnifeGG, CKnife );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_KNIFE_GG; }
};
