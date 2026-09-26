//====== Copyright (c) 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "dmxserializationdictionary.h"
#include "dmxloader/dmxelement.h"
#include "dmxloader/dmxattribute.h"
#include <algorithm>


//-----------------------------------------------------------------------------
//
// Element dictionary used in serialization
//
//-----------------------------------------------------------------------------
CDmxSerializationDictionary::CDmxSerializationDictionary( int nElementsHint /* = 0 */ )
{
	m_Dict.reserve( nElementsHint );
	m_Handles.reserve( nElementsHint );
}


//-----------------------------------------------------------------------------
// Finds the handle of the element
//-----------------------------------------------------------------------------
DmxSerializationHandle_t CDmxSerializationDictionary::Find( CDmxElement *pElement )
{
	auto it = m_Handles.find( pElement );
	return it == m_Handles.end() ? DMX_SERIALIZATION_HANDLE_INVALID : it->second;
}

	
//-----------------------------------------------------------------------------
// Creates the list of all things to serialize
//-----------------------------------------------------------------------------
void CDmxSerializationDictionary::BuildElementList_R( CDmxElement *pElement, bool bFlatMode, bool bIsRoot )
{
	if ( !pElement )
		return;

	// FIXME: Right here we should ask the element if it's an external
	// file reference and exit immediately if so.

	// This means we've already encountered this guy.
	// Therefore, he can never be a root element
	DmxSerializationHandle_t h = Find( pElement );
	if ( h != DMX_SERIALIZATION_HANDLE_INVALID )
	{
		m_Dict[h].m_bRoot = true;
		return;
	}

	m_Handles.emplace( pElement, (DmxSerializationHandle_t)m_Dict.size() );
	m_Dict.push_back( { pElement, bFlatMode || bIsRoot } );

	int nCount = pElement->AttributeCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxAttribute *pAttribute = pElement->GetAttribute(i);
		switch( pAttribute->GetType() )
		{
		case AT_ELEMENT:
			{
				CDmxElement *pChild = pAttribute->GetValue<CDmxElement*>();
				if ( !pChild )
					break;

				BuildElementList_R( pChild, bFlatMode, false );
			}
			break;

		case AT_ELEMENT_ARRAY:
			{
				const std::vector< CDmxElement* > &array = pAttribute->GetArray<CDmxElement*>( );
				int nCount = std::ssize( array );
				for ( int i = 0; i < nCount; ++i )
				{
					CDmxElement *pChild = array[ i ];
					if ( !pChild )
						break;

					BuildElementList_R( pChild, bFlatMode, false );
				}
			}
			break;
		}
	}
}

void CDmxSerializationDictionary::BuildElementList( CDmxElement *pElement, bool bFlatMode )
{
	BuildElementList_R( pElement, bFlatMode, true );
}


//-----------------------------------------------------------------------------
// Should I inline the serialization of this element?
//-----------------------------------------------------------------------------
bool CDmxSerializationDictionary::ShouldInlineElement( CDmxElement *pElement )
{
	// This means we've already encountered this guy.
	// Therefore, he can never be a root element
	DmxSerializationHandle_t h = Find( pElement );
	if ( h != DMX_SERIALIZATION_HANDLE_INVALID )
		return !m_Dict[h].m_bRoot;

	// If we didn't find the element, it means it's a reference to an external
	// element (or it's NULL), so don't inline ie.
	return false;
}


//-----------------------------------------------------------------------------
// Clears the dictionary
//-----------------------------------------------------------------------------
void CDmxSerializationDictionary::Clear()
{
	m_Dict.clear();
	m_Handles.clear();
}


//-----------------------------------------------------------------------------
// How many root elements do we have?
//-----------------------------------------------------------------------------
int CDmxSerializationDictionary::RootElementCount() const
{
	return (int)std::ranges::count_if( m_Dict, &DmxElementInfo_t::m_bRoot );
}

	
//-----------------------------------------------------------------------------
// Iterates over all root elements to serialize
//-----------------------------------------------------------------------------
DmxSerializationHandle_t CDmxSerializationDictionary::FirstRootElement() const
{
	// Insertion order puts the actual root element first in the file.
	int nCount = (int)m_Dict.size();
	for ( DmxSerializationHandle_t h = 0; h < nCount; ++h )
	{
		if ( m_Dict[h].m_bRoot )
			return h;
	}
	return DMX_SERIALIZATION_HANDLE_INVALID;
}

DmxSerializationHandle_t CDmxSerializationDictionary::NextRootElement( DmxSerializationHandle_t h ) const
{
	++h;
	int nCount = (int)m_Dict.size();
	for ( ; h < nCount; ++h )
	{
		if ( m_Dict[h].m_bRoot )
			return h;
	}
	return DMX_SERIALIZATION_HANDLE_INVALID;
}

CDmxElement *CDmxSerializationDictionary::GetRootElement( DmxSerializationHandle_t h )
{
	Assert( m_Dict[h].m_bRoot );
	return m_Dict[h].m_pElement;
}


