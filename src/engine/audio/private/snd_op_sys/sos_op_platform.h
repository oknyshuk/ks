#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "sos_op.h"

//-----------------------------------------------------------------------------
// mapname
//-----------------------------------------------------------------------------
struct CSosOperatorPlatform_t : CSosOperator_t
{

	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )

};
class CSosOperatorPlatform : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorPlatform )

};
