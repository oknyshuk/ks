#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: implements various common send proxies

#include "dt_send.h"


void SendProxy_Color32ToInt( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );
void SendProxy_EHandleToInt( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID );
void SendProxy_IntAddOne( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID );
void SendProxy_ShortAddOne( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID );
// Defined in player.cpp and used by several tables, which previously forward-declared it in
// each .cpp. A Proxy<> annotation needs it visible where the class is declared, so it belongs
// here with the other shared proxies.
void SendProxy_CropFlagsToPlayerFlagBitsLength( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID );

SendProp SendPropBool(
	const char *pVarName,
	int offset,
	int sizeofVar );

SendProp SendPropEHandle(
	const char *pVarName,
	int offset,
	int flags = 0,
	int sizeofVar=kSizeofIgnore,
	SendVarProxyFn proxyFn=SendProxy_EHandleToInt );

SendProp SendPropTime(
	const char *pVarName,
	int offset,
	int sizeofVar=kSizeofIgnore );

SendProp SendPropIntWithMinusOneFlag(
	const char *pVarName,
	int offset,
	int bits,
	int sizeofVar=kSizeofIgnore,
	SendVarProxyFn proxyFn=SendProxy_IntAddOne );


// Send a string_t as a string property.
SendProp SendPropStringT( const char *pVarName, int offset, int sizeofVar );

//-----------------------------------------------------------------------------
// Purpose: Proxy that only sends data to team members
//-----------------------------------------------------------------------------
void* SendProxy_OnlyToTeam( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID );
