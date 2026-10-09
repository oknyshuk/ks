#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

class IMaterialSystem;


// To stub out the material system in a block of code (if mat_stub is 1),
// make an instance of this class. You can unstub it by calling End() or 
// it will automatically unstub in its destructor.
class CMatStubHandler
{
public:
						CMatStubHandler();
						~CMatStubHandler();

	void				End();

public:

	IMaterialSystem		*m_pOldMaterialSystem;
};


// Returns true if mat_stub is 1.
bool IsMatStubEnabled();
