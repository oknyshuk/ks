#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.platform;
import ks.simd;
import ks.tier1.bitbuf;
#include "dt_send.h"

class CBaseClientState;
class ServerClass;

// For shortcutting when server and client have the same game .dll
//  data
void DataTable_CreateClientTablesFromServerTables();
void DataTable_CreateClientClassInfosFromServerClasses( CBaseClientState *pState );

void DataTable_ClearWriteFlags( ServerClass *pClasses );
bool DataTable_LoadDataTablesFromBuffer( bf_read *pBuf, int nDemoProtocol );

void DataTable_WriteSendTablesBuffer( ServerClass *pClasses, bf_write *pBuf );
void DataTable_WriteClassInfosBuffer(ServerClass *pClasses, bf_write *pBuf );

bool DataTable_SetupReceiveTableFromSendTable( SendTable *sendTable, bool bNeedsDecoder );
