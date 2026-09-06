// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// Shared core of the byte-compare gate. The send and recv comparators live next to their own
// tables; registration, diff reporting and the fatal summary are common.

#ifndef KS_REFLECT_TABLE_VERIFY_H
#define KS_REFLECT_TABLE_VERIFY_H

namespace ks::reflect
{

void ReportDiff( const char *table, const char *prop, const char *field, const char *fmt, ... );
bool SameStr( const char *a, const char *b );
const char *SafeName( const char *s );

void RegisterVerify( void ( *fn )() );
void RunAllVerifications();
int  DiffCount();

struct VerifyRegistrar { VerifyRegistrar( void ( *fn )() ) { RegisterVerify( fn ); } };

} // namespace ks::reflect

#endif // KS_REFLECT_TABLE_VERIFY_H
