#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2004 Valve Corporation

import ks.core.basetypes;
import ks.core.platform;
import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.byte_reader;
import ks.tier1.byte_writer;
import ks.tier1.commandbuffer;
import ks.tier1.intern_pool;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.serialization;
import ks.tier1.string_ops;
import ks.tier1.strtools;
import ks.tier1.text_reader;
import ks.tier1.text_writer;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
#include "datamodel/dmattributetypes.h"
import std;
#include "dmxloader/dmxloader.h"

//-----------------------------------------------------------------------------
// Forward declarations: 
//-----------------------------------------------------------------------------
class CDmxElement;





#define DECLARE_DMX_ATTRIBUTE_TYPE_INTERNAL( _className, _storageType, _attributeType, _attributeName, _defaultSetStatement ) \
	template< > class CDmAttributeInfo< _className >						\
	{																		\
	private:																\
		enum { ATTRIBUTE_TYPE = _attributeType };							\
		typedef _storageType StorageType_t;									\
		static DmAttributeType_t AttributeType() { return _attributeType; }	\
		static const char *AttributeTypeName() { return _attributeName; }	\
		static void SetDefaultValue( _className& value ) { _defaultSetStatement }	\
		friend class CDmxAttribute;											\
		friend class CDmxElement;											\
	};																		\

#define DECLARE_DMX_ATTRIBUTE_ARRAY_TYPE_INTERNAL( _className, _storageType, _attributeType, _attributeName ) \
	template< > class CDmAttributeInfo< std::vector< _className > >				\
	{																			\
	private:																	\
		enum { ATTRIBUTE_TYPE = _attributeType };								\
		typedef _storageType StorageType_t;										\
		static DmAttributeType_t AttributeType() { return _attributeType; }		\
		static const char *AttributeTypeName() { return _attributeName; }		\
		static void SetDefaultValue( std::vector< _className >& value ) { value.clear(); }	\
		friend class CDmxAttribute;												\
		friend class CDmxElement;												\
	};																			\

#define DECLARE_DMX_ATTRIBUTE_TYPE( _className, _attributeType, _attributeName, _defaultSetStatement ) \
	DECLARE_DMX_ATTRIBUTE_TYPE_INTERNAL( _className, _className, _attributeType, _attributeName, _defaultSetStatement )

#define DECLARE_DMX_ATTRIBUTE_ARRAY_TYPE( _className, _attributeType, _attributeName )\
	DECLARE_DMX_ATTRIBUTE_ARRAY_TYPE_INTERNAL( _className, std::vector< _className >, _attributeType, _attributeName )



//-----------------------------------------------------------------------------
// Attribute info, modified for use in mod code
//-----------------------------------------------------------------------------
DECLARE_DMX_ATTRIBUTE_TYPE( CDmxElement*,		AT_ELEMENT,				"element",		value = 0; )
DECLARE_DMX_ATTRIBUTE_ARRAY_TYPE( CDmxElement*,	AT_ELEMENT_ARRAY,		"element_array" )

DECLARE_DMX_ATTRIBUTE_TYPE( std::string,			AT_STRING,				"string",		value.assign( ks::nz( nullptr ) ); )
DECLARE_DMX_ATTRIBUTE_ARRAY_TYPE( std::string,	AT_STRING_ARRAY,		"string_array" )


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDmxAttribute
{
	DECLARE_DMX_ALLOCATOR( );

public:
	// Returns attribute name and type
	DmAttributeType_t GetType() const;
	const char *GetTypeString() const;
	template< class T > bool IsA() const;

	// Returns the name; the symbol is a view into s_AttributeNameSymbols (compare by data()).
	const char *GetName() const;
	std::string_view GetNameSymbol() const;
	void SetName( const char *pName );

	// Gets values
	template< class T >	const T& GetValue( ) const;
	template< class T > const std::vector< T >& GetArray( ) const;
	const char *GetValueString() const;

	// Sets values (+ type)
	template< class T > void SetValue( const T& value );
	void SetValue( const char *pString );
	void SetValue( char *pString );
	void SetValue( const void *pBuffer, std::size_t nLen );
	void SetValue( const CDmxAttribute *pAttribute );

	// Method to set values in an array (just directly operate on the array)
	// NOTE: This will create a new array of the appropriate type if 
	// the type doesn't match the current type
	template< class T > std::vector< T >& GetArrayForEdit();

	// Sets the attribute to its default value based on its type
	void	SetToDefaultValue();

	// Convert to and from string
	void SetValueFromString( const char *pValue );
	const char *GetValueAsString( char *pBuffer, std::size_t nBufLen ) const;

	// Gets the size of an array, returns 0 if it's not an array type
	int GetArrayCount() const;

	// Read from file
	bool Unserialize( DmAttributeType_t type, ks::byte_reader &buf );
	bool Unserialize( DmAttributeType_t type, ks::text_reader &buf );
	bool UnserializeElement( DmAttributeType_t type, ks::byte_reader &buf );
	bool UnserializeElement( DmAttributeType_t type, ks::text_reader &buf );
	bool Serialize( ks::byte_writer &buf ) const;
	bool Serialize( ks::text_writer &buf ) const;
	bool SerializeElement( int nIndex, ks::byte_writer &buf ) const;
	bool SerializeElement( int nIndex, ks::text_writer &buf ) const;
	bool SerializesOnMultipleLines() const;

	// Returns the size of the variables storing the various attribute types
	static int AttributeDataSize( DmAttributeType_t type );
	// Gets the basic type for a given array attribute type (e.g. AT_INT_ARRAY -> AT_INT)
	static DmAttributeType_t ArrayAttributeBasicType( DmAttributeType_t type );

private:
	CDmxAttribute( const char *pAttributeName );
	CDmxAttribute( std::string_view attributeName );
	~CDmxAttribute();

	// Allocate, free memory for data
	void AllocateDataMemory( DmAttributeType_t type );
	void AllocateDataMemory_AndConstruct( DmAttributeType_t type );
	void FreeDataMemory( );


	// Untyped methods for getting/setting used by unpack
	void SetValue( DmAttributeType_t type, const void *pSrc, int nLen );
	// NOTE: [Get|Set]ArrayValue don't currently support AT_STRING_ARRAY, AT_STRING_VOID or AT_ELEMENT_ARRAY
	void SetArrayValue( DmAttributeType_t type, const void *pSrc, int nDataTypeSize, int nArrayLength, int nSrcStride );
	void GetArrayValue( DmAttributeType_t type, void *pDest, int nDataTypeSize, int nArrayLength, const char *pDefaultString = nullptr ) const;
	void SetArrayCount( int nArrayCount );
	const void *GetArrayBase( void ) const;

	// Helper templated methods called from untyped methods (VT is vector datatype, T is basic datatype, VT will be the same as T if the attribute is non-array)
	template < class VT, class T > void ConstructDataMemory( void );
	template < class VT, class T > void DestructDataMemory( void );
	template < class VT, class T > void SetArrayCount( int nArrayCount );
	template < class VT, class T > void GetArrayCount( int &nArrayCount ) const;
	template < class VT, class T > void GetArrayBase( const void * &pBasePtr ) const;
	template < class VT, class T > void SerializesOnMultipleLines( bool &bResult ) const;
	template < class VT, class T, class Writer > void SerializeType( bool &bSuccess, Writer &buf ) const;
	template < class VT, class T, class Writer > void SerializeTypedElement( bool &bSuccess, int nIndex, Writer &buf ) const;
	template < class VT, class T, class Reader > void UnserializeType( bool &bSuccess, Reader &buf );
	template < class VT, class T, class Reader > void UnserializeTypedElement( bool &bSuccess, Reader &buf );
	template < class VT, class T > void SetDefaultValue( void );


	DmAttributeType_t m_Type;
	std::string_view m_Name;
	void *m_pData;

	static ks::intern_pool_mt s_AttributeNameSymbols;

	friend class CDmxElement;

public: 
	
	static const char *s_pAttributeTypeName[AT_TYPE_COUNT];

};


//-----------------------------------------------------------------------------
// Inline methods 
//-----------------------------------------------------------------------------
inline DmAttributeType_t CDmxAttribute::GetType() const
{
	return m_Type;
}

template< class T > inline bool CDmxAttribute::IsA() const
{
	return GetType() == CDmAttributeInfo< T >::ATTRIBUTE_TYPE;
}

inline std::string_view CDmxAttribute::GetNameSymbol() const
{
	return m_Name;
}


//-----------------------------------------------------------------------------
// Sets a value in the attribute
//-----------------------------------------------------------------------------
template< class T > void CDmxAttribute::SetValue( const T& value )
{
	AllocateDataMemory( CDmAttributeInfo<T>::AttributeType() );
	if ( !m_pData )
		return;
	CopyConstruct( (T*)m_pData, value );
}


//-----------------------------------------------------------------------------
// Returns data in the attribute
//-----------------------------------------------------------------------------
inline const char *CDmxAttribute::GetValueString() const
{
	if ( m_Type == AT_STRING )
		return ((std::string*)m_pData)->c_str();
	return "";
}

template< class T >
inline const T& CDmxAttribute::GetValue( ) const
{
	if ( CDmAttributeInfo<T>::AttributeType() == m_Type )
		return *(T*)m_pData;

	static T defaultValue;
	CDmAttributeInfo<T>::SetDefaultValue( defaultValue );
	return defaultValue;
}

template< class T > 
inline const std::vector< T >& CDmxAttribute::GetArray( ) const
{
	if ( CDmAttributeInfo< std::vector< T > >::AttributeType() == m_Type )
		return *( std::vector< T > *)m_pData;

	static std::vector< T > defaultArray;
	return defaultArray;
}

template< class T > 
inline std::vector< T >& CDmxAttribute::GetArrayForEdit( )
{
	if ( CDmAttributeInfo< std::vector< T > >::AttributeType() == m_Type )
		return *( std::vector< T > *)m_pData;

	AllocateDataMemory( CDmAttributeInfo< std::vector< T > >::AttributeType() );
	Construct( (std::vector< T >*)m_pData );
	return *(std::vector< T > *)m_pData;
}
