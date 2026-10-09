#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Packet IDs.
// Be sure to register new ones with VMPI_REGISTER_SUBPACKET_ID.
	inline constexpr auto VMPI_SUBPACKETID_DIRECTORIES = 0; // qdir directories.
	inline constexpr auto VMPI_SUBPACKETID_DBINFO = 1; // MySQL database info.
	inline constexpr auto VMPI_SUBPACKETID_CRASH = 3; // A worker saying it crashed.
	inline constexpr auto VMPI_SUBPACKETID_MULTICAST_ADDR = 4; // Filesystem multicast address.


class CDBInfo;
class CIPAddr;


// Send/receive the qdir info.
void SendQDirInfo();
void RecvQDirInfo();

void SendDBInfo( const CDBInfo *pInfo, unsigned long jobPrimaryID );
void RecvDBInfo( CDBInfo *pInfo, unsigned long *pJobPrimaryID );

void SendMulticastIP( const CIPAddr *pAddr );
void RecvMulticastIP( CIPAddr *pAddr );

void VMPI_HandleCrash( const char *pMessage, uint uCode, void *pvExceptionInfo, bool bAssert );

// Call this from an exception handler (set by SetUnhandledExceptionHandler).
// uCode			= ExceptionInfo->ExceptionRecord->ExceptionCode.
// pvExceptionInfo	= ExceptionInfo
void VMPI_ExceptionFilter( unsigned long uCode, void *pvExceptionInfo );

void HandleMPIDisconnect( int procID, const char *pReason );
