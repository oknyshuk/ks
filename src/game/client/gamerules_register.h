// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#pragma once

#define REGISTER_GAMERULES_CLASS( className ) \
	void __CreateGameRules_##className() { new className; } \
	static CGameRulesRegister __g_GameRulesRegister_##className( #className, __CreateGameRules_##className );
