//====== Copyright (c) 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DMXSERIALIZATIONDICTIONARY_H
#define DMXSERIALIZATIONDICTIONARY_H


#include <unordered_map>
#include <vector>


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmxElement;


//-----------------------------------------------------------------------------
// Element dictionary used in unserialization
//-----------------------------------------------------------------------------
typedef int DmxSerializationHandle_t;
enum
{
	DMX_SERIALIZATION_HANDLE_INVALID = (DmxSerializationHandle_t)~0
};


//-----------------------------------------------------------------------------
// Element dictionary used in serialization
//-----------------------------------------------------------------------------
class CDmxSerializationDictionary
{
public:
	CDmxSerializationDictionary( int nElementsHint = 0 );

	// Creates the list of all things to serialize
	void BuildElementList( CDmxElement *pRoot, bool bFlatMode );

	// Should I inline the serialization of this element?
	bool ShouldInlineElement( CDmxElement *pElement );

	// Clears the dictionary
	void Clear();

	// Iterates over all root elements to serialize
	DmxSerializationHandle_t FirstRootElement() const;
	DmxSerializationHandle_t NextRootElement( DmxSerializationHandle_t h ) const;
 	CDmxElement* GetRootElement( DmxSerializationHandle_t h );

	// Finds the handle of the element
	DmxSerializationHandle_t Find( CDmxElement *pElement );

	// How many root elements do we have?
	int RootElementCount() const;

private:
	struct DmxElementInfo_t
	{
		CDmxElement* m_pElement;
		bool m_bRoot;
	};

	// Creates the list of all things to serialize
	void BuildElementList_R( CDmxElement *pRoot, bool bFlatMode, bool bIsRoot );
	// Handles are insertion-order positions; binary DMX writes them as element indices.
	std::vector< DmxElementInfo_t > m_Dict;
	std::unordered_map< CDmxElement *, DmxSerializationHandle_t > m_Handles;
};


#endif // DMXSERIALIZATIONDICTIONARY_H
