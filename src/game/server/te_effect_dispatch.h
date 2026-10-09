#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "effect_dispatch_data.h"
class IRecipientFilter;

void DispatchEffect( const char *pName, const CEffectData &data );
void DispatchEffect( IRecipientFilter& filter, float flDelay, const char *pName, const CEffectData &data );
