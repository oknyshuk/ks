// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

module;
#include "usermessages_schema.h"
export module ks.game.cstrike15.playerdecals_signature;

import ks.platform;
import ks.simd;
import ks.tier1.byte_writer;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;

export
{
// Purpose: Player decals signature validation code

//
// We will be using RSA 1024-bit private signing key
// PKCS1 signature length is guaranteed to be 128 bytes
//
inline constexpr auto PLAYERDECALS_SIGNATURE_VERSION = 1;
inline constexpr auto PLAYERDECALS_SIGNATURE_BYTELEN = 128;

inline constexpr auto PLAYERDECALS_NUMCHARGES = 50;

inline constexpr auto PLAYERDECALS_UNITS_SIZE = 48;
inline constexpr auto PLAYERDECALS_COOLDOWN_SECONDS = 45;

inline constexpr auto PLAYERDECALS_DURATION_SOLID = 240;
inline constexpr auto PLAYERDECALS_DURATION_FADE1 = 40;
inline constexpr auto PLAYERDECALS_DURATION_FADE2 = 120;
inline constexpr auto PLAYERDECALS_DURATION_APPLY = 1.5f;

inline constexpr auto PLAYERDECALS_LIMIT_COUNT = 100;

//
// SECURITY INFORMATION: this file is included in DLLs that are
// shipping to clients and to community gameservers with the intent
// for those processes to verify signatures.
// NEVER include/reference private key data in this header!
// Private key must be used only on GC.
//

inline bool BClientPlayerDecalSignatureComposeSignBuffer( ks::net::PlayerDecalDigitalSignature const &data, ks::byte_writer &buf )
{
	if ( data.endpos.size() != 3 ) return false;
	for ( int k = 0; k < 3; ++ k ) buf.write< float >( data.endpos[ k ] );
	if ( data.startpos.size() != 3 ) return false;
	for ( int k = 0; k < 3; ++k ) buf.write< float >( data.startpos[ k ] );
	if ( data.right.size() != 3 ) return false;
	for ( int k = 0; k < 3; ++k ) buf.write< float >( data.right[ k ] );
	if ( data.normal.size() != 3 ) return false;
	for ( int k = 0; k < 3; ++k ) buf.write< float >( data.normal[ k ] );
	
	buf.write< int >( data.tx_defidx );
	buf.write< int >( data.tint_id );
	buf.write< int >( data.entindex );
	buf.write< int >( data.hitbox );
	
	buf.write< float >( data.creationtime );

	buf.write< unsigned int >( data.accountid );
	buf.write< unsigned int >( data.rtime );
	buf.write< unsigned int >( data.trace_id );

	return true;
}

inline bool BValidateClientPlayerDecalSignature( ks::net::PlayerDecalDigitalSignature const &data )
{
	ks::byte_writer bufData( PLAYERDECALS_SIGNATURE_BYTELEN );
	if ( !BClientPlayerDecalSignatureComposeSignBuffer( data, bufData ) )
		return false;

	// Removed for partner depot
	return true;
}
}
