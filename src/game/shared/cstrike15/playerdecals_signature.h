//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: Player decals signature validation code
//
//=============================================================================//

#ifndef PLAYERDECALS_SIGNATURE_H
#define PLAYERDECALS_SIGNATURE_H

#include "tier1/byte_writer.h"

//
// We will be using RSA 1024-bit private signing key
// PKCS1 signature length is guaranteed to be 128 bytes
//
#define PLAYERDECALS_SIGNATURE_VERSION 1
#define PLAYERDECALS_SIGNATURE_BYTELEN 128

#define PLAYERDECALS_NUMCHARGES 50

#define PLAYERDECALS_UNITS_SIZE 48
#define PLAYERDECALS_COOLDOWN_SECONDS 45

#define PLAYERDECALS_DURATION_SOLID 240
#define PLAYERDECALS_DURATION_FADE1  40
#define PLAYERDECALS_DURATION_FADE2 120
#define PLAYERDECALS_DURATION_APPLY 1.5f

#define PLAYERDECALS_LIMIT_COUNT 100



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

#endif // PLAYERDECALS_SIGNATURE_H
