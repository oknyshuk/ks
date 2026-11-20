// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math.mathlib;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;

static bool s_bAllowSSE2 = true;

static void OnEnableFastMathChanged( ConVar *var = nullptr, const char *pOldValue = nullptr, float flOldValue = 0.0f);

ConVar enable_fast_math( "enable_fast_math", "1" , FCVAR_RELEASE, "Turns Denormals-Are-Zeroes and Flush-to-Zero on or off", OnEnableFastMathChanged );

static void OnEnableFastMathChanged( ConVar *var, const char *pOldValue, float flOldValue )
{
	ks::simd::set_flush_to_zero( enable_fast_math.GetBool() );
}

void InitMathlib( void )
{
	MathLib_Init( 2.2f, // v_gamma.GetFloat()
		2.2f, // v_texgamma.GetFloat()
		0.0f /*v_brightness.GetFloat() */, 
		2.0f /*mat_overbright.GetInt() */, true, s_bAllowSSE2, true );
	OnEnableFastMathChanged();
}
