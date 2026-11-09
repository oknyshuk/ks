#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
import std;

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------

class IMaterial;
struct model_t;
typedef unsigned short ModelInstanceHandle_t;
class IClientRenderable;
class ITexture;
struct FlashlightInstance_t;
struct FlashlightState_t;

// change this when the new version is incompatable with the old
inline constexpr const char *ENGINE_SHADOWMGR_INTERFACE_VERSION = "VEngineShadowMgr002";


//-----------------------------------------------------------------------------
// Flags for the creation method
//-----------------------------------------------------------------------------
enum ShadowFlags_t
{
	SHADOW_FLAGS_FLASHLIGHT				= (1 << 0),
	SHADOW_FLAGS_SHADOW					= (1 << 1),
	SHADOW_FLAGS_SIMPLE_PROJECTION		= (1 << 2),

	// Update this if you add flags
	SHADOW_FLAGS_LAST_FLAG				= SHADOW_FLAGS_SIMPLE_PROJECTION
};

inline constexpr auto SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK = ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SHADOW | SHADOW_FLAGS_SIMPLE_PROJECTION );


//-----------------------------------------------------------------------------
//
// Shadow-related functionality exported by the engine
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// This is a handle	to shadows, clients can create as many as they want
//-----------------------------------------------------------------------------
typedef unsigned short ShadowHandle_t;

enum
{
	SHADOW_HANDLE_INVALID = (ShadowHandle_t)~0
};


//-----------------------------------------------------------------------------
// Used for the creation Flags field of CreateShadow
//-----------------------------------------------------------------------------
enum ShadowCreateFlags_t
{
	SHADOW_CACHE_VERTS			= ( 1 << 0 ),
	SHADOW_FLASHLIGHT			= ( 1 << 1 ),
	SHADOW_SIMPLE_PROJECTION	= ( 1 << 2 ),
	SHADOW_ANY_SPLITSCREEN_SLOT = ( 1 << 3 ),

	SHADOW_LAST_FLAG = SHADOW_SIMPLE_PROJECTION,
};


//-----------------------------------------------------------------------------
// Information about a particular shadow
//-----------------------------------------------------------------------------
struct ShadowInfo_t
{
	// Transforms from world space into texture space of the shadow
	VMatrix		m_WorldToShadow;

	// The shadow should no longer be drawn once it's further than MaxDist
	// along z in shadow texture coordinates.
	float			m_FalloffOffset;
	float			m_MaxDist;
	float			m_FalloffAmount;	// how much to lighten the shadow maximally
	Vector2D		m_TexOrigin;
	Vector2D		m_TexSize;
	unsigned char	m_FalloffBias;
};

typedef void (*ShadowDrawCallbackFn_t)( void * );

//-----------------------------------------------------------------------------
// The engine's interface to the shadow manager
//-----------------------------------------------------------------------------
class IShadowMgr
{
public:
	// Create, destroy shadows (see ShadowCreateFlags_t for creationFlags)
	ShadowHandle_t CreateShadow( IMaterial* pMaterial, IMaterial* pModelMaterial, void* pBindProxy, int creationFlags );
	void DestroyShadow( ShadowHandle_t handle );

	// Resets the shadow material (useful for shadow LOD.. doing blobby at distance) 
	void SetShadowMaterial( ShadowHandle_t handle, IMaterial* pMaterial, IMaterial* pModelMaterial, void* pBindProxy );

	// Shadow opacity
//	virtual void SetShadowOpacity( ShadowHandle_t handle, float alpha ) = 0;
//	virtual float GetShadowOpacity( ShadowHandle_t handle ) const = 0;

	// Project a shadow into the world
	// The two points specify the upper left coordinate and the lower-right
	// coordinate of the shadow specified in a shadow "viewplane". The
	// projection matrix is a shadow viewplane->world transformation,
	// and can be orthographic orperspective.

	// I expect that the client DLL will call this method any time the shadow
	// changes because the light changes, or because the entity casting the
	// shadow moves

	// Note that we can't really control the shadows from the engine because
	// the engine only knows about pevs, which don't exist on the client

	// The shadow matrix specifies a world-space transform for the shadow
	// the shadow is projected down the z direction, and the origin of the
	// shadow matrix is the origin of the projection ray. The size indicates
	// the shadow size measured in the space of the shadow matrix; the
	// shadow goes from +/- size.x/2 along the x axis of the shadow matrix
	// and +/- size.y/2 along the y axis of the shadow matrix.
	void ProjectShadow( ShadowHandle_t handle, const Vector &origin, 
		const Vector& projectionDir, const VMatrix& worldToShadow, const Vector2D& size,
		int nLeafCount, const int *pLeafList,
		float maxHeight, float falloffOffset, float falloffAmount, const Vector &vecCasterOrigin );

	void ProjectFlashlight( ShadowHandle_t handle, const VMatrix &worldToShadow, int nLeafCount, const int *pLeafList );

	// Gets at information about a particular shadow
	const ShadowInfo_t &GetInfo( ShadowHandle_t handle );

	const Frustum_t &GetFlashlightFrustum( ShadowHandle_t handle );

	// Methods related to shadows on brush models
	void AddShadowToBrushModel( ShadowHandle_t handle, 
		model_t* pModel, const Vector& origin, const QAngle& angles );

	// Removes all shadows from a brush model
	void RemoveAllShadowsFromBrushModel( model_t* pModel );

	// Sets the texture coordinate range for a shadow...
	void SetShadowTexCoord( ShadowHandle_t handle, float x, float y, float w, float h );

	// Methods related to shadows on studio models
	void AddShadowToModel( ShadowHandle_t shadow, ModelInstanceHandle_t instance );
	void RemoveAllShadowsFromModel( ModelInstanceHandle_t instance );

	// Set extra clip planes related to shadows...
	// These are used to prevent pokethru and back-casting
	void ClearExtraClipPlanes( ShadowHandle_t shadow );
	void AddExtraClipPlane( ShadowHandle_t shadow, const Vector& normal, float dist );

	// Allows us to disable particular shadows
	void EnableShadow( ShadowHandle_t shadow, bool bEnable );

	// Set the darkness falloff bias
	void SetFalloffBias( ShadowHandle_t shadow, unsigned char ucBias );

	// Update the state for a flashlight.
	void UpdateFlashlightState( ShadowHandle_t shadowHandle, const FlashlightState_t &lightState );

	void DrawFlashlightDepthTexture( );

	ShadowHandle_t CreateShadowEx( IMaterial* pMaterial, IMaterial* pModelMaterial, void* pBindProxy, int creationFlags, int nEntIndex );

	void SetFlashlightDepthTexture( ShadowHandle_t shadowHandle, ITexture *pFlashlightDepthTexture, unsigned char ucShadowStencilBit );

	const FlashlightState_t &GetFlashlightState( ShadowHandle_t handle );

	void SetFlashlightRenderState( ShadowHandle_t handle );

	void DrawVolumetrics();

	int GetNumShadowsOnModel( ModelInstanceHandle_t instance );
	int GetShadowsOnModel( ModelInstanceHandle_t instance, ShadowHandle_t* pShadowArray, bool bNormalShadows, bool bFlashlightShadows );

	void FlashlightDrawCallback( ShadowDrawCallbackFn_t pCallback, void *pData ); //used to draw each additive flashlight pass. The callback is called once per flashlight state for an additive pass.

	//Way for the client to determine which flashlight to use in single-pass modes. Does not actually enable the flashlight in any way.
	void SetSinglePassFlashlightRenderState( ShadowHandle_t handle );

	//Enable/Disable the flashlight state set with SetSinglePassFlashlightRenderState.
	void PushSinglePassFlashlightStateEnabled( bool bEnable );
	void PopSinglePassFlashlightStateEnabled( void );

	bool SinglePassFlashlightModeEnabled( void );

	// Determine a unique list of flashlights which hit at least one of the specified models
	// Accepts an instance count and an array of ModelInstanceHandle_ts.
	// Returns the number of FlashlightInstance_ts it's found that affect the models.
	// Also fills in a mask of which flashlights affect each ModelInstanceHandle_t
	// There can be at most MAX_FLASHLIGHTS_PER_INSTANCE_DRAW_CALL pFlashlights,
	// and the size of the pModelUsageMask array must be nInstanceCount.
	int SetupFlashlightRenderInstanceInfo( ShadowHandle_t *pUniqueFlashlights, uint32 *pModelUsageMask, int nUsageStride, int nInstanceCount, const ModelInstanceHandle_t *pInstance );

	// Returns the flashlight state for multiple flashlights
	void GetFlashlightRenderInfo( FlashlightInstance_t *pFlashlightState, int nCount, const ShadowHandle_t *pHandles );

	void RemoveAllDecalsFromShadow( ShadowHandle_t handle );

	void SkipShadowForEntity( int nEntIndex );

	//designed for portal usage. Recursive drawing leaves the scissor rect in a bad state for translucents
	void PushFlashlightScissorBounds( void );
	void PopFlashlightScissorBounds( void );

	// Disables projected shadows
	void DisableDropShadows();
};
