import ks.core.basetypes;
import ks.tier1.interfaces;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IColorCorrectionSystem class to its one implementation, CColorCorrectionSystem.
// Included by CColorCorrection.cpp, which defines ColorCorrectionSystemImpl().

void IColorCorrectionSystem::Init()
{ ColorCorrectionSystemImpl().Init(); }
void IColorCorrectionSystem::Shutdown()
{ ColorCorrectionSystemImpl().Shutdown(); }
ColorCorrectionHandle_t IColorCorrectionSystem::AddLookup( const char *pName )
{ return ColorCorrectionSystemImpl().AddLookup( pName ); }
bool IColorCorrectionSystem::RemoveLookup( ColorCorrectionHandle_t handle )
{ return ColorCorrectionSystemImpl().RemoveLookup( handle ); }
void IColorCorrectionSystem::SetLookupWeight( ColorCorrectionHandle_t handle, float flWeight )
{ ColorCorrectionSystemImpl().SetLookupWeight( handle, flWeight ); }
float IColorCorrectionSystem::GetLookupWeight( ColorCorrectionHandle_t handle )
{ return ColorCorrectionSystemImpl().GetLookupWeight( handle ); }
float IColorCorrectionSystem::GetLookupWeight( int i )
{ return ColorCorrectionSystemImpl().GetLookupWeight( i ); }
void IColorCorrectionSystem::LockLookup()
{ ColorCorrectionSystemImpl().LockLookup(); }
void IColorCorrectionSystem::LockLookup( ColorCorrectionHandle_t handle )
{ ColorCorrectionSystemImpl().LockLookup( handle ); }
void IColorCorrectionSystem::UnlockLookup()
{ ColorCorrectionSystemImpl().UnlockLookup(); }
void IColorCorrectionSystem::UnlockLookup( ColorCorrectionHandle_t handle, bool bDownload )
{ ColorCorrectionSystemImpl().UnlockLookup( handle, bDownload ); }
void IColorCorrectionSystem::SetLookup( RGBX5551_t inColor, color24 outColor )
{ ColorCorrectionSystemImpl().SetLookup( inColor, outColor ); }
void IColorCorrectionSystem::SetLookup( ColorCorrectionHandle_t handle, RGBX5551_t inColor, color24 outColor )
{ ColorCorrectionSystemImpl().SetLookup( handle, inColor, outColor ); }
color24 IColorCorrectionSystem::GetLookup( RGBX5551_t inColor )
{ return ColorCorrectionSystemImpl().GetLookup( inColor ); }
color24 IColorCorrectionSystem::GetLookup( ColorCorrectionHandle_t handle, RGBX5551_t inColor )
{ return ColorCorrectionSystemImpl().GetLookup( handle, inColor ); }
void IColorCorrectionSystem::LoadLookup( const char *pLookupName )
{ ColorCorrectionSystemImpl().LoadLookup( pLookupName ); }
void IColorCorrectionSystem::LoadLookup( ColorCorrectionHandle_t handle, const char *pLookupName )
{ ColorCorrectionSystemImpl().LoadLookup( handle, pLookupName ); }
void IColorCorrectionSystem::CopyLookup( const color24 *pSrcColorCorrection )
{ ColorCorrectionSystemImpl().CopyLookup( pSrcColorCorrection ); }
void IColorCorrectionSystem::CopyLookup( ColorCorrectionHandle_t handle, const color24 *pSrcColorCorrection )
{ ColorCorrectionSystemImpl().CopyLookup( handle, pSrcColorCorrection ); }
void IColorCorrectionSystem::ResetLookup( ColorCorrectionHandle_t handle )
{ ColorCorrectionSystemImpl().ResetLookup( handle ); }
void IColorCorrectionSystem::ResetLookup()
{ ColorCorrectionSystemImpl().ResetLookup(); }
void IColorCorrectionSystem::ReleaseTextures()
{ ColorCorrectionSystemImpl().ReleaseTextures(); }
void IColorCorrectionSystem::RestoreTextures()
{ ColorCorrectionSystemImpl().RestoreTextures(); }
void IColorCorrectionSystem::ResetLookupWeights()
{ ColorCorrectionSystemImpl().ResetLookupWeights(); }
int IColorCorrectionSystem::GetNumLookups()
{ return ColorCorrectionSystemImpl().GetNumLookups(); }
color24 IColorCorrectionSystem::ConvertToColor24( RGBX5551_t inColor )
{ return ColorCorrectionSystemImpl().ConvertToColor24( inColor ); }
void IColorCorrectionSystem::SetResetable( ColorCorrectionHandle_t handle, bool bResetable )
{ ColorCorrectionSystemImpl().SetResetable( handle, bResetable ); }
void IColorCorrectionSystem::EnableColorCorrection( bool bEnable )
{ ColorCorrectionSystemImpl().EnableColorCorrection( bEnable ); }
void IColorCorrectionSystem::GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo )
{ ColorCorrectionSystemImpl().GetCurrentColorCorrection( pInfo ); }
void IColorCorrectionSystem::OnProceduralRegenComplete( ColorCorrectionHandle_t handle )
{ ColorCorrectionSystemImpl().OnProceduralRegenComplete( handle ); }
ColorCorrectionHandle_t IColorCorrectionSystem::FindLookup( const char *pName )
{ return ColorCorrectionSystemImpl().FindLookup( pName ); }
