// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1.ns_address;

import std;
import ks.core.basetypes;
import ks.core.vprof;
import ks.platform;
import ks.simd;
export import ks.tier1.netadr;
export import ks.tier1.strtools;

export extern "C++" {

enum NetworkSystemAddressType_t
{
	NSAT_NETADR,
};

// Used to represent a remote network address
struct ns_address
{
	netadr_t m_adr; // ip:port and network type (NULL/IP/BROADCAST/etc).
	NetworkSystemAddressType_t m_AddrType;

	ns_address() : m_AddrType( NSAT_NETADR ){}

	void Clear ( )
	{
		m_AddrType = NSAT_NETADR;
		m_adr.Clear ( );
	}

	ns_address ( const netadr_t &other )
		: m_adr ( other )
		, m_AddrType ( NSAT_NETADR )
	{
	}

	ns_address& operator=(const netadr_t &other)
	{
		Clear ( );
		m_AddrType = NSAT_NETADR;
		m_adr = other;
		return *this;
	}

	bool IsLoopback ( void ) const
	{
		return (m_AddrType == NSAT_NETADR) && m_adr.IsLoopback ( );
	}

	bool IsLocalhost ( void ) const
	{
		return (m_AddrType == NSAT_NETADR) && m_adr.IsLocalhost ( );
	}

	bool IsBroadcast( void ) const
	{
		return (m_AddrType == NSAT_NETADR) && m_adr.GetType() == NA_BROADCAST;
	}

	bool IsReservedAdr( void ) const
	{
		return (m_AddrType == NSAT_NETADR) && m_adr.IsReservedAdr();
	}

	bool IsNull ( void ) const
	{
		switch ( m_AddrType )
		{
		case NSAT_NETADR:
			return m_adr.GetType ( ) == NA_NULL;
		};

		return true;
	}

	bool IsValid ( ) const
	{
		switch ( m_AddrType )
		{
		case NSAT_NETADR:
			return m_adr.IsValid ( );
		};

		return false;
	}

	bool CompareAdr ( const ns_address &other, bool onlyBase = false ) const
	{
		if ( m_AddrType != other.m_AddrType )
			return false;

		switch ( m_AddrType )
		{
		case NSAT_NETADR:
			return m_adr.CompareAdr ( other.m_adr, onlyBase );
		};

		return false;
	}

	bool operator==( const ns_address &rhs ) const
	{
		return CompareAdr( rhs, false );
	}

	bool operator!=( const ns_address &rhs ) const
	{
		return !CompareAdr( rhs, false );
	}

	NetworkSystemAddressType_t GetAddressType ( void ) const
	{
		return m_AddrType;
	}

	void SetAddrType( NetworkSystemAddressType_t type )
	{
		Clear();
		m_AddrType = type;
	}

	void SetFromAddress ( const ks::platform::address &a )
	{
		m_AddrType = NSAT_NETADR;
		m_adr.SetFromAddress ( a );
	}

	unsigned int GetIP ( void ) const
	{
		return (m_AddrType == NSAT_NETADR) ? m_adr.GetIPHostByteOrder( ) : 0;
	}

	unsigned short GetPort ( void ) const
	{
		return (m_AddrType == NSAT_NETADR) ? m_adr.GetPort ( ) : 0;
	}

	template <typename T> bool IsType ( void ) const
	{
		return false;
	}

	template <typename T> T *Get ( void )
	{
		return nullptr;
	}

	template <typename T> const T *Get ( void ) const
	{
		return nullptr;
	}

	template <typename T> T &AsType ( void )
	{
		contract_assert( false );
		static T dummy;
		return dummy;
	}

	template <typename T> const T &AsType ( void ) const
	{
		contract_assert( false );
		static T dummy;
		return dummy;
	}

	bool SetFromString( const char *s )
	{
		Clear();
		if ( !s )
			return false;
		if ( m_adr.SetFromString( s, true ) )
		{
			m_AddrType = NSAT_NETADR;
			return true;
		}

		return false;
	}

};

template <>
inline bool ns_address::IsType<netadr_t> ( void ) const
{
	return (m_AddrType == NSAT_NETADR);
}

template <>
inline netadr_t *ns_address::Get<netadr_t> ( void )
{
	return IsType<netadr_t> ( ) ? &m_adr : nullptr;
}

template <>
inline const netadr_t *ns_address::Get<netadr_t> ( void ) const
{
	return IsType<netadr_t> ( ) ? &m_adr : nullptr;
}

template <>
inline netadr_t &ns_address::AsType<netadr_t> ( void )
{
	contract_assert( IsType<netadr_t> ( ) );
	return m_adr;
}

template <>
inline const netadr_t &ns_address::AsType<netadr_t> ( void ) const
{
	contract_assert( IsType<netadr_t> ( ) );
	return m_adr;
}

/// Utility class used to render an address
class ns_address_render
{
	char m_buf[64];
public:
	ns_address_render( const ns_address &a )
	{
		switch ( a.m_AddrType )
		{
		case NSAT_NETADR:
			a.m_adr.ToString( m_buf, sizeof( m_buf ) );
			break;

		default:
			m_buf[0] = '\0';
		};
	}
	const char *String() const { return m_buf; }
};

}
