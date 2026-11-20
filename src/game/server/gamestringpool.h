// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#pragma once
import ks.core.vprof;

#define AssertIsValidString( s )	contract_assert( s == NULL_STRING || s == FindPooledString( STRING(s) ) );
