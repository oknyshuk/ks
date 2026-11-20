// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#pragma once

#define ADD_SCREENSPACE_EFFECT( CEffect, pEffectName )			CEffect pEffectName##_effect;														\
																CScreenSpaceEffectRegistration pEffectName##_reg( #pEffectName, &pEffectName##_effect );	
