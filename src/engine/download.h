#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

//--------------------------------------------------------------------------------------------------------------
// download.h
// 
// Header file for optional HTTP asset downloading
// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2004
//--------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
// Function Prototypes
//--------------------------------------------------------------------------------------------------------------
void	CL_HTTPStop_f(void);
bool	CL_DownloadUpdate(void);
void	CL_QueueDownload( const char *filename );
bool	CL_FileReceived( const char *filename, unsigned int requestID, bool isReplayDemoFile );
bool	CL_FileDenied( const char *filename, unsigned int requestID, bool isReplayDemoFile );
int		CL_GetDownloadQueueSize(void);
int		CL_CanUseHTTPDownload(void);
void	CL_MarkMapAsUsingHTTPDownload(void);
bool	CL_ShouldRedownloadFile( const char *filename );
