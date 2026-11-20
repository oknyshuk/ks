import ks.core.basetypes;
import ks.math.vmatrix;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMaterialVar class to CMaterialVar, its one implementation.
// Included by cmaterialvar.cpp after the definition of CMaterialVar.

ITexture * IMaterialVar::GetTextureValue()
{ return static_cast<CMaterialVar *>( this )->GetTextureValue(); }
bool IMaterialVar::IsTextureValueInternalEnvCubemap() const
{ return static_cast<const CMaterialVar *>( this )->IsTextureValueInternalEnvCubemap(); }
char const * IMaterialVar::GetName() const
{ return static_cast<const CMaterialVar *>( this )->GetName(); }
MaterialVarSym_t IMaterialVar::GetNameAsSymbol() const
{ return static_cast<const CMaterialVar *>( this )->GetNameAsSymbol(); }
void IMaterialVar::SetFloatValue( float val )
{ static_cast<CMaterialVar *>( this )->SetFloatValue( val ); }
void IMaterialVar::SetIntValue( int val )
{ static_cast<CMaterialVar *>( this )->SetIntValue( val ); }
void IMaterialVar::SetStringValue( char const *val )
{ static_cast<CMaterialVar *>( this )->SetStringValue( val ); }
char const * IMaterialVar::GetStringValue() const
{ return static_cast<const CMaterialVar *>( this )->GetStringValue(); }
void IMaterialVar::SetFourCCValue( FourCC type, void *pData )
{ static_cast<CMaterialVar *>( this )->SetFourCCValue( type, pData ); }
void IMaterialVar::GetFourCCValue( FourCC *type, void **ppData )
{ static_cast<CMaterialVar *>( this )->GetFourCCValue( type, ppData ); }
void IMaterialVar::SetVecValue( float const* val, int numcomps )
{ static_cast<CMaterialVar *>( this )->SetVecValue( val, numcomps ); }
void IMaterialVar::SetVecValue( float x, float y )
{ static_cast<CMaterialVar *>( this )->SetVecValue( x, y ); }
void IMaterialVar::SetVecValue( float x, float y, float z )
{ static_cast<CMaterialVar *>( this )->SetVecValue( x, y, z ); }
void IMaterialVar::SetVecValue( float x, float y, float z, float w )
{ static_cast<CMaterialVar *>( this )->SetVecValue( x, y, z, w ); }
void IMaterialVar::GetLinearVecValue( float *val, int numcomps ) const
{ static_cast<const CMaterialVar *>( this )->GetLinearVecValue( val, numcomps ); }
void IMaterialVar::SetTextureValue( ITexture * arg0 )
{ static_cast<CMaterialVar *>( this )->SetTextureValue( arg0 ); }
IMaterial * IMaterialVar::GetMaterialValue()
{ return static_cast<CMaterialVar *>( this )->GetMaterialValue(); }
void IMaterialVar::SetMaterialValue( IMaterial * arg0 )
{ static_cast<CMaterialVar *>( this )->SetMaterialValue( arg0 ); }
bool IMaterialVar::IsDefined() const
{ return static_cast<const CMaterialVar *>( this )->IsDefined(); }
void IMaterialVar::SetUndefined()
{ static_cast<CMaterialVar *>( this )->SetUndefined(); }
void IMaterialVar::SetMatrixValue( VMatrix const& matrix )
{ static_cast<CMaterialVar *>( this )->SetMatrixValue( matrix ); }
const VMatrix & IMaterialVar::GetMatrixValue()
{ return static_cast<CMaterialVar *>( this )->GetMatrixValue(); }
bool IMaterialVar::MatrixIsIdentity() const
{ return static_cast<const CMaterialVar *>( this )->MatrixIsIdentity(); }
void IMaterialVar::CopyFrom( IMaterialVar *pMaterialVar )
{ static_cast<CMaterialVar *>( this )->CopyFrom( pMaterialVar ); }
void IMaterialVar::SetValueAutodetectType( char const *val )
{ static_cast<CMaterialVar *>( this )->SetValueAutodetectType( val ); }
IMaterial * IMaterialVar::GetOwningMaterial()
{ return static_cast<CMaterialVar *>( this )->GetOwningMaterial(); }
void IMaterialVar::SetVecComponentValue( float fVal, int nComponent )
{ static_cast<CMaterialVar *>( this )->SetVecComponentValue( fVal, nComponent ); }
int IMaterialVar::GetIntValueInternal() const
{ return static_cast<const CMaterialVar *>( this )->GetIntValueInternal(); }
float IMaterialVar::GetFloatValueInternal() const
{ return static_cast<const CMaterialVar *>( this )->GetFloatValueInternal(); }
float const* IMaterialVar::GetVecValueInternal() const
{ return static_cast<const CMaterialVar *>( this )->GetVecValueInternal(); }
void IMaterialVar::GetVecValueInternal( float *val, int numcomps ) const
{ static_cast<const CMaterialVar *>( this )->GetVecValueInternal( val, numcomps ); }
int IMaterialVar::VectorSizeInternal() const
{ return static_cast<const CMaterialVar *>( this )->VectorSizeInternal(); }
