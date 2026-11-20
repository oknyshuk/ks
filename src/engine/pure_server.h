#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.platform;
import ks.simd;
import ks.tier1.byte_reader;
import ks.tier1.byte_writer;
import ks.tier1.checksum_crc;
import ks.tier1.convar;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.map_ops;
import ks.tier1.strtools;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
#include "ifilelist.h"
import std;
#include "filesystem.h"
#include "userid.h"



class CPureServerWhitelist
{
public:
	friend class CPureFileTracker;

	// Call this to free it.
	static CPureServerWhitelist* Create( IFileSystem *pFileSystem );
	void				Release();
	
	// Get the file lists.
	IFileList*			GetAllowFromDiskList();		// This is the list of files that can be loaded off disk. Make sure to Release() it when finished!
	IFileList*			GetForceMatchList();		// This is the list of files that must match CRCs. Make sure to Release() it when finished!
	
	// Load up any entries in the KV file.
	bool				LoadFromKeyValues( KeyValues *kv );

	// Encode for networking.
	void				Encode( ks::byte_writer &buf );
	void				Decode( ks::byte_reader &buf );

	// Calls IFileSystem::CacheFileCRCs for all files and directories marked check_crc.
	void				CacheFileCRCs();

	// Instead of loading keyvalues, you can call this to make it force no files to come from disk.
	// This is used with sv_pure 2.
	void				EnableFullyPureMode();
	bool				IsInFullyPureMode() const;

	// Used by sv_pure command when you're connected to a server.
	void				PrintWhitelistContents();

private:

	CPureServerWhitelist();
	~CPureServerWhitelist();

	void				Init( IFileSystem *pFileSystem );
	void				Term();

private:

	friend class IFileList;
	bool				IsAllowedFromDisk( const char *pFilename );
	bool				IsInForceMatchList( const char *pFilename );

	class CCommand
	{
	public:
		CCommand();
		~CCommand();

	public:		
		bool			m_bAllowFromDisk;
		bool			m_bCheckCRC;
		unsigned short	m_LoadOrder;	// What order this thing was specified in the whitelist file.
	};
	typedef ks::idict< std::unique_ptr< CCommand > > CommandDict_t;


private:

	void				UpdateCommandStats( CommandDict_t &commands, int *pHighest, int *pLongestPathName );
	void				PrintCommand( const char *pFileSpec, const char *pExt, int maxPathnameLen, CPureServerWhitelist::CCommand *pCommand );
	CommandDict_t::iterator	FindCommandByLoadOrder( CommandDict_t &commands, int iLoadOrder );

	void InternalCacheFileCRCs( CommandDict_t &theList, ECacheCRCType eType );

	CCommand* GetBestEntry( const char *pFilename );

	void EncodeCommandList( CommandDict_t &theList, ks::byte_writer &buf );
	void DecodeCommandList( CommandDict_t &theList, ks::byte_reader &buf );

	CPureServerWhitelist::CCommand* CheckEntry( 
		CommandDict_t &dict, 
		const char *pEntryName, 
		CPureServerWhitelist::CCommand *pBestEntry );

private:
	unsigned short	m_LoadCounter;	// Incremented as we load things so their m_LoadOrder increases.
	int m_RefCount;

	// Commands are applied to files in order.
	CommandDict_t	m_FileCommands;				// file commands
	CommandDict_t	m_RecursiveDirCommands;		// ... commands
	CommandDict_t	m_NonRecursiveDirCommands;	// *.* commands
	IFileSystem *m_pFileSystem;
	
	IFileList m_AllowFromDiskList;
	IFileList m_ForceMatchList;
	
	// In this mode, no files are allowed from disk.
	bool m_bFullyPureMode;
};


CPureServerWhitelist* CreatePureServerWhitelist( IFileSystem *pFileSystem );

struct UserReportedFileHash_t
{
	int m_idxFile;
	USERID_t m_userID;
	mutable FileHash_t m_FileHash;	// not part of the ordering, updated in place

	struct Less
	{
		bool operator()( const UserReportedFileHash_t& lhs, const UserReportedFileHash_t& rhs ) const
		{
			if ( lhs.m_idxFile < rhs.m_idxFile )
				return true;
			if ( lhs.m_idxFile > rhs.m_idxFile )
				return false;
			if ( lhs.m_userID.accountid < rhs.m_userID.accountid )
				return true;
			return false;
		}
	};
};

struct UserReportedFile_t
{
	CRC32_t m_crcIdentifier;
	std::string m_filename;
	std::string m_path;
	int m_nFileFraction;

	struct Less
	{
		bool operator()( const UserReportedFile_t& lhs, const UserReportedFile_t& rhs ) const
		{
			if ( lhs.m_crcIdentifier < rhs.m_crcIdentifier )
				return true;
			if ( lhs.m_crcIdentifier > rhs.m_crcIdentifier )
				return false;
			if ( lhs.m_nFileFraction < rhs.m_nFileFraction )
				return true;
			if ( lhs.m_nFileFraction > rhs.m_nFileFraction )
				return false;
			int nCmp = V_strcmp( lhs.m_filename.c_str(), rhs.m_filename.c_str() );
			if ( nCmp < 0 )
				return true;
			if ( nCmp > 0 )
				return false;
			nCmp = V_strcmp( lhs.m_path.c_str(), rhs.m_path.c_str() );
			if ( nCmp < 0 )
				return true;
			return false;
		}
	};
};

struct MasterFileHash_t
{
	int m_idxFile;
	int m_cMatches;
	FileHash_t m_FileHash;

	struct Less
	{
		bool operator()( const MasterFileHash_t& lhs, const MasterFileHash_t& rhs ) const
		{
			return lhs.m_idxFile < rhs.m_idxFile;
		}
	};

};

class CPureFileTracker
{
public:
	CPureFileTracker()
	  {
		  m_flLastFileReceivedTime = 0.f;
		  m_cMatchedFile = 0;
		  m_cMatchedMasterFile = 0;
		  m_cMatchedMasterFileHash = 0;
		  m_cMatchedFileFullHash = 0;
	  }

	  void AddUserReportedFileHash( int idxFile, FileHash_t *pFileHash, USERID_t userID, bool bAddMasterRecord );
	  bool DoesFileMatch( const char *pPathID, const char *pRelativeFilename, int nFileFraction, FileHash_t *pFileHash, USERID_t userID );
	  int ListUserFiles( bool bListAll, const char *pchFilenameFind );
	  int ListAllTrackedFiles( bool bListAll, const char *pchFilenameFind, int nFileFractionMin, int nFileFractionMax );

	  // file -> its index; the other trees refer to files by index, in order of first report
	  std::map< UserReportedFile_t, int, UserReportedFile_t::Less > m_treeAllReportedFiles;
	  std::vector< const UserReportedFile_t * > m_vecReportedFilesByIndex;
	  std::set< MasterFileHash_t, MasterFileHash_t::Less > m_treeMasterFileHashes;
	  std::set< UserReportedFileHash_t, UserReportedFileHash_t::Less > m_treeUserReportedFileHash;

	  float m_flLastFileReceivedTime;
	  int m_cMatchedFile;
	  int m_cMatchedMasterFile;
	  int m_cMatchedMasterFileHash;
	  int m_cMatchedFileFullHash;

};

extern CPureFileTracker g_PureFileTracker;
