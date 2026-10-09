// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: NetAdr.cpp: implementation of the CNetAdr class.

import std;
import ks.tier1;


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

bool netadr_t::CompareAdr (const netadr_t &a, bool onlyBase) const
{
	if ( a.type != type )
		return false;

	if ( type == NA_LOOPBACK )
		return true;

	if ( type == NA_BROADCAST )
		return true;

	if ( type == NA_IP )
	{
		if ( !onlyBase && (port != a.port) )
			return false;

		if ( a.ip[0] == ip[0] && a.ip[1] == ip[1] && a.ip[2] == ip[2] && a.ip[3] == ip[3] )
			return true;
	}

	return false;
}

bool netadr_t::CompareClassBAdr (const netadr_t &a) const
{
	if ( a.type != type )
		return false;

	if ( type == NA_LOOPBACK )
		return true;

	if ( type == NA_IP )
	{
		if (a.ip[0] == ip[0] && a.ip[1] == ip[1] )
			return true;
	}

	return false;
}

bool netadr_t::CompareClassCAdr (const netadr_t &a) const
{
	if ( a.type != type )
		return false;

	if ( type == NA_LOOPBACK )
		return true;

	if ( type == NA_IP )
	{
		if (a.ip[0] == ip[0] && a.ip[1] == ip[1] && a.ip[2] == ip[2] )
			return true;
	}

	return false;
}
// reserved addresses are not routeable, so they can all be used in a LAN game
bool netadr_t::IsReservedAdr () const
{
	if ( type == NA_LOOPBACK )
		return true;

	if ( type == NA_IP )
	{
		if ( (ip[0] == 10) ||									// 10.x.x.x is reserved
			 (ip[0] == 127) ||									// 127.x.x.x 
			 (ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31) ||	// 172.16.x.x  - 172.31.x.x 
			 (ip[0] == 192 && ip[1] >= 168) ) 					// 192.168.x.x
			return true;
	}
	return false;
}

const char * netadr_t::ToString( bool onlyBase ) const
{
	// Select a static buffer
	static	char	s[4][64];
	static int slot = 0;
	int useSlot = ( slot++ ) % 4;

	// Render into it
	ToString( s[useSlot], sizeof(s[0]), onlyBase );

	// Pray the caller uses it before it gets clobbered
	return s[useSlot];
}

void netadr_t::ToString( char *pchBuffer, uint32 unBufferSize, bool onlyBase ) const
{

	if (type == NA_LOOPBACK)
	{
		V_strncpy( pchBuffer, "loopback", unBufferSize );
	}
	else if (type == NA_BROADCAST)
	{
		V_strncpy( pchBuffer, "broadcast", unBufferSize );
	}
	else if (type == NA_IP)
	{
		if ( onlyBase )
		{
			V_snprintf( pchBuffer, unBufferSize, "%i.%i.%i.%i", ip[0], ip[1], ip[2], ip[3]);
		}
		else
		{
			V_snprintf( pchBuffer, unBufferSize, "%i.%i.%i.%i:%i", ip[0], ip[1], ip[2], ip[3], WordSwapC(port));
		}
	}
	else
	{
		V_strncpy( pchBuffer, "unknown", unBufferSize );
	}
}

bool netadr_t::IsLocalhost() const
{
	// are we 127.0.0.1 ?
	return (ip[0] == 127) && (ip[1] == 0) && (ip[2] == 0) && (ip[3] == 1);
}

bool netadr_t::IsLoopback() const
{
	// are we useding engine loopback buffers
	return type == NA_LOOPBACK;
}

void netadr_t::Clear()
{
	ip[0] = ip[1] = ip[2] = ip[3] = 0;
	port = 0;
	type = NA_NULL;
}

void netadr_t::SetIP(uint8 b1, uint8 b2, uint8 b3, uint8 b4)
{
	ip[0] = b1;
	ip[1] = b2;
	ip[2] = b3;
	ip[3] = b4;
}

void netadr_t::SetIP(uint unIP)
{
	*((uint*)ip) = DWordSwapC( unIP );
}

void netadr_t::SetType(netadrtype_t newtype)
{
	type = newtype;
}

netadrtype_t netadr_t::GetType() const
{
	return type;
}

unsigned short netadr_t::GetPort() const
{
	return WordSwapC( port );
}

unsigned int netadr_t::GetIPNetworkByteOrder() const
{
	return *(unsigned int *)&ip;
}

unsigned int netadr_t::GetIPHostByteOrder() const
{
	return DWordSwapC( GetIPNetworkByteOrder() );
}

ks::platform::address netadr_t::ToAddress() const
{
	ks::platform::address a{};
	a.port = GetPort();
	if ( type == NA_BROADCAST )
		a.ip = 0xFFFFFFFFu;
	else if ( type == NA_LOOPBACK )
		a.ip = 0x7F000001u;
	else if ( type == NA_IP )
		a.ip = GetIPHostByteOrder();
	return a;
}

void netadr_t::SetFromAddress( const ks::platform::address &a )
{
	type = NA_IP;
	SetIP( a.ip );
	SetPort( a.port );
}

bool netadr_t::IsValid() const
{
	return ( (port !=0 ) && (type != NA_NULL) &&
			 ( ip[0] != 0 || ip[1] != 0 || ip[2] != 0 || ip[3] != 0 ) );
}

bool netadr_t::IsBaseAdrValid() const
{
	return ( (type != NA_NULL) &&
		( ip[0] != 0 || ip[1] != 0 || ip[2] != 0 || ip[3] != 0 ) );
}


void netadr_t::SetPort(unsigned short newport)
{
	port = WordSwapC( newport );
}

bool netadr_t::SetFromString( const char *pch, bool bUseDNS )
{
	Clear();
	type = NA_IP;

	contract_assert( pch );		// invalid to call this with NULL pointer; fix your code bug!
	if ( !pch )			// but let's not crash
		return false;

	char address[ 128 ];
	V_strcpy_safe( address, pch );

	if ( !V_strnicmp( address, "loopback", 8 ) )
	{
		char newaddress[ 128 ];
		type = NA_LOOPBACK;
		V_strcpy_safe( newaddress, "127.0.0.1" );
		V_strcat_safe( newaddress, address + 8 ); // copy anything after "loopback"

		V_strcpy_safe( address, newaddress );
	}

	if ( !V_strnicmp( address, "localhost", 9 ) )
	{
		V_memcpy( address, "127.0.0.1", 9 ); // Note use of memcpy allows us to keep the colon and rest of string since localhost and 127.0.0.1 are both 9 characters.
	}

	// Starts with a number and has a dot
	if ( address[0] >= '0' && 
		 address[0] <= '9' && 
		 std::strchr( address, '.' ) )
	{
		int n1 = -1, n2 = -1, n3 = -1, n4 = -1, n5 = 0; // set port to 0 if we don't parse one
		int nRes = std::sscanf( address, "%d.%d.%d.%d:%d", &n1, &n2, &n3, &n4, &n5 );
		if (
			nRes < 4
			|| n1 < 0 || n1 > 255
			|| n2 < 0 || n2 > 255
			|| n3 < 0 || n3 > 255
			|| n4 < 0 || n4 > 255
			|| n5 < 0 || n5 > 65535
		)
			return false;
		SetIP( n1, n2, n3, n4 );
		SetPort( ( uint16 ) n5 );
		return true;
	}

	if ( bUseDNS )
	{
// X360TBD:
	// dgoodenough - since this is skipped on X360, seems reasonable to skip as well on PS3
	// PS3_BUILDFIX
	// FIXME - Leap of faith, this works without asserting on X360, so I assume it will on PS3
		// Null out the colon if there is one
		char *pchColon = std::strchr( address, ':' );
		if ( pchColon )
		{
			*pchColon = 0;
		}
		
		// DNS it base name
		unsigned int resolved = 0;
		if ( !ks::platform::resolve_host( address, &resolved ) )
			return false;

		SetIP( resolved );

		// Set Port to whatever was specified after the colon
		if ( pchColon )
		{
			SetPort( V_atoi( ++pchColon ) );
		}
		return true;
	}

	return false;
}

bool netadr_t::operator<(const netadr_t &netadr) const
{
	if ( *((uint *)netadr.ip) < *((uint *)ip) )
		return true;
	else if ( *((uint *)netadr.ip) > *((uint *)ip) )
		return false;
	return ( netadr.port < port );
}
