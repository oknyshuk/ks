#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: This is temporary obfuscation code to verify that a base shader
// DLL comes from us (because it includes some interfaces that we don't
// give out with the SDK).

import std;
import ks.tier1;


inline constexpr auto SHADER_DLL_VERIFY_DATA_LEN1 = 4101;
inline constexpr auto SHADER_DLL_VERIFY_DATA_PTR_OFFSET = 43;

inline constexpr const char *SHADER_DLL_FNNAME_1 = "_ftol3";
typedef void (*ShaderDLLVerifyFn)( char *pData );

class IShaderDLLVerification
{
public:

	virtual CRC32_t Function1( unsigned char *pData ) = 0;
	virtual void Function2( int a, int b, int c ) = 0;
	virtual void Function3( int a, int b, int c ) = 0;
	virtual void Function4( int a, int b, int c ) = 0;
	virtual CRC32_t Function5() = 0;
};
