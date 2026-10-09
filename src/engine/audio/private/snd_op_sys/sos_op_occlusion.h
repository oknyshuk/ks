#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "sos_op.h"

//-----------------------------------------------------------------------------
// Occlusion
//-----------------------------------------------------------------------------
struct CSosOperatorOcclusion_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInputPosition, SO_VEC3 )
	SOS_INPUT_FLOAT( m_flInputScalar, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInputTraceInterval, SO_SINGLE )
	float m_flLastTraceTime;
	float m_flOccludedDBLoss;
};
class CSosOperatorOcclusion : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorOcclusion )
};
