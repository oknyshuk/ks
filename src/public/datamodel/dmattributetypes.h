#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2004 Valve Corporation

import ks.core.basetypes;
import ks.core.color;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.math.vector2d;
import ks.math.vector4d;
import ks.math.vmatrix;
import ks.platform;
import ks.simd;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
#include "datamodel/dmelementhandle.h"
import std;

//-----------------------------------------------------------------------------
// Object Id
//-----------------------------------------------------------------------------
typedef UniqueId_t DmObjectId_t;


//-----------------------------------------------------------------------------
// Necessary for template specialization for AT_UNKNOWN
//-----------------------------------------------------------------------------
struct DmUnknownAttribute_t
{
	bool operator==( const DmUnknownAttribute_t& src ) const { return true; }
};


//-----------------------------------------------------------------------------
// Element array
//-----------------------------------------------------------------------------
struct DmElementAttribute_t
{
	DmElementAttribute_t() {}

	operator DmElementHandle_t&() { return m_Handle; }
	operator const DmElementHandle_t&() const { return m_Handle; }

	DmElementHandle_t m_Handle;
	std::string_view m_ElementType;	// pooled type name; null when untyped
};

struct DmElementArray_t : public std::vector< DmElementHandle_t >
{
	std::string_view m_ElementType;	// pooled type name; null when untyped
};


//-----------------------------------------------------------------------------
// Attribute chunk type enum
//-----------------------------------------------------------------------------
enum DmAttributeType_t
{
	AT_UNKNOWN = 0,

	AT_FIRST_VALUE_TYPE,

	AT_ELEMENT = AT_FIRST_VALUE_TYPE,
	AT_INT,
	AT_FLOAT,
	AT_BOOL,
	AT_STRING,
	AT_VOID,
	AT_TIME,
	AT_COLOR, //rgba
	AT_VECTOR2,
	AT_VECTOR3,
	AT_VECTOR4,
	AT_QANGLE,
	AT_QUATERNION,
	AT_VMATRIX,

	AT_FIRST_ARRAY_TYPE,

	AT_ELEMENT_ARRAY = AT_FIRST_ARRAY_TYPE,
	AT_INT_ARRAY,
	AT_FLOAT_ARRAY,
	AT_BOOL_ARRAY,
	AT_STRING_ARRAY,
	AT_VOID_ARRAY,
	AT_TIME_ARRAY,
	AT_COLOR_ARRAY,
	AT_VECTOR2_ARRAY,
	AT_VECTOR3_ARRAY,
	AT_VECTOR4_ARRAY,
	AT_QANGLE_ARRAY,
	AT_QUATERNION_ARRAY,
	AT_VMATRIX_ARRAY,
	AT_TYPE_COUNT,

	AT_TYPE_INVALID,
};

const char *GetTypeString( DmAttributeType_t type );

inline bool IsValueType( DmAttributeType_t type )
{
	return type >= AT_FIRST_VALUE_TYPE && type < AT_FIRST_ARRAY_TYPE;
}

inline bool IsArrayType( DmAttributeType_t type )
{
	return type >= AT_FIRST_ARRAY_TYPE && type < AT_TYPE_COUNT;
}

inline bool IsTopological( DmAttributeType_t type )
{
	return type == AT_ELEMENT || type == AT_ELEMENT_ARRAY;
}

inline DmAttributeType_t ValueTypeToArrayType( DmAttributeType_t type )
{
	contract_assert( IsValueType( type ) );
	return ( DmAttributeType_t )( ( type - AT_FIRST_VALUE_TYPE ) + AT_FIRST_ARRAY_TYPE );
}

inline DmAttributeType_t ArrayTypeToValueType( DmAttributeType_t type )
{
	contract_assert( IsArrayType( type ) );
	return ( DmAttributeType_t )( ( type - AT_FIRST_ARRAY_TYPE ) + AT_FIRST_VALUE_TYPE );
}

inline int NumComponents( DmAttributeType_t type )
{
	switch ( type )
	{
	case AT_BOOL:
	case AT_INT:
	case AT_FLOAT:
	case AT_TIME:
		return 1;

	case AT_VECTOR2:
		return 2;

	case AT_VECTOR3:
	case AT_QANGLE:
		return 3;

	case AT_COLOR: //rgba
	case AT_VECTOR4:
	case AT_QUATERNION:
		return 4;

	case AT_VMATRIX:
		return 16;

	case AT_ELEMENT:
	case AT_STRING:
	case AT_VOID:
	default:
		return 0;
	}
}

template< typename T >
inline float GetComponent( const T &value, int i )
{
	contract_assert( 0 );
	return 0.0f;
}

template <> inline float GetComponent( const bool &value, int i )
{
	contract_assert( i == 0 );
	return value ? 1.0f : 0.0f;
}

template <> inline float GetComponent( const int &value, int i )
{
	contract_assert( i == 0 );
	return float( value );
}

template <> inline float GetComponent( const float &value, int i )
{
	contract_assert( i == 0 );
	return value;
}

template <> inline float GetComponent( const DmeTime_t &value, int i )
{
	contract_assert( i == 0 );
	return value.GetSeconds();
}

template <> inline float GetComponent( const Vector2D &value, int i )
{
	return value[ i ];
}

template <> inline float GetComponent( const Vector &value, int i )
{
	return value[ i ];
}

template <> inline float GetComponent( const QAngle &value, int i )
{
	return value[ i ];
}

template <> inline float GetComponent( const Color &value, int i )
{
	return value[ i ];
}

template <> inline float GetComponent( const Vector4D &value, int i )
{
	return value[ i ];
}

template <> inline float GetComponent( const Quaternion &value, int i )
{
	return value[ i ];
}

template <> inline float GetComponent( const VMatrix &value, int i )
{
	return value.Base()[ i ];
}


//-----------------------------------------------------------------------------
// Attribute info... 
//-----------------------------------------------------------------------------

template <typename T>
class CDmAttributeInfo
{
private:
	enum { ATTRIBUTE_TYPE = AT_TYPE_INVALID };

	typedef T StorageType_t;
	friend class CDmxAttribute;

	static DmAttributeType_t AttributeType()
	{
		return AT_TYPE_INVALID;
	}

	static const char *AttributeTypeName()
	{
		return "invalid";
	}

	static void SetDefaultValue( T& value )
	{
		contract_assert( 0 );
	}
};


template <>
class CDmAttributeInfo< DmUnknownAttribute_t >
{
public:
	enum { ATTRIBUTE_TYPE = AT_UNKNOWN };

	typedef DmUnknownAttribute_t StorageType_t;

	static DmAttributeType_t AttributeType()
	{
		return AT_UNKNOWN;
	}

	static const char *AttributeTypeName()
	{
		return "unknown";
	}

	static void SetDefaultValue( DmUnknownAttribute_t& value )
	{
		contract_assert( 0 );
	}
};


#define DECLARE_ATTRIBUTE_TYPE_INTERNAL( _className, _storageType, _attributeType, _attributeName, _defaultSetStatement ) \
	template< > class CDmAttributeInfo< _className >						\
	{																		\
	public:																	\
		enum { ATTRIBUTE_TYPE = _attributeType };							\
		typedef _storageType StorageType_t;									\
		static DmAttributeType_t AttributeType() { return _attributeType; }	\
		static const char *AttributeTypeName() { return _attributeName; }	\
		static void SetDefaultValue( _className& value ) { _defaultSetStatement }	\
	};																		\

#define DECLARE_ATTRIBUTE_ARRAY_TYPE_INTERNAL( _className, _storageType, _attributeType, _attributeName ) \
	template< > class CDmAttributeInfo< std::vector< _className > >				\
	{																			\
	public:																		\
		enum { ATTRIBUTE_TYPE = _attributeType };								\
		typedef _storageType StorageType_t;										\
		static DmAttributeType_t AttributeType() { return _attributeType; }		\
		static const char *AttributeTypeName() { return _attributeName; }		\
		static void SetDefaultValue( std::vector< _className >& value ) { value.clear(); }	\
	};																			\

#define DECLARE_ATTRIBUTE_TYPE( _className, _attributeType, _attributeName, _defaultSetStatement ) \
	DECLARE_ATTRIBUTE_TYPE_INTERNAL( _className, _className, _attributeType, _attributeName, _defaultSetStatement )

#define DECLARE_ATTRIBUTE_ARRAY_TYPE( _className, _attributeType, _attributeName )\
	DECLARE_ATTRIBUTE_ARRAY_TYPE_INTERNAL( _className, std::vector< _className >, _attributeType, _attributeName )

// NOTE: If you add an attribute type here, also add it to the list of DEFINE_ATTRIBUTE_TYPES in dmattribute.cpp
DECLARE_ATTRIBUTE_TYPE( int,					AT_INT,					"int",			value = 0; )
DECLARE_ATTRIBUTE_TYPE( float,					AT_FLOAT,				"float",		value = 0.0f; )
DECLARE_ATTRIBUTE_TYPE( bool,					AT_BOOL,				"bool",			value = false; )
DECLARE_ATTRIBUTE_TYPE( Color,					AT_COLOR,				"color",		value.SetColor( 0, 0, 0, 255 ); )
DECLARE_ATTRIBUTE_TYPE( Vector2D,				AT_VECTOR2,				"vector2",		value.Init( 0.0f, 0.0f ); )
DECLARE_ATTRIBUTE_TYPE( Vector,					AT_VECTOR3,				"vector3",		value.Init( 0.0f, 0.0f, 0.0f ); )
DECLARE_ATTRIBUTE_TYPE( Vector4D,				AT_VECTOR4,				"vector4",		value.Init( 0.0f, 0.0f, 0.0f, 0.0f ); )
DECLARE_ATTRIBUTE_TYPE( QAngle,					AT_QANGLE,				"qangle",		value.Init( 0.0f, 0.0f, 0.0f ); )
DECLARE_ATTRIBUTE_TYPE( Quaternion,				AT_QUATERNION,			"quaternion",	value.Init( 0.0f, 0.0f, 0.0f, 1.0f ); )
DECLARE_ATTRIBUTE_TYPE( VMatrix,				AT_VMATRIX,				"matrix",		MatrixSetIdentity( value ); )
// AT_STRING and AT_STRING_ARRAY are std::string, declared in dmxloader/dmxattribute.h.
DECLARE_ATTRIBUTE_TYPE( std::vector< unsigned char >,	AT_VOID,				"binary",		value.clear(); )
DECLARE_ATTRIBUTE_TYPE( DmeTime_t,				AT_TIME,				"time",			value.SetTenthsOfMS( 0 ); )
DECLARE_ATTRIBUTE_TYPE_INTERNAL( DmElementHandle_t, DmElementAttribute_t, AT_ELEMENT,	"element", value = DMELEMENT_HANDLE_INVALID; )

DECLARE_ATTRIBUTE_ARRAY_TYPE( int,				AT_INT_ARRAY,			"int_array" )
DECLARE_ATTRIBUTE_ARRAY_TYPE( float,			AT_FLOAT_ARRAY,			"float_array" )
DECLARE_ATTRIBUTE_ARRAY_TYPE( bool,				AT_BOOL_ARRAY,			"bool_array" )
DECLARE_ATTRIBUTE_ARRAY_TYPE( Color,			AT_COLOR_ARRAY,			"color_array" )
DECLARE_ATTRIBUTE_ARRAY_TYPE( Vector2D,			AT_VECTOR2_ARRAY,		"vector2_array" )
DECLARE_ATTRIBUTE_ARRAY_TYPE( Vector,			AT_VECTOR3_ARRAY,		"vector3_array" )
DECLARE_ATTRIBUTE_ARRAY_TYPE( Vector4D,			AT_VECTOR4_ARRAY,		"vector4_array" )
DECLARE_ATTRIBUTE_ARRAY_TYPE( QAngle,			AT_QANGLE_ARRAY,		"qangle_array" )
DECLARE_ATTRIBUTE_ARRAY_TYPE( Quaternion,		AT_QUATERNION_ARRAY,	"quaternion_array" )
DECLARE_ATTRIBUTE_ARRAY_TYPE( VMatrix,			AT_VMATRIX_ARRAY,		"matrix_array" )
DECLARE_ATTRIBUTE_ARRAY_TYPE( std::vector< unsigned char >,	AT_VOID_ARRAY,			"binary_array" )
DECLARE_ATTRIBUTE_ARRAY_TYPE( DmeTime_t,		AT_TIME_ARRAY,			"time_array" )
DECLARE_ATTRIBUTE_ARRAY_TYPE_INTERNAL( DmElementHandle_t, DmElementArray_t,	AT_ELEMENT_ARRAY, "element_array" )
