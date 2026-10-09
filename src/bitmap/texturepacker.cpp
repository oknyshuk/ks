// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "bitmap/texturepacker.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTexturePacker::CTexturePacker( int texWidth, int texHeight, int pixelGap ) 
{
	m_PageWidth = texWidth;
	m_PageHeight = texHeight;
	m_PixelGap = pixelGap;

	Clear();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTexturePacker::~CTexturePacker() = default;

//-----------------------------------------------------------------------------
// Node allocation; children are always linked as child[0] (left) then child[1] (right)
//-----------------------------------------------------------------------------
int CTexturePacker::AllocNode( int parentIndex )
{
	int nodeIndex;
	if ( m_FreeNodes.empty() )
	{
		nodeIndex = (int)m_Nodes.size();
		m_Nodes.emplace_back();
	}
	else
	{
		nodeIndex = m_FreeNodes.back();
		m_FreeNodes.pop_back();
	}
	Node_t &node = m_Nodes[nodeIndex];
	node.entry = {};
	node.parent = parentIndex;
	node.child[0] = node.child[1] = -1;
	node.bInTree = true;
	if ( parentIndex >= 0 )
	{
		Node_t &parent = m_Nodes[parentIndex];
		parent.child[ parent.child[0] < 0 ? 0 : 1 ] = nodeIndex;
	}
	return nodeIndex;
}

void CTexturePacker::FreeLeaf( int nodeIndex )
{
	Node_t &node = m_Nodes[nodeIndex];
	contract_assert( node.bInTree && node.child[0] < 0 );
	if ( node.parent >= 0 )
	{
		Node_t &parent = m_Nodes[node.parent];
		if ( parent.child[0] == nodeIndex )
		{
			parent.child[0] = parent.child[1];
			parent.child[1] = -1;
		}
		else
		{
			contract_assert( parent.child[1] == nodeIndex );
			parent.child[1] = -1;
		}
	}
	node.bInTree = false;
	m_FreeNodes.push_back( nodeIndex );
}

//-----------------------------------------------------------------------------
// Purpose: Resets the tree
//----------------------------------------------------------------------------- 
void CTexturePacker::Clear()
{ 
	// remove all existing data
	m_Nodes.clear();
	m_FreeNodes.clear();

	// Add root item, everything else is procedurally generated
	m_Root = AllocNode( -1 );
	TreeEntry_t &topNode = m_Nodes[m_Root].entry;
	topNode.rc.x = 0;
	topNode.rc.y = 0;
	topNode.rc.width = m_PageWidth;
	topNode.rc.height = m_PageHeight;
	topNode.bInUse = false;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CTexturePacker::IsLeaf( int nodeIndex )
{
	return m_Nodes[nodeIndex].child[0] < 0 || m_Nodes[nodeIndex].child[1] < 0;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CTexturePacker::IsLeftChild( int nodeIndexParent, int nodeIndexChild ) const
{
	return m_Nodes[nodeIndexParent].child[0] == nodeIndexChild;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CTexturePacker::IsRightChild( int nodeIndexParent, int nodeIndexChild ) const
{
	return !IsLeftChild( nodeIndexParent, nodeIndexChild );
}



//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CTexturePacker::InsertRect( const Rect_t& texRect, int nodeIndex )
{
	if ( nodeIndex == -1 ) 
	{
		nodeIndex = m_Root;
		contract_assert( nodeIndex >= 0 );
	}

    if ( !IsLeaf( nodeIndex) ) // not a leaf
	{
        // try inserting under left child
		int leftChildIndex = m_Nodes[nodeIndex].child[0];
		int newIndex = InsertRect( texRect, leftChildIndex );
		if ( newIndex >= 0 )
		{
			return newIndex;
		} 

        // no room, insert under right child
		int rightChildIndex = m_Nodes[nodeIndex].child[1];
        return InsertRect( texRect, rightChildIndex );
	}
    else
	{
		if ( m_Nodes[nodeIndex].entry.bInUse ) // there is already a glpyh here
		{
			return -1;
		}

		int cacheSlotWidth = m_Nodes[nodeIndex].entry.rc.width;
		int cacheSlotHeight = m_Nodes[nodeIndex].entry.rc.height;

		if ( ( texRect.width > cacheSlotWidth ) || 
			 ( texRect.height > cacheSlotHeight ) )
		{
			// if this node's box is too small, return
			return -1;
		}

		
		if ( ( texRect.width == cacheSlotWidth) && 
			 ( texRect.height == cacheSlotHeight ) )
		{
			// if we're just right, accept
			m_Nodes[nodeIndex].entry.bInUse = true;
			return nodeIndex; 
		}
        
        // otherwise, gotta split this node and create some kids  
        // decide which way to split
        int dw = cacheSlotWidth - texRect.width;
        int dh = cacheSlotHeight - texRect.height;
        
		int leftChildIndex;
        if ( dw > dh )   // split along x
		{			
			leftChildIndex = AllocNode( nodeIndex );
			contract_assert( IsLeftChild( nodeIndex, leftChildIndex ) );
			TreeEntry_t &leftChild =  m_Nodes[leftChildIndex].entry;
			leftChild.rc.x = m_Nodes[nodeIndex].entry.rc.x;
			leftChild.rc.y = m_Nodes[nodeIndex].entry.rc.y;
			leftChild.rc.width = texRect.width;
			leftChild.rc.height = cacheSlotHeight;
			leftChild.bInUse = false;

			int rightChildIndex = AllocNode( nodeIndex );
			contract_assert( IsRightChild( nodeIndex, rightChildIndex ) );
			TreeEntry_t &rightChild =  m_Nodes[rightChildIndex].entry;
			rightChild.rc.x = m_Nodes[nodeIndex].entry.rc.x + texRect.width + m_PixelGap;
			rightChild.rc.y = m_Nodes[nodeIndex].entry.rc.y;
			rightChild.rc.width = dw - m_PixelGap;  
			rightChild.rc.height = cacheSlotHeight;
			rightChild.bInUse = false;

			contract_assert( m_Nodes[leftChildIndex].parent == m_Nodes[rightChildIndex].parent );
			contract_assert( m_Nodes[leftChildIndex].parent == nodeIndex );
		}
        else // split along y
		{			
			leftChildIndex = AllocNode( nodeIndex );
			contract_assert( IsLeftChild( nodeIndex, leftChildIndex ) );
			TreeEntry_t &leftChild =  m_Nodes[leftChildIndex].entry;
			leftChild.rc.x = m_Nodes[nodeIndex].entry.rc.x;
			leftChild.rc.y = m_Nodes[nodeIndex].entry.rc.y;
			leftChild.rc.width = cacheSlotWidth;
			leftChild.rc.height = texRect.height;
			leftChild.bInUse = false;
			
			int rightChildIndex = AllocNode( nodeIndex );
			contract_assert( IsRightChild( nodeIndex, rightChildIndex ) );
			TreeEntry_t &rightChild =  m_Nodes[rightChildIndex].entry;
			rightChild.rc.x = m_Nodes[nodeIndex].entry.rc.x;
			rightChild.rc.y = m_Nodes[nodeIndex].entry.rc.y + texRect.height + m_PixelGap;
			rightChild.rc.width = cacheSlotWidth;
			rightChild.rc.height = dh - m_PixelGap;
			rightChild.bInUse = false;

			contract_assert( m_Nodes[leftChildIndex].parent == m_Nodes[rightChildIndex].parent );
			contract_assert( m_Nodes[leftChildIndex].parent == nodeIndex );
		}
        
        // insert into first child we created
		return InsertRect( texRect, leftChildIndex );  
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CTexturePacker::RemoveRect( int nodeIndex )
{
	if ( !IsInTree( nodeIndex ) )
	{
		return false;
	}

	m_Nodes[nodeIndex].entry.bInUse = false;

	// If its a leaf, see if its peer is empty, if it is the split can go away. The root has no peer.
	int parentIndex = m_Nodes[nodeIndex].parent;
	if ( IsLeaf( nodeIndex) && parentIndex >= 0 ) // a leaf
	{
		// Is this node the left child?
		if ( IsLeftChild( parentIndex, nodeIndex ) )
		{
			// Get the index of the right child
			int peerIndex = m_Nodes[parentIndex].child[1];
			if ( IsLeaf( peerIndex ) && !m_Nodes[peerIndex].entry.bInUse )
			{
				// Both children are leaves and neither is in use, remove the split here.
				FreeLeaf( nodeIndex );
				FreeLeaf( peerIndex );

			}
		}
		else // Its the right child
		{
			// Get the index of the left child
			int peerIndex = m_Nodes[parentIndex].child[0];
			contract_assert( nodeIndex == m_Nodes[parentIndex].child[1] );

			if ( IsLeaf( peerIndex ) && !m_Nodes[peerIndex].entry.bInUse )
			{
				// Both children are leaves and neither is in use, remove the split here.
				FreeLeaf( nodeIndex );
				FreeLeaf( peerIndex );

			}
		}
	}

	return true;
}
