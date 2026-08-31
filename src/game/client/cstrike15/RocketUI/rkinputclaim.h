// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#ifndef KISAKSTRIKE_RKINPUTCLAIM_H
#define KISAKSTRIKE_RKINPUTCLAIM_H

// Hook the client's "does the UI own the mouse" predicate up to RocketUI, or
// unhook it when the client DLL goes away (RocketUI outlives us).
void RocketUI_InstallInputClaimQuery( bool bInstall );

#endif // KISAKSTRIKE_RKINPUTCLAIM_H
