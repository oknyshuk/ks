#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation
import ks.tier1;

// Purpose: performs serialization for CSOAContainer (avoids tier1 depending on datamodel)

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmxElement;


//-----------------------------------------------------------------------------
// Serialization/Unserialization
//-----------------------------------------------------------------------------
bool SerializeCSOAContainer(   const CSOAContainer *pContainer, CDmxElement *pRootElement );
bool UnserializeCSOAContainer( const CSOAContainer *pContainer, const CDmxElement *pRootElement );
