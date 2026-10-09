#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "sos_op.h"

//-----------------------------------------------------------------------------
// basic mixgroup operator
//-----------------------------------------------------------------------------
struct CSosOperatorPrintFloat_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInput, SO_SINGLE )

};

class CSosOperatorPrintFloat : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorPrintFloat )
};
