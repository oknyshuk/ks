#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "ispatialpartition.h"
import ks.math.compressed_vector;
import ks.math.vector;

//-----------------------------------------------------------------------------
// These methods of the spatial partition manager are only used in the engine
//-----------------------------------------------------------------------------
class ISpatialPartitionInternal : public ISpatialPartition
{
public:
	// Call this to clear out the spatial partition and to re-initialize
	// it given a particular world size
	void Init( const Vector& worldmin, const Vector& worldmax );

	void DrawDebugOverlays();
};


//-----------------------------------------------------------------------------
// Method to get at the singleton implementation of the spatial partition mgr
//-----------------------------------------------------------------------------
ISpatialPartitionInternal* SpatialPartition();

//-----------------------------------------------------------------------------
// Create/destroy a custom spatial partition
//-----------------------------------------------------------------------------
ISpatialPartition *CreateSpatialPartition( const Vector& worldmin, const Vector& worldmax );
void DestroySpatialPartition( ISpatialPartition * );


//-----------------------------------------------------------------------------
// Method to get at the singleton implementation of the spatial partition mgr
//-----------------------------------------------------------------------------
ISpatialPartitionInternal* SpatialPartitionOld();

//-----------------------------------------------------------------------------
// Create/destroy a custom spatial partition
//-----------------------------------------------------------------------------
ISpatialPartition *CreateSpatialPartitionOld( const Vector& worldmin, const Vector& worldmax );
void DestroySpatialPartitionOld( ISpatialPartition * );
