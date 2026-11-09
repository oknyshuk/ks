// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMorphInternal class to CMorph, its one implementation.
// Included by morph.cpp after the definition of CMorph.

void IMorphInternal::Init( MorphFormat_t format, const char *pDebugName )
{ static_cast<CMorph *>( this )->Init( format, pDebugName ); }
bool IMorphInternal::Bind( IMorphMgrRenderContext *pRenderContext )
{ return static_cast<CMorph *>( this )->Bind( pRenderContext ); }
MorphFormat_t IMorphInternal::GetMorphFormat() const
{ return static_cast<const CMorph *>( this )->GetMorphFormat(); }
