// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the ISPSharedMemory class to CSPSharedMemory. Included by singleplayersharedmemory.cpp.

bool ISPSharedMemory::Init( std::size_t iSize )
{ return static_cast<CSPSharedMemory*>(this)->Init( iSize ); }
uint8 * ISPSharedMemory::Base( void )
{ return static_cast<CSPSharedMemory*>(this)->Base(); }
std::size_t ISPSharedMemory::Size( void )
{ return static_cast<CSPSharedMemory*>(this)->Size(); }
void ISPSharedMemory::AddRef( void )
{ static_cast<CSPSharedMemory*>(this)->AddRef(); }
void ISPSharedMemory::Release( void )
{ static_cast<CSPSharedMemory*>(this)->Release(); }

