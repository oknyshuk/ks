// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMaterialSystemInternal class to the material system implementation.
// Included by cmaterialsystem.cpp, which defines MaterialSystemImpl().

IMaterial* IMaterialSystemInternal::GetCurrentMaterial()
{ return MaterialSystemImpl().GetCurrentMaterial(); }
int IMaterialSystemInternal::GetLightmapPage()
{ return MaterialSystemImpl().GetLightmapPage(); }
int IMaterialSystemInternal::GetLightmapWidth( int lightmap ) const
{ return MaterialSystemImpl().GetLightmapWidth( lightmap ); }
int IMaterialSystemInternal::GetLightmapHeight( int lightmap ) const
{ return MaterialSystemImpl().GetLightmapHeight( lightmap ); }
ITexture * IMaterialSystemInternal::GetLocalCubemap()
{ return MaterialSystemImpl().GetLocalCubemap(); }
void IMaterialSystemInternal::ForceDepthFuncEquals( bool bEnable )
{ MaterialSystemImpl().ForceDepthFuncEquals( bEnable ); }
enum MaterialHeightClipMode_t IMaterialSystemInternal::GetHeightClipMode()
{ return MaterialSystemImpl().GetHeightClipMode(); }
void IMaterialSystemInternal::AddMaterialToMaterialList( IMaterialInternal *pMaterial )
{ MaterialSystemImpl().AddMaterialToMaterialList( pMaterial ); }
void IMaterialSystemInternal::RemoveMaterial( IMaterialInternal *pMaterial )
{ MaterialSystemImpl().RemoveMaterial( pMaterial ); }
void IMaterialSystemInternal::RemoveMaterialSubRect( IMaterialInternal *pMaterial )
{ MaterialSystemImpl().RemoveMaterialSubRect( pMaterial ); }
bool IMaterialSystemInternal::InFlashlightMode() const
{ return MaterialSystemImpl().InFlashlightMode(); }
bool IMaterialSystemInternal::IsCascadedShadowMapping() const
{ return MaterialSystemImpl().IsCascadedShadowMapping(); }
bool IMaterialSystemInternal::CanUseEditorMaterials() const
{ return MaterialSystemImpl().CanUseEditorMaterials(); }
int IMaterialSystemInternal::GetConfigurationFlags() const
{ return MaterialSystemImpl().GetConfigurationFlags(); }
const char * IMaterialSystemInternal::GetForcedTextureLoadPathID()
{ return MaterialSystemImpl().GetForcedTextureLoadPathID(); }
CMatCallQueue * IMaterialSystemInternal::GetRenderCallQueue()
{ return MaterialSystemImpl().GetRenderCallQueue(); }
void IMaterialSystemInternal::UnbindMaterial( IMaterial *pMaterial )
{ MaterialSystemImpl().UnbindMaterial( pMaterial ); }
uint IMaterialSystemInternal::GetRenderThreadId() const
{ return MaterialSystemImpl().GetRenderThreadId(); }
