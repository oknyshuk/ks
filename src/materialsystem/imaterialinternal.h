#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.exprevaluator;
import ks.tier1.filename_table;
import ks.tier1.keyvalues;

// identifier was truncated to '255' characters in the debug information

#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterial.h"
#include "shaderapi/ishaderapi.h"
#include "filesystem.h"

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
enum MaterialPrimitiveType_t;
class IShader;
class IMesh;
class IVertexBuffer;
class IIndexBuffer;
struct Shader_VertexArrayData_t;
struct ShaderRenderState_t;
class CBasePerMaterialContextData;


struct tokencache_t
{
	unsigned short symbol;
	unsigned char varIndex;
	unsigned char cached : 1;
	unsigned char subrect : 1;
};


//-----------------------------------------------------------------------------
// Interface for materials used only within the material system
//-----------------------------------------------------------------------------
// The material implementations; every material is created as exactly one of them. IMaterial and
// IMaterialInternal forward each call to the implementation named by the kind (imaterialdispatch.cpp).
enum class MaterialKind : unsigned char
{
	Standard,		// CMaterial
	SubRect,		// CMaterialSubRect
	QueueFriendly,	// CMaterial_QueueFriendly: wraps a CMaterial
};

class IMaterialInternal : public IMaterial
{
public:
	MaterialKind Kind() const { return m_Kind; }

	// class factory methods
	static IMaterialInternal* CreateMaterial( char const* pMaterialName, const char *pTextureGroupName, KeyValues *pKeyValues = nullptr );
	static void DestroyMaterial( IMaterialInternal* pMaterial );

	// If supplied, pKeyValues and pPatchKeyValues should come from LoadVMTFile()
	static IMaterialInternal* CreateMaterialSubRect( char const* pMaterialName, const char *pTextureGroupName,
													KeyValues *pKeyValues = nullptr, KeyValues *pPatchKeyValues = nullptr, bool bAssumeCreateFromFile = false );
	static void DestroyMaterialSubRect( IMaterialInternal* pMaterial );

	// refcount
	int		GetReferenceCount( ) const;

	// enumeration id
	void	SetEnumerationID( int id );

	// White lightmap methods
	void	SetNeedsWhiteLightmap( bool val );
	bool	GetNeedsWhiteLightmap( ) const;

	// load/unload 
	void	Uncache( bool bPreserveVars = false );
	void	Precache();
	// If supplied, pKeyValues and pPatchKeyValues should come from LoadVMTFile()
	bool	PrecacheVars( KeyValues *pKeyValues = nullptr, KeyValues *pPatchKeyValues = nullptr, std::vector< FileNameHandle_t > *pIncludes = nullptr );

	// reload all textures used by this materals
	void	ReloadTextures();
	
	// lightmap pages associated with this material
	void	SetMinLightmapPageID( int pageID );
	void	SetMaxLightmapPageID( int pageID );;
	int		GetMinLightmapPageID( ) const;
	int		GetMaxLightmapPageID( ) const;

	IShader *GetShader() const;
	IMaterialVar** GetVars();

	// Can we use it?
	bool	IsPrecached( ) const;
	bool	IsPrecachedVars() const;

	// main draw method
	CBasePerMaterialContextData **GetContextData( int modulationFlags );

	StateSnapshot_t	GetSnapshotId( int modulation, int renderPass ); 
	unsigned char* GetInstanceCommandBuffer( int modulation );
	void	DrawMesh( VertexCompressionType_t vertexCompression, bool bIsAlphaModulating, bool bUsingPreTessPatches );

	// Gets the vertex format
	VertexFormat_t GetVertexFormat() const;
	VertexFormat_t GetVertexUsage() const;

	// Performs a debug trace on this material
	bool PerformDebugTrace() const;

	// Can we override this material in debug?
	bool NoDebugOverride() const;

	// Should we draw?
	void ToggleSuppression();

	// Are we suppressed?
	bool IsSuppressed() const;

	// Should we debug?
	void ToggleDebugTrace();

	// Do we use fog?
	bool UseFog() const;

	// Adds a material variable to the material
	void AddMaterialVar( IMaterialVar *pMaterialVar );

	// Gets the renderstate
	ShaderRenderState_t *GetRenderState();

	// Was this manually created (not read from a file?)
	bool IsManuallyCreated() const;

	bool NeedsFixedFunctionFlashlight() const;

	bool IsUsingVertexID() const;

	// Identifies a material mounted through the preload path
	void MarkAsPreloaded( bool bSet );
	bool IsPreloaded() const;

	// Conditonally increments the refcount
	void ArtificialAddRef( void );
	void ArtificialRelease( void );

	void			ReportVarChanged( IMaterialVar *pVar );
	uint32			GetChangeID() const;
	uint32			GetChangeTimestamp()  const;

	bool			IsTranslucentInternal( float fAlphaModulation ) const;

	//Is this the queue friendly or realtime version of the material?
	bool IsRealTimeVersion( void ) const;

	void ClearContextData( void );

	//easy swapping between the queue friendly and realtime versions of the material
	IMaterialInternal *GetRealTimeVersion( void );
	IMaterialInternal *GetQueueFriendlyVersion( void );	

	void PrecacheMappingDimensions( void );
	void FindRepresentativeTexture( void );

	// These are used when a new whitelist is passed in. First materials to be reloaded are flagged, then they are reloaded.
	void DecideShouldReloadFromWhitelist( IFileList *pFileList );
	void ReloadFromWhitelistIfMarked();

	void CompactMaterialVars();

	// Are any of the proxies attached to this material callable from the queued thread?
	bool HasQueueFriendlyProxies() const;

protected:
	explicit IMaterialInternal( MaterialKind kind ) : m_Kind( kind ) {}

private:
	MaterialKind m_Kind;
};

extern void InsertKeyValues( KeyValues& dst, KeyValues& src, bool bCheckForExistence );
extern void WriteKeyValuesToFile( const char *pFileName, KeyValues& keyValues );
extern void ExpandPatchFile( KeyValues& keyValues, KeyValues &patchKeyValues );
// patchKeyValues accumulates keys applied by VMT patch files (this is necessary to make $fallbackmaterial
// work properly - the patch keys need to be reapplied when the fallback VMT is loaded). It may contain
// previously accumulated patch keys on entry, and may contain more encountered patch keys on exit.
extern bool LoadVMTFile( KeyValues &vmtKeyValues, KeyValues &patchKeyValues, const char *pMaterialName, bool bUsesUNCFilename, std::vector< FileNameHandle_t > *pIncludes  );

extern void CompactMaterialVars( IMaterialVar **ppMaterialVars, int nVars );
extern void CompactMaterialVarHeap();
