// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the ISubDMgr class to its one implementation, CSubDMgr.
// Included by SubdMgr.cpp, which defines SubDMgrImpl().

bool ISubDMgr::ShouldAllocateTextures()
{ return SubDMgrImpl().ShouldAllocateTextures(); }
void ISubDMgr::AllocateTextures()
{ SubDMgrImpl().AllocateTextures(); }
void ISubDMgr::FreeTextures()
{ SubDMgrImpl().FreeTextures(); }
void ISubDMgr::ReleaseResources()
{ SubDMgrImpl().ReleaseResources(); }
void ISubDMgr::ReacquireResources()
{ SubDMgrImpl().ReacquireResources(); }
int ISubDMgr::GetWidth()
{ return SubDMgrImpl().GetWidth(); }
int ISubDMgr::GetHeight()
{ return SubDMgrImpl().GetHeight(); }
int ISubDMgr::GetOffsetGeometry()
{ return SubDMgrImpl().GetOffsetGeometry(); }
int ISubDMgr::GetOffsetTangents()
{ return SubDMgrImpl().GetOffsetTangents(); }
float* ISubDMgr::Lock( int nNumRows )
{ return SubDMgrImpl().Lock( nNumRows ); }
void ISubDMgr::Unlock()
{ SubDMgrImpl().Unlock(); }
ShaderAPITextureHandle_t ISubDMgr::SubDTexture()
{ return SubDMgrImpl().SubDTexture(); }
