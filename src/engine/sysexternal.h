#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// an error will cause the entire program to exit
void Sys_Error(const char *psz, ...) [[gnu::format(printf, 1, 2)]];
// an error will cause the entire program to exit but WITHOUT uploading a report to the crash backend, so for fatal but user controlled error
void Sys_Exit( const char *error, ... ) [[gnu::format(printf, 1, 2)]];
