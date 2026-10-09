#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "sos_op.h"

//-----------------------------------------------------------------------------
// simple operator for getting the dashboard value
//-----------------------------------------------------------------------------
struct CSosOperatorDashboard_t : CSosOperator_t
{
	bool m_bMusic;
	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
};

class CSosOperatorDashboard : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorDashboard )

};
