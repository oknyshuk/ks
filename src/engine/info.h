#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "filesystem.h"

inline constexpr auto MAX_INFO_STRING = 256;

void			Info_SetValueForStarKey ( char *s, const char *key, const char *value, int maxsize);
// userinfo functions
const char		*Info_ValueForKey ( const char *s, const char *key );
void			Info_RemoveKey ( char *s, const char *key );
void			Info_RemovePrefixedKeys ( char *start, char prefix );
void			Info_SetValueForKey ( char *s, const char *key, const char *value, int maxsize );
void			Info_Print ( const char *s );

void			Info_WriteVars( char *s, FileHandle_t fp );
