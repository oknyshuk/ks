#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core.basetypes;
import ks.core.types;
import ks.math.mathlib;
import ks.math.vector4d;
import ks.math.vmatrix;
import ks.platform;
import ks.simd;
class IMaterial;
class ITexture;

template <typename T1, typename T2, typename T3, typename T4> [[gnu::always_inline]] inline constexpr auto MAKE_MATERIALVAR_FOURCC( T1 ch0, T2 ch1, T3 ch2, T4 ch3 ) { return ((unsigned long)(ch0) | ((unsigned long)(ch1) << 8) |    		((unsigned long)(ch2) << 16) | ((unsigned long)(ch3) << 24 )); }

// This fourcc is reserved.
#define FOURCC_UNKNOWN	MAKE_MATERIALVAR_FOURCC('U','N','K','N')


//-----------------------------------------------------------------------------
// Various material var types
//-----------------------------------------------------------------------------
enum MaterialVarType_t 
{ 
	MATERIAL_VAR_TYPE_FLOAT = 0,
	MATERIAL_VAR_TYPE_STRING,
	MATERIAL_VAR_TYPE_VECTOR,
	MATERIAL_VAR_TYPE_TEXTURE,
	MATERIAL_VAR_TYPE_INT,
	MATERIAL_VAR_TYPE_FOURCC,
	MATERIAL_VAR_TYPE_UNDEFINED,
	MATERIAL_VAR_TYPE_MATRIX,
	MATERIAL_VAR_TYPE_MATERIAL,
};

typedef unsigned short MaterialVarSym_t;
inline constexpr MaterialVarSym_t INVALID_MATERIAL_VAR_SYMBOL = (MaterialVarSym_t)~0;

class IMaterialVar
{
public:
	typedef unsigned long FourCC;
	
protected:
	// base data and accessors
	char* m_pStringVal;
	int m_intVal;
	Vector4D m_VecVal;

	// member data. total = 4 bytes
	uint8 m_Type : 4;
	uint8 m_nNumVectorComps : 3;
	uint8 m_bFakeMaterialVar : 1;
	uint8 m_nTempIndex;
	MaterialVarSym_t m_Name = INVALID_MATERIAL_VAR_SYMBOL;

public:
	// class factory methods
	static IMaterialVar* Create( IMaterial* pMaterial, char const* pKey, VMatrix const& matrix );
	static IMaterialVar* Create( IMaterial* pMaterial, char const* pKey, char const* pVal );
	static IMaterialVar* Create( IMaterial* pMaterial, char const* pKey, float* pVal, int numcomps );
	static IMaterialVar* Create( IMaterial* pMaterial, char const* pKey, float val );
	static IMaterialVar* Create( IMaterial* pMaterial, char const* pKey, int val );
	static IMaterialVar* Create( IMaterial* pMaterial, char const* pKey );
	static void Destroy( IMaterialVar* pVar );
	static MaterialVarSym_t	GetSymbol( char const* pName );
	static MaterialVarSym_t	FindSymbol( char const* pName );
	static bool SymbolMatches( char const* pName, MaterialVarSym_t symbol );
	static void DeleteUnreferencedTextures( bool enable );

	ITexture		*GetTextureValue( void );
	bool			IsTextureValueInternalEnvCubemap( void ) const;

	char const *	GetName( void ) const;
	MaterialVarSym_t	GetNameAsSymbol() const;

	void			SetFloatValue( float val );
	
	void			SetIntValue( int val );
	
	void			SetStringValue( char const *val );
	char const *	GetStringValue( void ) const;

	// Use FourCC values to pass app-defined data structures between
	// the proxy and the shader. The shader should ignore the data if
	// its FourCC type not correct.	
	void			SetFourCCValue( FourCC type, void *pData );
	void			GetFourCCValue( FourCC *type, void **ppData );

	// Vec (dim 2-4)
	void			SetVecValue( float const* val, int numcomps );
	void			SetVecValue( float x, float y );
	void			SetVecValue( float x, float y, float z );
	void			SetVecValue( float x, float y, float z, float w );
	void			GetLinearVecValue( float *val, int numcomps ) const;

	// revisit: is this a good interface for textures?
	void			SetTextureValue( ITexture * );

	IMaterial *		GetMaterialValue( void );
	void			SetMaterialValue( IMaterial * );

	bool			IsDefined() const;
	void			SetUndefined();

	// Matrix
	void			SetMatrixValue( VMatrix const& matrix );
	const VMatrix  &GetMatrixValue( );
	bool			MatrixIsIdentity() const;

	// Copy....
	void			CopyFrom( IMaterialVar *pMaterialVar );

	void			SetValueAutodetectType( char const *val );

	IMaterial *		GetOwningMaterial();

	//set just 1 component
	void			SetVecComponentValue( float fVal, int nComponent );

protected:
	int				GetIntValueInternal( void ) const;
	float			GetFloatValueInternal( void ) const;
	float const*	GetVecValueInternal( ) const;
	void			GetVecValueInternal( float *val, int numcomps ) const;
	int				VectorSizeInternal() const;

public:
	[[gnu::always_inline]] inline MaterialVarType_t GetType( void ) const
	{
		return ( MaterialVarType_t )m_Type;
	}

	[[gnu::always_inline]] inline bool IsTexture() const
	{
		return m_Type == MATERIAL_VAR_TYPE_TEXTURE;
	}

	[[gnu::always_inline]] inline operator ITexture*()
	{
		return GetTextureValue();
	}

	// NOTE: Fast methods should only be called in thread-safe situations
	[[gnu::always_inline]] inline int GetIntValueFast( void ) const
	{
		// Set methods for float and vector update this
		return m_intVal;
	}

	[[gnu::always_inline]] inline float GetFloatValueFast( void ) const
	{
		return m_VecVal[0];
	}

	[[gnu::always_inline]] inline float const* GetVecValueFast( ) const
	{
		return m_VecVal.Base();
	}

	[[gnu::always_inline]] inline void GetVecValueFast( float *val, int numcomps ) const 
	{
		contract_assert( ( numcomps >0 ) && ( numcomps <= 4 ) );
		for( int i=0 ; i < numcomps; i++ )
		{
			val[i] = m_VecVal[ i ];
		}
	}

	[[gnu::always_inline]] inline int VectorSizeFast() const
	{
		return m_nNumVectorComps;
	}

#ifdef FAST_MATERIALVAR_ACCESS
	[[gnu::always_inline]] inline int GetIntValue( void ) const
	{
		return GetIntValueFast();
	}

	[[gnu::always_inline]] inline float GetFloatValue( void ) const
	{
		return GetFloatValueFast();
	}

	[[gnu::always_inline]] inline float const* GetVecValue( ) const
	{
		return GetVecValueFast();
	}

	[[gnu::always_inline]] inline void GetVecValue( float *val, int numcomps ) const 
	{
		GetVecValueFast( val, numcomps );
	}

	[[gnu::always_inline]] inline int VectorSize() const
	{
		return VectorSizeFast();
	}
#else // !FAST_MATERIALVAR_ACCESS
	[[gnu::always_inline]] inline int GetIntValue( void ) const
	{
		return GetIntValueInternal();
	}

	[[gnu::always_inline]] inline float GetFloatValue( void ) const
	{
		return GetFloatValueInternal();
	}

	[[gnu::always_inline]] inline float const* GetVecValue( ) const
	{
		return GetVecValueInternal();
	}

	[[gnu::always_inline]] inline void GetVecValue( float *val, int numcomps ) const 
	{
		return GetVecValueInternal( val, numcomps );
	}

	[[gnu::always_inline]] inline int VectorSize() const
	{
		return VectorSizeInternal();
	}
#endif

private:
	[[gnu::always_inline]] inline void SetTempIndex( int nIndex )
	{
		m_nTempIndex = nIndex;
	}

	friend void EnableThreadedMaterialVarAccess( bool bEnable, IMaterialVar **ppParams, int nVarCount );
};
