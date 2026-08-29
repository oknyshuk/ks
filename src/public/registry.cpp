//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#if defined( WIN32 )
#include <windows.h>
#endif
#include "tier0/platform.h"
#include "iregistry.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include <stdio.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Exposes registry interface to rest of launcher
//-----------------------------------------------------------------------------
class CRegistry : public IRegistry
{
public:
							CRegistry( void );
	virtual					~CRegistry( void );

	virtual bool			Init( const char *platformName );
	virtual bool			DirectInit( const char *subDirectoryUnderValve );
	virtual void			Shutdown( void );
	
	virtual int				ReadInt( const char *key, int defaultValue = 0);
	virtual void			WriteInt( const char *key, int value );

	virtual const char		*ReadString( const char *key, const char *defaultValue = NULL );
	virtual void			WriteString( const char *key, const char *value );

	// Read/write helper methods
	virtual int				ReadInt( const char *pKeyBase, const char *pKey, int defaultValue = 0 );
	virtual void			WriteInt( const char *pKeyBase, const char *key, int value );
	virtual const char		*ReadString( const char *pKeyBase, const char *key, const char *defaultValue );
	virtual void			WriteString( const char *pKeyBase, const char *key, const char *value );

private:
	bool			m_bValid;
#ifdef WIN32
	HKEY			m_hKey;
#endif
};

// Creates it and calls Init
IRegistry *InstanceRegistry( char const *subDirectoryUnderValve )
{
	CRegistry *instance = new CRegistry();
	instance->DirectInit( subDirectoryUnderValve );
	return instance;
}

// Calls Shutdown and deletes it
void ReleaseInstancedRegistry( IRegistry *reg )
{
	if ( !reg )
	{
		Assert( !"ReleaseInstancedRegistry( reg == NULL )!" );
		return;
	}

	reg->Shutdown();
	delete reg;
}

// Expose to launcher
static CRegistry g_Registry;
IRegistry *registry = ( IRegistry * )&g_Registry;

//-----------------------------------------------------------------------------
// Read/write helper methods
//-----------------------------------------------------------------------------
int CRegistry::ReadInt( const char *pKeyBase, const char *pKey, int defaultValue )
{
	int nLen = strlen( pKeyBase );
	int nKeyLen = strlen( pKey );
	char *pFullKey = (char*)_alloca( nLen + nKeyLen + 2 );
	Q_snprintf( pFullKey, nLen + nKeyLen + 2, "%s\\%s", pKeyBase, pKey );
	return ReadInt( pFullKey, defaultValue );
}

void CRegistry::WriteInt( const char *pKeyBase, const char *pKey, int value )
{
	int nLen = strlen( pKeyBase );
	int nKeyLen = strlen( pKey );
	char *pFullKey = (char*)_alloca( nLen + nKeyLen + 2 );
	Q_snprintf( pFullKey, nLen + nKeyLen + 2, "%s\\%s", pKeyBase, pKey );
	WriteInt( pFullKey, value );
}

const char *CRegistry::ReadString( const char *pKeyBase, const char *pKey, const char *defaultValue )
{
	int nLen = strlen( pKeyBase );
	int nKeyLen = strlen( pKey );
	char *pFullKey = (char*)_alloca( nLen + nKeyLen + 2 );
	Q_snprintf( pFullKey, nLen + nKeyLen + 2, "%s\\%s", pKeyBase, pKey );
	return ReadString( pFullKey, defaultValue );
}

void CRegistry::WriteString( const char *pKeyBase, const char *pKey, const char *value )
{
	int nLen = strlen( pKeyBase );
	int nKeyLen = strlen( pKey );
	char *pFullKey = (char*)_alloca( nLen + nKeyLen + 2 );
	Q_snprintf( pFullKey, nLen + nKeyLen + 2, "%s\\%s", pKeyBase, pKey );
	WriteString( pFullKey, value );
}







//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CRegistry::CRegistry( void )
{
	// Assume failure
	m_bValid	= false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CRegistry::~CRegistry( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Read integer from registry
// Input  : *key - 
//			defaultValue - 
// Output : int
//-----------------------------------------------------------------------------
int CRegistry::ReadInt( const char *key, int defaultValue /*= 0*/ )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Save integer to registry
// Input  : *key - 
//			value - 
//-----------------------------------------------------------------------------
void CRegistry::WriteInt( const char *key, int value )
{
}

//-----------------------------------------------------------------------------
// Purpose: Read string value from registry
// Input  : *key - 
//			*defaultValue - 
// Output : const char
//-----------------------------------------------------------------------------
const char *CRegistry::ReadString( const char *key, const char *defaultValue /* = NULL */ )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Save string to registry
// Input  : *key - 
//			*value - 
//-----------------------------------------------------------------------------
void CRegistry::WriteString( const char *key, const char *value )
{
}



bool CRegistry::DirectInit( const char *subDirectoryUnderValve )
{

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Open default launcher key based on game directory
//-----------------------------------------------------------------------------
bool CRegistry::Init( const char *platformName )
{
	char subDir[ 512 ];
	snprintf( subDir, sizeof(subDir), "%s\\Settings", platformName );
	return DirectInit( subDir );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRegistry::Shutdown( void )
{
	if ( !m_bValid )
		return;

	// Make invalid
	m_bValid = false;
}

