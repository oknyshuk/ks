#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: Tier2 logging helpers. Adds support for file I/O

import std;
import ks.core;

const int MAX_SIMULTANEOUS_LOGGING_FILE_COUNT = 16;
const int INVALID_LOGGING_FILE_HANDLE = -1;

typedef int LoggingFileHandle_t;
typedef void * FileHandle_t;

inline constexpr const char *FILELOGGINGLISTENER_INTERFACE_VERSION = "FileLoggingListener001";

class CFileLoggingListener : public ILoggingListener
{
public:
	CFileLoggingListener();
	~CFileLoggingListener();

	void Log( const LoggingContext_t *pContext, const char *pMessage );

	LoggingFileHandle_t BeginLoggingToFile( const char *pFilename, const char *pOptions, const char *pPathID = nullptr );
	void EndLoggingToFile( LoggingFileHandle_t fileHandle );

	void AssignLogChannel( LoggingChannelID_t channelID, LoggingFileHandle_t loggingFileHandle );
	void UnassignLogChannel( LoggingChannelID_t channelID );
	void AssignAllLogChannels( LoggingFileHandle_t loggingFileHandle );
	void UnassignAllLogChannels();

private:
	int GetUnusedFileInfo() const;

	struct FileInfo_t
	{
		FileHandle_t m_FileHandle;

		bool IsOpen() const { return m_FileHandle != 0; }
		void Reset() { m_FileHandle = 0; }
	};

	FileInfo_t m_OpenFiles[MAX_SIMULTANEOUS_LOGGING_FILE_COUNT];

	// Table which maps logging channel IDs to open files
	int m_FileIndices[MAX_LOGGING_CHANNEL_COUNT];
};
