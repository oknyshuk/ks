// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IColorCorrectionTools class to CColorCorrectionTools. Included by colorcorrectionpanel.cpp.

void IColorCorrectionTools::Init( void )
{ static_cast<CColorCorrectionTools*>(this)->Init(); }
void IColorCorrectionTools::Shutdown( void )
{ static_cast<CColorCorrectionTools*>(this)->Shutdown(); }
bool IColorCorrectionTools::ShouldPause() const
{ return static_cast<const CColorCorrectionTools*>(this)->ShouldPause(); }
void IColorCorrectionTools::GrabPreColorCorrectedFrame( int x, int y, int width, int height )
{ static_cast<CColorCorrectionTools*>(this)->GrabPreColorCorrectedFrame( x, y, width, height ); }
void IColorCorrectionTools::UpdateColorCorrection()
{ static_cast<CColorCorrectionTools*>(this)->UpdateColorCorrection(); }
void IColorCorrectionTools::SetFinalOperation( IColorOperation *pOp )
{ static_cast<CColorCorrectionTools*>(this)->SetFinalOperation( pOp ); }

