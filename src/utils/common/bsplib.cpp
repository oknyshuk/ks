//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#include "cmdlib.h"
#include "tier3/tier3.h"
#include "mathlib/mathlib.h"
#include "bsplib.h"
#include "zip_utils.h"
#include "scriplib.h"
#include "utllinkedlist.h"
#include "bsptreedata.h"
#include "cmodel.h"
#include "gamebspfile.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/hardwareverts.h"
#include "utlbuffer.h"
#include "utlrbtree.h"
#include "utlsymbol.h"
#include "utlstring.h"
#include "checksum_crc.h"
#include "physdll.h"
#include "tier0/dbg.h"
#include "lumpfiles.h"
#include "vtf/vtf.h"
#include "collisionutils.h"
#include "tier1/tier1.h"
#include "vphysics2_interface.h"
#include "vphysics2_interface_flags.h"
#include "../common/datalinker.h"
#include "fmtstr.h"
#include "studio.h"

//=============================================================================

// Boundary each lump should be aligned to
// DO NOT CHANGE THIS! THIS IS FOR CLARITY! LUMP CENTRIC ALIGNMENT OCCURS PER LUMP
#define LUMP_ALIGNMENT				4
// PhysLevels require SIMD alignment
#define LUMP_PHYSLEVEL_ALIGNMENT	16

// Data descriptions for byte swapping - only needed
// for structures that are written to file for use by the game.
// From gamebspfile.h
// version 10

// From vradstaticprops.h
namespace HardwareVerts
{
} // end namespace


static char *s_LumpNames[] = {
	"LUMP_ENTITIES",						// 0
	"LUMP_PLANES",							// 1
	"LUMP_TEXDATA",							// 2
	"LUMP_VERTEXES",						// 3
	"LUMP_VISIBILITY",						// 4
	"LUMP_NODES",							// 5
	"LUMP_TEXINFO",							// 6
	"LUMP_FACES",							// 7
	"LUMP_LIGHTING",						// 8
	"LUMP_OCCLUSION",						// 9
	"LUMP_LEAFS",							// 10
	"LUMP_FACEIDS",							// 11
	"LUMP_EDGES",							// 12
	"LUMP_SURFEDGES",						// 13
	"LUMP_MODELS",							// 14
	"LUMP_WORLDLIGHTS",						// 15
	"LUMP_LEAFFACES",						// 16
	"LUMP_LEAFBRUSHES",						// 17
	"LUMP_BRUSHES",							// 18
	"LUMP_BRUSHSIDES",						// 19
	"LUMP_AREAS",							// 20
	"LUMP_AREAPORTALS",						// 21
	"LUMP_FACEBRUSHES",						// 22
	"LUMP_FACEBRUSHLIST",					// 23
	"LUMP_UNUSED1",							// 24
	"LUMP_UNUSED2",							// 25
	"LUMP_DISPINFO",						// 26
	"LUMP_ORIGINALFACES",					// 27
	"LUMP_PHYSDISP",						// 28
	"LUMP_PHYSCOLLIDE",						// 29
	"LUMP_VERTNORMALS",						// 30
	"LUMP_VERTNORMALINDICES",				// 31
	"LUMP_DISP_LIGHTMAP_ALPHAS",			// 32
	"LUMP_DISP_VERTS",						// 33
	"LUMP_DISP_LIGHTMAP_SAMPLE_POSITIONS",	// 34
	"LUMP_GAME_LUMP",						// 35
	"LUMP_LEAFWATERDATA",					// 36
	"LUMP_PRIMITIVES",						// 37
	"LUMP_PRIMVERTS",						// 38
	"LUMP_PRIMINDICES",						// 39
	"LUMP_PAKFILE",							// 40
	"LUMP_CLIPPORTALVERTS",					// 41
	"LUMP_CUBEMAPS",						// 42
	"LUMP_TEXDATA_STRING_DATA",				// 43
	"LUMP_TEXDATA_STRING_TABLE",			// 44
	"LUMP_OVERLAYS",						// 45
	"LUMP_LEAFMINDISTTOWATER",				// 46
	"LUMP_FACE_MACRO_TEXTURE_INFO",			// 47
	"LUMP_DISP_TRIS",						// 48
	"LUMP_PROP_BLOB",						// 49
	"LUMP_WATEROVERLAYS",					// 50
	"LUMP_LEAF_AMBIENT_INDEX_HDR",			// 51
	"LUMP_LEAF_AMBIENT_INDEX",				// 52
	"LUMP_LIGHTING_HDR",					// 53
	"LUMP_WORLDLIGHTS_HDR",					// 54
	"LUMP_LEAF_AMBIENT_LIGHTING_HDR",		// 55
	"LUMP_LEAF_AMBIENT_LIGHTING",			// 56
	"LUMP_XZIPPAKFILE",						// 57
	"LUMP_FACES_HDR",						// 58
	"LUMP_MAP_FLAGS",						// 59
	"LUMP_OVERLAY_FADES",					// 60
	"LUMP_OVERLAY_SYSTEM_LEVELS",           // 61
	"LUMP_PHYSLEVEL",                       // 62
	"LUMP_DISP_MULTIBLEND"					// 63
};

const char *GetLumpName( unsigned int lumpnum )
{
	if ( lumpnum >= ARRAYSIZE( s_LumpNames ) )
	{
		return "UNKNOWN";
	}
	return s_LumpNames[lumpnum];
}

// "-hdr" tells us to use the HDR fields (if present) on the light sources.  Also, tells us to write
// out the HDR lumps for lightmaps, ambient leaves, and lights sources.
bool g_bHDR = false;

uint32 g_LevelFlags = 0;

int			nummodels;
dmodel_t	dmodels[MAX_MAP_MODELS];

int			visdatasize;
byte		dvisdata[MAX_MAP_VISIBILITY];
dvis_t		*dvis = (dvis_t *)dvisdata;

CUtlVector<byte> dlightdataHDR;
CUtlVector<byte> dlightdataLDR;
CUtlVector<byte> *pdlightdata = &dlightdataLDR;

CUtlVector<char> dentdata;

int			numleafs;
#if !defined( BSP_USE_LESS_MEMORY )
dleaf_t		dleafs[MAX_MAP_LEAFS];
#else
dleaf_t		*dleafs;
#endif

CUtlVector<dleafambientindex_t> g_LeafAmbientIndexLDR;
CUtlVector<dleafambientindex_t> g_LeafAmbientIndexHDR;
CUtlVector<dleafambientindex_t> *g_pLeafAmbientIndex = nullptr;
CUtlVector<dleafambientlighting_t> g_LeafAmbientLightingLDR;
CUtlVector<dleafambientlighting_t> g_LeafAmbientLightingHDR;
CUtlVector<dleafambientlighting_t> *g_pLeafAmbientLighting = nullptr;

unsigned short  g_LeafMinDistToWater[MAX_MAP_LEAFS];

int			numplanes;
dplane_t	dplanes[MAX_MAP_PLANES];

int			numvertexes;
dvertex_t	dvertexes[MAX_MAP_VERTS];

int				g_numvertnormalindices;	// dfaces reference these. These index g_vertnormals.
unsigned short	g_vertnormalindices[MAX_MAP_VERTNORMALS];

int				g_numvertnormals;	
Vector			g_vertnormals[MAX_MAP_VERTNORMALS];

int			numnodes;
dnode_t		dnodes[MAX_MAP_NODES];

CUtlVector<texinfo_t> texinfo( MAX_MAP_TEXINFO );

int			numtexdata;
dtexdata_t	dtexdata[MAX_MAP_TEXDATA];

//
// displacement map bsp file info: dispinfo
//
CUtlVector<ddispinfo_t> g_dispinfo;
CUtlVector<CDispVert> g_DispVerts;
CUtlVector<CDispTri> g_DispTris;
CUtlVector<CDispMultiBlend> g_DispMultiBlend;
CUtlVector<unsigned char> g_DispLightmapSamplePositions; // LUMP_DISP_LIGHTMAP_SAMPLE_POSITIONS

int         numorigfaces;
dface_t     dorigfaces[MAX_MAP_FACES];

int				g_numprimitives = 0;
dprimitive_t	g_primitives[MAX_MAP_PRIMITIVES];

int				g_numprimverts = 0;
dprimvert_t		g_primverts[MAX_MAP_PRIMVERTS];

int				g_numprimindices = 0;
unsigned short	g_primindices[MAX_MAP_PRIMINDICES];

int			numfaces;
dface_t		dfaces[MAX_MAP_FACES];

int			numfaceids;
CUtlVector<dfaceid_t>	dfaceids;

CUtlVector<uint16>	dfacebrushes;
CUtlVector<dfacebrushlist_t>	dfacebrushlists;

int			numfaces_hdr;
dface_t		dfaces_hdr[MAX_MAP_FACES];

int			numedges;
dedge_t		dedges[MAX_MAP_EDGES];

int			numleaffaces;
unsigned short		dleaffaces[MAX_MAP_LEAFFACES];

int			numleafbrushes;
unsigned short		dleafbrushes[MAX_MAP_LEAFBRUSHES];

int			numsurfedges;
int			dsurfedges[MAX_MAP_SURFEDGES];

int			numbrushes;
dbrush_t	dbrushes[MAX_MAP_BRUSHES];

int			numbrushsides;
dbrushside_t	dbrushsides[MAX_MAP_BRUSHSIDES];

int			numareas;
darea_t		dareas[MAX_MAP_AREAS];

int			numareaportals;
dareaportal_t	dareaportals[MAX_MAP_AREAPORTALS];

int			numworldlightsLDR;
dworldlight_t dworldlightsLDR[MAX_MAP_WORLDLIGHTS];

int			numworldlightsHDR;
dworldlight_t dworldlightsHDR[MAX_MAP_WORLDLIGHTS];

int			*pNumworldlights = &numworldlightsLDR;
dworldlight_t *dworldlights = dworldlightsLDR;

int			numleafwaterdata = 0;
dleafwaterdata_t dleafwaterdata[MAX_MAP_LEAFWATERDATA]; 

CUtlVector<CFaceMacroTextureInfo>	g_FaceMacroTextureInfos;

Vector				g_ClipPortalVerts[MAX_MAP_PORTALVERTS];
int					g_nClipPortalVerts;

dcubemapsample_t	g_CubemapSamples[MAX_MAP_CUBEMAPSAMPLES];
int					g_nCubemapSamples = 0;

int						g_nOverlayCount;
doverlay_t				g_Overlays[MAX_MAP_OVERLAYS];
doverlayfade_t			g_OverlayFades[MAX_MAP_OVERLAYS];
doverlaysystemlevel_t	g_OverlaySystemLevels[MAX_MAP_OVERLAYS];

int					g_nWaterOverlayCount;
dwateroverlay_t		g_WaterOverlays[MAX_MAP_WATEROVERLAYS];

CUtlVector<char>	g_TexDataStringData;
CUtlVector<int>		g_TexDataStringTable;

byte				*g_pPhysCollide = nullptr;
int					g_PhysCollideSize = 0;
int					g_MapRevision = 0;

byte               *g_pPhysLevel = nullptr;
int                 g_PhysLevelSize = 0;

byte				*g_pPhysDisp = nullptr;
int					g_PhysDispSize = 0;

CUtlVector<doccluderdata_t>	g_OccluderData( 256, 256 );
CUtlVector<doccluderpolydata_t>	g_OccluderPolyData( 1024, 1024 );
CUtlVector<int>	g_OccluderVertexIndices( 2048, 2048 );

template <class T> static void WriteData( T *pData, int count = 1 );
template <class T> static void WriteData( int fieldType, T *pData, int count = 1 );
template <class T> static void AddLump( int lumpnum, T *pData, int count, int version = 0, int nAlignment = 1 );
template <class T> static void AddLump( int lumpnum, CUtlVector<T> &data, int version = 0, int nAlignment = 1 );

BSPHeader_t		*g_pBSPHeader;
FileHandle_t	g_hBSPFile;

struct Lumps_t
{
	void	*pLumps[HEADER_LUMPS];
	int		size[HEADER_LUMPS];
	bool	bLumpParsed[HEADER_LUMPS];
	bool	bLumpFixed[HEADER_LUMPS];
} g_Lumps;

CGameLump	g_GameLumps;

static IZip *s_pakFile = 0;

//-----------------------------------------------------------------------------
// Keep the file position aligned to an arbitrary boundary.
// Returns updated file position.
//-----------------------------------------------------------------------------
static unsigned int AlignFilePosition( FileHandle_t hFile, int alignment )
{
	unsigned int currPosition = g_pFileSystem->Tell( hFile );

	if ( alignment >= 2 )
	{
		unsigned int newPosition = AlignValue( currPosition, alignment );
		unsigned int count = newPosition - currPosition;
		if ( count )
		{
			char *pBuffer;
			char smallBuffer[4096];
			if ( count > sizeof( smallBuffer ) )
			{
				pBuffer = (char *)malloc( count );
			}
			else
			{
				pBuffer = smallBuffer;
			}

			memset( pBuffer, 0, count );
			SafeWrite( hFile, pBuffer, count );

			if ( pBuffer != smallBuffer )
			{
				free( pBuffer );
			}

			currPosition = newPosition;
		}
	}

	return currPosition;
}

//-----------------------------------------------------------------------------
// Purpose: // Get a pakfile instance
// Output : IZip*
//-----------------------------------------------------------------------------
IZip* GetPakFile( void )
{
	if ( !s_pakFile )
	{
		s_pakFile = IZip::CreateZip();
	}
	return s_pakFile;
}

//-----------------------------------------------------------------------------
// Purpose: Free the pak files
//-----------------------------------------------------------------------------
void ReleasePakFileLumps( void )
{
	// Release the pak files
	IZip::ReleaseZip( s_pakFile );
	s_pakFile = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Set the sector alignment for all subsequent zip operations
//-----------------------------------------------------------------------------
void ForceAlignment( IZip *pak, bool bAlign, bool bCompatibleFormat, unsigned int alignmentSize )
{
	pak->ForceAlignment( bAlign, bCompatibleFormat, alignmentSize );
}

//-----------------------------------------------------------------------------
// Purpose: Store data back out to .bsp file
//-----------------------------------------------------------------------------
static void WritePakFileLump( void )
{
	CUtlBuffer buf( 0, 0 );
	GetPakFile()->SaveToBuffer( buf );

	// must respect pak file alignment
	// pad up and ensure lump starts on same aligned boundary
	AlignFilePosition( g_hBSPFile, GetPakFile()->GetAlignment() );
	
	// Now store final buffers out to file
	AddLump( LUMP_PAKFILE, (byte*)buf.Base(), buf.TellPut() );
}

//-----------------------------------------------------------------------------
// Purpose: Remove all entries
//-----------------------------------------------------------------------------
void ClearPakFile( IZip *pak )
{
	pak->Reset();
}

//-----------------------------------------------------------------------------
// Purpose: Add file from disk to .bsp PAK lump
// Input  : *relativename - 
//			*fullpath - 
//-----------------------------------------------------------------------------
void AddFileToPak( IZip *pak, const char *relativename, const char *fullpath )
{
	pak->AddFileToZip( relativename, fullpath );
}

//-----------------------------------------------------------------------------
// Purpose: Add buffer to .bsp PAK lump as named file
// Input  : *relativename - 
//			*data - 
//			length - 
//-----------------------------------------------------------------------------
void AddBufferToPak( IZip *pak, const char *pRelativeName, void *data, int length, bool bTextMode )
{
	pak->AddBufferToZip( pRelativeName, data, length, bTextMode );
}

//-----------------------------------------------------------------------------
// Purpose: Check if a file already exists in the pack file.
// Input  : *relativename - 
//-----------------------------------------------------------------------------
bool FileExistsInPak( IZip *pak, const char *pRelativeName )
{
	return pak->FileExistsInZip( pRelativeName );
}


//-----------------------------------------------------------------------------
// Read a file from the pack file
//-----------------------------------------------------------------------------
bool ReadFileFromPak( IZip *pak, const char *pRelativeName, bool bTextMode, CUtlBuffer &buf )
{
	return pak->ReadFileFromZip( pRelativeName, bTextMode, buf );
}


//-----------------------------------------------------------------------------
// Purpose: Remove file from .bsp PAK lump
// Input  : *relativename - 
//-----------------------------------------------------------------------------
void RemoveFileFromPak( IZip *pak, const char *relativename )
{
	pak->RemoveFileFromZip( relativename );
}


//-----------------------------------------------------------------------------
// Purpose: Get next filename in directory
// Input  : id, -1 to start, returns next id, or -1 at list conclusion 
//-----------------------------------------------------------------------------
int GetNextFilename( IZip *pak, int id, char *pBuffer, int bufferSize, int &fileSize )
{
	return pak->GetNextFilename( id, pBuffer, bufferSize, fileSize );
}

//-----------------------------------------------------------------------------
// Convert four-CC code to a handle	+ back
//-----------------------------------------------------------------------------
GameLumpHandle_t CGameLump::GetGameLumpHandle( GameLumpId_t id )
{
	// NOTE: I'm also expecting game lump id's to be four-CC codes
	Assert( id > HEADER_LUMPS );

	FOR_EACH_LL(m_GameLumps, i)
	{
		if (m_GameLumps[i].m_Id == id)
			return i;
	}

	return InvalidGameLump();
}

GameLumpId_t CGameLump::GetGameLumpId( GameLumpHandle_t handle )
{
	return m_GameLumps[handle].m_Id;
}

int	CGameLump::GetGameLumpFlags( GameLumpHandle_t handle )
{
	return m_GameLumps[handle].m_Flags;
}

int	CGameLump::GetGameLumpVersion( GameLumpHandle_t handle )
{
	return m_GameLumps[handle].m_Version;
}


//-----------------------------------------------------------------------------
// Game lump accessor methods 
//-----------------------------------------------------------------------------

void*	CGameLump::GetGameLump( GameLumpHandle_t id )
{
	return m_GameLumps[id].m_Memory.Base();
}

int		CGameLump::GameLumpSize( GameLumpHandle_t id )
{
	return m_GameLumps[id].m_Memory.NumAllocated();
}


//-----------------------------------------------------------------------------
// Game lump iteration methods 
//-----------------------------------------------------------------------------

GameLumpHandle_t	CGameLump::FirstGameLump()
{
	return (m_GameLumps.Count()) ? m_GameLumps.Head() : InvalidGameLump();
}

GameLumpHandle_t	CGameLump::NextGameLump( GameLumpHandle_t handle )
{

	return (m_GameLumps.IsValidIndex(handle)) ? m_GameLumps.Next(handle) : InvalidGameLump();
}

GameLumpHandle_t	CGameLump::InvalidGameLump()
{
	return 0xFFFF;
}


//-----------------------------------------------------------------------------
// Game lump creation/destruction method
//-----------------------------------------------------------------------------

GameLumpHandle_t	CGameLump::CreateGameLump( GameLumpId_t id, int size, int flags, int version )
{
	Assert( GetGameLumpHandle(id) == InvalidGameLump() );
	GameLumpHandle_t handle = m_GameLumps.AddToTail();
	m_GameLumps[handle].m_Id = id;
	m_GameLumps[handle].m_Flags = flags;
	m_GameLumps[handle].m_Version = version;
	m_GameLumps[handle].m_Memory.EnsureCapacity( size );
	return handle;
}

void	CGameLump::DestroyGameLump( GameLumpHandle_t handle )
{
	m_GameLumps.Remove( handle );
}

void	CGameLump::DestroyAllGameLumps()
{
	m_GameLumps.RemoveAll();
}

//-----------------------------------------------------------------------------
// Compute file size and clump count
//-----------------------------------------------------------------------------

void CGameLump::ComputeGameLumpSizeAndCount( int& size, int& clumpCount )
{
	// Figure out total size of the client lumps
	size = 0;
	clumpCount = 0;
	GameLumpHandle_t h;
	for( h = FirstGameLump(); h != InvalidGameLump(); h = NextGameLump( h ) )
	{
		++clumpCount;
		size += GameLumpSize( h );
	}

	// Add on headers
	size += sizeof( dgamelumpheader_t ) + clumpCount * sizeof( dgamelump_t );
}


//-----------------------------------------------------------------------------
// Game lump file I/O
//-----------------------------------------------------------------------------
void CGameLump::ParseGameLump( BSPHeader_t* pHeader )
{
	g_GameLumps.DestroyAllGameLumps();

	g_Lumps.bLumpParsed[LUMP_GAME_LUMP] = true;

	int length = pHeader->lumps[LUMP_GAME_LUMP].filelen;
	int ofs = pHeader->lumps[LUMP_GAME_LUMP].fileofs;
	
	if (length > 0)
	{
		// Read dictionary...
		dgamelumpheader_t* pGameLumpHeader = (dgamelumpheader_t*)((byte *)pHeader + ofs);
		dgamelump_t* pGameLump = (dgamelump_t*)(pGameLumpHeader + 1);
		for (int i = 0; i < pGameLumpHeader->lumpCount; ++i )
		{
			int length = pGameLump[i].filelen;
			GameLumpHandle_t lump = g_GameLumps.CreateGameLump( pGameLump[i].id, length, pGameLump[i].flags, pGameLump[i].version );
			memcpy( g_GameLumps.GetGameLump(lump), (byte *)pHeader + pGameLump[i].fileofs, length );
		}
	}
}


//-----------------------------------------------------------------------------
// String table methods
//-----------------------------------------------------------------------------
const char *TexDataStringTable_GetString( int stringID )
{
	return &g_TexDataStringData[g_TexDataStringTable[stringID]];
}

int	TexDataStringTable_AddOrFindString( const char *pString )
{
	int i;
	// garymcthack: Make this use an RBTree!
	for( i = 0; i < g_TexDataStringTable.Count(); i++ )
	{
		if( stricmp( pString, &g_TexDataStringData[g_TexDataStringTable[i]] ) == 0 )
		{
			return i;
		}
	}

	int len = strlen( pString );
	int outOffset = g_TexDataStringData.AddMultipleToTail( len+1, pString );
	int outIndex = g_TexDataStringTable.AddToTail( outOffset );
	return outIndex;
}

//-----------------------------------------------------------------------------
// Adds all game lumps into one big block
//-----------------------------------------------------------------------------
static void AddGameLumps()
{
	// Figure out total size of the client lumps
	int size, clumpCount;
	g_GameLumps.ComputeGameLumpSizeAndCount( size, clumpCount );

	// Set up the main lump dictionary entry
	g_Lumps.size[LUMP_GAME_LUMP] = 0;	// mark it written

	lump_t* lump = &g_pBSPHeader->lumps[LUMP_GAME_LUMP];
	
	lump->fileofs = g_pFileSystem->Tell( g_hBSPFile );
	lump->filelen = size;

	// write header
	dgamelumpheader_t header;
	header.lumpCount = clumpCount;
	WriteData( &header );

	// write dictionary
	dgamelump_t dict;
	// offset is absolute
	int offset = lump->fileofs + sizeof( header ) + clumpCount * sizeof( dgamelump_t );
	GameLumpHandle_t h;
	for ( h = g_GameLumps.FirstGameLump(); h != g_GameLumps.InvalidGameLump(); h = g_GameLumps.NextGameLump( h ) )
	{
		dict.id = g_GameLumps.GetGameLumpId(h);
		dict.version = g_GameLumps.GetGameLumpVersion(h);
		dict.flags = g_GameLumps.GetGameLumpFlags(h);
		dict.fileofs = offset;
		dict.filelen = g_GameLumps.GameLumpSize( h );
		offset += dict.filelen;

		WriteData( &dict );
	}

	// write game lump data
	// offsets march along in the same manner as the dictionary encoded them
	for ( h = g_GameLumps.FirstGameLump(); h != g_GameLumps.InvalidGameLump(); h = g_GameLumps.NextGameLump( h ) )
	{
		unsigned int lumpsize = g_GameLumps.GameLumpSize(h);
		SafeWrite( g_hBSPFile, g_GameLumps.GetGameLump(h), lumpsize );
	}

	// stay aligned on lump boundaries
	AlignFilePosition( g_hBSPFile, LUMP_ALIGNMENT );
}


//-----------------------------------------------------------------------------
// Adds the occluder lump...
//-----------------------------------------------------------------------------
static void AddOcclusionLump( )
{
	g_Lumps.size[LUMP_OCCLUSION] = 0;	// mark it written

	int nOccluderCount = g_OccluderData.Count();
	int nOccluderPolyDataCount = g_OccluderPolyData.Count();
	int nOccluderVertexIndices = g_OccluderVertexIndices.Count();

	int nLumpLength = nOccluderCount * sizeof(doccluderdata_t) +
		nOccluderPolyDataCount * sizeof(doccluderpolydata_t) +
		nOccluderVertexIndices * sizeof(int) +
		3 * sizeof(int);

	lump_t *lump = &g_pBSPHeader->lumps[LUMP_OCCLUSION];
	
	lump->fileofs = g_pFileSystem->Tell( g_hBSPFile );
	lump->filelen = nLumpLength;
	lump->version = LUMP_OCCLUSION_VERSION;
	lump->fourCC[0] = ( char )0;
	lump->fourCC[1] = ( char )0;
	lump->fourCC[2] = ( char )0;
	lump->fourCC[3] = ( char )0;

	// Data is swapped in place, so the 'Count' variables aren't safe to use after they're written
	WriteData( FIELD_INTEGER, &nOccluderCount );
	WriteData( (doccluderdata_t*)g_OccluderData.Base(), g_OccluderData.Count() );
	WriteData( FIELD_INTEGER, &nOccluderPolyDataCount );
	WriteData( (doccluderpolydata_t*)g_OccluderPolyData.Base(), g_OccluderPolyData.Count() );
	WriteData( FIELD_INTEGER, &nOccluderVertexIndices );
	WriteData( FIELD_INTEGER, (int*)g_OccluderVertexIndices.Base(), g_OccluderVertexIndices.Count() );
}


//-----------------------------------------------------------------------------
// Loads the occluder lump...
//-----------------------------------------------------------------------------
static void UnserializeOcclusionLumpV2( CUtlBuffer &buf )
{
	int nCount = buf.GetInt();
	if ( nCount )
	{
		g_OccluderData.SetCount( nCount );
		buf.GetObjects( g_OccluderData.Base(), nCount );
	}

	nCount = buf.GetInt();
	if ( nCount )
	{
		g_OccluderPolyData.SetCount( nCount );
		buf.GetObjects( g_OccluderPolyData.Base(), nCount );
	}

	nCount = buf.GetInt();
	if ( nCount )
	{
		g_OccluderVertexIndices.SetCount( nCount );
		buf.Get( g_OccluderVertexIndices.Base(), nCount * sizeof(g_OccluderVertexIndices[0]) );
	}
}


static void LoadOcclusionLump()
{
	g_OccluderData.RemoveAll();
	g_OccluderPolyData.RemoveAll();
	g_OccluderVertexIndices.RemoveAll();

	int		length, ofs;

	g_Lumps.bLumpParsed[LUMP_OCCLUSION] = true;

	length = g_pBSPHeader->lumps[LUMP_OCCLUSION].filelen;
	ofs = g_pBSPHeader->lumps[LUMP_OCCLUSION].fileofs;
	
	CUtlBuffer buf( (byte *)g_pBSPHeader + ofs, length, CUtlBuffer::READ_ONLY );
	switch ( g_pBSPHeader->lumps[LUMP_OCCLUSION].version )
	{
	case 2:
		UnserializeOcclusionLumpV2( buf );
		break;

	case 0:
		break;

	default:
		Error("Unknown occlusion lump version!\n");
		break;
	}
}


/*
===============
CompressVis

===============
*/
int CompressVis (byte *vis, byte *dest)
{
	int		j;
	int		rep;
	int		visrow;
	byte	*dest_p;
	
	dest_p = dest;
//	visrow = (r_numvisleafs + 7)>>3;
	visrow = (dvis->numclusters + 7)>>3;
	
	for (j=0 ; j<visrow ; j++)
	{
		*dest_p++ = vis[j];
		if (vis[j])
			continue;

		rep = 1;
		for ( j++; j<visrow ; j++)
			if (vis[j] || rep == 255)
				break;
			else
				rep++;
		*dest_p++ = rep;
		j--;
	}
	
	return dest_p - dest;
}


/*
===================
DecompressVis
===================
*/
void DecompressVis (byte *in, byte *decompressed)
{
	int		c;
	byte	*out;
	int		row;

//	row = (r_numvisleafs+7)>>3;	
	row = (dvis->numclusters+7)>>3;	
	out = decompressed;

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		if (!c)
			Error ("DecompressVis: 0 repeat");
		in += 2;
		if ((out - decompressed) + c > row)
		{
			c = row - (out - decompressed);
			Warning( "warning: Vis decompression overrun\n" );
		}
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);
}

// UNDONE: This code is not yet tested.

// UNDONE: This code is not yet tested.

//=============================================================================
void Lumps_Init( void )
{
	memset( &g_Lumps, 0, sizeof(g_Lumps) );
}

int LumpVersion( int lump )
{
	return g_pBSPHeader->lumps[lump].version;
}

bool HasLump( int lump )
{
	return g_pBSPHeader->lumps[lump].filelen > 0;
}

void ValidateLump( int lump, int length, int size, int forceVersion )
{
	if ( length % size )
	{
		Error( "ValidateLump: odd size for lump %d", lump );
	}

	if ( forceVersion >= 0 && forceVersion != g_pBSPHeader->lumps[lump].version )
	{
		Error( "ValidateLump: old version for lump %d in map!", lump );
	}
}

//-----------------------------------------------------------------------------
//	Add Lumps of integral types without datadescs
//-----------------------------------------------------------------------------
template <class T>
int CopyLumpInternal( int nFieldType, int nLump, T *pDest, int nForceVersion )
{
	g_Lumps.bLumpParsed[nLump] = true;

	// Vectors are passed in as floats
	int nFieldSize = ( nFieldType == FIELD_VECTOR ) ? sizeof( Vector ) : sizeof( T );

	unsigned int nLength = g_pBSPHeader->lumps[nLump].filelen;
	if ( g_Lumps.bLumpFixed[nLump] )
	{
		nLength = g_Lumps.size[nLump];
	}

	ValidateLump( nLump, nLength, nFieldSize, nForceVersion );

	// count must be of the integral type
	unsigned int nCount = nLength / sizeof( T );

	byte *pLumpData;
	if ( g_Lumps.bLumpFixed[nLump] )
	{
		pLumpData = (byte *)g_Lumps.pLumps[nLump];
	}
	else
	{
		unsigned int ofs = g_pBSPHeader->lumps[nLump].fileofs;
		pLumpData = ( (byte*)g_pBSPHeader + ofs );
	}

	memcpy( pDest, pLumpData, nLength );

	// Return actual count of elements
	return nLength / nFieldSize;
}

template <class T>
int CopyLump( int fieldType, int lump, T *dest, int forceVersion = -1 )
{
	return CopyLumpInternal( fieldType, lump, dest, forceVersion );
}

template <class T>
void CopyLump( int fieldType, int lump, CUtlVector<T> &dest, int forceVersion = -1 )
{
	Assert( fieldType != FIELD_VECTOR ); // TODO: Support this if necessary
	dest.SetSize( g_pBSPHeader->lumps[lump].filelen / sizeof(T) );
	CopyLumpInternal( fieldType, lump, dest.Base(), forceVersion );
}

template <class T>
void CopyOptionalLump( int fieldType, int lump, CUtlVector<T> &dest, int forceVersion = -1 )
{
	// not fatal if not present
	if ( !HasLump( lump ) )
		return;

	dest.SetSize( g_pBSPHeader->lumps[lump].filelen / sizeof(T) );
	CopyLumpInternal( fieldType, lump, dest.Base(), forceVersion );
}

template <class T>
int CopyVariableLump( int fieldType, int lump, void **dest, int forceVersion = -1 )
{
	int length = g_pBSPHeader->lumps[lump].filelen;
	*dest = malloc( length );

	return CopyLumpInternal<T>( fieldType, lump, (T*)*dest, forceVersion );
}

//-----------------------------------------------------------------------------
//	Add Lumps of object types with datadescs
//-----------------------------------------------------------------------------
template <class T>
int CopyLumpInternal( int nLump, T *pDest, int forceVersion )
{
	g_Lumps.bLumpParsed[nLump] = true;

	unsigned int nLength = g_pBSPHeader->lumps[nLump].filelen;
	if ( g_Lumps.bLumpFixed[nLump] )
	{
		nLength = g_Lumps.size[nLump];
	}

	ValidateLump( nLump, nLength, sizeof( T ), forceVersion );

	unsigned int nCount = nLength / sizeof( T );

	byte *pLumpData;
	if ( g_Lumps.bLumpFixed[nLump] )
	{
		// lump has been pre-fixed, use that data
		pLumpData = (byte *)g_Lumps.pLumps[nLump];
	}
	else
	{
		unsigned int nFileOffset = g_pBSPHeader->lumps[nLump].fileofs;
		pLumpData = (byte*)g_pBSPHeader + nFileOffset;
	}

	memcpy( pDest, pLumpData, nLength );

	return nCount;
}

template <class T>
int CopyLump( int lump, T *dest, int forceVersion = -1 )
{
	return CopyLumpInternal( lump, dest, forceVersion );
}

template <class T>
void CopyLump( int lump, CUtlVector<T> &dest, int forceVersion = -1 )
{
	dest.SetSize( g_pBSPHeader->lumps[lump].filelen / sizeof(T) );
	CopyLumpInternal( lump, dest.Base(), forceVersion );
}

template <class T>
void CopyOptionalLump( int lump, CUtlVector<T> &dest, int forceVersion = -1 )
{
	// not fatal if not present
	if ( !HasLump( lump ) )
		return;

	dest.SetSize( g_pBSPHeader->lumps[lump].filelen / sizeof(T) );
	CopyLumpInternal( lump, dest.Base(), forceVersion );
}

template <class T>
int CopyVariableLump( int lump, void **dest, int forceVersion = -1 )
{
	int length = g_pBSPHeader->lumps[lump].filelen;
	*dest = malloc( length );

	return CopyLumpInternal<T>( lump, (T*)*dest, forceVersion );
}

//-----------------------------------------------------------------------------
//	Add/Write unknown lumps
//-----------------------------------------------------------------------------
void Lumps_Parse( void )
{
	int i;

	for ( i = 0; i < HEADER_LUMPS; i++ )
	{
		if ( !g_Lumps.bLumpParsed[i] && g_pBSPHeader->lumps[i].filelen )
		{
			g_Lumps.size[i] = CopyVariableLump<byte>( FIELD_CHARACTER, i, &g_Lumps.pLumps[i], -1 );
			Msg( "Reading unknown lump #%d (%d bytes)\n", i, g_Lumps.size[i] );
		}
	}
}

void Lumps_Write( void )
{
	int i;

	for ( i = 0; i < HEADER_LUMPS; i++ )
	{
		if ( g_Lumps.size[i] )
		{
			Msg( "Writing unknown lump #%d (%d bytes)\n", i, g_Lumps.size[i] );
			AddLump( i, (byte*)g_Lumps.pLumps[i], g_Lumps.size[i] );
		}
		if ( g_Lumps.pLumps[i] )
		{
			free( g_Lumps.pLumps[i] );
			g_Lumps.pLumps[i] = nullptr;
		}
	}
}

int LoadLeafs( void )
{
#if defined( BSP_USE_LESS_MEMORY )
	dleafs = (dleaf_t*)malloc( g_pBSPHeader->lumps[LUMP_LEAFS].filelen );
#endif

	switch ( LumpVersion( LUMP_LEAFS ) )
	{
	case 0:
		{
			g_Lumps.bLumpParsed[LUMP_LEAFS] = true;
			int length = g_pBSPHeader->lumps[LUMP_LEAFS].filelen;
			int size = sizeof( dleaf_version_0_t );
			if ( length % size )
			{
				Error( "odd size for LUMP_LEAFS\n" );
			}
			int count = length / size;

			void *pSrcBase = ( ( byte * )g_pBSPHeader + g_pBSPHeader->lumps[LUMP_LEAFS].fileofs );
			dleaf_version_0_t *pSrc = (dleaf_version_0_t *)pSrcBase;
			dleaf_t *pDst = dleafs;

			// version 0 predates HDR, build the LDR
			g_LeafAmbientLightingLDR.SetCount( count );
			g_LeafAmbientIndexLDR.SetCount( count );

			dleafambientlighting_t *pDstLeafAmbientLighting = &g_LeafAmbientLightingLDR[0];
			for ( int i = 0; i < count; i++ )
			{
				g_LeafAmbientIndexLDR[i].ambientSampleCount = 1;
				g_LeafAmbientIndexLDR[i].firstAmbientSample = i;
		
				// pDst is a subset of pSrc;
				*pDst = *( ( dleaf_t * )( void * )pSrc );
				pDstLeafAmbientLighting->cube = pSrc->m_AmbientLighting;
				pDstLeafAmbientLighting->x = pDstLeafAmbientLighting->y = pDstLeafAmbientLighting->z = pDstLeafAmbientLighting->pad = 0;
				pDst++;
				pSrc++;
				pDstLeafAmbientLighting++;
			}
			return count;
		}

	case 1:
		return CopyLump( LUMP_LEAFS, dleafs );

	default:
		Assert( 0 );
		Error( "Unknown LUMP_LEAFS version\n" );
		return 0;
	}
}

void LoadLeafAmbientLighting( int numLeafs )
{
	if ( LumpVersion( LUMP_LEAFS ) == 0 )
	{
		// an older leaf version already built the LDR ambient lighting on load
		return;
	}

	// old BSP with ambient, or new BSP with no lighting, convert ambient light to new format or create dummy ambient
	if ( !HasLump( LUMP_LEAF_AMBIENT_INDEX ) )
	{
		// a bunch of legacy maps, have these lumps with garbage versions
		// expect them to be NOT the current version
		if ( HasLump(LUMP_LEAF_AMBIENT_LIGHTING) )
		{
			Assert( LumpVersion( LUMP_LEAF_AMBIENT_LIGHTING ) != LUMP_LEAF_AMBIENT_LIGHTING_VERSION );
		}
		if ( HasLump(LUMP_LEAF_AMBIENT_LIGHTING_HDR) )
		{
			Assert( LumpVersion( LUMP_LEAF_AMBIENT_LIGHTING_HDR ) != LUMP_LEAF_AMBIENT_LIGHTING_VERSION );
		}

		void *pSrcBase = ( ( byte * )g_pBSPHeader + g_pBSPHeader->lumps[LUMP_LEAF_AMBIENT_LIGHTING].fileofs );
		CompressedLightCube *pSrc = nullptr;
		if ( HasLump( LUMP_LEAF_AMBIENT_LIGHTING ) )
		{
			pSrc = (CompressedLightCube*)pSrcBase;
		}
		g_LeafAmbientIndexLDR.SetCount( numLeafs );
		g_LeafAmbientLightingLDR.SetCount( numLeafs );

		void *pSrcBaseHDR = ( ( byte * )g_pBSPHeader + g_pBSPHeader->lumps[LUMP_LEAF_AMBIENT_LIGHTING_HDR].fileofs );
		CompressedLightCube *pSrcHDR = nullptr;
		if ( HasLump( LUMP_LEAF_AMBIENT_LIGHTING_HDR ) )
		{
			pSrcHDR = (CompressedLightCube*)pSrcBaseHDR;
		}
		g_LeafAmbientIndexHDR.SetCount( numLeafs );
		g_LeafAmbientLightingHDR.SetCount( numLeafs );

		for ( int i = 0; i < numLeafs; i++ )
		{
			g_LeafAmbientIndexLDR[i].ambientSampleCount = 1;
			g_LeafAmbientIndexLDR[i].firstAmbientSample = i;
			g_LeafAmbientIndexHDR[i].ambientSampleCount = 1;
			g_LeafAmbientIndexHDR[i].firstAmbientSample = i;

			Q_memset( &g_LeafAmbientLightingLDR[i], 0, sizeof(g_LeafAmbientLightingLDR[i]) );
			Q_memset( &g_LeafAmbientLightingHDR[i], 0, sizeof(g_LeafAmbientLightingHDR[i]) );

			if ( pSrc )
			{
				g_LeafAmbientLightingLDR[i].cube = pSrc[i];
			}
			if ( pSrcHDR )
			{
				g_LeafAmbientLightingHDR[i].cube = pSrcHDR[i];
			}
		}

		g_Lumps.bLumpParsed[LUMP_LEAF_AMBIENT_LIGHTING] = true;
		g_Lumps.bLumpParsed[LUMP_LEAF_AMBIENT_INDEX] = true;
		g_Lumps.bLumpParsed[LUMP_LEAF_AMBIENT_LIGHTING_HDR] = true;
		g_Lumps.bLumpParsed[LUMP_LEAF_AMBIENT_INDEX_HDR] = true;
	}
	else
	{
		CopyOptionalLump( LUMP_LEAF_AMBIENT_LIGHTING, g_LeafAmbientLightingLDR );
		CopyOptionalLump( LUMP_LEAF_AMBIENT_INDEX, g_LeafAmbientIndexLDR );
		CopyOptionalLump( LUMP_LEAF_AMBIENT_LIGHTING_HDR, g_LeafAmbientLightingHDR );
		CopyOptionalLump( LUMP_LEAF_AMBIENT_INDEX_HDR, g_LeafAmbientIndexHDR );
	}
}

int LoadWorldLights( int lumpnum,  dworldlight_t *pWorldlights )
{
	Assert( lumpnum == LUMP_WORLDLIGHTS || lumpnum == LUMP_WORLDLIGHTS_HDR );
	if ( lumpnum != LUMP_WORLDLIGHTS && lumpnum != LUMP_WORLDLIGHTS_HDR )
	{
		Error( "Unexpected worldlight lumpnum %d\n", lumpnum );
	}
	if ( !HasLump( lumpnum ) )
	{
		return 0;
	}

	int count = 0;
	int version = LumpVersion( lumpnum );
	if ( version == 0 )
	{
		// old version
		dworldlight_version0_t *pOldWorldlights = (dworldlight_version0_t *)malloc( g_pBSPHeader->lumps[lumpnum].filelen );
		count = CopyLump( lumpnum, pOldWorldlights );

		for ( int i = 0; i < count; i++ )
		{
			pWorldlights[i].origin = pOldWorldlights[i].origin;
			pWorldlights[i].intensity = pOldWorldlights[i].intensity;
			pWorldlights[i].normal = pOldWorldlights[i].normal;
			pWorldlights[i].shadow_cast_offset.Init( 0.0f, 0.0f, 0.0f );
			pWorldlights[i].cluster = pOldWorldlights[i].cluster;
			pWorldlights[i].type = pOldWorldlights[i].type;
			pWorldlights[i].style = pOldWorldlights[i].style;
			pWorldlights[i].stopdot = pOldWorldlights[i].stopdot;
			pWorldlights[i].stopdot2 = pOldWorldlights[i].stopdot2;
			pWorldlights[i].exponent = pOldWorldlights[i].exponent;
			pWorldlights[i].radius = pOldWorldlights[i].radius;
			pWorldlights[i].constant_attn = pOldWorldlights[i].constant_attn;	
			pWorldlights[i].linear_attn = pOldWorldlights[i].linear_attn;
			pWorldlights[i].quadratic_attn = pOldWorldlights[i].quadratic_attn;
			pWorldlights[i].flags = pOldWorldlights[i].flags;
			pWorldlights[i].texinfo = pOldWorldlights[i].texinfo;
			pWorldlights[i].owner = pOldWorldlights[i].owner;
		}

		free( pOldWorldlights );
	}
	else if ( version == LUMP_WORLDLIGHTS_VERSION )
	{
		count = CopyLump( lumpnum, pWorldlights );
	}
	else
	{
		Error( "Unexpected worldlights lump version %d\n", version );
	}

	return count;
}

void ValidateHeader( const char *filename, const BSPHeader_t *pHeader )
{
	if ( pHeader->ident != IDBSPHEADER )
	{
		Error ("%s is not a IBSP file", filename);
	}
	if ( pHeader->m_nVersion < MINBSPVERSION || pHeader->m_nVersion > BSPVERSION )
	{
		Error ("%s is version %i, not %i", filename, pHeader->m_nVersion, BSPVERSION);
	}
}

//-----------------------------------------------------------------------------
//	Low level BSP opener for external parsing. Parses headers, but nothing else.
//	You must close the BSP, via CloseBSPFile().
//-----------------------------------------------------------------------------
void OpenBSPFile( const char *filename )
{
	Lumps_Init();

	// load the file header
	LoadFile( filename, (void **)&g_pBSPHeader );

	ValidateHeader( filename, g_pBSPHeader );

	g_MapRevision = g_pBSPHeader->mapRevision;

	g_LevelFlags = 0;
	if ( HasLump( LUMP_MAP_FLAGS ) )
	{
		dflagslump_t flagsLump;

		memcpy( &flagsLump, (dflagslump_t *)((byte *)g_pBSPHeader + g_pBSPHeader->lumps[LUMP_MAP_FLAGS].fileofs ), sizeof( flagsLump ) );

		g_LevelFlags = flagsLump.m_LevelFlags;
	}
}

//-----------------------------------------------------------------------------
//	CloseBSPFile
//-----------------------------------------------------------------------------
void CloseBSPFile( void )
{
	free( g_pBSPHeader );
	g_pBSPHeader = nullptr;
}

//-----------------------------------------------------------------------------
//	LoadBSPFile
//-----------------------------------------------------------------------------
void LoadBSPFile( const char *filename )
{
	OpenBSPFile( filename );

	nummodels = CopyLump( LUMP_MODELS, dmodels );
	numvertexes = CopyLump( LUMP_VERTEXES, dvertexes );
	numplanes = CopyLump( LUMP_PLANES, dplanes );
	numleafs = LoadLeafs();
	numnodes = CopyLump( LUMP_NODES, dnodes );
	CopyLump( LUMP_TEXINFO, texinfo );
	numtexdata = CopyLump( LUMP_TEXDATA, dtexdata );
    
	CopyLump( LUMP_DISPINFO, g_dispinfo );
    CopyLump( LUMP_DISP_VERTS, g_DispVerts );
	CopyLump( LUMP_DISP_TRIS, g_DispTris );
	CopyLump( LUMP_DISP_MULTIBLEND, g_DispMultiBlend );
    CopyLump( FIELD_CHARACTER, LUMP_DISP_LIGHTMAP_SAMPLE_POSITIONS, g_DispLightmapSamplePositions );
	CopyLump( LUMP_FACE_MACRO_TEXTURE_INFO, g_FaceMacroTextureInfos );
	
	numfaces = CopyLump(LUMP_FACES, dfaces, LUMP_FACES_VERSION);
	if ( HasLump( LUMP_FACES_HDR ) )
		numfaces_hdr = CopyLump( LUMP_FACES_HDR, dfaces_hdr, LUMP_FACES_VERSION );
	else
		numfaces_hdr = 0;

	CopyOptionalLump( LUMP_FACEIDS, dfaceids );

	g_numprimitives = CopyLump( LUMP_PRIMITIVES, g_primitives );
	g_numprimverts = CopyLump( LUMP_PRIMVERTS, g_primverts );
	g_numprimindices = CopyLump( FIELD_SHORT, LUMP_PRIMINDICES, g_primindices );
    numorigfaces = CopyLump( LUMP_ORIGINALFACES, dorigfaces );   // original faces
	numleaffaces = CopyLump( FIELD_SHORT, LUMP_LEAFFACES, dleaffaces );
	numleafbrushes = CopyLump( FIELD_SHORT, LUMP_LEAFBRUSHES, dleafbrushes );
	CopyLump( FIELD_SHORT, LUMP_FACEBRUSHES, dfacebrushes );
	CopyLump( LUMP_FACEBRUSHLIST, dfacebrushlists );

	numsurfedges = CopyLump( FIELD_INTEGER, LUMP_SURFEDGES, dsurfedges );
	numedges = CopyLump( LUMP_EDGES, dedges );
	numbrushes = CopyLump( LUMP_BRUSHES, dbrushes );
	numbrushsides = CopyLump( LUMP_BRUSHSIDES, dbrushsides );
	numareas = CopyLump( LUMP_AREAS, dareas );
	numareaportals = CopyLump( LUMP_AREAPORTALS, dareaportals );

	visdatasize = CopyLump ( FIELD_CHARACTER, LUMP_VISIBILITY, dvisdata );
	CopyOptionalLump( FIELD_CHARACTER, LUMP_LIGHTING, dlightdataLDR, LUMP_LIGHTING_VERSION );
	CopyOptionalLump( FIELD_CHARACTER, LUMP_LIGHTING_HDR, dlightdataHDR, LUMP_LIGHTING_VERSION );

	LoadLeafAmbientLighting( numleafs );

	CopyLump( FIELD_CHARACTER, LUMP_ENTITIES, dentdata );

	numworldlightsLDR = LoadWorldLights( LUMP_WORLDLIGHTS, dworldlightsLDR );
	numworldlightsHDR = LoadWorldLights( LUMP_WORLDLIGHTS_HDR, dworldlightsHDR );
	
	numleafwaterdata = CopyLump( LUMP_LEAFWATERDATA, dleafwaterdata );
	g_PhysCollideSize = CopyVariableLump<byte>( FIELD_CHARACTER, LUMP_PHYSCOLLIDE, (void**)&g_pPhysCollide );
	g_PhysLevelSize = CopyVariableLump<byte>( FIELD_CHARACTER, LUMP_PHYSLEVEL, (void**)&g_pPhysLevel );
	g_PhysDispSize = CopyVariableLump<byte>( FIELD_CHARACTER, LUMP_PHYSDISP, (void**)&g_pPhysDisp );

	g_numvertnormals = CopyLump( FIELD_VECTOR, LUMP_VERTNORMALS, (float*)g_vertnormals );
	g_numvertnormalindices = CopyLump( FIELD_SHORT, LUMP_VERTNORMALINDICES, g_vertnormalindices );

	g_nClipPortalVerts = CopyLump( FIELD_VECTOR, LUMP_CLIPPORTALVERTS, (float*)g_ClipPortalVerts );
	g_nCubemapSamples = CopyLump( LUMP_CUBEMAPS, g_CubemapSamples );	

	CopyLump( FIELD_CHARACTER, LUMP_TEXDATA_STRING_DATA, g_TexDataStringData );
	CopyLump( FIELD_INTEGER, LUMP_TEXDATA_STRING_TABLE, g_TexDataStringTable );

	g_nOverlayCount = CopyLump( LUMP_OVERLAYS, g_Overlays );
	g_nWaterOverlayCount = CopyLump( LUMP_WATEROVERLAYS, g_WaterOverlays );
	CopyLump( LUMP_OVERLAY_FADES, g_OverlayFades );
	CopyLump( LUMP_OVERLAY_SYSTEM_LEVELS, g_OverlaySystemLevels );
	
	dflagslump_t flags_lump;
	
	if ( HasLump( LUMP_MAP_FLAGS ) )
		CopyLump ( LUMP_MAP_FLAGS, &flags_lump );
	else
		memset( &flags_lump, 0, sizeof( flags_lump ) );			// default flags to 0

	g_LevelFlags = flags_lump.m_LevelFlags;

	LoadOcclusionLump();

	CopyLump( FIELD_SHORT, LUMP_LEAFMINDISTTOWATER, g_LeafMinDistToWater );

	/*
	int crap;
	for( crap = 0; crap < g_nBSPStringTable; crap++ )
	{
		Msg( "stringtable %d", ( int )crap );
		Msg( " %d:",  ( int )g_BSPStringTable[crap] );
		puts( &g_BSPStringData[g_BSPStringTable[crap]] );
		puts( "\n" );
	}
	*/
		
	// Load PAK file lump into appropriate data structure
	byte *pakbuffer = nullptr;
	int paksize = CopyVariableLump<byte>( FIELD_CHARACTER, LUMP_PAKFILE, ( void ** )&pakbuffer );
	if ( paksize > 0 )
	{
		GetPakFile()->ParseFromBuffer( pakbuffer, paksize );
	}
	else
	{
		GetPakFile()->Reset();
	}

	free( pakbuffer );

	g_GameLumps.ParseGameLump( g_pBSPHeader );

	// NOTE: Do NOT call CopyLump after Lumps_Parse() it parses all un-Copied lumps
	// parse any additional lumps
	Lumps_Parse();

	// everything has been copied out
	CloseBSPFile();
}

//-----------------------------------------------------------------------------
// Reset any state.
//-----------------------------------------------------------------------------
void UnloadBSPFile()
{
	nummodels = 0;
	numvertexes = 0;
	numplanes = 0;

	numleafs = 0;
#if defined( BSP_USE_LESS_MEMORY )
	if ( dleafs )
	{ 
		free( dleafs );
		dleafs = nullptr;
	}
#endif

	numnodes = 0;
	texinfo.Purge();
	numtexdata = 0;

	g_dispinfo.Purge();
	g_DispVerts.Purge();
	g_DispTris.Purge();
	g_DispMultiBlend.Purge();

	g_DispLightmapSamplePositions.Purge();
	g_FaceMacroTextureInfos.Purge();

	numfaces = 0;
	numfaces_hdr = 0;

	dfaceids.Purge();
	dfacebrushes.Purge();
	dfacebrushlists.Purge();

	g_numprimitives = 0;
	g_numprimverts = 0;
	g_numprimindices = 0;
	numorigfaces = 0;
	numleaffaces = 0;
	numleafbrushes = 0;
	numsurfedges = 0;
	numedges = 0;
	numbrushes = 0;
	numbrushsides = 0;
	numareas = 0;
	numareaportals = 0;

	visdatasize = 0;
	dlightdataLDR.Purge();
	dlightdataHDR.Purge();

	g_LeafAmbientLightingLDR.Purge();
	g_LeafAmbientLightingHDR.Purge();
	g_LeafAmbientIndexHDR.Purge();
	g_LeafAmbientIndexLDR.Purge();

	dentdata.Purge();
	numworldlightsLDR = 0;
	numworldlightsHDR = 0;

	numleafwaterdata = 0;

	if ( g_pPhysCollide )
	{
		free( g_pPhysCollide );
		g_pPhysCollide = nullptr;
	}
	g_PhysCollideSize = 0;

	if ( g_pPhysDisp )
	{
		free( g_pPhysDisp );
		g_pPhysDisp = nullptr;
	}
	g_PhysDispSize = 0;

	g_numvertnormals = 0;
	g_numvertnormalindices = 0;

	g_nClipPortalVerts = 0;
	g_nCubemapSamples = 0;	

	g_TexDataStringData.Purge();
	g_TexDataStringTable.Purge();

	g_nOverlayCount = 0;
	g_nWaterOverlayCount = 0;

	g_LevelFlags = 0;

	g_OccluderData.Purge();
	g_OccluderPolyData.Purge();
	g_OccluderVertexIndices.Purge();

	g_GameLumps.DestroyAllGameLumps();

	for ( int i = 0; i < HEADER_LUMPS; i++ )
	{
		if ( g_Lumps.pLumps[i] )
		{
			free( g_Lumps.pLumps[i] );
			g_Lumps.pLumps[i] = nullptr;
		}
	}

	ReleasePakFileLumps();
}

//-----------------------------------------------------------------------------
//	LoadBSPFileFilesystemOnly
//-----------------------------------------------------------------------------
void LoadBSPFile_FileSystemOnly( const char *filename )
{
	Lumps_Init();

	//
	// load the file header
	//
	LoadFile( filename, (void **)&g_pBSPHeader );
	ValidateHeader( filename, g_pBSPHeader );

	// Load PAK file lump into appropriate data structure
	byte *pakbuffer = nullptr;
	int paksize = CopyVariableLump<byte>( FIELD_CHARACTER, LUMP_PAKFILE, ( void ** )&pakbuffer, 1 );
	if ( paksize > 0 )
	{
		GetPakFile()->ParseFromBuffer( pakbuffer, paksize );
	}
	else
	{
		GetPakFile()->Reset();
	}

	free( pakbuffer );

	// everything has been copied out
	free( g_pBSPHeader );
	g_pBSPHeader = nullptr;
}

/*
=============
LoadBSPFileTexinfo

Only loads the texinfo lump, so qdata can scan for textures
=============
*/
void LoadBSPFileTexinfo( const char *filename )
{
	FILE		*f;
	int		length, ofs;

	g_pBSPHeader = new BSPHeader_t;

	f = fopen( filename, "rb" );
	fread( g_pBSPHeader, sizeof( *g_pBSPHeader ), 1, f);

	ValidateHeader( filename, g_pBSPHeader );

	length = g_pBSPHeader->lumps[LUMP_TEXINFO].filelen;
	ofs = g_pBSPHeader->lumps[LUMP_TEXINFO].fileofs;

	int nCount = length / sizeof(texinfo_t);

	texinfo.Purge();
	texinfo.AddMultipleToTail( nCount );

	fseek( f, ofs, SEEK_SET );
	fread( texinfo.Base(), length, 1, f );
	fclose( f );

	// everything has been copied out
	free( g_pBSPHeader );
	g_pBSPHeader = nullptr;
}

static void AddLumpInternal( int lumpnum, void *data, int len, int version, int nAlignment = 1 )
{
	Assert( IsPowerOfTwo( nAlignment ) );

	lump_t *lump;

	g_Lumps.size[lumpnum] = 0;	// mark it written

	lump = &g_pBSPHeader->lumps[lumpnum];

	// pre-align to desired write position
	// this may be increased alignment as needed by lumps
	AlignFilePosition( g_hBSPFile, nAlignment );
	lump->fileofs = g_pFileSystem->Tell( g_hBSPFile );

	lump->filelen = len;
	lump->version = version;
	lump->fourCC[0] = ( char )0;
	lump->fourCC[1] = ( char )0;
	lump->fourCC[2] = ( char )0;
	lump->fourCC[3] = ( char )0;

	SafeWrite( g_hBSPFile, data, len );

	// pad out to the next default aligned position
	AlignFilePosition( g_hBSPFile, LUMP_ALIGNMENT );
}

//-----------------------------------------------------------------------------
//	Add raw data chunk to file (not a lump)
//-----------------------------------------------------------------------------
template <class T>
static void WriteData( int fieldType, T *pData, int count )
{
	SafeWrite( g_hBSPFile, pData, count * sizeof(T) );
}

template <class T>
static void WriteData( T *pData, int count )
{
	SafeWrite( g_hBSPFile, pData, count * sizeof(T) );
}

//-----------------------------------------------------------------------------
//	Add Lump of object types with datadescs
//-----------------------------------------------------------------------------
template <class T>
static void AddLump( int lumpnum, T *pData, int count, int version, int nAlignment )
{
	AddLumpInternal( lumpnum, pData, count * sizeof(T), version, nAlignment );
}

template <class T>
static void AddLump( int lumpnum, CUtlVector<T> &data, int version, int nAlignment )
{
	AddLumpInternal( lumpnum, data.Base(), data.Count() * sizeof(T), version, nAlignment );
}

/*
=============
WriteBSPFile

Swaps the bsp file in place, so it should not be referenced again
=============
*/
void WriteBSPFile( const char *filename, char *pUnused )
{		
	if ( texinfo.Count() > MAX_MAP_TEXINFO )
	{
		Error( "Map has too many texinfos (has %d, can have at most %d)\n", texinfo.Count(), MAX_MAP_TEXINFO );
		return;
	}

	BSPHeader_t outHeader;
	g_pBSPHeader = &outHeader;
	memset( g_pBSPHeader, 0, sizeof( BSPHeader_t ) );

	g_pBSPHeader->ident = IDBSPHEADER;
	g_pBSPHeader->m_nVersion = BSPVERSION;
	g_pBSPHeader->mapRevision = g_MapRevision;

	g_hBSPFile = SafeOpenWrite( filename );
	WriteData( g_pBSPHeader );	// overwritten later

	// start on aligned boundary
	AlignFilePosition( g_hBSPFile, LUMP_ALIGNMENT );

	// NOTE: Write these in the order the engine will read them
	dflagslump_t flags_lump;
	flags_lump.m_LevelFlags = g_LevelFlags;
	AddLump( LUMP_MAP_FLAGS, &flags_lump, 1 );

	// first write texture data
	AddLump( LUMP_TEXINFO, texinfo );
	AddLump( LUMP_TEXDATA, dtexdata, numtexdata );    
	AddLump( LUMP_TEXDATA_STRING_DATA, g_TexDataStringData );
	AddLump( LUMP_TEXDATA_STRING_TABLE, g_TexDataStringTable );
	// Now write collision data
	AddLump( LUMP_LEAFS, dleafs, numleafs, LUMP_LEAFS_VERSION );
	AddLump( LUMP_LEAFBRUSHES, dleafbrushes, numleafbrushes );
	AddLump( LUMP_PLANES, dplanes, numplanes );
	AddLump( LUMP_BRUSHES, dbrushes, numbrushes );
	AddLump( LUMP_BRUSHSIDES, dbrushsides, numbrushsides );
	AddLump( LUMP_MODELS, dmodels, nummodels );
	AddLump( LUMP_NODES, dnodes, numnodes );
	// visibility data
	AddLump( LUMP_AREAS, dareas, numareas );
	AddLump( LUMP_AREAPORTALS, dareaportals, numareaportals );
	AddLump( LUMP_VISIBILITY, dvisdata, visdatasize );
	// entity text
	AddLump( LUMP_ENTITIES, dentdata );

	if ( g_pPhysCollide )
	{
		AddLump( LUMP_PHYSCOLLIDE, g_pPhysCollide, g_PhysCollideSize );
	}
	if ( g_pPhysLevel )
	{
		// PhysLevel is 16-byte aligned (will be 128-byte aligned for PS/3)
		AddLump( LUMP_PHYSLEVEL, g_pPhysLevel, g_PhysLevelSize, LUMP_PHYSLEVEL_ALIGNMENT );
	}

    AddLump( LUMP_DISPINFO, g_dispinfo );
	AddLump( LUMP_VERTEXES, dvertexes, numvertexes );
	AddLump( LUMP_EDGES, dedges, numedges );
	AddLump( LUMP_SURFEDGES, dsurfedges, numsurfedges );
    AddLump( LUMP_FACES, dfaces, numfaces, LUMP_FACES_VERSION );
    if ( numfaces_hdr )
	{
		AddLump( LUMP_FACES_HDR, dfaces_hdr, numfaces_hdr, LUMP_FACES_VERSION );
	}

    AddLump( LUMP_DISP_VERTS, g_DispVerts );
	AddLump( LUMP_DISP_TRIS, g_DispTris );
	AddLump( LUMP_DISP_MULTIBLEND, g_DispMultiBlend );
	
	if ( g_pPhysDisp )
	{
		AddLump ( LUMP_PHYSDISP, g_pPhysDisp, g_PhysDispSize );
	}

	AddOcclusionLump();

	AddLump( LUMP_LIGHTING, dlightdataLDR, LUMP_LIGHTING_VERSION );
	AddLump( LUMP_LIGHTING_HDR, dlightdataHDR, LUMP_LIGHTING_VERSION );

	AddLump( LUMP_PRIMITIVES, g_primitives, g_numprimitives );
	AddLump( LUMP_PRIMVERTS, g_primverts, g_numprimverts );
	AddLump( LUMP_PRIMINDICES, g_primindices, g_numprimindices );
	AddLump( LUMP_VERTNORMALS, (float*)g_vertnormals, g_numvertnormals * 3 );
	AddLump( LUMP_VERTNORMALINDICES, g_vertnormalindices, g_numvertnormalindices );
	AddLump( LUMP_LEAF_AMBIENT_LIGHTING, g_LeafAmbientLightingLDR, LUMP_LEAF_AMBIENT_LIGHTING_VERSION );
	AddLump( LUMP_LEAF_AMBIENT_INDEX, g_LeafAmbientIndexLDR );
	AddLump( LUMP_LEAF_AMBIENT_INDEX_HDR, g_LeafAmbientIndexHDR );
	AddLump( LUMP_LEAF_AMBIENT_LIGHTING_HDR, g_LeafAmbientLightingHDR, LUMP_LEAF_AMBIENT_LIGHTING_VERSION );
	AddLump( LUMP_LEAFFACES, dleaffaces, numleaffaces );
	AddLump( LUMP_LEAFWATERDATA, dleafwaterdata, numleafwaterdata );

	AddLump( LUMP_OVERLAYS, g_Overlays, g_nOverlayCount );
	AddLump( LUMP_WATEROVERLAYS, g_WaterOverlays, g_nWaterOverlayCount );
	AddLump( LUMP_OVERLAY_FADES, g_OverlayFades, g_nOverlayCount );
	AddLump( LUMP_OVERLAY_SYSTEM_LEVELS, g_OverlaySystemLevels, g_nOverlayCount );

	AddLump( LUMP_LEAFMINDISTTOWATER, g_LeafMinDistToWater, numleafs );

	AddLump( LUMP_CUBEMAPS, g_CubemapSamples, g_nCubemapSamples );
	AddLump( LUMP_CLIPPORTALVERTS, (float*)g_ClipPortalVerts, g_nClipPortalVerts * 3 );
	AddLump( LUMP_WORLDLIGHTS, dworldlightsLDR, numworldlightsLDR, LUMP_WORLDLIGHTS_VERSION );
	AddLump( LUMP_WORLDLIGHTS_HDR, dworldlightsHDR, numworldlightsHDR, LUMP_WORLDLIGHTS_VERSION );


    AddLump( LUMP_DISP_LIGHTMAP_SAMPLE_POSITIONS, g_DispLightmapSamplePositions );
	AddLump( LUMP_FACE_MACRO_TEXTURE_INFO, g_FaceMacroTextureInfos );
 
	AddLump ( LUMP_FACEIDS, dfaceids, numfaceids );
	AddLump ( LUMP_FACEBRUSHES, dfacebrushes );
	AddLump ( LUMP_FACEBRUSHLIST, dfacebrushlists );

	AddLump( LUMP_ORIGINALFACES, dorigfaces, numorigfaces );     // original faces lump

	// NOTE: This is just for debugging, so it is disabled in release maps

	AddGameLumps();

	// Write pakfile lump to disk
	WritePakFileLump();

	// NOTE: Do NOT call AddLump after Lumps_Write() it writes all un-Added lumps
	// write any additional lumps
	Lumps_Write();

	g_pFileSystem->Seek( g_hBSPFile, 0, FILESYSTEM_SEEK_HEAD );
	WriteData( g_pBSPHeader );
	g_pFileSystem->Close( g_hBSPFile );
}

// Generate the next clear lump filename for the bsp file
bool GenerateNextLumpFileName( const char *bspfilename, char *lumpfilename, int buffsize )
{
	for (int i = 0; i < MAX_LUMPFILES; i++)
	{
		GenerateLumpFileName( bspfilename, lumpfilename, buffsize, i );
	
		if ( !g_pFileSystem->FileExists( lumpfilename ) )
			return true;
	}

	return false;
}

void WriteLumpToFile( char *filename, int lump )
{
	if ( !HasLump(lump) )
		return;

	char lumppre[MAX_PATH];	
	if ( !GenerateNextLumpFileName( filename, lumppre, MAX_PATH ) )
	{
		Warning( "Failed to find valid lump filename for bsp %s.\n", filename );
		return;
	}

	// Open the file
	FileHandle_t lumpfile = g_pFileSystem->Open(lumppre, "wb");
	if ( !lumpfile )
	{
		Error ("Error opening %s! (Check for write enable)\n",filename);
		return;
	}

	int ofs = g_pBSPHeader->lumps[lump].fileofs;
	int length = g_pBSPHeader->lumps[lump].filelen;

	// Write the header
	lumpfileheader_t lumpHeader;
	lumpHeader.lumpID = lump;
	lumpHeader.lumpVersion = LumpVersion(lump);
	lumpHeader.lumpLength = length;
	lumpHeader.mapRevision = LittleLong( g_MapRevision );
	lumpHeader.lumpOffset = sizeof(lumpfileheader_t);	// Lump starts after the header
	SafeWrite (lumpfile, &lumpHeader, sizeof(lumpfileheader_t));

	// Write the lump
	SafeWrite (lumpfile, (byte *)g_pBSPHeader + ofs, length);
}

//============================================================================
#define ENTRIES(a)		(sizeof(a)/sizeof(*(a)))
#define ENTRYSIZE(a)	(sizeof(*(a)))

int ArrayUsage( char *szItem, int items, int maxitems, int itemsize )
{
	float	percentage = maxitems ? items * 100.0 / maxitems : 0.0;

    Msg("%-17.17s %8i/%-8i %8i/%-8i (%4.1f%%) ", 
		   szItem, items, maxitems, items * itemsize, maxitems * itemsize, percentage );
	if ( percentage > 80.0 )
		Msg( "VERY FULL!\n" );
	else if ( percentage > 95.0 )
		Msg( "SIZE DANGER!\n" );
	else if ( percentage > 99.9 )
		Msg( "SIZE OVERFLOW!!!\n" );
	else
		Msg( "\n" );
	return items * itemsize;
}

int GlobUsage( char *szItem, int itemstorage, int maxstorage )
{
	float	percentage = maxstorage ? itemstorage * 100.0 / maxstorage : 0.0;
    Msg("%-17.17s     [variable]    %8i/%-8i (%4.1f%%) ", 
		   szItem, itemstorage, maxstorage, percentage );
	if ( percentage > 80.0 )
		Msg( "VERY FULL!\n" );
	else if ( percentage > 95.0 )
		Msg( "SIZE DANGER!\n" );
	else if ( percentage > 99.9 )
		Msg( "SIZE OVERFLOW!!!\n" );
	else
		Msg( "\n" );
	return itemstorage;
}

/*
=============
PrintBSPFileSizes

Dumps info about current file
=============
*/
void PrintBSPFileSizes (void)
{
	int	totalmemory = 0;

//	if (!num_entities)
//		ParseEntities ();

	Msg("\n");
	Msg( "%-17s %16s %16s %9s \n", "Object names", "Objects/Maxobjs", "Memory / Maxmem", "Fullness" );
	Msg( "%-17s %16s %16s %9s \n",  "------------", "---------------", "---------------", "--------" );

	totalmemory += ArrayUsage( "models",		nummodels,		ENTRIES(dmodels),		ENTRYSIZE(dmodels) );
	totalmemory += ArrayUsage( "brushes",		numbrushes,		ENTRIES(dbrushes),		ENTRYSIZE(dbrushes) );
	totalmemory += ArrayUsage( "brushsides",	numbrushsides,	ENTRIES(dbrushsides),	ENTRYSIZE(dbrushsides) );
	totalmemory += ArrayUsage( "planes",		numplanes,		ENTRIES(dplanes),		ENTRYSIZE(dplanes) );
	totalmemory += ArrayUsage( "vertexes",		numvertexes,	ENTRIES(dvertexes),		ENTRYSIZE(dvertexes) );
	totalmemory += ArrayUsage( "nodes",			numnodes,		ENTRIES(dnodes),		ENTRYSIZE(dnodes) );
	totalmemory += ArrayUsage( "texinfos",		texinfo.Count(),MAX_MAP_TEXINFO,		sizeof(texinfo_t) );
	totalmemory += ArrayUsage( "texdata",		numtexdata,		ENTRIES(dtexdata),		ENTRYSIZE(dtexdata) );
    
	totalmemory += ArrayUsage( "dispinfos",			g_dispinfo.Count(),			0,			sizeof( ddispinfo_t ) );
    totalmemory += ArrayUsage( "disp_verts",		g_DispVerts.Count(),		0,			sizeof( g_DispVerts[0] ) );
    totalmemory += ArrayUsage( "disp_tris",			g_DispTris.Count(),			0,			sizeof( g_DispTris[0] ) );
	totalmemory += ArrayUsage( "disp_multiblend",	g_DispMultiBlend.Count(),	0,			sizeof( g_DispMultiBlend[0] ) );
    totalmemory += ArrayUsage( "disp_lmsamples",	g_DispLightmapSamplePositions.Count(),0,sizeof( g_DispLightmapSamplePositions[0] ) );
	
	totalmemory += ArrayUsage( "faces",			numfaces,		ENTRIES(dfaces),		ENTRYSIZE(dfaces) );
	totalmemory += ArrayUsage( "hdr faces",     numfaces_hdr,	ENTRIES(dfaces_hdr),	ENTRYSIZE(dfaces_hdr) );
    totalmemory += ArrayUsage( "origfaces",     numorigfaces,   ENTRIES(dorigfaces),    ENTRYSIZE(dorigfaces) );    // original faces
	totalmemory += ArrayUsage( "facebrushes",	dfacebrushes.Count(),			0,		ENTRYSIZE(dfacebrushes.Base()) );
	totalmemory += ArrayUsage( "facebrushlists",dfacebrushlists.Count(),		0,		ENTRYSIZE(dfacebrushlists.Base()) );
	totalmemory += ArrayUsage( "leaves",		numleafs,		ENTRIES(dleafs),		ENTRYSIZE(dleafs) );
	totalmemory += ArrayUsage( "leaffaces",		numleaffaces,	ENTRIES(dleaffaces),	ENTRYSIZE(dleaffaces) );
	totalmemory += ArrayUsage( "leafbrushes",	numleafbrushes,	ENTRIES(dleafbrushes),	ENTRYSIZE(dleafbrushes) );
	totalmemory += ArrayUsage( "areas",	numareas,	ENTRIES(dareas),	ENTRYSIZE(dareas) );
	totalmemory += ArrayUsage( "surfedges",		numsurfedges,	ENTRIES(dsurfedges),	ENTRYSIZE(dsurfedges) );
	totalmemory += ArrayUsage( "edges",			numedges,		ENTRIES(dedges),		ENTRYSIZE(dedges) );
	totalmemory += ArrayUsage( "LDR worldlights",	numworldlightsLDR,	ENTRIES(dworldlightsLDR),	ENTRYSIZE(dworldlightsLDR) );
	totalmemory += ArrayUsage( "HDR worldlights",	numworldlightsHDR,	ENTRIES(dworldlightsHDR),	ENTRYSIZE(dworldlightsHDR) );

	totalmemory += ArrayUsage( "leafwaterdata",	numleafwaterdata,ENTRIES(dleafwaterdata),	ENTRYSIZE(dleafwaterdata) );
	totalmemory += ArrayUsage( "waterstrips",	g_numprimitives,ENTRIES(g_primitives),	ENTRYSIZE(g_primitives) );
	totalmemory += ArrayUsage( "waterverts",	g_numprimverts,	ENTRIES(g_primverts),	ENTRYSIZE(g_primverts) );
	totalmemory += ArrayUsage( "waterindices",	g_numprimindices,ENTRIES(g_primindices),ENTRYSIZE(g_primindices) );
	totalmemory += ArrayUsage( "cubemapsamples", g_nCubemapSamples,ENTRIES(g_CubemapSamples),ENTRYSIZE(g_CubemapSamples) );
	totalmemory += ArrayUsage( "overlays",      g_nOverlayCount, ENTRIES(g_Overlays),   ENTRYSIZE(g_Overlays) );
	
	totalmemory += GlobUsage( "LDR lightdata",		dlightdataLDR.Count(),	0 );
	totalmemory += GlobUsage( "HDR lightdata",	dlightdataHDR.Count(),	0 );
	totalmemory += GlobUsage( "visdata",		visdatasize,	sizeof(dvisdata) );
	totalmemory += GlobUsage( "entdata",		dentdata.Count(), 384*1024 );	// goal is <384K

	totalmemory += ArrayUsage( "LDR ambient table", g_LeafAmbientIndexLDR.Count(), MAX_MAP_LEAFS, sizeof( g_LeafAmbientIndexLDR[0] ) );
	totalmemory += ArrayUsage( "HDR ambient table", g_LeafAmbientIndexHDR.Count(), MAX_MAP_LEAFS, sizeof( g_LeafAmbientIndexHDR[0] ) );
	totalmemory += ArrayUsage( "LDR leaf ambient lighting", g_LeafAmbientLightingLDR.Count(), MAX_MAP_LEAFS, sizeof( g_LeafAmbientLightingLDR[0] ) );
	totalmemory += ArrayUsage( "HDR leaf ambient lighting", g_LeafAmbientLightingHDR.Count(), MAX_MAP_LEAFS, sizeof( g_LeafAmbientLightingHDR[0] ) );

	totalmemory += ArrayUsage( "occluders",     g_OccluderData.Count(),	0, sizeof( g_OccluderData[0] ) );
    totalmemory += ArrayUsage( "occluder polygons",	g_OccluderPolyData.Count(),	0, sizeof( g_OccluderPolyData[0] ) );
    totalmemory += ArrayUsage( "occluder vert ind",g_OccluderVertexIndices.Count(),0, sizeof( g_OccluderVertexIndices[0] ) );

	GameLumpHandle_t h = g_GameLumps.GetGameLumpHandle( GAMELUMP_DETAIL_PROPS );
	if (h != g_GameLumps.InvalidGameLump())
		totalmemory += GlobUsage( "detail props",	1,	g_GameLumps.GameLumpSize(h) );
	h = g_GameLumps.GetGameLumpHandle( GAMELUMP_DETAIL_PROP_LIGHTING );
	if (h != g_GameLumps.InvalidGameLump())
		totalmemory += GlobUsage( "dtl prp lght",	1,	g_GameLumps.GameLumpSize(h) );
	h = g_GameLumps.GetGameLumpHandle( GAMELUMP_DETAIL_PROP_LIGHTING_HDR );
	if (h != g_GameLumps.InvalidGameLump())
		totalmemory += GlobUsage( "HDR dtl prp lght",	1,	g_GameLumps.GameLumpSize(h) );
	h = g_GameLumps.GetGameLumpHandle( GAMELUMP_STATIC_PROPS );
	if (h != g_GameLumps.InvalidGameLump())
		totalmemory += GlobUsage( "static props",	1,	g_GameLumps.GameLumpSize(h) );

	totalmemory += GlobUsage( "pakfile",		GetPakFile()->EstimateSize(), 0 );
	// HACKHACK: Set physics limit at 4MB, in reality this is totally dynamic
	totalmemory += GlobUsage( "physics",		g_PhysCollideSize, 4*1024*1024 );
	totalmemory += GlobUsage( "physics terrain",		g_PhysDispSize, 1*1024*1024 );

	Msg( "\nLevel flags = %x\n", g_LevelFlags );

	Msg( "\n" );

	int triangleCount = 0;

	for ( int i = 0; i < numfaces; i++ )
	{
		// face tris = numedges - 2
		triangleCount += dfaces[i].numedges - 2;
	}
	Msg("Total triangle count: %d\n", triangleCount );

	// UNDONE: 
	// areaportals, portals, texdata, clusters, worldlights, portalverts
}

/*
=============
PrintBSPPackDirectory

Dumps a list of files stored in the bsp pack.
=============
*/
void PrintBSPPackDirectory( void )
{
	GetPakFile()->PrintDirectory();	
}


//============================================

int			num_entities;
entity_t	entities[MAX_MAP_ENTITIES];

void StripTrailing (char *e)
{
	char	*s;

	s = e + strlen(e)-1;
	while (s >= e && *s <= 32)
	{
		*s = 0;
		s--;
	}
}

/*
=================
ParseEpair
=================
*/
epair_t *ParseEpair (void)
{
	epair_t	*e;

	e = (epair_t*)malloc (sizeof(epair_t));
	memset (e, 0, sizeof(epair_t));
	StripTrailing( token );
	if (strlen(token) >= MAX_KEY-1)
		Error ("ParseEpar: key token too long [%s]", token);
	e->key = copystring(token);
	GetToken (false);
	StripTrailing( token );
	if (strlen(token) >= MAX_VALUE-1)
		Error ("ParseEpar: value token too long [%s]", token);
	e->value = copystring(token);

	return e;
}

void RemoveKey( entity_t *pMapEnt, const char *pKey )
{
	epair_t **pPrev = &pMapEnt->epairs;
	for ( epair_t *pSearch = pMapEnt->epairs; pSearch != nullptr; pPrev = &pSearch->next, pSearch = pSearch->next)
	{
		if (!Q_stricmp( pSearch->key, pKey ) )
		{
			(*pPrev) = pSearch->next;
			free( pSearch );
			return;
		}
	}
}
/*
================
ParseEntity
================
*/
qboolean	ParseEntity (void)
{
	epair_t		*e;
	entity_t	*mapent;

	if (!GetToken (true))
		return false;

	if (Q_stricmp (token, "{") )
		Error ("ParseEntity: { not found");
	
	if (num_entities == MAX_MAP_ENTITIES)
		Error ("num_entities == MAX_MAP_ENTITIES");

	mapent = &entities[num_entities];
	num_entities++;

	do
	{
		if (!GetToken (true))
			Error ("ParseEntity: EOF without closing brace");
		if (!Q_stricmp (token, "}") )
			break;
		e = ParseEpair ();
		e->next = mapent->epairs;
		mapent->epairs = e;
	} while (1);
	
	return true;
}

/*
================
ParseEntities

Parses the dentdata string into entities
================
*/
void ParseEntities (void)
{
	num_entities = 0;
	ParseFromMemory (dentdata.Base(), dentdata.Count());

	while (ParseEntity ())
	{
	}	
}


/*
================
UnparseEntities

Generates the dentdata string from all the entities
================
*/
void UnparseEntities (void)
{
	epair_t	*ep;
	char	line[2048];
	int		i;
	char	key[1024], value[1024];

	CUtlBuffer buffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
	buffer.EnsureCapacity( 256 * 1024 );
	
	for (i=0 ; i<num_entities ; i++)
	{
		ep = entities[i].epairs;
		if (!ep)
			continue;	// ent got removed
		
		buffer.PutString( "{\n" );
				
		for (ep = entities[i].epairs ; ep ; ep=ep->next)
		{
			if ( ep->key[ 0 ] == 0 )
			{
				continue;
			}
			strcpy (key, ep->key);
			StripTrailing (key);
			strcpy (value, ep->value);
			StripTrailing (value);
				
			V_sprintf_safe(line, "\"%s\" \"%s\"\n", key, value);
			buffer.PutString( line );
		}
		buffer.PutString("}\n");
	}
	int entdatasize = buffer.TellPut()+1;

	dentdata.SetSize( entdatasize );
	memcpy( dentdata.Base(), buffer.Base(), entdatasize-1 );
	dentdata[entdatasize-1] = 0;
}

void PrintEntity (entity_t *ent)
{
	epair_t	*ep;
	
	Msg ("------- entity %p -------\n", ent);
	for (ep=ent->epairs ; ep ; ep=ep->next)
	{
		Msg ("%s = %s\n", ep->key, ep->value);
	}

}

epair_t *SetKeyValue( entity_t *ent, const char *key, const char *value, bool bAllowDuplicates )
{
	epair_t	*ep;
	
	if ( bAllowDuplicates == false )
	{
		for (ep=ent->epairs ; ep ; ep=ep->next)
		{
			if (!Q_stricmp (ep->key, key) )
			{
				free (ep->value);
				ep->value = copystring(value);
				return ep;
			}
		}
	}
	ep = (epair_t*)malloc (sizeof(*ep));
	ep->next = ent->epairs;
	ent->epairs = ep;
	ep->key = copystring(key);
	ep->value = copystring(value);

	return ep;
}

char 	*ValueForKey (entity_t *ent, const char *key)
{
	for (epair_t *ep=ent->epairs ; ep ; ep=ep->next)
		if (!Q_stricmp (ep->key, key) )
			return ep->value;
	return "";
}

vec_t	FloatForKey (entity_t *ent, const char *key)
{
	char *k = ValueForKey (ent, key);
	return atof(k);
}

vec_t	FloatForKeyWithDefault (entity_t *ent, const char *key, float default_value)
{
	for (epair_t *ep=ent->epairs ; ep ; ep=ep->next)
		if (!Q_stricmp (ep->key, key) )
			return atof( ep->value );
	return default_value;
}



int		IntForKey (entity_t *ent, const char *key)
{
	char *k = ValueForKey (ent, key);
	return atol(k);
}

void 	GetVectorForKey (entity_t *ent, const char *key, Vector& vec)
{

	char *k = ValueForKey (ent, key);
// scanf into doubles, then assign, so it is vec_t size independent
	double	v1, v2, v3;
	v1 = v2 = v3 = 0;
	sscanf (k, "%lf %lf %lf", &v1, &v2, &v3);
	vec[0] = v1;
	vec[1] = v2;
	vec[2] = v3;
}

void 	GetVector2DForKey (entity_t *ent, const char *key, Vector2D& vec)
{
	double	v1, v2;

	char *k = ValueForKey (ent, key);
// scanf into doubles, then assign, so it is vec_t size independent
	v1 = v2 = 0;
	sscanf (k, "%lf %lf", &v1, &v2);
	vec[0] = v1;
	vec[1] = v2;
}

void 	GetAnglesForKey (entity_t *ent, const char *key, QAngle& angle)
{
	char	*k;
	double	v1, v2, v3;

	k = ValueForKey (ent, key);
// scanf into doubles, then assign, so it is vec_t size independent
	v1 = v2 = v3 = 0;
	sscanf (k, "%lf %lf %lf", &v1, &v2, &v3);
	angle[0] = v1;
	angle[1] = v2;
	angle[2] = v3;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void BuildFaceCalcWindingData( dface_t *pFace, int *points )
{
	for( int i = 0; i < pFace->numedges; i++ )
	{
		int eIndex = dsurfedges[pFace->firstedge+i];
		if( eIndex < 0 )
		{
			points[i] = dedges[-eIndex].v[1];
		}
		else
		{
			points[i] = dedges[eIndex].v[0];
		}
	}
}


void TriStripToTriList( 
	unsigned short const *pTriStripIndices,
	int nTriStripIndices,
	unsigned short **pTriListIndices,
	int *pnTriListIndices )
{
	int nMaxTriListIndices = (nTriStripIndices - 2) * 3;
	*pTriListIndices = new unsigned short[ nMaxTriListIndices ];
	*pnTriListIndices = 0;

	for( int i=0; i < nTriStripIndices - 2; i++ )
	{
		if( pTriStripIndices[i]   == pTriStripIndices[i+1] || 
			pTriStripIndices[i]   == pTriStripIndices[i+2] ||
			pTriStripIndices[i+1] == pTriStripIndices[i+2] )
		{
		}
		else
		{
			// Flip odd numbered tris..
			if( i & 1 )
			{
				(*pTriListIndices)[(*pnTriListIndices)++] = pTriStripIndices[i+2];
				(*pTriListIndices)[(*pnTriListIndices)++] = pTriStripIndices[i+1];
				(*pTriListIndices)[(*pnTriListIndices)++] = pTriStripIndices[i];
			}
			else
			{
				(*pTriListIndices)[(*pnTriListIndices)++] = pTriStripIndices[i];
				(*pTriListIndices)[(*pnTriListIndices)++] = pTriStripIndices[i+1];
				(*pTriListIndices)[(*pnTriListIndices)++] = pTriStripIndices[i+2];
			}
		}
	}
}


void CalcTextureCoordsAtPoints(
	float const texelsPerWorldUnits[2][4],
	int const subtractOffset[2],
	const Vector *pPoints,
	int const nPoints,
	Vector2D *pCoords )
{
	for( int i=0; i < nPoints; i++ )
	{
		for( int iCoord=0; iCoord < 2; iCoord++ )
		{
			float *pDestCoord = &pCoords[i][iCoord];

			*pDestCoord = 0;
			for( int iDot=0; iDot < 3; iDot++ )
				*pDestCoord += pPoints[i][iDot] * texelsPerWorldUnits[iCoord][iDot];

			*pDestCoord += texelsPerWorldUnits[iCoord][3];
			*pDestCoord -= subtractOffset[iCoord];
		}
	}
}


/*
================
CalcFaceExtents

Fills in s->texmins[] and s->texsize[]
================
*/
void CalcFaceExtents(dface_t *s, int lightmapTextureMinsInLuxels[2], int lightmapTextureSizeInLuxels[2])
{
	vec_t	    mins[2], maxs[2], val=0;
	int		    i,j, e=0;
	dvertex_t	*v=nullptr;
	texinfo_t	*tex=nullptr;
	
	mins[0] = mins[1] = 1e24;
	maxs[0] = maxs[1] = -1e24;

	tex = &texinfo[s->texinfo];
	
	for (i=0 ; i<s->numedges ; i++)
	{
		e = dsurfedges[s->firstedge+i];
		if (e >= 0)
			v = dvertexes + dedges[e].v[0];
		else
			v = dvertexes + dedges[-e].v[1];
		
		for (j=0 ; j<2 ; j++)
		{
			val = v->point[0] * tex->lightmapVecsLuxelsPerWorldUnits[j][0] + 
				  v->point[1] * tex->lightmapVecsLuxelsPerWorldUnits[j][1] + 
				  v->point[2] * tex->lightmapVecsLuxelsPerWorldUnits[j][2] + 
				  tex->lightmapVecsLuxelsPerWorldUnits[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	int nMaxLightmapDim = (s->dispinfo == -1) ? MAX_LIGHTMAP_DIM_WITHOUT_BORDER : MAX_DISP_LIGHTMAP_DIM_WITHOUT_BORDER;
	for (i=0 ; i<2 ; i++)
	{	
		mins[i] = ( float )floor( mins[i] );
		maxs[i] = ( float )ceil( maxs[i] );

		lightmapTextureMinsInLuxels[i] = ( int )mins[i];
		lightmapTextureSizeInLuxels[i] = ( int )( maxs[i] - mins[i] );
		if( lightmapTextureSizeInLuxels[i] > nMaxLightmapDim + 1 )
		{
			Vector point = vec3_origin;
			for (int j=0 ; j<s->numedges ; j++)
			{
				e = dsurfedges[s->firstedge+j];
				v = (e<0)?dvertexes + dedges[-e].v[1] : dvertexes + dedges[e].v[0];
				point += v->point;
				Warning( "Bad surface extents point: %f %f %f\n", v->point.x, v->point.y, v->point.z );
			}
			point *= 1.0f/s->numedges;
			Error( "Bad surface extents - surface is too big to have a lightmap\n\tmaterial %s around point (%.1f %.1f %.1f)\n\t(dimension: %d, %d>%d)\n", 
				TexDataStringTable_GetString( dtexdata[texinfo[s->texinfo].texdata].nameStringTableID ), 
				point.x, point.y, point.z,
				( int )i,
				( int )lightmapTextureSizeInLuxels[i],
				( int )( nMaxLightmapDim + 1 )
				);
		}
	}
}


void UpdateAllFaceLightmapExtents()
{
	for( int i=0; i < numfaces; i++ )
	{
		dface_t *pFace = &dfaces[i];

		if ( texinfo[pFace->texinfo].flags & (SURF_SKY|SURF_NOLIGHT) )
			continue;		// non-lit texture

		CalcFaceExtents( pFace, pFace->m_LightmapTextureMinsInLuxels, pFace->m_LightmapTextureSizeInLuxels );
	}
}


//-----------------------------------------------------------------------------
//
// Helper class to iterate over leaves, used by tools
//
//-----------------------------------------------------------------------------

#define TEST_EPSILON	(0.03125)


class CToolBSPTree : public ISpatialQuery
{
public:
	// Returns the number of leaves
	int LeafCount() const;

	// Enumerates the leaves along a ray, box, etc.
	bool EnumerateLeavesAtPoint( const Vector& pt, ISpatialLeafEnumerator* pEnum, intp context );
	bool EnumerateLeavesInBox( const Vector& mins, const Vector& maxs, ISpatialLeafEnumerator* pEnum, intp context );
	bool EnumerateLeavesInSphere( const Vector& center, float radius, ISpatialLeafEnumerator* pEnum, intp context );
	bool EnumerateLeavesAlongRay( Ray_t const& ray, ISpatialLeafEnumerator* pEnum, intp context );
	bool EnumerateLeavesInSphereWithFlagSet( const Vector& center, float radius, ISpatialLeafEnumerator* pEnum, intp context, int nFlagsMask )
	{
		// engine flags not supported. Callers of this are supposed to examine the node anyway
		return EnumerateLeavesInSphere( center, radius, pEnum, context );
	}
	int ListLeavesInBox( const Vector& mins, const Vector& maxs, unsigned short *pList, int listMax );

	virtual int ListLeavesInSphereWithFlagSet( int *pLeafsInSphere, const Vector& vecCenter, float flRadius, int nLeafCount, const uint16 *pLeafs, int nLeafStride, int nFlagsCheck );
};


//-----------------------------------------------------------------------------
// Returns the number of leaves
//-----------------------------------------------------------------------------

int CToolBSPTree::LeafCount() const
{
	return numleafs;
}


//-----------------------------------------------------------------------------
// Enumerates the leaves at a point
//-----------------------------------------------------------------------------

bool CToolBSPTree::EnumerateLeavesAtPoint( const Vector& pt, 
									ISpatialLeafEnumerator* pEnum, intp context )
{
	int node = 0;
	while( node >= 0 )
	{
		dnode_t* pNode = &dnodes[node];
		dplane_t* pPlane = &dplanes[pNode->planenum];

		if (DotProduct( pPlane->normal, pt ) <= pPlane->dist)
		{
			node = pNode->children[1];
		}
		else
		{
			node = pNode->children[0];
		}
	}

	return pEnum->EnumerateLeaf( - node - 1, context );
}


//-----------------------------------------------------------------------------
// Enumerates the leaves in a box
//-----------------------------------------------------------------------------

static bool EnumerateLeavesInBox_R( int node, const Vector& mins, 
				const Vector& maxs, ISpatialLeafEnumerator* pEnum, intp context )
{
	Vector cornermin, cornermax;

	while( node >= 0 )
	{
		dnode_t* pNode = &dnodes[node];
		dplane_t* pPlane = &dplanes[pNode->planenum];

		// Arbitrary split plane here
		for (int i = 0; i < 3; ++i)
		{
			if (pPlane->normal[i] >= 0)
			{
				cornermin[i] = mins[i];
				cornermax[i] = maxs[i];
			}
			else
			{
				cornermin[i] = maxs[i];
				cornermax[i] = mins[i];
			}
		}

		if ( (DotProduct( pPlane->normal, cornermax ) - pPlane->dist) <= -TEST_EPSILON )
		{
			node = pNode->children[1];
		}
		else if ( (DotProduct( pPlane->normal, cornermin ) - pPlane->dist) >= TEST_EPSILON )
		{
			node = pNode->children[0];
		}
		else
		{
			if (!EnumerateLeavesInBox_R( pNode->children[0], mins, maxs, pEnum, context ))
			{
				return false;
			}

			return EnumerateLeavesInBox_R( pNode->children[1], mins, maxs, pEnum, context );
		}
	}

	return pEnum->EnumerateLeaf( - node - 1, context );
}

bool CToolBSPTree::EnumerateLeavesInBox( const Vector& mins, const Vector& maxs, 
									ISpatialLeafEnumerator* pEnum, intp context )
{
	return EnumerateLeavesInBox_R( 0, mins, maxs, pEnum, context );
}

// a little helper class to implement this interface for tools - engine uses this as a more efficient path
class CLeafMakeList : public ISpatialLeafEnumerator
{
public:
	CLeafMakeList( unsigned short *pListIn, int listMaxIn )
		: m_pList(pListIn), m_listMax(listMaxIn), m_count(0)
	{
	}
	virtual bool EnumerateLeaf( int leaf, intp context )
	{
		if ( m_count < m_listMax )
		{
			m_pList[m_count] = leaf;
			m_count++;
			return true;
		}
		return false;
	}
	unsigned short *m_pList;
	int m_listMax;
	int m_count;
};

int CToolBSPTree::ListLeavesInBox( const Vector& mins, const Vector& maxs, unsigned short *pList, int listMax )
{
	CLeafMakeList list(pList, listMax);
	EnumerateLeavesInBox(mins, maxs, &list, 0 );
	return list.m_count;
}


int CToolBSPTree::ListLeavesInSphereWithFlagSet( int *pLeafsInSphere, const Vector& vecCenter, float flRadius, int nLeafCount, const uint16 *pLeafs, int nLeafStride, int nFlagsCheck )
{
	// NOTE: We ignore flags here
	Vector vecMins, vecMaxs;
	int nLeavesFound = 0;
	const uint16 *pLeaf = pLeafs;
	for ( int i = 0; i < nLeafCount; ++i, pLeaf = (const uint16*)( (const uint8*)pLeaf + nLeafStride ) )
	{
		dleaf_t &leaf = dleafs[ *pLeaf ];
		VECTOR_COPY( leaf.mins, vecMins );
		VECTOR_COPY( leaf.maxs, vecMaxs );
		if ( !IsBoxIntersectingSphere( vecMins, vecMaxs, vecCenter, flRadius ) )
			continue;

		pLeafsInSphere[nLeavesFound++] = i;
	}
	return nLeavesFound;
}



//-----------------------------------------------------------------------------
// Enumerate leaves within a sphere
//-----------------------------------------------------------------------------

static bool EnumerateLeavesInSphere_R( int node, const Vector& origin, 
				float radius, ISpatialLeafEnumerator* pEnum, intp context )
{
	while( node >= 0 )
	{
		dnode_t* pNode = &dnodes[node];
		dplane_t* pPlane = &dplanes[pNode->planenum];

		if (DotProduct( pPlane->normal, origin ) + radius - pPlane->dist <= -TEST_EPSILON )
		{
			node = pNode->children[1];
		}
		else if (DotProduct( pPlane->normal, origin ) - radius - pPlane->dist >= TEST_EPSILON )
		{
			node = pNode->children[0];
		}
		else
		{
			if (!EnumerateLeavesInSphere_R( pNode->children[0], 
					origin, radius, pEnum, context ))
			{
				return false;
			}

			return EnumerateLeavesInSphere_R( pNode->children[1],
				origin, radius, pEnum, context );
		}
	}

	return pEnum->EnumerateLeaf( - node - 1, context );
}

bool CToolBSPTree::EnumerateLeavesInSphere( const Vector& center, float radius, ISpatialLeafEnumerator* pEnum, intp context )
{
	return EnumerateLeavesInSphere_R( 0, center, radius, pEnum, context );
}


//-----------------------------------------------------------------------------
// Enumerate leaves along a ray
//-----------------------------------------------------------------------------

static bool EnumerateLeavesAlongRay_R( int node, Ray_t const& ray, 
	const Vector& start, const Vector& end, ISpatialLeafEnumerator* pEnum, intp context )
{
	float front,back;

	while (node >= 0)
	{
		dnode_t* pNode = &dnodes[node];
		dplane_t* pPlane = &dplanes[pNode->planenum];

		if ( pPlane->type <= PLANE_Z )
		{
			front = start[pPlane->type] - pPlane->dist;
			back = end[pPlane->type] - pPlane->dist;
		}
		else
		{
			front = DotProduct(start, pPlane->normal) - pPlane->dist;
			back = DotProduct(end, pPlane->normal) - pPlane->dist;
		}

		if (front <= -TEST_EPSILON && back <= -TEST_EPSILON)
		{
			node = pNode->children[1];
		}
		else if (front >= TEST_EPSILON && back >= TEST_EPSILON)
		{
			node = pNode->children[0];
		}
		else
		{
			// test the front side first
			bool side = front < 0;

			// Compute intersection point based on the original ray
			float splitfrac;
			float denom = DotProduct( ray.m_Delta, pPlane->normal );
			if ( denom == 0.0f )
			{
				splitfrac = 1.0f;
			}
			else
			{
				splitfrac = (	pPlane->dist - DotProduct( ray.m_Start, pPlane->normal ) ) / denom;
				if (splitfrac < 0)
					splitfrac = 0;
				else if (splitfrac > 1)
					splitfrac = 1;
			}

			// Compute the split point
			Vector split;
			VectorMA( ray.m_Start, splitfrac, ray.m_Delta, split );

			bool r = EnumerateLeavesAlongRay_R (pNode->children[side], ray, start, split, pEnum, context );
			if (!r)
				return r;
			return EnumerateLeavesAlongRay_R (pNode->children[!side], ray, split, end, pEnum, context);
		}
	}

	return pEnum->EnumerateLeaf( - node - 1, context );
}

bool CToolBSPTree::EnumerateLeavesAlongRay( Ray_t const& ray, ISpatialLeafEnumerator* pEnum, intp context )
{
	if (!ray.m_IsSwept)
	{
		Vector mins, maxs;
		VectorAdd( ray.m_Start, ray.m_Extents, maxs );
		VectorSubtract( ray.m_Start, ray.m_Extents, mins );

		return EnumerateLeavesInBox_R( 0, mins, maxs, pEnum, context );
	}

	// FIXME: Extruded ray not implemented yet
	Assert( ray.m_IsRay );

	Vector end;
	VectorAdd( ray.m_Start, ray.m_Delta, end );
	return EnumerateLeavesAlongRay_R( 0, ray, ray.m_Start, end, pEnum, context );
}


//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------

ISpatialQuery* ToolBSPTree()
{
	static CToolBSPTree s_ToolBSPTree;
	return &s_ToolBSPTree;
}



//-----------------------------------------------------------------------------
// Enumerates nodes in front to back order...
//-----------------------------------------------------------------------------

// FIXME: Do we want this in the IBSPTree interface?

static bool EnumerateNodesAlongRay_R( int node, Ray_t const& ray, float start, float end,
	IBSPNodeEnumerator* pEnum, intp context )
{
	float front, back;
	float startDotN, deltaDotN;

	while (node >= 0)
	{
		dnode_t* pNode = &dnodes[node];
		dplane_t* pPlane = &dplanes[pNode->planenum];

		if ( pPlane->type <= PLANE_Z )
		{
			startDotN = ray.m_Start[pPlane->type];
			deltaDotN = ray.m_Delta[pPlane->type];
		}
		else
		{
			startDotN = DotProduct( ray.m_Start, pPlane->normal );
			deltaDotN = DotProduct( ray.m_Delta, pPlane->normal );
		}

		front = startDotN + start * deltaDotN - pPlane->dist;
		back = startDotN + end * deltaDotN - pPlane->dist;

		if (front <= -TEST_EPSILON && back <= -TEST_EPSILON)
		{
			node = pNode->children[1];
		}
		else if (front >= TEST_EPSILON && back >= TEST_EPSILON)
		{
			node = pNode->children[0];
		}
		else
		{
			// test the front side first
			bool side = front < 0;

			// Compute intersection point based on the original ray
			float splitfrac;
			if ( deltaDotN == 0.0f )
			{
				splitfrac = 1.0f;
			}
			else
			{
				splitfrac = ( pPlane->dist - startDotN ) / deltaDotN;
				if (splitfrac < 0.0f)
					splitfrac = 0.0f;
				else if (splitfrac > 1.0f)
					splitfrac = 1.0f;
			}

			bool r = EnumerateNodesAlongRay_R (pNode->children[side], ray, start, splitfrac, pEnum, context );
			if (!r)
				return r;

			// Visit the node...
			if (!pEnum->EnumerateNode( node, ray, splitfrac, context ))
				return false;

			return EnumerateNodesAlongRay_R (pNode->children[!side], ray, splitfrac, end, pEnum, context);
		}
	}

	// Visit the leaf...
	return pEnum->EnumerateLeaf( - node - 1, ray, start, end, context );
}


bool EnumerateNodesAlongRay( Ray_t const& ray, IBSPNodeEnumerator* pEnum, intp context )
{
	Vector end;
	VectorAdd( ray.m_Start, ray.m_Delta, end );
	return EnumerateNodesAlongRay_R( 0, ray, 0.0f, 1.0f, pEnum, context );
}


//-----------------------------------------------------------------------------
// Helps us find all leaves associated with a particular cluster
//-----------------------------------------------------------------------------
CUtlVector<clusterlist_t> g_ClusterLeaves;

void BuildClusterTable( void )
{
	int i, j;
	int leafCount;
	int	leafList[MAX_MAP_LEAFS];

	g_ClusterLeaves.SetCount( dvis->numclusters );
	for ( i = 0; i < dvis->numclusters; i++ )
	{
		leafCount = 0;
		for ( j = 0; j < numleafs; j++ )
		{
			if ( dleafs[j].cluster == i )
			{
				leafList[ leafCount ] = j;
				leafCount++;
			}
		}

		g_ClusterLeaves[i].leafCount = leafCount;
		if ( leafCount )
		{
			g_ClusterLeaves[i].leafs.SetCount( leafCount );
			memcpy( g_ClusterLeaves[i].leafs.Base(), leafList, sizeof(int) * leafCount );
		}
	}
}

// There's a version of this in host.cpp!!!  Make sure that they match.
void GetPlatformMapPath( const char *pMapPath, char *pPlatformMapPath, int dxlevel, int maxLength )
{
	Q_StripExtension( pMapPath, pPlatformMapPath, maxLength );

//	if( dxlevel <= 60 )
//	{
//		Q_strncat( pPlatformMapPath, "_dx60", maxLength, COPY_ALL_CHARACTERS );
//	}

	Q_strncat( pPlatformMapPath, ".bsp", maxLength, COPY_ALL_CHARACTERS );
}

// There's a version of this in checksum_engine.cpp!!! Make sure that they match.
static bool CRC_MapFile(CRC32_t *crcvalue, const char *pszFileName)
{
	byte chunk[1024];
	lump_t *curLump;

	FileHandle_t fp = g_pFileSystem->Open( pszFileName, "rb" );
	if ( !fp )
		return false;

	// CRC across all lumps except for the Entities lump
	for ( int l = 0; l < HEADER_LUMPS; ++l )
	{
		if (l == LUMP_ENTITIES)
			continue;

		curLump = &g_pBSPHeader->lumps[l];
		unsigned int nSize = curLump->filelen;

		g_pFileSystem->Seek( fp, curLump->fileofs, FILESYSTEM_SEEK_HEAD );

		// Now read in 1K chunks
		while ( nSize > 0 )
		{
			int nBytesRead = 0;

			if ( nSize > 1024 )
				nBytesRead = g_pFileSystem->Read( chunk, 1024, fp );
			else
				nBytesRead = g_pFileSystem->Read( chunk, nSize, fp );

			// If any data was received, CRC it.
			if ( nBytesRead > 0 )
			{
				nSize -= nBytesRead;
				CRC32_ProcessBuffer( crcvalue, chunk, nBytesRead );
			}
			else
			{
				g_pFileSystem->Close( fp );
				return false;
			}
		}	
	}
	
	g_pFileSystem->Close( fp );
	return true;
}


void SetHDRMode( bool bHDR )
{
	g_bHDR = bHDR;
	if ( bHDR )
	{
		pdlightdata = &dlightdataHDR;		
		g_pLeafAmbientLighting = &g_LeafAmbientLightingHDR;
		g_pLeafAmbientIndex = &g_LeafAmbientIndexHDR;
		pNumworldlights = &numworldlightsHDR;
		dworldlights = dworldlightsHDR;
#ifdef VRAD
		extern void VRadDetailProps_SetHDRMode( bool bHDR );
		VRadDetailProps_SetHDRMode( bHDR );
#endif
	}
	else
	{
		pdlightdata = &dlightdataLDR;		
		g_pLeafAmbientLighting = &g_LeafAmbientLightingLDR;
		g_pLeafAmbientIndex = &g_LeafAmbientIndexLDR;
		pNumworldlights = &numworldlightsLDR;
		dworldlights = dworldlightsLDR;
#ifdef VRAD
		extern void VRadDetailProps_SetHDRMode( bool bHDR );
		VRadDetailProps_SetHDRMode( bHDR );
#endif
	}
}

//-----------------------------------------------------------------------------
// Iterate files in pak file, distribute to converters
// pak file will be ready for serialization upon completion
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Generate a table of all static props, used for resolving static prop lighting
// files back to their actual mdl.
//-----------------------------------------------------------------------------
int ComputeLightmapSize( dface_t *pFace, texinfo_t *pTexInfos )
{
	if ( pFace->lightofs == -1 )
	{
		// no lighting data on this face
		return 0;
	}

	bool bNeedsBumpmap = false;
	if ( pTexInfos[pFace->texinfo].flags & SURF_BUMPLIGHT )
	{
		bNeedsBumpmap = true;
	}

	// determine how many lightstyles this face has
	int nLightstyles;
	for ( nLightstyles = 0; nLightstyles < MAXLIGHTMAPS; nLightstyles++ )
	{
		if ( pFace->styles[nLightstyles] == 255 )
			break;
	}

	int nLuxels = ( pFace->m_LightmapTextureSizeInLuxels[0] + 1 ) * ( pFace->m_LightmapTextureSizeInLuxels[1] + 1 );
	if ( bNeedsBumpmap )
	{
		return nLuxels * 4 * nLightstyles * ( NUM_BUMP_VECTS + 1 );
	}

	return nLuxels * 4 * nLightstyles;
}

int ComputeLightmapPrefixSize( dface_t *pFace )
{
	if ( pFace->lightofs == -1 )
	{
		// no lighting data on this face
		return 0;
	}

	// determine how many lightstyles this face has
	int nLightstyles;
	for ( nLightstyles = 0; nLightstyles < MAXLIGHTMAPS; nLightstyles++ )
	{
		if ( pFace->styles[nLightstyles] == 255 )
			break;
	}

	int nHdrBytes = nLightstyles * sizeof( ColorRGBExp32 );
	return nHdrBytes;
}

void StripUnusedLightmapAlphaData()
{
// 7LS CSM's now working on console => we need the lightmap alpha data
}

int AlignBuffer( CUtlBuffer &buffer, int alignment )
{
	unsigned int newPosition = AlignValue( buffer.TellPut(), alignment );
	int padLength = newPosition - buffer.TellPut();
	for ( int i = 0; i<padLength; i++ )
	{
		buffer.PutChar( '\0' );
	}
	return buffer.TellPut();
}

struct SortedLump_t
{
	int		lumpNum;
	lump_t	*pLump;
};

int SortLumpsByOffset( const SortedLump_t *pSortedLumpA, const SortedLump_t *pSortedLumpB ) 
{
	int fileOffsetA = pSortedLumpA->pLump->fileofs;
	int fileOffsetB = pSortedLumpB->pLump->fileofs;

	int fileSizeA = pSortedLumpA->pLump->filelen;
	int fileSizeB = pSortedLumpB->pLump->filelen;

	// invalid or empty lumps get sorted together
	if ( !fileSizeA )
	{
		fileOffsetA = 0;
	}
	if ( !fileSizeB )
	{
		fileOffsetB = 0;
	}

	// compare by offset, want ascending
	if ( fileOffsetA < fileOffsetB )
	{
		return -1;
	}
	else if ( fileOffsetA > fileOffsetB )
	{
		return 1;
	}

	return 0;
}

//-----------------------------------------------------------------------------
//  For all lumps in a bsp: Loads the lump from file A, swaps it, writes it to file B.
//  This limits the memory used for the swap process which helps the Xbox 360.
//
//	NOTE: These lumps will be written to the file in exactly the order they appear here,
//	so they can be shifted around if desired for file access optimization.
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Get the pak lump from a BSP
//-----------------------------------------------------------------------------
// compare function for qsort below
static int LumpOffsetCompare( const void *pElem1, const void *pElem2 )
{
	int lump1 = *(byte *)pElem1;
	int lump2 = *(byte *)pElem2;

	if ( lump1 != lump2 )
	{
		// force LUMP_MAP_FLAGS to be first, always
		if ( lump1 == LUMP_MAP_FLAGS )
		{
			return -1;
		}
		else if ( lump2 == LUMP_MAP_FLAGS )
		{
			return 1;
		}

		// force LUMP_PAKFILE to be last, always
		if ( lump1 == LUMP_PAKFILE )
		{
			return 1;
		}
		else if ( lump2 == LUMP_PAKFILE )
		{
			return -1;
		}
	}

	int fileOffset1 = g_pBSPHeader->lumps[lump1].fileofs;
	int fileOffset2 = g_pBSPHeader->lumps[lump2].fileofs;

	// invalid or empty lumps will get sorted together
	if ( !g_pBSPHeader->lumps[lump1].filelen )
	{
		fileOffset1 = 0;
	}

	if ( !g_pBSPHeader->lumps[lump2].filelen )
	{
		fileOffset2 = 0;
	}

	// compare by offset
	if ( fileOffset1 < fileOffset2 )
	{
		return -1;
	}
	else if ( fileOffset1 > fileOffset2 )
	{
		return 1;
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Replace the pak lump in a BSP. This is strictly for the 360 conversion process
// and encodes expected properties of the BSP w.r.t lumps, their positions, and
// alignments.
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Build a list of files that BSP owns, world/cubemap materials, static props, etc.
//-----------------------------------------------------------------------------
bool GetBSPDependants( const char *pBSPFilename, CUtlVector< CUtlString > *pList )
{
	if ( !g_pFileSystem->FileExists( pBSPFilename ) )
	{
		Warning( "Error! Couldn't open file %s!\n", pBSPFilename ); 
		return false;
	}

	// must be set, but exact hdr not critical for dependant traversal	
	SetHDRMode( false );

	LoadBSPFile( pBSPFilename );

	char szBspName[MAX_PATH];
	V_FileBase( pBSPFilename, szBspName, sizeof( szBspName ) );
	V_SetExtension( szBspName, ".bsp", sizeof( szBspName ) );

	// get embedded pak files, and internals
	char szFilename[MAX_PATH];
	int fileSize;
	int fileId = -1;
	for ( ;; )
	{
		fileId = GetPakFile()->GetNextFilename( fileId, szFilename, sizeof( szFilename ), fileSize );
		if ( fileId == -1 )
		{
			break;
		}
		pList->AddToTail( szFilename );
	}

	// get all the world materials
	for ( int i=0; i<numtexdata; i++ )
	{
		const char *pName = TexDataStringTable_GetString( dtexdata[i].nameStringTableID );
		V_ComposeFileName( "materials", pName, szFilename, sizeof( szFilename ) );
		V_SetExtension( szFilename, ".vmt", sizeof( szFilename ) );
		pList->AddToTail( szFilename );
	}

	// get all the static props
	GameLumpHandle_t hGameLump = g_GameLumps.GetGameLumpHandle( GAMELUMP_STATIC_PROPS );
	if ( hGameLump != g_GameLumps.InvalidGameLump() )
	{
		byte *pGameLumpData = (byte *)g_GameLumps.GetGameLump( hGameLump );
		if ( pGameLumpData && g_GameLumps.GameLumpSize( hGameLump ) )
		{
			int count = ((int *)pGameLumpData)[0];
			pGameLumpData += sizeof( int );

			StaticPropDictLump_t *pStaticPropDictLump = (StaticPropDictLump_t *)pGameLumpData;
			for ( int i=0; i<count; i++ )
			{
				pList->AddToTail( pStaticPropDictLump[i].m_Name );
			}
		}
	}

	// get all the detail props
	hGameLump = g_GameLumps.GetGameLumpHandle( GAMELUMP_DETAIL_PROPS );
	if ( hGameLump != g_GameLumps.InvalidGameLump() )
	{
		byte *pGameLumpData = (byte *)g_GameLumps.GetGameLump( hGameLump );
		if ( pGameLumpData && g_GameLumps.GameLumpSize( hGameLump ) )
		{
			int count = ((int *)pGameLumpData)[0];
			pGameLumpData += sizeof( int );

			DetailObjectDictLump_t *pDetailObjectDictLump = (DetailObjectDictLump_t *)pGameLumpData;
			for ( int i=0; i<count; i++ )
			{
				pList->AddToTail( pDetailObjectDictLump[i].m_Name );
			}
			pGameLumpData += count * sizeof( DetailObjectDictLump_t );

			if ( g_GameLumps.GetGameLumpVersion( hGameLump ) == 4 )
			{
				count = ((int *)pGameLumpData)[0];
				pGameLumpData += sizeof( int );
				if ( count )
				{
					// All detail prop sprites must lie in the material detail/detailsprites
					pList->AddToTail( "materials/detail/detailsprites.vmt" );
				}
			}
		}
	}

	// Get all the physics excludes
	dphyslevelV0_t *pPhysLevelData = (dphyslevelV0_t *)g_pPhysLevel;
	if ( g_PhysLevelSize && pPhysLevelData && pPhysLevelData->dataVersion == dphyslevelV0_t::DATA_VERSION )
	{
		char szFilename[MAX_PATH];
		for( int i = 0; i < pPhysLevelData->levelStaticModels.size; ++i )
		{
			const char *pPhz = pPhysLevelData->levelStaticModels[i];

			// These MDLs need to exclude their PHZs
			V_strncpy( szFilename, pPhz, sizeof( szFilename ) );		
			V_SetExtension( szFilename, ".phz", sizeof( szFilename ) );

			// add with prefix exclude token
			pList->AddToTail( CFmtStr( "*%s", szFilename ).Access() );
		}
	}

	UnloadBSPFile();

	return true;
}

