// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMorph class to CMorph, its one implementation.
// Included by morph.cpp after the definition of CMorph.

void IMorph::Lock( float flFloatToFixedScale )
{ static_cast<CMorph *>( this )->Lock( flFloatToFixedScale ); }
void IMorph::AddMorph( const MorphVertexInfo_t &info )
{ static_cast<CMorph *>( this )->AddMorph( info ); }
void IMorph::Unlock()
{ static_cast<CMorph *>( this )->Unlock(); }
