// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation


import ks.core.cmdline;
import ks.core.platform;
import ks.platform;
import ks.simd;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.strtools;
#include "eiface.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "sv_uploadgamestats.h"

class CUploadGameStats : public IUploadGameStats
{
public:
	bool UploadGameStats( char const *mapname,
		unsigned int blobversion, unsigned int blobsize, const void *pvBlobData )
	{
		return false;
	}

	void InitConnection( void )
	{
	}

	void UpdateConnection( void )
	{
	}

	// If user has disabled stats tracking, do nothing
	bool IsGameStatsLoggingEnabled()
	{
		return !CommandLine()->FindParm( "-nogamestats" );
	}

	// Buffer should be 16 bytes
	void GetPseudoUniqueId( char *buf, std::size_t bufsize )
	{
		V_memset( buf, 0, bufsize );
		V_strncpy( buf, "unknown", bufsize );
	}

	bool IsCyberCafeUser( void )
	{
		return false;
	}

	// Only works in single player
	bool IsHDREnabled( void )
	{
#if defined( DEDICATED )
		return false;
#else
		return g_pMaterialSystemHardwareConfig->GetHDREnabled();
#endif
	}
};

#include "facade/iuploadgamestats.inl"


static CUploadGameStats g_UploadGameStats;
IUploadGameStats *g_pUploadGameStats = &g_UploadGameStats;
static InterfaceReg s_Reg_CUploadGameStats_IUploadGameStats( []() -> void * { return static_cast<IUploadGameStats *>( &g_UploadGameStats ); }, INTERFACEVERSION_UPLOADGAMESTATS );;

void AsyncUpload_QueueData( char const *szMapName, uint uiBlobVersion, uint uiBlobSize, const void *pvBlob )
{
}
