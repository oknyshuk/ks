// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1:kvpacker;

import std;
import ks.math;
export import :keyvalues;

export extern "C++" {

namespace ks
{
	class byte_reader;
	class byte_writer;
}

//-----------------------------------------------------------------------------
// Purpose: Handles packing KeyValues binary packing and unpacking in a a
//			consistent way across all branches. If you change
//			this packing format you need to do so in a backward compatible way
//			so that the servers and all the various GCs can still talk to
//			each other.
//-----------------------------------------------------------------------------
class KVPacker
{
public:
	bool WriteAsBinary( KeyValues *pNode, ks::byte_writer &buffer );
	bool ReadAsBinary( KeyValues *pNode, ks::byte_reader &buffer );

private:
	// These types are used for serialization of KeyValues nodes.
	// Do not renumber them or you will break serialization across
	// branches.
	enum EPackType
	{
		PACKTYPE_NONE = 0,
		PACKTYPE_STRING,
		PACKTYPE_INT,
		PACKTYPE_FLOAT,
		PACKTYPE_PTR,
		PACKTYPE_WSTRING,
		PACKTYPE_COLOR,
		PACKTYPE_UINT64,
		PACKTYPE_NULLMARKER,				// used to mark the end of a block in the binary format
	};
};

}
