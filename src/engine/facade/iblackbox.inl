// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IBlackBox class to CBlackBox. Included by blackbox.cpp.

void IBlackBox::Record( int type, const char *fmt )
{ static_cast<CBlackBox*>(this)->Record( type, fmt ); }
void IBlackBox::SetLimit( int type, unsigned int count )
{ static_cast<CBlackBox*>(this)->SetLimit( type, count ); }
const char * IBlackBox::Get( int type, unsigned int index )
{ return static_cast<CBlackBox*>(this)->Get( type, index ); }
int IBlackBox::Count( int type )
{ return static_cast<CBlackBox*>(this)->Count( type ); }
void IBlackBox::Flush( int type )
{ static_cast<CBlackBox*>(this)->Flush( type ); }
const char * IBlackBox::GetTypeName( int type )
{ return static_cast<CBlackBox*>(this)->GetTypeName( type ); }
int IBlackBox::GetTypeCount()
{ return static_cast<CBlackBox*>(this)->GetTypeCount(); }

