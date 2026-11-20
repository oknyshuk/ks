import ks.core.basetypes;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IDemoBuffer class to CDiskDemoBuffer. Included by demobuffer.cpp.

bool IDemoBuffer::Init( DemoBufferInitParams_t const& params )
{ return static_cast<CDiskDemoBuffer*>(this)->Init( params ); }
void IDemoBuffer::NotifySignonComplete()
{ static_cast<CDiskDemoBuffer*>(this)->NotifySignonComplete(); }
bool IDemoBuffer::IsInitialized() const
{ return static_cast<const CDiskDemoBuffer*>(this)->IsInitialized(); }
bool IDemoBuffer::IsValid() const
{ return static_cast<const CDiskDemoBuffer*>(this)->IsValid(); }
void IDemoBuffer::WriteHeader( const void *pData, int nSize )
{ static_cast<CDiskDemoBuffer*>(this)->WriteHeader( pData, nSize ); }
void IDemoBuffer::NotifyBeginFrame()
{ static_cast<CDiskDemoBuffer*>(this)->NotifyBeginFrame(); }
void IDemoBuffer::NotifyEndFrame()
{ static_cast<CDiskDemoBuffer*>(this)->NotifyEndFrame(); }
void IDemoBuffer::SeekPut( bool bAbsolute, int offset )
{ static_cast<CDiskDemoBuffer*>(this)->SeekPut( bAbsolute, offset ); }
void IDemoBuffer::SeekGet( bool bAbsolute, int offset )
{ static_cast<CDiskDemoBuffer*>(this)->SeekGet( bAbsolute, offset ); }
int IDemoBuffer::TellPut() const
{ return static_cast<const CDiskDemoBuffer*>(this)->TellPut(); }
int IDemoBuffer::TellGet() const
{ return static_cast<const CDiskDemoBuffer*>(this)->TellGet(); }
int IDemoBuffer::TellMaxPut() const
{ return static_cast<const CDiskDemoBuffer*>(this)->TellMaxPut(); }
char IDemoBuffer::GetChar()
{ return static_cast<CDiskDemoBuffer*>(this)->GetChar(); }
unsigned char IDemoBuffer::GetUnsignedChar()
{ return static_cast<CDiskDemoBuffer*>(this)->GetUnsignedChar(); }
int IDemoBuffer::GetInt()
{ return static_cast<CDiskDemoBuffer*>(this)->GetInt(); }
void IDemoBuffer::Get( void* pMem, int size )
{ static_cast<CDiskDemoBuffer*>(this)->Get( pMem, size ); }
void IDemoBuffer::PutChar( char c )
{ static_cast<CDiskDemoBuffer*>(this)->PutChar( c ); }
void IDemoBuffer::PutUnsignedChar( unsigned char uc )
{ static_cast<CDiskDemoBuffer*>(this)->PutUnsignedChar( uc ); }
void IDemoBuffer::PutInt( int i )
{ static_cast<CDiskDemoBuffer*>(this)->PutInt( i ); }
void IDemoBuffer::Put( const void* pMem, int size )
{ static_cast<CDiskDemoBuffer*>(this)->Put( pMem, size ); }
void IDemoBuffer::WriteTick( int nTick )
{ static_cast<CDiskDemoBuffer*>(this)->WriteTick( nTick ); }
void IDemoBuffer::UpdateStartTick( int& nStartTick ) const
{ static_cast<const CDiskDemoBuffer*>(this)->UpdateStartTick( nStartTick ); }
void IDemoBuffer::DumpToFile( char const* pFilename, const demoheader_t &header ) const
{ static_cast<const CDiskDemoBuffer*>(this)->DumpToFile( pFilename, header ); }

