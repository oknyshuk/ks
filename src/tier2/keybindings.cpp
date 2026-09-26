//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "tier2/keybindings.h"
#include "tier2/tier2.h"
#include "inputsystem/iinputsystem.h"



//-----------------------------------------------------------------------------
// Set a key binding
//-----------------------------------------------------------------------------
void CKeyBindings::SetBinding( ButtonCode_t code, const char *pBinding )
{
	if ( code == BUTTON_CODE_INVALID || code == KEY_NONE )
		return;

	// free old bindings
	if ( !m_KeyInfo[code].empty() )
	{
		// Exactly the same, don't re-bind and fragment memory
		if ( !Q_stricmp( m_KeyInfo[code].c_str(), pBinding ) )
			return;
	}
			
	// allocate memory for new binding
	m_KeyInfo[code] = ks::nz( pBinding );
}

void CKeyBindings::SetBinding( const char *pButtonName, const char *pBinding )
{
	ButtonCode_t code = g_pInputSystem->StringToButtonCode( pButtonName );
	SetBinding( code, pBinding );
}

void CKeyBindings::Unbind( ButtonCode_t code )
{
	if ( code != KEY_NONE && code != BUTTON_CODE_INVALID )
	{
		m_KeyInfo[code] = "";
	}
}

void CKeyBindings::Unbind( const char *pButtonName )
{
	ButtonCode_t code = g_pInputSystem->StringToButtonCode( pButtonName );
	Unbind( code );
}

void CKeyBindings::UnbindAll()
{
	for ( int i = 0; i < BUTTON_CODE_LAST; i++ )
	{
		m_KeyInfo[i] = "";
	}
}


//-----------------------------------------------------------------------------
// Count number of lines of bindings we'll be writing
//-----------------------------------------------------------------------------
int CKeyBindings::GetBindingCount( ) const
{
	int	nCount = 0;
	for ( int i = 0; i < BUTTON_CODE_LAST; i++ )
	{
		if ( std::ssize( m_KeyInfo[i] ) )
		{
			nCount++;
		}
	}

	return nCount;
}


//-----------------------------------------------------------------------------
// Writes lines containing "bind key value"
//-----------------------------------------------------------------------------
void CKeyBindings::WriteBindings( ks::text_writer &buf )
{
	for ( int i = 0; i < BUTTON_CODE_LAST; i++ )
	{
		if ( std::ssize( m_KeyInfo[i] ) )
		{
			const char *pButtonCode = g_pInputSystem->ButtonCodeToString( (ButtonCode_t)i );
			buf.printf( "bind \"%s\" \"%s\"\n", pButtonCode, m_KeyInfo[i].data() );
		}
	}
}


//-----------------------------------------------------------------------------
// Returns the keyname to which a binding string is bound.  E.g., if 
// TAB is bound to +use then searching for +use will return "TAB"
//-----------------------------------------------------------------------------
const char *CKeyBindings::ButtonNameForBinding( const char *pBinding )
{
	const char *pBind = pBinding;
	if ( pBinding[0] == '+' )
	{
		++pBind;
	}

	for ( int i = 0; i < BUTTON_CODE_LAST; i++ )
	{
		if ( !std::ssize( m_KeyInfo[i] ) )
			continue;

		if ( m_KeyInfo[i][0] == '+' )
		{
			if ( !Q_stricmp( &m_KeyInfo[i][1], pBind ) )
				return g_pInputSystem->ButtonCodeToString( (ButtonCode_t)i );
		}
		else
		{
			if ( !Q_stricmp( m_KeyInfo[i].c_str(), pBind ) )
				return g_pInputSystem->ButtonCodeToString( (ButtonCode_t)i );
		}
	}

	return nullptr;
}

const char *CKeyBindings::GetBindingForButton( ButtonCode_t code )
{
	if ( m_KeyInfo[code].empty() )
		return nullptr;

	return m_KeyInfo[ code ].c_str();
}



