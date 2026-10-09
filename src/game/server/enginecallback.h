#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "eiface.h"

#define random random_valve // stdlib.h defined random()..., and so does vstdlib/random.h

#include "tier3/tier3.h"
#include "tier2/tier2_logging.h"
#include "proto_types.h"
import std;

class IFileSystem;				// include FileSystem.h
class IUniformRandomStream;		// include vstdlib/random.h
class IEngineSound;				// include engine/IEngineSound.h
class IVEngineServer;			
class IVoiceServer;
class IStaticPropMgrServer;
class ISpatialPartition;
class IVModelInfo;
class IEngineTrace;
class IGameEventManager2;
class IVDebugOverlay;
class IDataCache;
class IMDLCache;
class IServerEngineTools;
class IScriptManager;
class IServerFoundry;

extern IVEngineServer			*engine;
extern IVoiceServer				*g_pVoiceServer;
extern IFileSystem				*filesystem;
extern IStaticPropMgrServer		*staticpropmgr;
extern ISpatialPartition		*partition;
extern IEngineSound				*enginesound;
extern IVModelInfo				*modelinfo;
extern IEngineTrace				*enginetrace;
extern IFileLoggingListener		*filelogginglistener;
extern IGameEventManager2		*gameeventmanager;
extern IVDebugOverlay			*debugoverlay;
extern IServerEngineTools		*serverenginetools;
extern IServerFoundry			*serverfoundry;
extern IScriptManager			*scriptmanager;



//-----------------------------------------------------------------------------
// Precaches a material
//-----------------------------------------------------------------------------
void PrecacheMaterial( const char *pMaterialName );

//-----------------------------------------------------------------------------
// Converts a previously precached material into an index
//-----------------------------------------------------------------------------
int GetMaterialIndex( const char *pMaterialName );

//-----------------------------------------------------------------------------
// Converts a previously precached material index into a string
//-----------------------------------------------------------------------------
const char *GetMaterialNameFromIndex( int nMaterialIndex );


//-----------------------------------------------------------------------------
// Precache-related methods for particle systems
//-----------------------------------------------------------------------------
int PrecacheParticleSystem( const char *pParticleSystemName );
int GetParticleSystemIndex( const char *pParticleSystemName );
const char *GetParticleSystemNameFromIndex( int nIndex );

//-----------------------------------------------------------------------------
// Precache-related methods for movies
//-----------------------------------------------------------------------------
void PrecacheMovie( const char *pMovieName );
int GetMovieIndex( const char *pMovieName );
const char *GetMovieNameFromIndex( int nMovieIndex );

//-----------------------------------------------------------------------------
// Precache-related methods for effects (used by DispatchEffect)
//-----------------------------------------------------------------------------
void PrecacheEffect( const char *pParticleSystemName );
int GetEffectIndex( const char *pParticleSystemName );
const char *GetEffectNameFromIndex( int nIndex );


class IRecipientFilter;
void EntityMessageBegin( CBaseEntity * entity, bool reliable = false );
void MessageEnd( void );

// bytewise
void MessageWriteByte( int iValue);
void MessageWriteChar( int iValue);
void MessageWriteShort( int iValue);
void MessageWriteWord( int iValue );
void MessageWriteLong( int iValue);
void MessageWriteFloat( float flValue);
void MessageWriteAngle( float flValue);
void MessageWriteCoord( float flValue);
void MessageWriteVec3Coord( const Vector& rgflValue);
void MessageWriteVec3Normal( const Vector& rgflValue);
void MessageWriteAngles( const QAngle& rgflValue);
void MessageWriteString( const char *sz );
void MessageWriteEntity( int iValue);
void MessageWriteEHandle( CBaseEntity *pEntity ); //encoded as a long


// bitwise
void MessageWriteBool( bool bValue );
void MessageWriteUBitLong( unsigned int data, int numbits );
void MessageWriteSBitLong( int data, int numbits );
void MessageWriteBits( const void *pIn, int nBits );
void MessageWriteBitVecIntegral( const Vector& vecValue );


// Bytewise
#define WRITE_BYTE		(MessageWriteByte)
#define WRITE_CHAR		(MessageWriteChar)
#define WRITE_SHORT		(MessageWriteShort)
#define WRITE_WORD		(MessageWriteWord)
inline constexpr auto WRITE_LONG = (MessageWriteLong);
inline constexpr auto WRITE_FLOAT = (MessageWriteFloat);
inline constexpr auto WRITE_ANGLE = (MessageWriteAngle);
inline constexpr auto WRITE_COORD = (MessageWriteCoord);
inline constexpr auto WRITE_VEC3COORD = (MessageWriteVec3Coord);
inline constexpr auto WRITE_VEC3NORMAL = (MessageWriteVec3Normal);
inline constexpr auto WRITE_ANGLES = (MessageWriteAngles);
inline constexpr auto WRITE_STRING = (MessageWriteString);
inline constexpr auto WRITE_ENTITY = (MessageWriteEntity);
inline constexpr auto WRITE_EHANDLE = (MessageWriteEHandle);

// Bitwise
inline constexpr auto WRITE_BOOL = (MessageWriteBool);
inline constexpr auto WRITE_UBITLONG = (MessageWriteUBitLong);
inline constexpr auto WRITE_SBITLONG = (MessageWriteSBitLong);
inline constexpr auto WRITE_BITS = (MessageWriteBits);
inline constexpr auto WRITE_VEC3_INTEGRAL = (MessageWriteBitVecIntegral);

//-----------------------------------------------------------------------------
// Send a user message
//-----------------------------------------------------------------------------
class IRecipientFilter;
// Serializes and hands the engine bytes, so the interface carries no schema.
template < class T >
void SendUserMessage( IRecipientFilter &filter, int message, const T &msg )
{
	std::string data( ks::proto::byte_size( msg ), '\0' );
	ks::proto::write( msg, ( std::byte * ) data.data() );
	engine->SendUserMessage( filter, message, data.data(), int( data.size() ) );
}
