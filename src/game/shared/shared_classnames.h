#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Hacky macros to allow shared code to work without even worse macro-izing
#if defined( CLIENT_DLL )

#define CBaseEntity				C_BaseEntity
#define CBaseCombatCharacter	C_BaseCombatCharacter
#define CBaseAnimating			C_BaseAnimating
#define CBasePlayer				C_BasePlayer

#endif
