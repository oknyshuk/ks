#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.basetypes;
import ks.platform;
import ks.simd;
import ks.tier1.icvar;
import std;

#define DEFAULT_TEXTURE_PAGE_WIDTH	1024
#define DEFAULT_TEXTURE_PAGE_WIDTH	1024

//-----------------------------------------------------------------------------
// Purpose: manages texture packing of textures as they are added.
//-----------------------------------------------------------------------------
class CTexturePacker
{
public:
	struct TreeEntry_t
	{
		Rect_t			rc;
		bool			bInUse;
	};

	CTexturePacker( int texWidth = DEFAULT_TEXTURE_PAGE_WIDTH, int texHeight = DEFAULT_TEXTURE_PAGE_WIDTH, int pixelGap = 0 );
	~CTexturePacker();

	// Use -1 if you want to insert at the root.
	int InsertRect( const Rect_t& texRect, int nodeIndex = -1 );
	bool RemoveRect( int nodeIndex );
	const TreeEntry_t &GetEntry( int i )
	{
		return m_Nodes[i].entry;
	}
	int GetPageWidth()
	{
		return m_PageWidth;
	}
	int GetPageHeight()
	{
		return m_PageHeight;
	}

	// clears the tree
	void Clear();

private:
	// Binary split tree; nodes are split into, and merged from, exactly two children.
	struct Node_t
	{
		TreeEntry_t entry;
		int parent;
		int child[2];
		bool bInTree;
	};

	bool IsLeaf( int nodeIndex );
	bool IsLeftChild( int nodeIndexParent, int nodeIndexChild ) const;
	bool IsRightChild( int nodeIndexParent, int nodeIndexChild ) const;
	bool IsInTree( int nodeIndex ) const { return nodeIndex >= 0 && nodeIndex < (int)m_Nodes.size() && m_Nodes[nodeIndex].bInTree; }
	int AllocNode( int parentIndex );
	void FreeLeaf( int nodeIndex );
	
	// Pixel gap between textures.
	int m_PixelGap;
	int m_PageWidth;
	int m_PageHeight;

	// node indices are handed out to callers, so slots are recycled (LIFO) instead of compacted
	std::vector< Node_t > m_Nodes;
	std::vector< int > m_FreeNodes;
	int m_Root;
};
