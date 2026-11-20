// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

export module ks.game.postprocess_shared;

import std;

export
{
// Purpose: common definitions for post-processing effects

enum PostProcessParameterNames_t
{
	PPPN_FADE_TIME = 0,
	PPPN_LOCAL_CONTRAST_STRENGTH,
	PPPN_LOCAL_CONTRAST_EDGE_STRENGTH,
	PPPN_VIGNETTE_START,
	PPPN_VIGNETTE_END,
	PPPN_VIGNETTE_BLUR_STRENGTH,
	PPPN_FADE_TO_BLACK_STRENGTH,
	PPPN_DEPTH_BLUR_FOCAL_DISTANCE,
	PPPN_DEPTH_BLUR_STRENGTH,
	PPPN_SCREEN_BLUR_STRENGTH,
	PPPN_FILM_GRAIN_STRENGTH,

	POST_PROCESS_PARAMETER_COUNT
};

struct PostProcessParameters_t
{
	PostProcessParameters_t()
	{
		std::memset( m_flParameters, 0, sizeof( m_flParameters ) );
		m_flParameters[ PPPN_VIGNETTE_START ] = 0.8f;
		m_flParameters[ PPPN_VIGNETTE_END ] = 1.1f;
	}

	float m_flParameters[ POST_PROCESS_PARAMETER_COUNT ];
};
}
