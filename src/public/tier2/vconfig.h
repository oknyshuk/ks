#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Utilities for setting vproject settings

// The registry keys that vconfig uses to store the current vproject directory.
inline constexpr const char *VPROJECT_REG_KEY = "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";

// For accessing the environment variables we store the current vproject in.
void SetVConfigRegistrySetting( const char *pName, const char *pValue, bool bNotify = true );
bool GetVConfigRegistrySetting( const char *pName, char *pReturn, int size );
bool ConvertObsoleteVConfigRegistrySetting( const char *pValueName );
