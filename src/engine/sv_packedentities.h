#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "server.h"
#include "framesnapshot.h"
#include "server_class.h"
import ks.tier1.bitbuf;
import ks.tier1.checksum_crc;

void SV_ComputeClientPacks( 
	int clientCount, 
	CGameClient** clients,
	CFrameSnapshot *snapshot );

void SV_WriteSendTables( ServerClass *pClasses, bf_write &pBuf );
void SV_WriteClassInfos( ServerClass *pClasses, bf_write &pBuf );

void SV_ComputeClassInfosCRC( CRC32_t* crc );
void SV_EnsureInstanceBaseline( ServerClass *pServerClass, int iEdict, SerializedEntityHandle_t handle );

void SV_EnableChangeFrames( bool state );
