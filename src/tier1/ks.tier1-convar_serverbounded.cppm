// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1:convar_serverbounded;

import std;
import ks.math;
export import :convar;

export extern "C++" {

// This class is used to bound a ConVar's value, so the client can restrict its 
// value while connected to a server.  Every read of the ConVar (GetFloat/GetInt/GetBool, from
// any module) goes through the bounding function.
//
// NOTE: FCVAR_USERINFO vars are not bounded before they are sent to the server
//       (we have no way to detect if the bounded value would change), so
//       if you want to use a bounded cvar's value on the server, you must rebound it
//       the same way the client does.
class ConVar_ServerBounded : public ConVar
{
public:
	// Computes the bounded value; use GetBaseFloatValue() to get the underlying cvar's value.
	typedef float ( *FnBoundedValue_t )( const ConVar_ServerBounded &cvar );

	ConVar_ServerBounded( FnBoundedValue_t fnBounded, char const *pName, char const *pDefaultValue, int flags, char const *pHelpString )
		: ConVar( pName, pDefaultValue, flags, pHelpString ), m_fnBounded( fnBounded )
	{
		SetKind( ConKind::VarServerBounded );
	}

	ConVar_ServerBounded( FnBoundedValue_t fnBounded, char const *pName, char const *pDefaultValue, int flags, char const *pHelpString, FnChangeCallback_t callback )
		: ConVar( pName, pDefaultValue, flags, pHelpString, callback ), m_fnBounded( fnBounded )
	{
		SetKind( ConKind::VarServerBounded );
	}
	
	ConVar_ServerBounded( FnBoundedValue_t fnBounded, char const *pName, char const *pDefaultValue, int flags, char const *pHelpString, bool bMin, float fMin, bool bMax, float fMax )
		: ConVar( pName, pDefaultValue, flags, pHelpString, bMin, fMin, bMax, fMax ), m_fnBounded( fnBounded )
	{
		SetKind( ConKind::VarServerBounded );
	}

	// Use this to get the underlying cvar's value.
	float GetBaseFloatValue() const
	{
		return GetRawFloat();
	}

	float GetBoundedValue() const
	{
		return m_fnBounded( *this );
	}

private:
	FnBoundedValue_t m_fnBounded;
};

}
