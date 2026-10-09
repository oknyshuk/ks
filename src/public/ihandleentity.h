#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

class CBaseHandle;


// An IHandleEntity-derived class can go into an entity list and use ehandles.
class IHandleEntity
{
public:
	virtual ~IHandleEntity() = default;
	virtual void SetRefEHandle( const CBaseHandle &handle ) = 0;
	virtual const CBaseHandle& GetRefEHandle() const = 0;
};
