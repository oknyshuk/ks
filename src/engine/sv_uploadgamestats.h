#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// bool UploadGameStats( char const *cserIP, unsigned int buildnumber, char const *exe, char const *gamedir, char const *mapname,
// 	unsigned int blobversion, unsigned int blobsize, const void *pvBlobData );

import ks.core.platform;
import ks.platform;
import ks.simd;

class IUploadGameStats;
extern IUploadGameStats *g_pUploadGameStats;

void AsyncUpload_QueueData( char const *szMapName, uint uiBlobVersion, uint uiBlobSize, const void *pvBlob );
