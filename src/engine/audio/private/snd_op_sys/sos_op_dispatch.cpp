// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// Switch dispatch over the sound operators: the one place that knows every operator class.

import std;
import ks.tier1;

#include "sound.h"
#include "snd_channels.h"

#include "sos_op_block_entry.h"
#include "sos_op_convar.h"
#include "sos_op_dashboard.h"
#include "sos_op_delta.h"
#include "sos_op_distant_dsp.h"
#include "sos_op_entity_info.h"
#include "sos_op_entry_time.h"
#include "sos_op_falloff.h"
#include "sos_op_filters.h"
#include "sos_op_iterate_merge_speakers.h"
#include "sos_op_map_name.h"
#include "sos_op_math.h"
#include "sos_op_mixer.h"
#include "sos_op_mixlayer.h"
#include "sos_op_occlusion.h"
#include "sos_op_opvar.h"
#include "sos_op_output.h"
#include "sos_op_platform.h"
#include "sos_op_player_info.h"
#include "sos_op_pos_vec8.h"
#include "sos_op_source_info.h"
#include "sos_op_spatialize.h"
#include "sos_op_start_entry.h"
#include "sos_op_stop_entry.h"
#include "sos_op_sys_time.h"
#include "sos_op_tracks.h"
#include "sos_op_util.h"

#define KS_SOS_OP_CASE( cls, Name, Args ) \
	case SosOperatorKind::cls: return static_cast< std::conditional_t< std::is_const_v< std::remove_pointer_t< decltype( pOp ) > >, const cls, cls > * >( pOp )->Name Args;

#define KS_SOS_OP_DEFINE_ENTRY( Ret, Name, Const, Params, Args ) \
	Ret SosOps::Name( KS_SOS_OP_SIGNATURE( Const, KS_SOS_OP_UNPAREN Params ) ) \
	{ \
		switch ( pOp->m_Kind ) \
		{ \
		KS_SOS_OPERATORS( KS_SOS_OP_CASE, Name, Args ) \
		} \
		std::unreachable(); \
	}
KS_SOS_OP_ENTRIES( KS_SOS_OP_DEFINE_ENTRY )
