// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "shaderapibase.h"
#include "shaderapi/ishaderutil.h"

//-----------------------------------------------------------------------------
//
// The Base implementation of the shader render class
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CShaderAPIBase::CShaderAPIBase()
{
}

CShaderAPIBase::~CShaderAPIBase()
{
}


//-----------------------------------------------------------------------------
// Methods of IShaderDynamicAPI
//-----------------------------------------------------------------------------
void CShaderAPIBase::GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo )
{
	g_pShaderUtil->GetCurrentColorCorrection( pInfo );
}
