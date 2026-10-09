#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "sos_op.h"

//-----------------------------------------------------------------------------
// basic mixgroup operator
//-----------------------------------------------------------------------------
struct CSosOperatorMixGroup_t : CSosOperator_t
{
	int	m_nMixGroupIndex;
	bool m_bSetMixGroupOnChannel;
	SOS_OUTPUT_FLOAT( m_flOutputVolume, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputLevel, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputDSP, SO_SINGLE )

};

class CSosOperatorMixGroup : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorMixGroup )
};
