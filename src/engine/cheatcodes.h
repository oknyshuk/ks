#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "inputsystem/ButtonCode.h"

void	ClearCheatCommands( void );
void	ReadCheatCommandsFromFile( const char *pchFileName );
void	LogKeyPress( ButtonCode_t code );
void	CheckCheatCodes();
