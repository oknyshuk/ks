#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
#include "inputsystem/ButtonCode.h"

class CKeyBindings
{
public:
	void SetBinding( ButtonCode_t code, const char *pBinding );
	void SetBinding( const char *pButtonName, const char *pBinding );

	void Unbind( ButtonCode_t code );
	void Unbind( const char *pButtonName );
	void UnbindAll();

	int GetBindingCount() const;
	void WriteBindings( ks::text_writer &buf );
	const char *ButtonNameForBinding( const char *pBinding );
	const char *GetBindingForButton( ButtonCode_t code );

private:
	std::string m_KeyInfo[ BUTTON_CODE_LAST ];
};
