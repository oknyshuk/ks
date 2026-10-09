#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Rumble effects mixer for XBox

extern void RumbleEffect( int userID, unsigned char effectIndex, unsigned char rumbleData, unsigned char rumbleFlags );
extern void UpdateRumbleEffects( int userID );
extern void UpdateScreenShakeRumble( int userID, float shake, float balance = 0 );
extern void EnableRumbleOutput( int userID, bool bEnable );
extern void StopAllRumbleEffects( int userID );
