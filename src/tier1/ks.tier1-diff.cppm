// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1.diff;

import std;
import ks.core.types;
import ks.platform;
import ks.simd;

export extern "C++" {

int FindDiffs(uint8 const *NewBlock, uint8 const *OldBlock,
			  int NewSize, int OldSize, int &DiffListSize,uint8 *Output,uint32 OutSize);

int FindDiffsForLargeFiles(uint8 const *NewBlock, uint8 const *OldBlock,
						   int NewSize, int OldSize, int &DiffListSize,uint8 *Output,
						   uint32 OutSize,
						   int hashsize=65536);

void ApplyDiffs(uint8 const *OldBlock, uint8 const *DiffList,
                int OldSize, int DiffListSize, int &ResultListSize,uint8 *Output,uint32 OutSize);

int FindDiffsLowMemory(uint8 const *NewBlock, uint8 const *OldBlock,
					   int NewSize, int OldSize, int &DiffListSize,uint8 *Output,uint32 OutSize);

}
