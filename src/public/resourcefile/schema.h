#pragma once

import std;
import ks.core;

#include "resourcefile/resourcestream.h"
#include "resourcefile/resourcefile.h"
#include "resourcefile/resourcetype.h"

class CResourceStructIntrospection;


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Pay no attention to the code behind the curtain

#define DECLARE_SCHEMA_CLASS_HELPER( _className, _bindingType ) \
	friend class _bindingType < _className >; \
	friend class CSchemaVerificationFor##_className; \
public: \
	static _bindingType < _className > s_##_className##SchemaBinding; \
	static const CResourceStructIntrospection *Schema_StaticBinding( ) { return (s_##_className##SchemaBinding).GetIntrospection(); }

#define DECLARE_SCHEMA_VIRTUAL_CLASS_HELPER( _className, _bindingType ) \
	DECLARE_SCHEMA_CLASS_HELPER( _className, _bindingType ); \
public: \
	virtual const CSchemaClassBindingBase *Schema_GetBinding( ) const { return &s_##_className##SchemaBinding; } \
	const CResourceStructIntrospection *Schema_GetIntrospection( ) const { return Schema_GetBinding()->GetIntrospection(); }

#define DECLARE_SCHEMA_PLAIN_CLASS_HELPER( _className, _bindingType ) \
	DECLARE_SCHEMA_CLASS_HELPER( _className, _bindingType ); \
public: \
	const CSchemaClassBindingBase *Schema_GetBinding( ) const { return &s_##_className##SchemaBinding; } \
	const CResourceStructIntrospection *Schema_GetIntrospection( ) const { return Schema_GetBinding()->GetIntrospection(); }

#define DEFINE_SCHEMA_CLASS_HELPER( _className, _bindingType ) \
	_bindingType < _className > _className :: s_##_className##SchemaBinding( #_className );

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Data Classes (no vtable)

#define DECLARE_SCHEMA_DATA_CLASS( _className ) \
	 \
	DECLARE_SCHEMA_PLAIN_CLASS_HELPER( _className, CSchemaClassBinding )

#define DEFINE_SCHEMA_DATA_CLASS( _className ) DEFINE_SCHEMA_CLASS_HELPER( _className, CSchemaClassBinding )

///////////////////////////////////////

// Virtual Classes

#define DECLARE_SCHEMA_VIRTUAL_CLASS( _className ) \
	 \
	DECLARE_SCHEMA_VIRTUAL_CLASS_HELPER( _className, CSchemaClassBinding )

#define DEFINE_SCHEMA_VIRTUAL_CLASS( _className ) DEFINE_SCHEMA_CLASS_HELPER( _className, CSchemaClassBinding )

///////////////////////////////////////

// Abstract Classes

#define DECLARE_SCHEMA_ABSTRACT_CLASS( _className ) \
	 \
	DECLARE_SCHEMA_VIRTUAL_CLASS_HELPER( _className, CSchemaAbstractClassBinding )

#define DEFINE_SCHEMA_ABSTRACT_CLASS( _className ) DEFINE_SCHEMA_CLASS_HELPER( _className, CSchemaAbstractClassBinding )

///////////////////////////////////////

// Simple Classes

// For classes where you don't want to use a DEFINE_ macro
// - Only works for classes with no vtable.
// - If unserialized, it will be memzeroed rather than constructed

#define DECLARE_SCHEMA_SIMPLE_CLASS( _name ) \
	 \
	const CResourceStructIntrospection *Schema_GetIntrospection( ) const { return g_pResourceSystem->FindStructIntrospection( #_name ); }

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/*
template<class T> void Serialize( const T* pObj )
{
	const CResourceStructIntrospection *pIntrospection = pObj ? pObj->Schema_GetIntrospection() : NULL;
	SerializeGeneric( (const void*)pObj, pIntrospection );
}

template<class T> void Print( const T* pObj )
{
	const CResourceStructIntrospection *pIntrospection = pObj ? pObj->Schema_GetIntrospection() : NULL;
	PrintGeneric( (const void*)pObj, pIntrospection );
}

void SerializeGeneric( const void *pData, const CResourceStructIntrospection *pIntrospection );
void PrintGeneric( const void *pData, const CResourceStructIntrospection *pIntrospection );
*/

//////////////////////////////////////////////////////////////////////////

class CSchemaClassBindingBase
{
public:
	CSchemaClassBindingBase( const char* pClassName ):
		m_pClassName(pClassName),
		m_pIntrospection(nullptr)
	{
		// Hook into the local class binding list
		m_pNextBinding = sm_pClassBindingList;
		sm_pClassBindingList = this; 
	}

	inline const char* GetName() const
	{
		return m_pClassName;
	}

	virtual void ConstructInPlace( void* pMemory ) const = 0;
	virtual void DestructInPlace( void* pMemory ) const = 0;
	virtual int GetSize() const = 0;

	const CResourceStructIntrospection *GetIntrospection() const;
	static void Install();

protected:
	const char *m_pClassName;
	mutable const CResourceStructIntrospection *m_pIntrospection;
	CSchemaClassBindingBase *m_pNextBinding;

	static CSchemaClassBindingBase *sm_pClassBindingList;
};

template<class TSchemaClass> class CSchemaClassBinding: public CSchemaClassBindingBase
{
public:
	CSchemaClassBinding( const char* pClassName ):
		CSchemaClassBindingBase( pClassName )
	{
		// nop
	}

	virtual void ConstructInPlace( void* pMemory ) const
	{
		new(pMemory) TSchemaClass;
	}

	virtual void DestructInPlace( void* pMemory ) const
	{
		((TSchemaClass*)(pMemory))->~TSchemaClass();
	}

	virtual int GetSize() const
	{
		return sizeof(TSchemaClass);
	}
};

template<class TSchemaClass> class CSchemaAbstractClassBinding: public CSchemaClassBindingBase
{
public:
	CSchemaAbstractClassBinding( const char* pClassName ):
		CSchemaClassBindingBase(pClassName)
	{
		// nop
	}

	virtual void ConstructInPlace( void* pMemory ) const
	{
		Error( "Cannot construct abstract class %s\n", m_pClassName );
	}

	virtual void DestructInPlace( void* pMemory ) const
	{
		Error( "Cannot destruct abstract class %s\n", m_pClassName );
	}

	virtual int GetSize() const
	{
		return sizeof(TSchemaClass);
	}
};

