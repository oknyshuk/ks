// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
#include "choreoactor.h"
#include "choreochannel.h"
#include "choreoscene.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CChoreoActor::CChoreoActor( void )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CChoreoActor::CChoreoActor( const char *name )
{
	Init();
	SetName( name );
}

//-----------------------------------------------------------------------------
// Purpose: // Assignment
// Input  : src - 
// Output : CChoreoActor&
//-----------------------------------------------------------------------------
CChoreoActor& CChoreoActor::operator=( const CChoreoActor& src )
{
	m_bActive = src.m_bActive;

	V_strncpy( m_szName, src.m_szName, sizeof( m_szName ) );
	V_strncpy( m_szFacePoserModelName, src.m_szFacePoserModelName, sizeof( m_szFacePoserModelName ) );

	for ( int i = 0; i < std::ssize( src.m_Channels ); i++ )
	{
		CChoreoChannel *c = src.m_Channels[ i ];
		CChoreoChannel *newChannel = new CChoreoChannel();
		newChannel->SetActor( this );
		*newChannel = *c;
		AddChannel( newChannel );
	}

	return *this;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoActor::Init( void )
{
	m_szName[ 0 ] = 0;
	m_szFacePoserModelName[ 0 ] = 0;
	m_bActive = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void CChoreoActor::SetName( const char *name )
{
	contract_assert( std::strlen( name ) < MAX_ACTOR_NAME );
	V_strncpy( m_szName, name, sizeof( m_szName ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CChoreoActor::GetName( void )
{
	return m_szName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoActor::GetNumChannels( void )
{
	return std::ssize( m_Channels );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : channel - 
// Output : CChoreoChannel
//-----------------------------------------------------------------------------
CChoreoChannel *CChoreoActor::GetChannel( int channel )
{
	if ( channel < 0 || channel >= std::ssize( m_Channels ) )
	{
		return nullptr;
	}

	return m_Channels[ channel ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *channel - 
//-----------------------------------------------------------------------------
void CChoreoActor::AddChannel( CChoreoChannel *channel )
{
	m_Channels.push_back( channel );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *channel - 
//-----------------------------------------------------------------------------
void CChoreoActor::RemoveChannel( CChoreoChannel *channel )
{
	int idx = FindChannelIndex( channel );
	if ( idx == -1 )
		return;

	m_Channels.erase( m_Channels.begin() + idx );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoActor::RemoveAllChannels()
{
	m_Channels.clear();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : c1 - 
//			c2 - 
//-----------------------------------------------------------------------------
void CChoreoActor::SwapChannels( int c1, int c2 )
{
	CChoreoChannel *temp;

	temp = m_Channels[ c1 ];
	m_Channels[ c1 ] = m_Channels[ c2 ];
	m_Channels[ c2 ] = temp;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *channel - 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoActor::FindChannelIndex( CChoreoChannel *channel )
{
	for ( int i = 0; i < std::ssize( m_Channels ); i++ )
	{
		if ( channel == m_Channels[ i ] )
		{
			return i;
		}
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void CChoreoActor::SetFacePoserModelName( const char *name )
{
	V_strncpy( m_szFacePoserModelName, name, sizeof( m_szFacePoserModelName ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *CChoreoActor::GetFacePoserModelName( void ) const
{
	return m_szFacePoserModelName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : active - 
//-----------------------------------------------------------------------------
void CChoreoActor::SetActive( bool active )
{
	m_bActive = active;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CChoreoActor::GetActive( void ) const
{
	return m_bActive;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoActor::MarkForSaveAll( bool mark )
{
	SetMarkedForSave( mark );

	int c = GetNumChannels();
	for ( int i = 0; i < c; i++ )
	{
		CChoreoChannel *channel = GetChannel( i );
		channel->MarkForSaveAll( mark );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : CChoreoChannel
//-----------------------------------------------------------------------------
CChoreoChannel *CChoreoActor::FindChannel( const char *name )
{
	int c = GetNumChannels();
	for ( int i = 0; i < c; i++ )
	{
		CChoreoChannel *channel = GetChannel( i );
		if ( !V_stricmp( channel->GetName(), name ) )
			return channel;
	}

	return nullptr;
}

void CChoreoActor::SaveToBuffer( ks::byte_writer& buf, CChoreoScene *pScene, IChoreoStringPool *pStringPool )
{
	buf.write< short >( pStringPool->FindOrAddString( GetName() ) );

	int c = GetNumChannels();
	contract_assert( c <= 255 );
	buf.write< unsigned char >( c );

	for ( int i = 0; i < c; i++ )
	{
		CChoreoChannel *channel = GetChannel( i );
		contract_assert( channel );
		channel->SaveToBuffer( buf, pScene, pStringPool );
	}

	/*
	if ( V_strlen( a->GetFacePoserModelName() ) > 0 )
	{
		FilePrintf( buf, level + 1, "faceposermodel \"%s\"\n", a->GetFacePoserModelName() );
	}
	*/
	buf.write< char >( GetActive() ? 1 : 0 );
}

bool CChoreoActor::RestoreFromBuffer( ks::byte_reader& buf, CChoreoScene *pScene, IChoreoStringPool *pStringPool )
{
	char sz[ 256 ];
	pStringPool->GetString( buf.read< short >(), sz, sizeof( sz ) );

	SetName( sz );

	int i;
	int c = buf.read< unsigned char >();
	for ( i = 0; i < c; i++ )
	{
		CChoreoChannel *channel = pScene->AllocChannel();
		contract_assert( channel );
		if ( channel->RestoreFromBuffer( buf, pScene, this, pStringPool ) )
		{
			AddChannel( channel );
			channel->SetActor( this );
			continue;
		}

		return false;
	}

	SetActive( buf.read< char >() == 1 ? true : false );

	return true;
}
