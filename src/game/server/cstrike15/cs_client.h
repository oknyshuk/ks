#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

class CBaseEntity;
class CCSPlayer;

void respawn( CBaseEntity *pEdict, bool fCopyCorpse );

void FinishClientPutInServer( CCSPlayer *pPlayer );
