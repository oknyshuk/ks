// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2004 Valve Corporation

import ks.core.basetypes;
import ks.core.dbg;
import ks.math.compressed_vector;
import ks.math.fltx4;
import ks.platform;
import ks.simd;
import ks.tier1.intern_pool;
import ks.tier1.string_ops;
import ks.tier1.strtools;
import ks.tier1.uniqueid;
#include "dmxloader/dmxelement.h"
#include "dmxloader/dmxattribute.h"
import std;

//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------
ks::intern_pool_mt CDmxElement::s_TypeSymbols;


//-----------------------------------------------------------------------------
// Creates a dmx element
//-----------------------------------------------------------------------------
CDmxElement* CreateDmxElement( const char *pType )
{
	return new CDmxElement( pType );
}


//-----------------------------------------------------------------------------
// Constructor, destructor 
//-----------------------------------------------------------------------------
CDmxElement::CDmxElement( const char *pType )
{
	m_Type = pType ? s_TypeSymbols.intern( pType ) : std::string_view();
	m_nLockCount = 0;
	m_bResortNeeded = false;
	m_bIsMarkedForDeletion = false;
	CreateUniqueId( &m_Id );
}

CDmxElement::~CDmxElement()
{
	CDmxElementModifyScope modify( this );
	RemoveAllAttributes();
}


//-----------------------------------------------------------------------------
// Utility method for getting at the type
//-----------------------------------------------------------------------------
std::string_view CDmxElement::GetType()  const
{
	return m_Type;
}

const char* CDmxElement::GetTypeString() const
{
	return m_Type.data() ? m_Type.data() : "";
}

const char* CDmxElement::GetName() const
{
	return GetValue< std::string >( "name" ).c_str();
}

const DmObjectId_t &CDmxElement::GetId() const
{
	return m_Id;
}


//-----------------------------------------------------------------------------
// Sets the object id, name
//-----------------------------------------------------------------------------
void CDmxElement::SetId( const DmObjectId_t &id )
{
	CopyUniqueId( id, &m_Id );
}

void CDmxElement::SetName( const char *pName )
{
	SetValue< std::string >( "name", ks::nz( pName ) );
}


//-----------------------------------------------------------------------------
// Sorts the vector when a change has occurred
//-----------------------------------------------------------------------------
void CDmxElement::Resort( )	const
{
	if ( m_bResortNeeded )
	{
		AttributeList_t *pAttributes = const_cast< AttributeList_t *>( &m_Attributes );
		std::ranges::sort( *pAttributes, CDmxAttributeLess(), &AttributePtr_t::get );
		m_bResortNeeded = false;

		// NOTE: This checks for duplicate attribute names
		int nCount = std::ssize( m_Attributes );
		for ( int i = nCount; --i >= 1; )
		{
			if ( m_Attributes[i] == nullptr || m_Attributes[i-1] == nullptr )
			{
				continue;
			}

			if ( m_Attributes[i]->GetNameSymbol().data() == m_Attributes[i-1]->GetNameSymbol().data() )
			{
				Warning( "Duplicate attribute name %s encountered!\n", m_Attributes[i]->GetName() );
				// dropped but not freed: whoever added it may still hold the pointer
				(void)( *pAttributes )[i].release();
				pAttributes->erase( pAttributes->begin() + i );
				contract_assert( 0 );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Enables modification of the DmxElement
//-----------------------------------------------------------------------------
void CDmxElement::LockForChanges( bool bLock )
{
	if ( bLock )
	{
		++m_nLockCount;
	}
	else
	{
		if ( --m_nLockCount == 0 )
		{
			Resort();
		}
		contract_assert( m_nLockCount >= 0 );
	}
}


//-----------------------------------------------------------------------------
// Adds, removes attributes
//-----------------------------------------------------------------------------
CDmxAttribute *CDmxElement::AddAttribute( const char *pAttributeName )
{
	int nIndex = FindAttribute( pAttributeName );
	if ( nIndex >= 0 )
		return m_Attributes[nIndex].get();

	CDmxElementModifyScope modify( this );
	m_bResortNeeded = true;
	CDmxAttribute *pAttribute = new CDmxAttribute( pAttributeName );
	m_Attributes.emplace_back( pAttribute );
	return pAttribute;
}

void CDmxElement::RemoveAttribute( const char *pAttributeName )
{
	CDmxElementModifyScope modify( this );
	int idx = FindAttribute( pAttributeName );
	if ( idx >= 0 )
	{
		m_Attributes.erase( m_Attributes.begin() + idx );
	}
}

void CDmxElement::RemoveAttributeByPtr( CDmxAttribute *pAttribute )
{	
	CDmxElementModifyScope modify( this );
	auto it = std::ranges::find( m_Attributes, pAttribute, &AttributePtr_t::get );
	if ( it != m_Attributes.end() )
	{
		m_Attributes.erase( it );
	}
}

void CDmxElement::RemoveAllAttributes()
{
	m_Attributes.clear();
	m_bResortNeeded = false;
}


//-----------------------------------------------------------------------------
// Rename an attribute
//-----------------------------------------------------------------------------
void CDmxElement::RenameAttribute( const char *pAttributeName, const char *pNewName )
{
	CDmxElementModifyScope modify( this );

	// No change...
	if ( !V_stricmp( pAttributeName, pNewName ) )
		return;

	int idx = FindAttribute( pAttributeName );
	if ( idx < 0 )
		return;

	if ( HasAttribute( pNewName ) )
	{
		Warning( "Tried to rename from \"%s\" to \"%s\", but \"%s\" already exists!\n",
			pAttributeName, pNewName, pNewName );
		return;
	}

	m_bResortNeeded = true;
	m_Attributes[ idx ]->SetName( pNewName );
}


//-----------------------------------------------------------------------------
// Find an attribute by name-based lookup
//-----------------------------------------------------------------------------
int CDmxElement::FindAttribute( const char *pAttributeName ) const
{
	// FIXME: The cost here is O(log M) + O(log N)
	// where log N is the binary search for the symbol match
	// and log M is the binary search for the attribute name->symbol
	// We can eliminate log M by using a hash table in the symbol lookup
	Resort();
	CDmxAttribute search( pAttributeName );
	auto it = std::ranges::lower_bound( m_Attributes, &search, CDmxAttributeLess(), &AttributePtr_t::get );
	if ( it == m_Attributes.end() || CDmxAttributeLess()( &search, it->get() ) )
		return -1;
	return (int)( it - m_Attributes.begin() );
}


//-----------------------------------------------------------------------------
// Find an attribute by name-based lookup
//-----------------------------------------------------------------------------
int CDmxElement::FindAttribute( std::string_view attributeName ) const
{
	Resort();
	CDmxAttribute search( attributeName );
	auto it = std::ranges::lower_bound( m_Attributes, &search, CDmxAttributeLess(), &AttributePtr_t::get );
	if ( it == m_Attributes.end() || CDmxAttributeLess()( &search, it->get() ) )
		return -1;
	return (int)( it - m_Attributes.begin() );
}


//-----------------------------------------------------------------------------
// Attribute finding
//-----------------------------------------------------------------------------
bool CDmxElement::HasAttribute( const char *pAttributeName ) const
{
	int idx = FindAttribute( pAttributeName );
	return ( idx >= 0 );
}

CDmxAttribute *CDmxElement::GetAttribute( const char *pAttributeName )
{
	int idx = FindAttribute( pAttributeName );
	if ( idx >= 0 )
		return m_Attributes[ idx ].get();
	return nullptr;
}

const CDmxAttribute *CDmxElement::GetAttribute( const char *pAttributeName ) const
{
	int idx = FindAttribute( pAttributeName );
	if ( idx >= 0 )
		return m_Attributes[ idx ].get();
	return nullptr;
}


//-----------------------------------------------------------------------------
// Attribute interation
//-----------------------------------------------------------------------------
int CDmxElement::AttributeCount() const
{
	return std::ssize( m_Attributes );
}

CDmxAttribute *CDmxElement::GetAttribute( int nIndex )
{
	return m_Attributes[ nIndex ].get();
}

const CDmxAttribute *CDmxElement::GetAttribute( int nIndex ) const
{
	return m_Attributes[ nIndex ].get();
}


//-----------------------------------------------------------------------------
// Removes all elements recursively
//-----------------------------------------------------------------------------
void CDmxElement::AddElementsToDelete( std::vector< CDmxElement * >& elementsToDelete )
{
	if ( m_bIsMarkedForDeletion )
		return;

	m_bIsMarkedForDeletion = true;
	elementsToDelete.push_back( this );

	int nCount = AttributeCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxAttribute *pAttribute = GetAttribute(i);
		if ( pAttribute->GetType() == AT_ELEMENT )
		{
			CDmxElement *pElement = pAttribute->GetValue< CDmxElement* >();
			if ( pElement )
			{
				pElement->AddElementsToDelete( elementsToDelete );
			}
			continue;
		}

		if ( pAttribute->GetType() == AT_ELEMENT_ARRAY )
		{
			const std::vector< CDmxElement * > &elements = pAttribute->GetArray< CDmxElement* >();
			int nElementCount = std::ssize( elements );
			for ( int j = 0; j < nElementCount; ++j )
			{
				if ( elements[j] )
				{
					elements[j]->AddElementsToDelete( elementsToDelete );
				}
			}
			continue;
		}
	}
}


//-----------------------------------------------------------------------------
// Removes all elements recursively
//-----------------------------------------------------------------------------
void CDmxElement::RemoveAllElementsRecursive()
{
	std::vector< CDmxElement * > elementsToDelete; 
	AddElementsToDelete( elementsToDelete );
	int nCount = std::ssize( elementsToDelete );
	for ( int i = 0; i < nCount; ++i )
	{
		delete elementsToDelete[i];
	}
}

//-----------------------------------------------------------------------------
// Template for unpacking a bitfield inside of an unpack structure.
// pUnpack->m_nSize is the number of bits in the bitfield
// pUnpack->m_nBitOffset is the number of bits to offset from pDest
//-----------------------------------------------------------------------------
template <typename T>
void CDmxElement::UnpackBitfield( T *pDest, const DmxElementUnpackStructure_t *pUnpack, const CDmxAttribute *pAttribute ) const
{
	// Determine if T is a signed type
	const bool bIsSigned = ( 0 > (T)(-1) );
	if ( bIsSigned )
	{
		// signed types need to be larger than 1 in size or else you get sign extension problems
		contract_assert( pUnpack->m_nSize > 1 );   
	}

	// Right now the max size bitfield we handle is 32.
	contract_assert( pUnpack->m_nSize <= 32 );
	contract_assert( pUnpack->m_nSize <= 8*sizeof(T) );
	contract_assert( pUnpack->m_nBitOffset + pUnpack->m_nSize <= 8*sizeof(T) );

	// Create a mask that covers the bitfield.
	T mask;
	T maskBeforeShift;
	if ( pUnpack->m_nSize == 8*sizeof(T) )
	{
		maskBeforeShift = (T)~0;
		mask = maskBeforeShift;
	}
	else
	{
		maskBeforeShift = ( 1 << pUnpack->m_nSize ) - 1;
		mask = maskBeforeShift << pUnpack->m_nBitOffset;	
	}
	mask = ~mask;

	T value = ( *(T *)pAttribute->m_pData );

	// Determine if value is in the range that this variable can hold.
	T signedMaskBeforeShift = bIsSigned ? ( 1 << (pUnpack->m_nSize-1) ) - 1 : maskBeforeShift;
	if ( !bIsSigned || ( value >= 0 ) )
	{
		if ( ( value & ~signedMaskBeforeShift ) != 0 )
		{
			Warning( "Value %s exeeds size of datatype. \n", pUnpack->m_pAttributeName );
			value = 0;    // Clear value
		}
	}
	else
	{
		if ( !bIsSigned || ( ( value & ~signedMaskBeforeShift ) != ~signedMaskBeforeShift ) )
		{
			Warning( "Value %s exeeds size of datatype. \n", pUnpack->m_pAttributeName );
			value = 0;    // Clear value
		}
	}

	// Mask value to the correct number of bits (important if value is a neg number!)
	value &= maskBeforeShift;
	
	// Pack it together.
	// Clear value
	*pDest &= mask;
	// Install value
	*pDest |=  ( value << pUnpack->m_nBitOffset );

}

//-----------------------------------------------------------------------------
// Method to unpack data into a structure
//-----------------------------------------------------------------------------
void CDmxElement::UnpackIntoStructure( void *pData, const DmxElementUnpackStructure_t *pUnpack ) const
{
	for ( ; pUnpack->m_AttributeType != AT_UNKNOWN; ++pUnpack )
	{
		char *pDest = (char*)pData + pUnpack->m_nOffset;

		// Recurse?
		if ( pUnpack->m_pSub )
		{
			UnpackIntoStructure( (void *)pDest, pUnpack->m_pSub );
			continue;
		}

		if ( IsArrayType( pUnpack->m_AttributeType ) )
		{
			// NOTE: This does not work with string/bitfield array data at the moment
			if ( ( pUnpack->m_AttributeType == AT_STRING_ARRAY ) || ( pUnpack->m_nBitOffset != NO_BIT_OFFSET ) )
			{
				contract_assert( 0 );
				continue;
			}
		}

		if ( ( pUnpack->m_AttributeType == AT_VOID ) || ( pUnpack->m_AttributeType == AT_VOID_ARRAY ) )
		{
			contract_assert( 0 );
			continue;
		}

		CDmxAttribute temp( nullptr );
		const CDmxAttribute *pAttribute = GetAttribute( pUnpack->m_pAttributeName );
		if ( !pAttribute )
		{
			if ( !pUnpack->m_pDefaultString )
				continue;

			temp.AllocateDataMemory_AndConstruct( pUnpack->m_AttributeType );
			if ( !IsArrayType( pUnpack->m_AttributeType ) )
			{
				// Convert the default string into the target (array types do this inside GetArrayValue below)
				temp.SetValueFromString( pUnpack->m_pDefaultString );
			}
			pAttribute = &temp;
		}

		if ( pUnpack->m_AttributeType != pAttribute->GetType() ) 
		{
			Warning( "CDmxElement::UnpackIntoStructure: Mismatched attribute type in attribute \"%s\"!\n", pUnpack->m_pAttributeName );
			continue;
		}

		if ( pAttribute->GetType() == AT_STRING )
		{
			if ( pUnpack->m_nSize == UTL_STRING_SIZE )  // the string is a UtlString.
			{
				*(std::string *)pDest = pAttribute->GetValueString();		
			}
			else  // the string is a preallocated char array.
			{
				// Strings get special treatment: they are stored as in-line arrays of chars
				V_strncpy( pDest, pAttribute->GetValueString(), pUnpack->m_nSize );
			}
			continue;
		}

		// Get the basic type, if the attribute is an array:
		DmAttributeType_t basicType = CDmxAttribute::ArrayAttributeBasicType( pAttribute->GetType() );

		// Special case - if data type is float, but dest size == 16, we are unpacking into simd by replication
		if ( ( basicType == AT_FLOAT ) && ( pUnpack->m_nSize == sizeof( fltx4 ) ) )
		{
			if ( IsArrayType( pUnpack->m_AttributeType ) )
			{
				// Copy from the attribute into a fixed-size array:
				float *pfDest = (float *)pDest;
				const std::vector< float > &floatVector = pAttribute->GetArray< float >();
				for ( int i = 0; i < pUnpack->m_nArrayLength; i++ )
				{
					for ( int j = 0; j < 4; j++ ) std::memcpy( pfDest++, &floatVector[ i ], sizeof( float ) );
				}
			}
			else
			{
				std::memcpy( pDest + 0 * sizeof( float ), pAttribute->m_pData, sizeof( float ) );
				std::memcpy( pDest + 1 * sizeof( float ), pAttribute->m_pData, sizeof( float ) );
				std::memcpy( pDest + 2 * sizeof( float ), pAttribute->m_pData, sizeof( float ) );
				std::memcpy( pDest + 3 * sizeof( float ), pAttribute->m_pData, sizeof( float ) );
			}
		}
		else
		{
			int nDataTypeSize = pUnpack->m_nSize;
			if ( basicType == AT_INT )
			{
				if ( pUnpack->m_nBitOffset == NO_BIT_OFFSET ) // This test is not for bitfields
				{
					contract_assert( nDataTypeSize <= CDmxAttribute::AttributeDataSize( basicType ) );
				}
			}
			else
			{
				contract_assert( nDataTypeSize == CDmxAttribute::AttributeDataSize( basicType ) );
			}

			if ( IsArrayType( pUnpack->m_AttributeType ) )
			{
				// Copy from the attribute into a fixed-size array (padding with the default value if need be):
				pAttribute->GetArrayValue( pUnpack->m_AttributeType, pDest, nDataTypeSize, pUnpack->m_nArrayLength, pUnpack->m_pDefaultString );
			}
			else if ( pUnpack->m_nBitOffset == NO_BIT_OFFSET )
			{
				std::memcpy( pDest, pAttribute->m_pData, pUnpack->m_nSize );
			}
			else
			{
				if ( pAttribute->GetType() == AT_INT )
				{
					// Int attribute types are used for char/short/int.
					switch ( pUnpack->m_BitfieldType )
					{
					case BITFIELD_TYPE_BOOL:
						// Note: unsigned char handles bools as bitfields.
						UnpackBitfield( (unsigned char *)pDest, pUnpack, pAttribute );
						break;
					case BITFIELD_TYPE_CHAR : 		
						UnpackBitfield( (char *)pDest, pUnpack, pAttribute );
						break;
					case BITFIELD_TYPE_UNSIGNED_CHAR : 		
						UnpackBitfield( (unsigned char *)pDest, pUnpack, pAttribute );
						break;
					case BITFIELD_TYPE_SHORT : 		
						UnpackBitfield( (short *)pDest, pUnpack, pAttribute );
						break;
					case BITFIELD_TYPE_UNSIGNED_SHORT : 		
						UnpackBitfield( (unsigned short *)pDest, pUnpack, pAttribute );
						break;
					case BITFIELD_TYPE_INT : 		
						UnpackBitfield( (int *)pDest, pUnpack, pAttribute );
						break;
					case BITFIELD_TYPE_UNSIGNED_INT : 		
						UnpackBitfield( (unsigned int *)pDest, pUnpack, pAttribute );
						break;
					default:
						contract_assert( 0 );
						break;
					};
				}
				else
				{
					UnpackBitfield( (char *)pDest, pUnpack, pAttribute );
				}	
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Creates attributes based on the unpack structure
//-----------------------------------------------------------------------------
void CDmxElement::AddAttributesFromStructure( const void *pData, const DmxElementUnpackStructure_t *pUnpack )
{
	for ( ; pUnpack->m_AttributeType != AT_UNKNOWN; ++pUnpack )
	{
		const char *pSrc = (const char*)pData + pUnpack->m_nOffset;
		if ( pUnpack->m_pSub )
		{
			CDmxElement *pDest = CreateDmxElement( pUnpack->m_pTypeName );
			pDest->AddAttributesFromStructure( pSrc, pUnpack->m_pSub );
			SetValue( pUnpack->m_pAttributeName, pDest );
			continue;
		}

		if ( IsArrayType( pUnpack->m_AttributeType ) )
		{
			// NOTE: This does not work with string/bitfield array data at the moment
			if ( ( pUnpack->m_AttributeType == AT_STRING_ARRAY ) || ( pUnpack->m_nBitOffset != NO_BIT_OFFSET ) )
			{
				contract_assert( 0 );
				continue;
			}
		}

		if ( ( pUnpack->m_AttributeType == AT_VOID ) || ( pUnpack->m_AttributeType == AT_VOID_ARRAY ) )
		{
			contract_assert( 0 );
			continue;
		}

		if ( HasAttribute( pUnpack->m_pAttributeName ) )
		{
			contract_assert( 0 );
			continue;
		}

		{
			CDmxElementModifyScope modify( this );
			CDmxAttribute *pAttribute = AddAttribute( pUnpack->m_pAttributeName );
			if ( pUnpack->m_AttributeType == AT_STRING )
			{
				if ( pUnpack->m_nSize == UTL_STRING_SIZE )	  // it is a UtlString. 
				{
					const char *test = (*(std::string *)pSrc).data();
					pAttribute->SetValue( test );
				}
				else
				{
					pAttribute->SetValue( pSrc );
				}
			}
			else
			{
				// Get the basic data type, if the attribute is an array:
				DmAttributeType_t basicType = CDmxAttribute::ArrayAttributeBasicType( pUnpack->m_AttributeType );
				int nDataTypeSize = pUnpack->m_nSize;

				// handle float attrs stored as replicated fltx4's
				if ( ( basicType == AT_FLOAT ) && ( nDataTypeSize == sizeof( fltx4 ) ) )
				{
					nDataTypeSize = sizeof( float );
				}

				if ( basicType == AT_INT )
				{
					if ( pUnpack->m_nBitOffset == NO_BIT_OFFSET ) // This test is not for bitfields.
					{
						contract_assert( nDataTypeSize <= CDmxAttribute::AttributeDataSize( basicType ) );
					}
				}
				else
				{
					contract_assert( nDataTypeSize == CDmxAttribute::AttributeDataSize( basicType ) );
				}

				if ( IsArrayType( pUnpack->m_AttributeType ) )
				{
					// Copy from a fixed-size array into the attribute:
					int nArrayStride = pUnpack->m_nSize;
					pAttribute->SetArrayValue( pUnpack->m_AttributeType, pSrc, nDataTypeSize, pUnpack->m_nArrayLength, nArrayStride );
				}
				else if ( pUnpack->m_nBitOffset == NO_BIT_OFFSET )
				{
					pAttribute->SetValue( pUnpack->m_AttributeType, pSrc, nDataTypeSize );
				}
				else
				{
					
					// Right now the max size bitfield we handle is 32.
					contract_assert( pUnpack->m_nBitOffset + pUnpack->m_nSize <= 32 );

					int mask = 0;
					if ( pUnpack->m_nSize == 32 )
					{
						mask = ~0;
					}
					else
					{
						mask = ( 1 << pUnpack->m_nSize ) - 1;
						mask <<= pUnpack->m_nBitOffset;
					}
					
					int value = ( ( *(int *)pSrc ) & mask );
					if ( ( pUnpack->m_BitfieldType == BITFIELD_TYPE_CHAR ) ||
						( pUnpack->m_BitfieldType == BITFIELD_TYPE_SHORT ) ||
						( pUnpack->m_BitfieldType == BITFIELD_TYPE_INT ) )
					{
						// move it all the way up to make sure we get the correct sign extension.
						value <<= ( 32 - ( pUnpack->m_nBitOffset + pUnpack->m_nSize ) );
						// move it back down into position.
						value >>= ( 32 - ( pUnpack->m_nBitOffset + pUnpack->m_nSize ) );	
					}
					value >>= pUnpack->m_nBitOffset;
					pAttribute->SetValue( pUnpack->m_AttributeType, &value, sizeof( int ) );
				}
			}
		}
	}
}
