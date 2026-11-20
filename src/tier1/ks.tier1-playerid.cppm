// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1.playerid;

import std;
import ks.core.types;
import ks.platform;
import ks.simd;

export extern "C++" {

typedef uint32 AccountID_t;

// Universes.  Each universe is a self-contained instance.
enum EUniverse
{
	k_EUniverseInvalid = 0,
	k_EUniversePublic = 1,
	k_EUniverseBeta = 2,
	k_EUniverseInternal = 3,
	k_EUniverseDev = 4,
	// k_EUniverseRC = 5,				// no such universe anymore
	k_EUniverseMax
};

// Account types
enum EAccountType
{
	k_EAccountTypeInvalid = 0,			
	k_EAccountTypeIndividual = 1,		// single user account
	k_EAccountTypeMultiseat = 2,		// multiseat (e.g. cybercafe) account
	k_EAccountTypeGameServer = 3,		// game server account
	k_EAccountTypeAnonGameServer = 4,	// anonymous game server account
	k_EAccountTypePending = 5,			// pending
	k_EAccountTypeContentServer = 6,	// content server
	k_EAccountTypeClan = 7,
	k_EAccountTypeChat = 8,
	k_EAccountTypeConsoleUser = 9,		// Fake player ID for local PSN account on PS3 or Live account on 360, etc.
	k_EAccountTypeAnonUser = 10,

	// Max of 16 items in this field
	k_EAccountTypeMax
};

inline const unsigned int k_unAccountIDMask = 0xFFFFFFFF;
inline const unsigned int k_unAccountInstanceMask = 0x000FFFFF;
// we allow 3 simultaneous user account instances right now, 1= desktop, 2 = console, 4 = web, 0 = all
inline const unsigned int k_unUserDesktopInstance	= 1;	 
inline const unsigned int k_unUserConsoleInstance	= 2;
inline const unsigned int k_unUserWebInstance		= 4;


// Special flags for Chat accounts - they go in the top 8 bits
// of the player ID's "instance", leaving 12 for the actual instances
enum EChatPlayerIDInstanceFlags
{
	k_EChatAccountInstanceMask = 0x00000FFF, // top 8 bits are flags

	k_EChatInstanceFlagClan = ( k_unAccountInstanceMask + 1 ) >> 1,	// top bit
	k_EChatInstanceFlagLobby = ( k_unAccountInstanceMask + 1 ) >> 2,	// next one down, etc
	k_EChatInstanceFlagMMSLobby = ( k_unAccountInstanceMask + 1 ) >> 3,	// next one down, etc

	// Max of 8 flags
};

#pragma pack( push, 1 )

// player ID structure (64 bits total)
class CPlayerId
{
public:

	//-----------------------------------------------------------------------------
	// Purpose: Constructor
	//-----------------------------------------------------------------------------
	CPlayerId()
	{
		m_playerid.m_comp.m_unAccountID = 0;
		m_playerid.m_comp.m_EAccountType = k_EAccountTypeInvalid;
		m_playerid.m_comp.m_EUniverse = k_EUniverseInvalid;
		m_playerid.m_comp.m_unAccountInstance = 0;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Constructor
	// Input  : unAccountID -	32-bit account ID
	//			eUniverse -		Universe this account belongs to
	//			eAccountType -	Type of account
	//-----------------------------------------------------------------------------
	CPlayerId( uint32 unAccountID, EUniverse eUniverse, EAccountType eAccountType )
	{
		Set( unAccountID, eUniverse, eAccountType );
	}

	//-----------------------------------------------------------------------------
	// Purpose: Constructor
	// Input  : unAccountID -	32-bit account ID
	//			unAccountInstance - instance 
	//			eUniverse -		Universe this account belongs to
	//			eAccountType -	Type of account
	//-----------------------------------------------------------------------------
	CPlayerId( uint32 unAccountID, unsigned int unAccountInstance, EUniverse eUniverse, EAccountType eAccountType )
	{
		InstancedSet( unAccountID, unAccountInstance, eUniverse, eAccountType );
	}

	//-----------------------------------------------------------------------------
	// Purpose: Constructor
	// Input  : ulPlayerID -		64-bit representation of a player ID
	// Note:	Will not accept a uint32 or int32 as input, as that is a probable mistake.
	//			See the stubbed out overloads in the private: section for more info.
	//-----------------------------------------------------------------------------
	CPlayerId( uint64 ulPlayerID )
	{
		SetFromUint64( ulPlayerID );
	}

	//-----------------------------------------------------------------------------
	// Purpose: Sets parameters for player ID
	// Input  : unAccountID -	32-bit account ID
	//			eUniverse -		Universe this account belongs to
	//			eAccountType -	Type of account
	//-----------------------------------------------------------------------------
	void Set( uint32 unAccountID, EUniverse eUniverse, EAccountType eAccountType )
	{
		m_playerid.m_comp.m_unAccountID = unAccountID;
		m_playerid.m_comp.m_EUniverse = eUniverse;
		m_playerid.m_comp.m_EAccountType = eAccountType;

		if ( eAccountType == k_EAccountTypeClan || eAccountType == k_EAccountTypeGameServer )
		{
			m_playerid.m_comp.m_unAccountInstance = 0;
		}
		else
		{
			// by default we pick the desktop instance
			m_playerid.m_comp.m_unAccountInstance = k_unUserDesktopInstance;
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: Sets parameters for player ID
	// Input  : unAccountID -	32-bit account ID
	//			eUniverse -		Universe this account belongs to
	//			eAccountType -	Type of account
	//-----------------------------------------------------------------------------
	void InstancedSet( uint32 unAccountID, uint32 unInstance, EUniverse eUniverse, EAccountType eAccountType )
	{
		m_playerid.m_comp.m_unAccountID = unAccountID;
		m_playerid.m_comp.m_EUniverse = eUniverse;
		m_playerid.m_comp.m_EAccountType = eAccountType;
		m_playerid.m_comp.m_unAccountInstance = unInstance;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Initializes a player ID from its 52 bit parts and universe/type
	// Input  : ulIdentifier - 52 bits of goodness
	//-----------------------------------------------------------------------------
	void FullSet( uint64 ulIdentifier, EUniverse eUniverse, EAccountType eAccountType )
	{
		m_playerid.m_comp.m_unAccountID = ( ulIdentifier & k_unAccountIDMask );						// account ID is low 32 bits
		m_playerid.m_comp.m_unAccountInstance = ( ( ulIdentifier >> 32 ) & k_unAccountInstanceMask );			// account instance is next 20 bits
		m_playerid.m_comp.m_EUniverse = eUniverse;
		m_playerid.m_comp.m_EAccountType = eAccountType;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Initializes a player ID from its 64-bit representation
	// Input  : ulPlayerID -		64-bit representation of a player ID
	//-----------------------------------------------------------------------------
	void SetFromUint64( uint64 ulPlayerID )
	{
		m_playerid.m_unAll64Bits = ulPlayerID;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Clear all fields, leaving an invalid ID.
	//-----------------------------------------------------------------------------
    void Clear()
	{
		m_playerid.m_comp.m_unAccountID = 0;
		m_playerid.m_comp.m_EAccountType = k_EAccountTypeInvalid;
		m_playerid.m_comp.m_EUniverse = k_EUniverseInvalid;
		m_playerid.m_comp.m_unAccountInstance = 0;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Converts player ID to its 64-bit representation
	// Output : 64-bit representation of a player ID
	//-----------------------------------------------------------------------------
	uint64 ConvertToUint64() const
	{
		return m_playerid.m_unAll64Bits;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Converts the static parts of a player ID to a 64-bit representation.
	//			For multiseat accounts, all instances of that account will have the
	//			same static account key, so they can be grouped together by the static
	//			account key.
	// Output : 64-bit static account key
	//-----------------------------------------------------------------------------
	uint64 GetStaticAccountKey() const
	{
		// note we do NOT include the account instance (which is a dynamic property) in the static account key
		return (uint64) ( ( ( (uint64) m_playerid.m_comp.m_EUniverse ) << 56 ) + ((uint64) m_playerid.m_comp.m_EAccountType << 52 ) + m_playerid.m_comp.m_unAccountID );
	}

	//-----------------------------------------------------------------------------
	// Purpose: create an anonymous game server login to be filled in by the AM
	//-----------------------------------------------------------------------------
	void CreateBlankAnonLogon( EUniverse eUniverse )
	{
		m_playerid.m_comp.m_unAccountID = 0;
		m_playerid.m_comp.m_EAccountType = k_EAccountTypeAnonGameServer;
		m_playerid.m_comp.m_EUniverse = eUniverse;
		m_playerid.m_comp.m_unAccountInstance = 0;
	}

	//-----------------------------------------------------------------------------
	// Purpose: create an anonymous game server login to be filled in by the AM
	//-----------------------------------------------------------------------------
	void CreateBlankAnonUserLogon( EUniverse eUniverse )
	{
		m_playerid.m_comp.m_unAccountID = 0;
		m_playerid.m_comp.m_EAccountType = k_EAccountTypeAnonUser;
		m_playerid.m_comp.m_EUniverse = eUniverse;
		m_playerid.m_comp.m_unAccountInstance = 0;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Is this an anonymous game server login that will be filled in?
	//-----------------------------------------------------------------------------
	bool BBlankAnonAccount() const
	{
		return m_playerid.m_comp.m_unAccountID == 0 && BAnonAccount() && m_playerid.m_comp.m_unAccountInstance == 0;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Is this a game server account id?  (Either persistent or anonymous)
	//-----------------------------------------------------------------------------
	bool BGameServerAccount() const
	{
		return m_playerid.m_comp.m_EAccountType == k_EAccountTypeGameServer || m_playerid.m_comp.m_EAccountType == k_EAccountTypeAnonGameServer;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Is this a persistent (not anonymous) game server account id?
	//-----------------------------------------------------------------------------
	bool BPersistentGameServerAccount() const
	{
		return m_playerid.m_comp.m_EAccountType == k_EAccountTypeGameServer;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Is this an anonymous game server account id?
	//-----------------------------------------------------------------------------
	bool BAnonGameServerAccount() const
	{
		return m_playerid.m_comp.m_EAccountType == k_EAccountTypeAnonGameServer;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Is this a content server account id?
	//-----------------------------------------------------------------------------
	bool BContentServerAccount() const
	{
		return m_playerid.m_comp.m_EAccountType == k_EAccountTypeContentServer;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Is this a clan account id?
	//-----------------------------------------------------------------------------
	bool BClanAccount() const
	{
		return m_playerid.m_comp.m_EAccountType == k_EAccountTypeClan;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Is this a chat account id?
	//-----------------------------------------------------------------------------
	bool BChatAccount() const
	{
		return m_playerid.m_comp.m_EAccountType == k_EAccountTypeChat;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Is this a chat account id?
	//-----------------------------------------------------------------------------
	bool IsLobby() const
	{
		return ( m_playerid.m_comp.m_EAccountType == k_EAccountTypeChat )
			&& ( m_playerid.m_comp.m_unAccountInstance & k_EChatInstanceFlagLobby );
	}

	//-----------------------------------------------------------------------------
	// Purpose: Is this an individual user account id?
	//-----------------------------------------------------------------------------
	bool BIndividualAccount() const
	{
		return m_playerid.m_comp.m_EAccountType == k_EAccountTypeIndividual || m_playerid.m_comp.m_EAccountType == k_EAccountTypeConsoleUser;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Is this an anonymous account?
	//-----------------------------------------------------------------------------
	bool BAnonAccount() const
	{
		return m_playerid.m_comp.m_EAccountType == k_EAccountTypeAnonUser || m_playerid.m_comp.m_EAccountType == k_EAccountTypeAnonGameServer;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Is this an anonymous user account? ( used to create an account or reset a password )
	//-----------------------------------------------------------------------------
	bool BAnonUserAccount() const
	{
		return m_playerid.m_comp.m_EAccountType == k_EAccountTypeAnonUser;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Is this a faked up player ID for a PSN friend account?
	//-----------------------------------------------------------------------------
	bool BConsoleUserAccount() const
	{
		return m_playerid.m_comp.m_EAccountType == k_EAccountTypeConsoleUser;
	}

	// simple accessors
	void SetAccountID( uint32 unAccountID )		{ m_playerid.m_comp.m_unAccountID = unAccountID; }
	void SetAccountInstance( uint32 unInstance ){ m_playerid.m_comp.m_unAccountInstance = unInstance; }
	void ClearIndividualInstance()				{ if ( BIndividualAccount() ) m_playerid.m_comp.m_unAccountInstance = 0; }
	bool HasNoIndividualInstance() const		{ return BIndividualAccount() && (m_playerid.m_comp.m_unAccountInstance==0); }
	AccountID_t GetAccountID() const			{ return m_playerid.m_comp.m_unAccountID; }
	uint32 GetUnAccountInstance() const			{ return m_playerid.m_comp.m_unAccountInstance; }
	EAccountType GetEAccountType() const		{ return ( EAccountType ) m_playerid.m_comp.m_EAccountType; }
	EUniverse GetEUniverse() const				{ return m_playerid.m_comp.m_EUniverse; }
	void SetEUniverse( EUniverse eUniverse )	{ m_playerid.m_comp.m_EUniverse = eUniverse; }
	bool IsValid() const;

	// this set of functions is hidden, will be moved out of class
	explicit CPlayerId( const char *pchPlayerID, EUniverse eDefaultUniverse = k_EUniverseInvalid );
	const char * Render() const;				// renders this player ID to string
	static const char * Render( uint64 ulPlayerID );	// static method to render a uint64 representation of a player ID to a string

	const char * RenderLink() const;				// renders this player ID to an admin console link string
	static const char * RenderLink( uint64 ulPlayerID );	// static method to render a uint64 representation of a player ID to a string

	void SetFromString( const char *pchPlayerID, EUniverse eDefaultUniverse );
    // SetFromString allows many partially-correct strings, constraining how
    // we might be able to change things in the future.
    // SetFromStringStrict requires the exact string forms that we support
    // and is preferred when the caller knows it's safe to be strict.
    // Returns whether the string parsed correctly.
	bool SetFromStringStrict( const char *pchPlayerID, EUniverse eDefaultUniverse );

	inline bool operator==( const CPlayerId &val ) const { return m_playerid.m_unAll64Bits == val.m_playerid.m_unAll64Bits; } 
	inline bool operator!=( const CPlayerId &val ) const { return !operator==( val ); }
	inline bool operator<( const CPlayerId &val ) const { return m_playerid.m_unAll64Bits < val.m_playerid.m_unAll64Bits; }
	inline bool operator>( const CPlayerId &val ) const { return m_playerid.m_unAll64Bits > val.m_playerid.m_unAll64Bits; }

	// DEBUG function
	bool BValidExternalPlayerID() const;

private:
	// These are defined here to prevent accidental implicit conversion of a u32AccountID to a CPlayerId.
	// If you get a compiler error about an ambiguous constructor/function then it may be because you're
	// passing a 32-bit int to a function that takes a CPlayerId. You should explicitly create the player ID
	// using the correct Universe and account Type/Instance values.
	CPlayerId( uint32 );
	CPlayerId( int32 );

	// 64 bits total
	union PlayerID_t
	{
		struct PlayerIDComponent_t
		{
			uint32				m_unAccountID : 32;			// unique account identifier
			unsigned int		m_unAccountInstance : 20;	// dynamic instance ID
			unsigned int		m_EAccountType : 4;			// type of account - can't show as EAccountType, due to signed / unsigned difference
			EUniverse			m_EUniverse : 8;	// universe this account belongs to
		} m_comp;

		uint64 m_unAll64Bits;
	} m_playerid;
};

inline bool CPlayerId::IsValid() const
{
	if ( m_playerid.m_comp.m_EAccountType <= k_EAccountTypeInvalid || m_playerid.m_comp.m_EAccountType >= k_EAccountTypeMax )
		return false;
	
	if ( m_playerid.m_comp.m_EUniverse <= k_EUniverseInvalid || m_playerid.m_comp.m_EUniverse >= k_EUniverseMax )
		return false;

	if ( m_playerid.m_comp.m_EAccountType == k_EAccountTypeIndividual )
	{
		if ( m_playerid.m_comp.m_unAccountID == 0 || m_playerid.m_comp.m_unAccountInstance > k_unUserWebInstance )
			return false;
	}

	if ( m_playerid.m_comp.m_EAccountType == k_EAccountTypeClan )
	{
		if ( m_playerid.m_comp.m_unAccountID == 0 || m_playerid.m_comp.m_unAccountInstance != 0 )
			return false;
	}

	if ( m_playerid.m_comp.m_EAccountType == k_EAccountTypeGameServer )
	{
		if ( m_playerid.m_comp.m_unAccountID == 0 )
			return false;
		// Any limit on instances?  We use them for local users and bots
	}
	return true;
}

// generic invalid CPlayerId
inline const auto k_playerIdNil = CPlayerId();

// This player ID comes from a user game connection to an out of date GS that hasnt implemented the protocol
// to provide its player ID
inline const auto k_playerIdOutOfDateGS = CPlayerId( 0, 0, k_EUniverseInvalid, k_EAccountTypeInvalid );
// This player ID comes from a user game connection to an sv_lan GS
inline const auto k_playerIdLanModeGS = CPlayerId( 0, 0, k_EUniversePublic, k_EAccountTypeInvalid );
// This player ID can come from a user game connection to a GS that has just booted but hasnt yet even initialized
// its component and started logging on.
inline const auto k_playerIdNotInitYetGS = CPlayerId( 1, 0, k_EUniverseInvalid, k_EAccountTypeInvalid );
// This player ID can come from a user game connection to a GS that isn't using an authentication system but still
// wants to support the "Join Game" option in the friends list
inline const auto k_playerIdNonGS = CPlayerId( 2, 0, k_EUniverseInvalid, k_EAccountTypeInvalid );

#pragma pack( pop )

}
