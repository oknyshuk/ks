#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

class CEntityReadInfo;
namespace ks::net { struct CSVCMsg_PacketEntities; }

void CL_DeleteDLLEntity( int iEnt, const char *reason, bool bOnRecreatingAllEntities = false );
void CL_CopyExistingEntity( CEntityReadInfo &u );
void CL_PreserveExistingEntity( int nOldEntity );
void CL_CopyNewEntity( CEntityReadInfo &u, int iClass, int iSerialNum );
void CL_PreprocessEntities( void );
bool CL_ProcessPacketEntities ( const ks::net::CSVCMsg_PacketEntities &msg );
