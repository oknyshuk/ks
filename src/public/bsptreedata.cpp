// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: The BSP tree leaf data system

import std;
import ks.tier1;
#include "bsptreedata.h"

//-----------------------------------------------------------------------------
// The BSP tree leaf data system
//-----------------------------------------------------------------------------
class CBSPTreeData : public IBSPTreeData, public ISpatialLeafEnumerator
{
public:
	// constructor, destructor
	CBSPTreeData();
	virtual ~CBSPTreeData();

	// Methods of IBSPTreeData
	void Init( ISpatialQuery* pBSPTree );
	void Shutdown();

	BSPTreeDataHandle_t Insert( int userId, Vector const& mins, Vector const& maxs );
	void Remove( BSPTreeDataHandle_t handle );
	void ElementMoved( BSPTreeDataHandle_t handle, Vector const& mins, Vector const& maxs );

	// Enumerate elements in a particular leaf
	bool EnumerateElementsInLeaf( int leaf, IBSPTreeDataEnumerator* pEnum, intp context );

	// For convenience, enumerates the leaves along a ray, box, etc.
	bool EnumerateLeavesAtPoint( Vector const& pt, ISpatialLeafEnumerator* pEnum, intp context );
	bool EnumerateLeavesInBox( Vector const& mins, Vector const& maxs, ISpatialLeafEnumerator* pEnum, intp context );
	bool EnumerateLeavesInSphere( Vector const& center, float radius, ISpatialLeafEnumerator* pEnum, intp context );
	bool EnumerateLeavesAlongRay( Ray_t const& ray, ISpatialLeafEnumerator* pEnum, intp context );

	// methods of IBSPLeafEnumerator
	bool EnumerateLeaf( int leaf, intp context );

	// Is the element in any leaves at all?
	bool IsElementInTree( BSPTreeDataHandle_t handle ) const;

private:
	// Creates a new handle
	BSPTreeDataHandle_t NewHandle( int userId );

	// Adds a handle to the list of handles
	void AddHandleToLeaf( int leaf, BSPTreeDataHandle_t handle );

	// insert, remove handles from leaves
	void InsertIntoTree( BSPTreeDataHandle_t handle, Vector const& mins, Vector const& maxs );
	void RemoveFromTree( BSPTreeDataHandle_t handle );

	bool IsValidHandle( BSPTreeDataHandle_t handle ) const
	{
		return handle < m_Handles.size() && m_Handles[handle].m_bInUse;
	}

private:
	// All the information associated with a particular handle
	struct HandleInfo_t
	{
		int					m_UserId;		// Client-defined id
		bool				m_bInUse;
		std::vector< int >	m_Leaves;		// What leafs is it in?
	};

	// The leaf keeps the handles it contains; newest last
	struct Leaf_t
	{
		std::vector< BSPTreeDataHandle_t >	m_Elements;
	};

	// Stores data associated with each leaf.
	std::vector< Leaf_t >	m_Leaf;

	// Handle slots; handles are indices, freed slots are reused
	std::vector< HandleInfo_t >	m_Handles;
	std::vector< BSPTreeDataHandle_t >	m_FreeHandles;

	// Interface to BSP tree
	ISpatialQuery*	m_pBSPTree;
};


//-----------------------------------------------------------------------------
// Class factory
//-----------------------------------------------------------------------------

IBSPTreeData* CreateBSPTreeData()
{
	return new CBSPTreeData;
}

void DestroyBSPTreeData( IBSPTreeData* pTreeData )
{
	if (pTreeData)
		delete pTreeData;
}


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------

CBSPTreeData::CBSPTreeData()
{
}

CBSPTreeData::~CBSPTreeData()
{
}


//-----------------------------------------------------------------------------
// Level init, shutdown
//-----------------------------------------------------------------------------

void CBSPTreeData::Init( ISpatialQuery* pBSPTree )
{
	contract_assert( pBSPTree );
	m_pBSPTree = pBSPTree;

	m_Handles.reserve( 1024 );

	// Add all the leaves we'll need
	m_Leaf.resize( m_pBSPTree->LeafCount() );
}

void CBSPTreeData::Shutdown()
{
	m_Handles.clear(); m_Handles.shrink_to_fit();
	m_FreeHandles.clear(); m_FreeHandles.shrink_to_fit();
	m_Leaf.clear(); m_Leaf.shrink_to_fit();
}


//-----------------------------------------------------------------------------
// Creates a new handle
//-----------------------------------------------------------------------------

BSPTreeDataHandle_t CBSPTreeData::NewHandle( int userId )
{
	BSPTreeDataHandle_t handle;
	if ( !m_FreeHandles.empty() )
	{
		handle = m_FreeHandles.back();
		m_FreeHandles.pop_back();
	}
	else
	{
		handle = (BSPTreeDataHandle_t)m_Handles.size();
		m_Handles.emplace_back();
	}
	m_Handles[handle].m_UserId = userId;
	m_Handles[handle].m_bInUse = true;
	contract_assert( m_Handles[handle].m_Leaves.empty() );

	return handle;
}

//-----------------------------------------------------------------------------
// Add/remove handle
//-----------------------------------------------------------------------------

BSPTreeDataHandle_t CBSPTreeData::Insert( int userId, Vector const& mins, Vector const& maxs )
{
	BSPTreeDataHandle_t handle = NewHandle( userId );
	InsertIntoTree( handle, mins, maxs );
	return handle;
}

void CBSPTreeData::Remove( BSPTreeDataHandle_t handle )
{
	if (!IsValidHandle(handle))
		return;

	RemoveFromTree( handle );
	m_Handles[handle].m_bInUse = false;
	m_FreeHandles.push_back( handle );
}


//-----------------------------------------------------------------------------
// Adds a handle to a leaf
//-----------------------------------------------------------------------------
void CBSPTreeData::AddHandleToLeaf( int leaf, BSPTreeDataHandle_t handle )
{	
	// Got to a leaf baby! Add the handle to the leaf's list of elements
	m_Leaf[leaf].m_Elements.push_back( handle );

	// Insert the leaf into the handles's list of leaves
	m_Handles[handle].m_Leaves.push_back( leaf );
}


//-----------------------------------------------------------------------------
// Inserts an element into the tree
//-----------------------------------------------------------------------------
bool CBSPTreeData::EnumerateLeaf( int leaf, intp context )
{
	BSPTreeDataHandle_t handle = (BSPTreeDataHandle_t)context;
	AddHandleToLeaf( leaf, handle );
	return true;
}

void CBSPTreeData::InsertIntoTree( BSPTreeDataHandle_t handle, Vector const& mins, Vector const& maxs )
{
	m_pBSPTree->EnumerateLeavesInBox( mins, maxs, this, handle );
}

//-----------------------------------------------------------------------------
// Removes an element from the tree
//-----------------------------------------------------------------------------

void CBSPTreeData::RemoveFromTree( BSPTreeDataHandle_t handle )
{
	// Iterate over the list of all leaves the handle is in
	for ( int leaf : m_Handles[handle].m_Leaves )
	{
		// Unhook the handle from the leaf handle list, keeping the others in order
		std::vector< BSPTreeDataHandle_t > &elements = m_Leaf[leaf].m_Elements;
		elements.erase( std::find( elements.begin(), elements.end(), handle ) );
	}

	m_Handles[handle].m_Leaves.clear();
}


//-----------------------------------------------------------------------------
// Call this when the element moves
//-----------------------------------------------------------------------------
void CBSPTreeData::ElementMoved( BSPTreeDataHandle_t handle, Vector const& mins, Vector const& maxs )
{
	if (handle != TREEDATA_INVALID_HANDLE)
	{
		RemoveFromTree( handle );
		InsertIntoTree( handle, mins, maxs );
	}
}


//-----------------------------------------------------------------------------
// Is the element in any leaves at all?
//-----------------------------------------------------------------------------
bool CBSPTreeData::IsElementInTree( BSPTreeDataHandle_t handle ) const
{
	return !m_Handles[handle].m_Leaves.empty();
}


//-----------------------------------------------------------------------------
// Enumerate elements in a particular leaf
//-----------------------------------------------------------------------------
bool CBSPTreeData::EnumerateElementsInLeaf( int leaf, IBSPTreeDataEnumerator* pEnum, intp context )
{
	const std::vector< BSPTreeDataHandle_t > &elements = m_Leaf[leaf].m_Elements;

	// Newest first
	for ( std::size_t i = elements.size(); i-- > 0; )
	{
		if (!pEnum->EnumerateElement( m_Handles[elements[i]].m_UserId, context ))
		{
			return false;
		}
	}


	return true;
}


//-----------------------------------------------------------------------------
// For convenience, enumerates the leaves along a ray, box, etc.
//-----------------------------------------------------------------------------
bool CBSPTreeData::EnumerateLeavesAtPoint( Vector const& pt, ISpatialLeafEnumerator* pEnum, intp context )
{
	return m_pBSPTree->EnumerateLeavesAtPoint( pt, pEnum, context );
}

bool CBSPTreeData::EnumerateLeavesInBox( Vector const& mins, Vector const& maxs, ISpatialLeafEnumerator* pEnum, intp context )
{
	return m_pBSPTree->EnumerateLeavesInBox( mins, maxs, pEnum, context );
}

bool CBSPTreeData::EnumerateLeavesInSphere( Vector const& center, float radius, ISpatialLeafEnumerator* pEnum, intp context )
{
	return m_pBSPTree->EnumerateLeavesInSphere( center, radius, pEnum, context );
}

bool CBSPTreeData::EnumerateLeavesAlongRay( Ray_t const& ray, ISpatialLeafEnumerator* pEnum, intp context )
{
	return m_pBSPTree->EnumerateLeavesAlongRay( ray, pEnum, context );
}
