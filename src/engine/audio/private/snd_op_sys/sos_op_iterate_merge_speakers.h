#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "sos_op.h"

//-----------------------------------------------------------------------------
// calculate and merge multiple pass speaker volumes
//-----------------------------------------------------------------------------
struct CSosOperatorIterateAndMergeSpeakers_t : CSosOperator_t
{
	char m_nStartOperatorName[64];
	int m_nMaxIterations;
	int m_nOperatorIndex;

	SOS_OUTPUT_FLOAT( m_flOutputIndex, SO_SINGLE )
		SOS_INPUT_FLOAT( m_flInputMaxIterations, SO_SINGLE )
		SOS_INPUT_FLOAT( m_flInputSpeakers, SO_SPEAKERS )
		SOS_OUTPUT_FLOAT( m_flOutputSpeakers, SO_SPEAKERS )

};

class CSosOperatorIterateAndMergeSpeakers : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorIterateAndMergeSpeakers )
};
