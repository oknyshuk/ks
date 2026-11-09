#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: implements various common send proxies

#include "dt_recv.h"



// This converts the int stored in pData to an EHANDLE in pOut.
void RecvProxy_IntToEHandle( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_IntToMoveParent( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_IntToColor32( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_IntSubOne( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_ShortSubOne( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_InterpolationAmountChanged( const CRecvProxyData *pData, void *pStruct, void *pOut );

RecvProp RecvPropTime(
	const char *pVarName,
	int offset, 
	int sizeofVar=kSizeofIgnore );

RecvProp RecvPropEHandle(
	const char *pVarName,
	int offset, 
	int sizeofVar=kSizeofIgnore,
	RecvVarProxyFn proxyFn=RecvProxy_IntToEHandle );

RecvProp RecvPropBool(
	const char *pVarName,
	int offset, 
	int sizeofVar );

RecvProp RecvPropIntWithMinusOneFlag(
	const char *pVarName,
	int offset, 
	int sizeofVar=kSizeofIgnore,
	RecvVarProxyFn proxyFn=RecvProxy_IntSubOne );
