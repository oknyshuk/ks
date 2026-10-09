#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "sos_op.h"
#include "snd_mixgroups.h"

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
struct CSosOperatorMixLayer_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInput, SO_SINGLE )
	MXRMixGroupFields_t m_nFieldType;
	int m_nMixLayerIndex;
	int m_nMixGroupIndex;
};

class CSosOperatorMixLayer : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorMixLayer )
};
