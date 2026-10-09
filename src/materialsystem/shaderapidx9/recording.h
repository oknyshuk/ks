#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

//-----------------------------------------------------------------------------
// Use this to put us into a 'recording' mode
//-----------------------------------------------------------------------------

//#define RECORDING

//-----------------------------------------------------------------------------
// Uncomment these to record special frames in the recording
// that reset the entire render state.
//-----------------------------------------------------------------------------

//#define RECORD_KEYFRAMES 1
inline constexpr auto KEYFRAME_INTERVAL = 100; // number of actual frames between each keyframe
 
//-----------------------------------------------------------------------------
// Use this to allow us to record crashes (write every command immediately)
//-----------------------------------------------------------------------------

//#define CRASH_RECORDING
 
//-----------------------------------------------------------------------------
// Use this to record textures (checkboards are used for textures otherwise)
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Use this to record debug strings . .these are only useful if you are doing "playback -list"
//-----------------------------------------------------------------------------

#define RECORD_DEBUG_STRINGS

//-----------------------------------------------------------------------------
// Recording state, if you change this, change the table in playback/playback.cpp
//-----------------------------------------------------------------------------

enum RecordingCommands_t
{
	DX8_CREATE_DEVICE = 0,
	DX8_DESTROY_DEVICE,
	DX8_RESET,
	DX8_SHOW_CURSOR,
	DX8_BEGIN_SCENE,
	DX8_END_SCENE,
	DX8_PRESENT,
	DX8_CREATE_TEXTURE,
	DX8_DESTROY_TEXTURE,
	DX8_SET_TEXTURE,
	DX8_SET_TRANSFORM,
	DX8_CREATE_VERTEX_SHADER,
	DX8_CREATE_PIXEL_SHADER,
	DX8_DESTROY_VERTEX_SHADER,
	DX8_DESTROY_PIXEL_SHADER,
	DX8_SET_VERTEX_SHADER,
	DX8_SET_PIXEL_SHADER,
	DX8_SET_VERTEX_SHADER_CONSTANT,
	DX8_SET_PIXEL_SHADER_CONSTANT,
	DX8_SET_MATERIAL,
	DX8_LIGHT_ENABLE,
	DX8_SET_LIGHT,
	DX8_SET_VIEWPORT,
	DX8_CLEAR,
	DX8_VALIDATE_DEVICE,
	DX8_SET_RENDER_STATE,
	DX8_SET_TEXTURE_STAGE_STATE,

	DX8_CREATE_VERTEX_BUFFER,
	DX8_DESTROY_VERTEX_BUFFER,
	DX8_LOCK_VERTEX_BUFFER,
	DX8_VERTEX_DATA,
	DX8_UNLOCK_VERTEX_BUFFER,

	DX8_CREATE_INDEX_BUFFER,
	DX8_DESTROY_INDEX_BUFFER,
	DX8_LOCK_INDEX_BUFFER,
	DX8_INDEX_DATA,
	DX8_UNLOCK_INDEX_BUFFER,

	DX8_SET_STREAM_SOURCE,
	DX8_SET_INDICES,
	DX8_DRAW_PRIMITIVE,
	DX8_DRAW_INDEXED_PRIMITIVE,

	DX8_LOCK_TEXTURE,
	DX8_UNLOCK_TEXTURE,

	DX8_KEYFRAME,	// isn't actually a dx8 command, used to help find particular frames

	DX8_SET_TEXTURE_DATA,
	DX8_BLIT_TEXTURE_BITS,
	
	DX8_GET_DEVICE_CAPS,
	DX8_GET_ADAPTER_IDENTIFIER,

	DX8_HARDWARE_SYNC,

	DX8_COPY_FRAMEBUFFER_TO_TEXTURE,
	DX8_DEBUG_STRING,
	DX8_CREATE_DEPTH_TEXTURE,
	DX8_DESTROY_DEPTH_TEXTURE,
	DX8_SET_RENDER_TARGET,

	DX8_TEST_COOPERATIVE_LEVEL,
	
	DX8_SET_VERTEX_BUFFER_FORMAT, // isn't actually a dx8 command. . let's playback know what format a buffer is for listing info
	
	DX8_SET_SAMPLER_STATE,
	DX8_SET_VERTEX_DECLARATION,
	DX8_CREATE_VERTEX_DECLARATION,
	DX8_SET_FVF,
	DX8_SET_CLIP_PLANE,

	DX8_SYNC_TOKEN,

	DX8_LOCK_VERTEX_TEXTURE,
	DX8_UNLOCK_VERTEX_TEXTURE,

	DX8_SET_SCISSOR_RECT,

	DX8_NUM_RECORDING_COMMANDS
};
