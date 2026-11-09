#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "demo.h"

#include <filesystem.h>
import ks.tier1;

import std;
//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IDemoBuffer;
struct CDemoPlaybackParameters_t;

//-----------------------------------------------------------------------------
// Demo file 
//-----------------------------------------------------------------------------
class CDemoFile
{
public:
	CDemoFile();
	~CDemoFile();

	bool	Open(const char *name, bool bReadOnly, bool bMemoryBuffer = false);
	bool	IsOpen();
	void	Close();

	// Functions specifically for in-memory demo file
	void	NotifySignonComplete();
	void	NotifyBeginFrame();
	void	NotifyEndFrame();
	void	DumpBufferToFile( char const* pFilename, const demoheader_t &header );
	void	UpdateStartTick( int& nStartTick );

	void	SeekTo( int position, bool bRead );
	unsigned int GetCurPos( bool bRead );
	int		GetSize();

	void	WriteRawData( const char *buffer, int length );
	int		ReadRawData( char *buffer, int length );

	void	WriteSequenceInfo(int nSeqNrIn, int nSeqNrOutAck);
	void	ReadSequenceInfo(int &nSeqNrIn, int &nSeqNrOutAck);

	void	WriteCmdInfo( democmdinfo_t& info );
	void	ReadCmdInfo( democmdinfo_t& info );

	void	WriteCmdHeader( unsigned char cmd, int tick, int nPlayerSlot );
	void	ReadCmdHeader( unsigned char& cmd, int& tick, int &nPlayerSlot );
	
	void	WriteConsoleCommand( const char *cmd, int tick, int nPlayerSlot );
	const char *ReadConsoleCommand( void );

	void	WriteNetworkDataTables( bf_write *buf, int tick );
	int		ReadNetworkDataTables( bf_read *buf );
	
	void	WriteStringTables( bf_write *buf, int tick );
	int		ReadStringTables( bf_read *buf );

	void	WriteUserCmd( int cmdnumber, const char *buffer, unsigned char bytes, int tick, int nPlayerSlot );
	int		ReadUserCmd( char *buffer, int &size );

	void	WriteCustomData( int iCallbackIndex, const void *pData, std::size_t iDataSize, int tick);
	int		ReadCustomData( int *pCallbackIndex, uint8 **ppDataChunk ); //returns message size, both parameters are outputs.

	void	WriteDemoHeader();
	demoheader_t * 	ReadDemoHeader( CDemoPlaybackParameters_t const *pPlaybackParameters );

	void	WriteFileBytes( FileHandle_t fh, int length );

	const char* GetUrl( void ) { return m_szFileName; }
	float GetTicksPerSecond();
	float GetTicksPerFrame();
	int	GetTotalTicks( void );
public:
	char			m_szFileName[MAX_PATH];	//name of current demo file
	demoheader_t    m_DemoHeader;  //general demo info

private:
	IDemoBuffer		*m_pBuffer;
};

inline constexpr auto DEMO_RECORD_BUFFER_SIZE = 2*1024*1024; // temp buffer big enough to fit both string tables and server classes
