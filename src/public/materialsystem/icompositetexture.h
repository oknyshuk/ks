#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2013 Valve Corporation

// Purpose: Interface to composite texture objects




class IVTFTexture;
// Non-virtual facade: the only implementation is the material system's CCompositeTexture.
class ICompositeTexture
{
public:
	bool IsReady() const;
	bool GenerationComplete() const;
	IVTFTexture *GetResultVTF();
	const char *GetName();
};
