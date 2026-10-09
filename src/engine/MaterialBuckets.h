#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: This is a helper class for the situation where you want to build lists of
// things that fall into buckets, and effeciently keep up with which buckets have
// been used since flush.

import std;

// FLASHLIGHTFIXME: Make all of the buckets share the same m_Elements (ie. make m_Elements static)

template <class Element_t>
class CMaterialsBuckets
{
public:
	typedef int ElementHandle_t;
	CMaterialsBuckets()
	{
		m_FlushCount = -1;
	}

	// Set the number of buckets that are needed.  This should get called every time 
	// a level is loaded.
	void SetNumMaterialSortIDs( int n )
	{
		m_MaterialSortInfoArray.clear(); m_MaterialSortInfoArray.shrink_to_fit();
		m_MaterialSortInfoArray.assign( n, {} );
		m_Elements.clear(); m_Elements.shrink_to_fit();

		m_UsedSortIDs.clear(); m_UsedSortIDs.shrink_to_fit();
	}

	// Clear out all buckets.  This should get called once a frame.
	void Flush( void )
	{
		m_FlushCount++;
		m_Elements.clear();
		m_UsedSortIDs.clear();
	}

	// The buckets used since flush, in first-use order.
	const std::vector<unsigned short> &UsedSortIDs() const
	{
		return m_UsedSortIDs;
	}


	//
	// These functions are used to get at the list of elements for each sortID.
	//
	ElementHandle_t GetElementListHead( int sortID )
	{
		return m_MaterialSortInfoArray[sortID].m_Head;
	}

	ElementHandle_t GetElementListNext( ElementHandle_t h )
	{
		return m_Elements[h].m_Next;
	}

	Element_t GetElement( ElementHandle_t h )
	{
		return m_Elements[h].m_Element;
	}

	ElementHandle_t InvalidElementHandle()
	{
		return -1;
	}


	// Add an element to the the bucket specified by sortID
	void AddElement( int sortID, Element_t elem )
	{
		ElementHandle_t elemID = std::ssize( m_Elements );
		
		if( m_MaterialSortInfoArray[sortID].m_FlushCount != m_FlushCount )
		{
			// This is the first element that has used this sort id since flush.
			// FLASHLIGHTFIXME: need to sort these by vertex format when shoving
			// them into this list!

			// Mark this sortID as having been used since the last flush.
			m_MaterialSortInfoArray[sortID].m_FlushCount = m_FlushCount;

			// Add this sortID to the list of sortIDs used since flush.
			m_UsedSortIDs.push_back( sortID );

			m_Elements.push_back( { elem, InvalidElementHandle() } );
		}
		else
		{
			// We already have an element in this sort id since flush, so chain
			// into thelist of elements for this sort id.
			m_Elements.push_back( { elem, m_MaterialSortInfoArray[sortID].m_Head } );
		}
		// Set the head pointer for this sort id to this element.
		m_MaterialSortInfoArray[sortID].m_Head = elemID;
	}

private:
	
	struct MaterialSortInfo_t
	{
		MaterialSortInfo_t() :
			m_FlushCount( -1 ),
			m_Head( -1 )  // i.e., InvalidElementHandle()
		{
		}
		
		int m_FlushCount;
		ElementHandle_t m_Head;
	};

	struct Element_s
	{
		Element_t m_Element;
		ElementHandle_t m_Next;	// next element in the same sortID
	};
	
	// This is a list of material sort info ids that have been used since flush.
	std::vector<unsigned short> m_UsedSortIDs;

	// This is m_NumMaterialSortIDs big.
	std::vector< MaterialSortInfo_t > m_MaterialSortInfoArray;
	
	// Pool of per-sortID singly linked chains, reset on flush.
	std::vector<Element_s> m_Elements;

	int m_FlushCount;
		
};
