#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2013 Valve Corporation

// Purpose: Interface to composite texture objects




class IVTFTexture;
class ICompositeTexture
{
public:
	virtual bool IsReady() const = 0;
	virtual bool GenerationComplete() const = 0;
	virtual IVTFTexture *GetResultVTF() = 0;
	virtual	const char *GetName() = 0;
};
