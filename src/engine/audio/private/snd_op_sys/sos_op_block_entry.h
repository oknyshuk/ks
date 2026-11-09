#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "sos_op.h"

//-----------------------------------------------------------------------------
// stop other sound entries
//-----------------------------------------------------------------------------
struct CSosOperatorBlockEntry_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInputActive, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInputDuration, SO_SINGLE )

	char m_nMatchEntryName[64];
	bool m_bMatchEntry;
	char m_nMatchSoundName[64];
	bool m_bMatchSound;
	bool m_bMatchSubString;
	bool m_bMatchEntity;
	bool m_bMatchChannel;

	int m_nEntryIndex;


};

class CSosOperatorBlockEntry : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorBlockEntry )

	void	StackShutdown( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex );
};
