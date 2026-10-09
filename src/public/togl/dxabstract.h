#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2012 Valve Corporation

import ks.tier1;
#include "togl/rendermechanism.h"
#include "togl/dxabstract_types.h"

import std;


typedef std::vector< D3DMATRIX > CD3DMATRIXStack;

struct  ID3DXMatrixStack
{
	int	m_refcount[2];
	bool m_mark;
	CD3DMATRIXStack	m_stack;
	int						m_stackTop;	// top of stack is at the highest index, this is that index.  push increases, pop decreases.

	ID3DXMatrixStack();
	void  AddRef( int which=0, char *comment = nullptr );
	ULONG Release( int which=0, char *comment = nullptr );
	
	HRESULT	Create( void );
	
	D3DXMATRIX* GetTop();
	void Push();
	void Pop();
	void LoadIdentity();
	void LoadMatrix( const D3DXMATRIX *pMat );
	void MultMatrix( const D3DXMATRIX *pMat );
	void MultMatrixLocal( const D3DXMATRIX *pMat );
	HRESULT ScaleLocal(FLOAT x, FLOAT y, FLOAT z);

	// Left multiply the current matrix with the computed rotation
	// matrix, counterclockwise about the given axis with the given angle.
	// (rotation is about the local origin of the object)
	HRESULT RotateAxisLocal(const D3DXVECTOR3* pV, FLOAT Angle);

	// Left multiply the current matrix with the computed translation
	// matrix. (transformation is about the local origin of the object)
	HRESULT TranslateLocal(FLOAT x, FLOAT y, FLOAT z);
};

typedef ID3DXMatrixStack* LPD3DXMATRIXSTACK;


// ------------------------------------------------------------------------------------------------------------------------------ //
// D3DX
// ------------------------------------------------------------------------------------------------------------------------------ //
struct ID3DXInclude
{
	virtual HRESULT Open(D3DXINCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes);
	virtual HRESULT Close(LPCVOID pData);
};
typedef ID3DXInclude* LPD3DXINCLUDE;


struct  ID3DXBuffer : public IUnknown
{
	void* GetBufferPointer();
	DWORD GetBufferSize();
};

typedef ID3DXBuffer* LPD3DXBUFFER;

class ID3DXConstantTable : public IUnknown
{
};
typedef ID3DXConstantTable* LPD3DXCONSTANTTABLE;

 const char* D3DXGetPixelShaderProfile( IDirect3DDevice9 *pDevice );

 D3DXMATRIX* D3DXMatrixMultiply( D3DXMATRIX *pOut, const D3DXMATRIX *pM1, const D3DXMATRIX *pM2 );
 D3DXVECTOR3* D3DXVec3TransformCoord( D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV, const D3DXMATRIX *pM );

 HRESULT D3DXCreateMatrixStack( DWORD Flags, LPD3DXMATRIXSTACK* ppStack);
 void D3DXMatrixIdentity( D3DXMATRIX * );

 D3DXINLINE D3DXVECTOR3* D3DXVec3Subtract( D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV1, const D3DXVECTOR3 *pV2 )
{
	pOut->x = pV1->x - pV2->x;
	pOut->y = pV1->y - pV2->y;
	pOut->z = pV1->z - pV2->z;
	return pOut;
}

 D3DXINLINE D3DXVECTOR3* D3DXVec3Cross( D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV1, const D3DXVECTOR3 *pV2 )
{
	D3DXVECTOR3 v;

	v.x = pV1->y * pV2->z - pV1->z * pV2->y;
	v.y = pV1->z * pV2->x - pV1->x * pV2->z;
	v.z = pV1->x * pV2->y - pV1->y * pV2->x;

	*pOut = v;
	return pOut;
}

 D3DXINLINE FLOAT D3DXVec3Dot( const D3DXVECTOR3 *pV1, const D3DXVECTOR3 *pV2 )
{
	return pV1->x * pV2->x + pV1->y * pV2->y + pV1->z * pV2->z;
}

 D3DXMATRIX* D3DXMatrixInverse( D3DXMATRIX *pOut, FLOAT *pDeterminant, const D3DXMATRIX *pM );

 D3DXMATRIX* D3DXMatrixTranspose( D3DXMATRIX *pOut, const D3DXMATRIX *pM );

 D3DXPLANE* D3DXPlaneNormalize( D3DXPLANE *pOut, const D3DXPLANE *pP);

 D3DXVECTOR4* D3DXVec4Transform( D3DXVECTOR4 *pOut, const D3DXVECTOR4 *pV, const D3DXMATRIX *pM );


 D3DXVECTOR4* D3DXVec4Normalize( D3DXVECTOR4 *pOut, const D3DXVECTOR4 *pV );

 D3DXMATRIX* D3DXMatrixTranslation( D3DXMATRIX *pOut, FLOAT x, FLOAT y, FLOAT z );

// Build an ortho projection matrix. (right-handed)
 D3DXMATRIX* D3DXMatrixOrthoOffCenterRH( D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn,FLOAT zf );

 D3DXMATRIX* D3DXMatrixPerspectiveRH( D3DXMATRIX *pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf );

 D3DXMATRIX* D3DXMatrixPerspectiveOffCenterRH( D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf );

// Transform a plane by a matrix.  The vector (a,b,c) must be normal.
// M should be the inverse transpose of the transformation desired.
 D3DXPLANE* D3DXPlaneTransform( D3DXPLANE *pOut, const D3DXPLANE *pP, const D3DXMATRIX *pM );
