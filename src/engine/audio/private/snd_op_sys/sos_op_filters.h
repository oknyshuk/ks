#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "sos_op.h"

//-----------------------------------------------------------------------------
// simple operator for setting a single position
//-----------------------------------------------------------------------------
struct CSosOperatorFloatFilter_t : CSosOperator_t
{

	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInput, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInputMaxVel, SO_SINGLE )

	float m_flPrevTime;
	float m_flPrevValue;

};

class CSosOperatorFloatFilter : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorFloatFilter )
};
