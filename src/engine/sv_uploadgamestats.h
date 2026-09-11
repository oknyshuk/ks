//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SV_UPLOADGAMESTATS_H
#define SV_UPLOADGAMESTATS_H

// bool UploadGameStats( char const *cserIP, unsigned int buildnumber, char const *exe, char const *gamedir, char const *mapname,
// 	unsigned int blobversion, unsigned int blobsize, const void *pvBlobData );

class IUploadGameStats;
extern IUploadGameStats *g_pUploadGameStats;

void AsyncUpload_QueueData( char const *szMapName, uint uiBlobVersion, uint uiBlobSize, const void *pvBlob );

#endif // SV_UPLOADGAMESTATS_H
