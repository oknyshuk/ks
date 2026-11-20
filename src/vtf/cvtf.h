#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Local header for CVTFTexture class declaration - allows platform-specific
//			implementation to be placed in separate cpp files.

#include "s3tc_decode.h"
#include "vtf/vtf.h"
#include "filesystem.h"
import ks.core.basetypes;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.tier1.byte_reader;
import ks.tier1.byte_writer;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.serialization;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;

class CEdgePos
{
public:
	CEdgePos() {}
	CEdgePos( int ix, int iy )
	{
		x = ix;
		y = iy;
	}

	void operator +=( const CEdgePos &other ) 
	{ 
		x += other.x;
		y += other.y;
	}

	void operator /=( int val )
	{ 
		x /= val;
		y /= val;
	}

	CEdgePos operator >>( int shift )
	{ 
		return CEdgePos( x >> shift, y >> shift );
	}

	CEdgePos operator *( int shift )
	{ 
		return CEdgePos( x * shift, y * shift );
	}

	CEdgePos operator -( const CEdgePos &other ) 
	{ 
		return CEdgePos( x - other.x, y - other.y );
	}

	CEdgePos operator +( const CEdgePos &other ) 
	{ 
		return CEdgePos( x + other.x, y + other.y );
	}

	bool operator!=( const CEdgePos &other )
	{
		return !( *this == other );
	}

	bool operator==( const CEdgePos &other )
	{
		return x==other.x && y==other.y;
	}

	int x, y;
};


class CEdgeIncrements
{
public:
	CEdgePos iFace1Start, iFace1End;
	CEdgePos iFace1Inc, iFace2Inc;
	CEdgePos iFace2Start, iFace2End;
};


class CEdgeMatch
{
public:
	int m_iFaces[2];	// Which faces are touching.
	int m_iEdges[2];	// Which edge on each face is touching.
	int m_iCubeVerts[2];// Which of the cube's verts comprise this edge?
	bool m_bFlipFace2Edge;
};


class CCornerMatch
{
public:
	// The info for the 3 edges that match at this corner.
	int m_iFaces[3];
	int m_iFaceEdges[3];
};


class CEdgeFaceIndex
{
public:
	int m_iEdge;
	int m_iFace;
};


inline constexpr auto NUM_EDGE_MATCHES = 12;
inline constexpr auto NUM_CORNER_MATCHES = 8;


//-----------------------------------------------------------------------------
// Implementation of the VTF Texture
//-----------------------------------------------------------------------------
class CVTFTexture : public IVTFTexture
{
public:
	CVTFTexture();
	~CVTFTexture();

	bool Init( int nWidth, int nHeight, int nDepth, ImageFormat fmt, int iFlags, int iFrameCount, int nForceMipCount );

	// Methods to initialize the low-res image
	void InitLowResImage( int nWidth, int nHeight, ImageFormat fmt );

	void *SetResourceData( uint32 eType, void const *pData, std::size_t nDataSize );
	void *GetResourceData( uint32 eType, std::size_t *pDataSize ) const;

	// Locates the resource entry info if it's present, easier than crawling array types
	bool HasResourceEntry( uint32 eType ) const;

	// Retrieve available resource types of this IVTFTextures
	//		arrTypesBuffer			buffer to be filled with resource types available.
	//		numTypesBufferElems		how many resource types the buffer can accomodate.
	// Returns:
	//		number of resource types available (can be greater than "numTypesBufferElems"
	//		in which case only first "numTypesBufferElems" are copied to "arrTypesBuffer")
	unsigned int GetResourceTypes( uint32 *arrTypesBuffer, int numTypesBufferElems ) const;

	// Methods to set other texture fields
	void SetBumpScale( float flScale );
	void SetReflectivity( const Vector &vecReflectivity );

	// These are methods to help with optimization of file access
	void LowResFileInfo( int *pStartLocation, int *pSizeInBytes ) const;
	void ImageFileInfo( int nFrame, int nFace, int nMip, int *pStartLocation, int *pSizeInBytes) const;
	int FileSize( int nMipSkipCount = 0 ) const;

	// When unserializing, we can skip a certain number of mip levels,
	// and we also can just load everything but the image data
	bool Unserialize( ks::byte_reader &buf, bool bBufferHeaderOnly = false, int nSkipMipLevels = 0 );
	bool Serialize( ks::byte_writer &buf );

	// Attributes...
	int Width() const;
	int Height() const;
	int Depth() const;
	int MipCount() const;

	int RowSizeInBytes( int nMipLevel ) const;
	int FaceSizeInBytes( int nMipLevel ) const;

	ImageFormat Format() const;
	int FaceCount() const;
	int FrameCount() const;
	int Flags() const;

	float BumpScale() const;
	const Vector &Reflectivity() const;

	bool IsCubeMap() const;
	bool IsNormalMap() const;
	bool IsVolumeTexture() const;

	int LowResWidth() const;
	int LowResHeight() const;
	ImageFormat LowResFormat() const;

	// Computes the size (in bytes) of a single mipmap of a single face of a single frame 
	int ComputeMipSize( int iMipLevel ) const;

	// Computes the size (in bytes) of a single face of a single frame
	// All mip levels starting at the specified mip level are included
	int ComputeFaceSize( int iStartingMipLevel = 0 ) const;

	// Computes the total size of all faces, all frames
	int ComputeTotalSize( ) const;

	// Computes the dimensions of a particular mip level
	void ComputeMipLevelDimensions( int iMipLevel, int *pWidth, int *pHeight, int *pMipDepth ) const;

	// Computes the size of a subrect (specified at the top mip level) at a particular lower mip level
	void ComputeMipLevelSubRect( Rect_t* pSrcRect, int nMipLevel, Rect_t *pSubRect ) const;

	// Returns the base address of the image data
	unsigned char *ImageData();

	// Returns a pointer to the data associated with a particular frame, face, and mip level
	unsigned char *ImageData( int iFrame, int iFace, int iMipLevel );

	// Returns a pointer to the data associated with a particular frame, face, mip level, and offset
	unsigned char *ImageData( int iFrame, int iFace, int iMipLevel, int x, int y, int z );

	// Returns the base address of the low-res image data
	unsigned char *LowResImageData();

	// Converts the texture's image format. Use IMAGE_FORMAT_DEFAULT
	void ConvertImageFormat( ImageFormat fmt, bool bNormalToDUDV, bool bNormalToDXT5GA );

	// Generate spheremap based on the current cube faces (only works for cubemaps)
	// The look dir indicates the direction of the center of the sphere
	void GenerateSpheremap( LookDir_t lookDir );

	void GenerateHemisphereMap( unsigned char *pSphereMapBitsRGBA, int targetWidth, 
		int targetHeight, LookDir_t lookDir, int iFrame );

	// Fixes the cubemap faces orientation from our standard to the
	// standard the material system needs.
	void FixCubemapFaceOrientation( );

	// Normalize the top mip level if necessary
	void NormalizeTopMipLevel();
	
	// Generates mipmaps from the base mip levels
	void GenerateMipmaps();

	// Put 1/miplevel (1..n) into alpha.
	void PutOneOverMipLevelInAlpha();

	// Scale alpha by miplevel/ mipcount
	void PremultAlphaWithMipFraction();

	// Computes the reflectivity
	void ComputeReflectivity( );

	// Computes the alpha flags
	void ComputeAlphaFlags();

	void Compute2DGradient();

	// Gets the texture all internally consistent assuming you've loaded
	// mip 0 of all faces of all frames
	void PostProcess(bool bGenerateSpheremap, LookDir_t lookDir = LOOK_DOWN_Z, bool bAllowFixCubemapOrientation = true, bool bLoadedMiplevels = false);
	void SetPostProcessingSettings( VtfProcessingOptions const *pOptions );

	// Generate the low-res image bits
	bool ConstructLowResImage();

	void MatchCubeMapBorders( int iStage, ImageFormat finalFormat, bool bSkybox );

	// Sets threshhold values for alphatest mipmapping
	void SetAlphaTestThreshholds( float flBase, float flHighFreq );

	bool IsPreTiled() const;

private:
	// Unserialization
	bool ReadHeader( ks::byte_reader &buf, VTFFileHeader_t &header );

	void BlendCubeMapEdgePalettes(
		int iFrame,
		int iMipLevel,
		const CEdgeMatch *pMatch );

	void BlendCubeMapCornerPalettes(
		int iFrame,
		int iMipLevel,
		const CCornerMatch *pMatch );

	void MatchCubeMapS3TCPalettes(
		CEdgeMatch edgeMatches[NUM_EDGE_MATCHES],
		CCornerMatch cornerMatches[NUM_CORNER_MATCHES]
		);
	
	void SetupFaceVert( int iMipLevel, int iVert, CEdgePos &out );
	void SetupEdgeIncrement( CEdgePos &start, CEdgePos &end, CEdgePos &inc );

	void SetupTextureEdgeIncrements( 
		int iMipLevel,
		int iFace1Edge,
		int iFace2Edge,
		bool bFlipFace2Edge,
		CEdgeIncrements *incs );
	
	void BlendCubeMapFaceEdges(
		int iFrame,
		int iMipLevel,
		const CEdgeMatch *pMatch );

	void BlendCubeMapFaceCorners(
		int iFrame,
		int iMipLevel,
		const CCornerMatch *pMatch );

	void BuildCubeMapMatchLists( CEdgeMatch edgeMatches[NUM_EDGE_MATCHES], CCornerMatch cornerMatches[NUM_CORNER_MATCHES], bool bSkybox );

	// Allocate image data blocks with an eye toward re-using memory
	bool AllocateImageData( int nMemorySize );
	bool AllocateLowResImageData( int nMemorySize );

	// Compute the mip count based on the size + flags
	int ComputeMipCount( ) const;

	// Unserialization of low-res data
	bool LoadLowResData( ks::byte_reader &buf );

	// Unserialization of new resource data
	bool LoadNewResources( ks::byte_reader &buf );

	// Unserialization of image data
	bool LoadImageData( ks::byte_reader &buf, const VTFFileHeader_t &header, int nSkipMipLevels );

	// Shutdown
	void Shutdown();
	void ReleaseResources();

	// Makes a single frame of spheremap
	void ComputeSpheremapFrame( unsigned char **ppCubeFaces, unsigned char *pSpheremap, LookDir_t lookDir );

	// Makes a single frame of spheremap
	void ComputeHemispheremapFrame( unsigned char **ppCubeFaces, unsigned char *pSpheremap, LookDir_t lookDir );

	// Serialization of image data
	bool WriteImageData( ks::byte_writer &buf );

	// Computes the size (in bytes) of a single mipmap of a single face of a single frame 
	int ComputeMipSize( int iMipLevel, ImageFormat fmt ) const;

	// Computes the size (in bytes) of a single face of a single frame
	// All mip levels starting at the specified mip level are included
	int ComputeFaceSize( int iStartingMipLevel, ImageFormat fmt ) const;

	// Computes the total size of all faces, all frames
	int ComputeTotalSize( ImageFormat fmt ) const;

	// Computes the location of a particular face, frame, and mip level
	int GetImageOffset( int iFrame, int iFace, int iMipLevel, ImageFormat fmt ) const;

	// Locates the resource entry info if it's present
	ResourceEntryInfo *FindResourceEntryInfo( unsigned int eType );
	ResourceEntryInfo const *FindResourceEntryInfo( unsigned int eType ) const;
	
	// Inserts the resource entry info if it's not present
	ResourceEntryInfo *FindOrCreateResourceEntryInfo( unsigned int eType );

	// Removes the resource entry info if it's present
	bool RemoveResourceEntryInfo( unsigned int eType );

private:
	// This is to make sure old-format .vtf files are read properly
	int				m_nVersion[2];

	int				m_nWidth;
	int				m_nHeight;
	int 			m_nDepth;
	ImageFormat		m_Format;

	int				m_nMipCount;
	int				m_nFaceCount;
	int				m_nFrameCount;

	int				m_nImageAllocSize;
	int				m_nFlags;
	unsigned char	*m_pImageData;

	Vector			m_vecReflectivity;
	float			m_flBumpScale;
	
	// FIXME: Do I need this?
	int				m_iStartFrame;

	// Low res data
	int				m_nLowResImageAllocSize;
	ImageFormat		m_LowResImageFormat;
	int				m_nLowResImageWidth;
	int				m_nLowResImageHeight;
	unsigned char	*m_pLowResImageData;

	// Used while fixing mipmap edges.
	std::vector< S3RGBA > m_OriginalData;

	// Alpha threshholds
	float			m_flAlphaThreshhold;
	float			m_flAlphaHiFreqThreshhold;

	std::vector< ResourceEntryInfo > m_arrResourcesInfo;

	struct ResourceMemorySection
	{
		ResourceMemorySection() { std::memset( this, 0, sizeof( *this ) ); }

		int				m_nDataAllocSize;
		int				m_nDataLength;
		unsigned char	*m_pData;

		bool AllocateData( int nMemorySize );
		bool LoadData( ks::byte_reader &buf );
		bool WriteData( ks::byte_writer &buf ) const;
	};
	std::vector< ResourceMemorySection > m_arrResourcesData;
	std::vector< ResourceMemorySection > m_arrResourcesData_ForReuse;	// Maintained to keep allocated memory blocks when unserializing from files

	VtfProcessingOptions m_Options;
};
