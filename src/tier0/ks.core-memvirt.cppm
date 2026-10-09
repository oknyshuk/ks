// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation
// Copyright (c) 2026 oknyshuk

// Purpose: CVirtualMemoryManager interface

export module ks.core:memvirt;

import std;

export extern "C++" {

// Allocate virtual sections via IMemAlloc::AllocateVirtualMemorySection
class IVirtualMemorySection
{
public:
	// Information about memory section
	virtual void * GetBaseAddress() = 0;
	virtual std::size_t GetPageSize() = 0;
	virtual std::size_t GetTotalSize() = 0;

	// Functions to manage physical memory mapped to virtual memory
	virtual bool CommitPages( void *pvBase, std::size_t numBytes ) = 0;
	virtual void DecommitPages( void *pvBase, std::size_t numBytes ) = 0;

	// Release the physical memory and associated virtual address space
	virtual void Release() = 0;
};

}
