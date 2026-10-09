// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation


import ks.core;
#include "eiface.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "sv_uploadgamestats.h"

class CUploadGameStats : public IUploadGameStats
{
public:
	virtual bool UploadGameStats( char const *mapname,
		unsigned int blobversion, unsigned int blobsize, const void *pvBlobData )
	{
		return false;
	}

	virtual void InitConnection( void )
	{
	}

	virtual void UpdateConnection( void )
	{
	}

	// If user has disabled stats tracking, do nothing
	virtual bool IsGameStatsLoggingEnabled()
	{
		return !CommandLine()->FindParm( "-nogamestats" );
	}

	// Buffer should be 16 bytes
	virtual void GetPseudoUniqueId( char *buf, std::size_t bufsize )
	{
		V_memset( buf, 0, bufsize );
		V_strncpy( buf, "unknown", bufsize );
	}

	virtual bool IsCyberCafeUser( void )
	{
		return false;
	}

	// Only works in single player
	virtual bool IsHDREnabled( void )
	{
#if defined( DEDICATED )
		return false;
#else
		return g_pMaterialSystemHardwareConfig->GetHDREnabled();
#endif
	}
};

static CUploadGameStats g_UploadGameStats;
IUploadGameStats *g_pUploadGameStats = &g_UploadGameStats;
static InterfaceReg s_Reg_CUploadGameStats_IUploadGameStats( []() -> void * { return static_cast<IUploadGameStats *>( &g_UploadGameStats ); }, INTERFACEVERSION_UPLOADGAMESTATS );;

void AsyncUpload_QueueData( char const *szMapName, uint uiBlobVersion, uint uiBlobSize, const void *pvBlob )
{
}
