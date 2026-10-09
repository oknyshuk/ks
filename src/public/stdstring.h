#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "isaverestore.h"

import std;

class CStdStringSaveRestoreOps : public CDefSaveRestoreOps
{
public:
	enum
	{
		MAX_SAVE_LEN = 4096,
	};
};

//-------------------------------------

inline ISaveRestoreOps *GetStdStringDataOps()
{
	static CStdStringSaveRestoreOps ops;
	return &ops;
}

//-------------------------------------

#define DEFINE_STDSTRING(name) \
	{ FIELD_CUSTOM, #name, (int)__builtin_offsetof(classNameTypedef,name), 1, FTYPEDESC_SAVE, nullptr, GetStdStringDataOps(), nullptr }
