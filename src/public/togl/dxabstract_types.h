#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2012 Valve Corporation

#include "togl/d3d9_prelude.h"
import ks.core.platform;

#define TOGL_INTERFACE
#define TOGL_OVERLOAD
#define TOGL_GLOBAL
#define TOGLMETHODCALLTYPE
[[gnu::always_inline]] inline auto DXABSTRACT_BREAK_ON_ERROR() { return DebuggerBreak(); }
#define D3DXINLINE inline
#define D3D_OK									S_OK
#define D3DXSHADER_DEBUG                    (1 << 0)
#define D3DXSHADER_AVOID_FLOW_CONTROL       (1 << 9)


class  D3DXMATRIX : public D3DMATRIX
{
public:
	D3DXMATRIX operator*( const D3DXMATRIX &o ) const;
	operator FLOAT* ();
	float& operator()( int row, int column );
	const float& operator()( int row, int column ) const;
	bool operator != ( const D3DXMATRIX& src ) const;
};

// ------------------------------------------------------------------------------------------------------------------------------ //
// STRUCTURES
// ------------------------------------------------------------------------------------------------------------------------------ //

typedef struct  D3DXPLANE
{
	float& operator[]( int i );
	bool operator==( const D3DXPLANE &o );
	bool operator!=( const D3DXPLANE &o );
	operator float*();
	operator const float*() const;

	float a, b, c, d;
} D3DXPLANE;

class  D3DXVECTOR2
{
public:
    operator FLOAT* ();
    operator const FLOAT* () const;

	float x,y;
};

class  D3DXVECTOR3 : public D3DVECTOR
{
public:
	D3DXVECTOR3() {}
	D3DXVECTOR3( float a, float b, float c );
    operator FLOAT* ();
    operator const FLOAT* () const;
};

typedef enum _D3DXINCLUDE_TYPE
{
    D3DXINC_LOCAL,

    // force 32-bit size enum
    D3DXINC_FORCE_DWORD = 0x7fffffff

} D3DXINCLUDE_TYPE;

class  D3DXVECTOR4
{
public:
	D3DXVECTOR4() {}
	D3DXVECTOR4( float a, float b, float c, float d );

	float x,y,z,w;
};

//----------------------------------------------------------------------------
// D3DXMACRO:
// ----------
// Preprocessor macro definition.  The application pass in a NULL-terminated
// array of this structure to various D3DX APIs.  This enables the application
// to #define tokens at runtime, before the file is parsed.
//----------------------------------------------------------------------------

typedef struct _D3DXMACRO
{
    LPCSTR Name;
    LPCSTR Definition;

} D3DXMACRO, *LPD3DXMACRO;
