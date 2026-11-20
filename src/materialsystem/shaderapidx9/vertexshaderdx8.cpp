// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

// Purpose: Vertex/Pixel Shaders



#define DISABLE_PROTECTED_THINGS
#undef SOCKET
typedef int SOCKET;
#define SOCKET_ERROR (-1)
constexpr auto SD_SEND = 0x01;
#define INVALID_SOCKET (~0)

import ks.core.basetypes;
import ks.core.cmdline;
import ks.core.dbg;
import ks.core.platform;
import ks.core.scratcharena;
import ks.core.types;
import ks.core.vprof;
import ks.platform;
import ks.simd;
import ks.tier1.byte_reader;
import ks.tier1.convar;
import ks.tier1.diff;
import ks.tier1.iconvar;
import ks.tier1.interfaces;
import ks.tier1.intern_pool;
import ks.tier1.keyvalues;
import ks.tier1.lzmadecoder;
import ks.tier1.map_ops;
import ks.tier1.strtools;
import ks.tier1.vector_ops;
#include "togl/rendermechanism.h"
#include "vertexshaderdx8.h"
import std;
#include "locald3dtypes.h"
#include "shaderapidx8_global.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "shaderapidx8.h"
#include "materialsystem/IShader.h"
#include "materialsystem/ishadersystem.h"
#include "filesystem.h"
#include "materialsystem/shader_vcs_version.h"
#include "shaderlib/shadercombosemantics.h"

#include "datacache/idatacache.h"
#include "shaderdevicedx8.h"
#include "tier2/tier2.h"
#include "shaderapi/ishaderutil.h"




// mapping from vcs file basename to shader combo semantics information.
static ks::idict<const ShaderComboSemantics_t *> s_ShaderComboInfoByName;

// It currently includes windows.h and we don't want that.

int BZ2_bzBuffToBuffDecompress( 
	  char*         dest, 
	  unsigned int* destLen,
	  char*         source, 
	  unsigned int  sourceLen,
	  int           small, 
	  int           verbosity 
   )
{
	return 0;
}

static ConVar mat_remoteshadercompile( "mat_remoteshadercompile", "127.0.0.1", FCVAR_MATERIAL_SYSTEM_THREAD | FCVAR_CHEAT );

//#define PROFILE_SHADER_CREATE

constexpr auto SHADER_COMBO_SPEW_VERBOSE = 1;

//#define NO_AMBIENT_CUBE
constexpr auto MAX_BONES = 3;

// debugging aid
constexpr auto MAX_SHADER_HISTORY = 16;

inline constexpr char SHADER_FNAME_EXTENSION[] = ".vcs";


//-----------------------------------------------------------------------------
// Explicit instantiation of shader buffer implementation
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Used to find unique shaders
//-----------------------------------------------------------------------------

static int s_NumPixelShadersCreated = 0;
static int s_NumVertexShadersCreated = 0;

static void RegisterVS( const void* pShaderBits, int nShaderSize, IDirect3DVertexShader9* pShader )
{
}

static void RegisterPS( const void* pShaderBits, int nShaderSize, IDirect3DPixelShader9* pShader )
{
}

static void UnregisterVS( IDirect3DVertexShader9* pShader )
{
}

static void UnregisterPS( IDirect3DPixelShader9* pShader )
{
}

//-----------------------------------------------------------------------------
// The lovely low-level dx call to create a vertex shader
//-----------------------------------------------------------------------------
static HardwareShader_t CreateD3DVertexShader( DWORD *pByteCode, int numBytes, const char *pShaderName, char *debugLabel = nullptr )
{

	if ( !pByteCode )
	{
		contract_assert( 0 );
		return INVALID_HARDWARE_SHADER;
	}

	// Compute the vertex specification
	HardwareShader_t hShader;


		HRESULT hr = Dx9Device()->CreateVertexShader( pByteCode, (IDirect3DVertexShader9 **)&hShader );


	// NOTE: This isn't recorded before the CreateVertexShader because
	// we don't know the value of shader until after the CreateVertexShader.
	 // hack hack hack

	if ( FAILED( hr ) )
	{
		contract_assert( 0 );
		hShader = INVALID_HARDWARE_SHADER;
	}
	else
	{
		s_NumVertexShadersCreated++;
		RegisterVS( pByteCode, numBytes, (IDirect3DVertexShader9 *)hShader );
	}
	return hShader;
}

static void PatchPixelShaderForAtiMsaaHack(DWORD *pShader, DWORD dwTexCoordMask) 
{ 
	bool bIsSampler, bIsTexCoord; 
	
	// Should be able to patch only ps2.0 
	if (*pShader != 0xFFFF0200) 
		return; 
	
	pShader++; 
	
	while (pShader) 
	{ 
		switch (*pShader & D3DSI_OPCODE_MASK) 
		{ 
		case D3DSIO_COMMENT: 
			// Process comment 
			pShader = pShader + (*pShader >> 16) + 1; 
			break; 
			
		case D3DSIO_END: 
			// End of shader 
			return; 
			
		case D3DSIO_DCL: 
			bIsSampler = (*(pShader + 1) & D3DSP_TEXTURETYPE_MASK) != D3DSTT_UNKNOWN; 
			bIsTexCoord = (((*(pShader + 2) & D3DSP_REGTYPE_MASK) >> D3DSP_REGTYPE_SHIFT) + 
				((*(pShader + 2) & D3DSP_REGTYPE_MASK2) >> D3DSP_REGTYPE_SHIFT2)) == D3DSPR_TEXTURE; 
			
			if (!bIsSampler && bIsTexCoord) 
			{ 
				DWORD dwTexCoord = *(pShader + 2) & D3DSP_REGNUM_MASK; 
				DWORD mask = 0x01; 
				for (DWORD i = 0; i < 16; i++) 
				{ 
					if (((dwTexCoordMask & mask) == mask) && (dwTexCoord == i)) 
					{ 
						// If found -- patch and get out 
//						*(pShader + 2) |= D3DSPDM_PARTIALPRECISION; 
						*(pShader + 2) |= D3DSPDM_MSAMPCENTROID; 
						break; 
					} 
					mask <<= 1; 
				} 
			} 
			// Intentionally fall through... 
			
		default: 
			// Skip instruction 
			pShader = pShader + ((*pShader & D3DSI_INSTLENGTH_MASK) >> D3DSI_INSTLENGTH_SHIFT) + 1; 
		} 
	}
} 

//-----------------------------------------------------------------------------
// The lovely low-level dx call to create a pixel shader
//-----------------------------------------------------------------------------
static HardwareShader_t CreateD3DPixelShader( DWORD *pByteCode, unsigned int nCentroidMask, int numBytes, const char* pShaderName, char *debugLabel = nullptr )
{
	
	if ( !pByteCode )
		return INVALID_HARDWARE_SHADER;

//	if ( nCentroidMask )
//	{
//		ConColorMsg( Color( 255, 187, 73, 255 ), "Centroid Mask for %s: 0x%x\n", pShaderName, nCentroidMask );
//	}

	if ( nCentroidMask && 
		HardwareConfig()->NeedsATICentroidHack() && 
		!HardwareConfig()->SuppressPixelShaderCentroidHackFixup() )
	{
		PatchPixelShaderForAtiMsaaHack( pByteCode, nCentroidMask );
	}

	HardwareShader_t shader;
		HRESULT hr = Dx9Device()->CreatePixelShader( pByteCode, ( IDirect3DPixelShader ** )&shader );
	
	// NOTE: We have to do this after creating the pixel shader since we don't know
	// lookup.m_PixelShader yet!!!!!!!
	  // hack hack hack
	
	if ( FAILED( hr ) )
	{
		contract_assert( 0 );
		shader = INVALID_HARDWARE_SHADER;
	}
	else
	{
		s_NumPixelShadersCreated++;
		RegisterPS( pByteCode, numBytes, ( IDirect3DPixelShader9* )shader );
	}

	return shader;
}

template<class T> int BinarySearchCombos( uint32 nStaticComboID, int nCombos, T const *pRecords )
{
	// Use binary search - data is sorted
	int nLowerIdx = 1;
	int nUpperIdx = nCombos;
	for (;;)
	{
		if ( nUpperIdx < nLowerIdx )
			return -1;

		int nMiddleIndex = ( nLowerIdx + nUpperIdx ) / 2;
		uint32 nProbe = pRecords[nMiddleIndex-1].m_nStaticComboID;
		if ( nStaticComboID < nProbe )
		{
			nUpperIdx = nMiddleIndex - 1;
		}
		else
		{
			if ( nStaticComboID > nProbe )
				nLowerIdx = nMiddleIndex + 1;
			else
				return nMiddleIndex - 1;
		}
	}
}

inline int FindShaderStaticCombo( uint32 nStaticComboID, const ShaderHeader_t& header, StaticComboRecord_t *pRecords )
{
	if ( header.m_nVersion < 5 )
		return -1;

	return BinarySearchCombos( nStaticComboID, header.m_nNumStaticCombos, pRecords );
}

// cache redundant i/o fetched components of the vcs files
struct ShaderFileCache_t
{
	std::string_view	m_Name;			// views into m_ShaderSymbolTable
	std::string_view	m_Filename;
	ShaderHeader_t		m_Header;
	bool				m_bVertexShader;

	// valid for diff version only - contains the microcode used as the reference for diff algorithm
	std::vector< std::byte >	m_ReferenceCombo;

	// valid for ver5 only - contains the directory
	std::vector< StaticComboRecord_t >	m_StaticComboRecords;
	std::vector< StaticComboAliasRecord_t > m_StaticComboDupRecords;

	ShaderFileCache_t()
	{
		// invalid until version established
		m_Header.m_nVersion = 0;
	}

	bool IsValid() const
	{
		return m_Header.m_nVersion != 0;
	}

	bool IsOldVersion() const
	{
		return m_Header.m_nVersion < 5;
	}

	int IsVersion6() const
	{
		return ( m_Header.m_nVersion == 6 );
	}

	int FindCombo( uint32 nStaticComboID )
	{
		int nSearchAliases = BinarySearchCombos( nStaticComboID, std::ssize( m_StaticComboDupRecords ), m_StaticComboDupRecords.data() );
		if ( nSearchAliases != -1 )
			nStaticComboID = m_StaticComboDupRecords[nSearchAliases].m_nSourceStaticCombo;
		return FindShaderStaticCombo( nStaticComboID, m_Header, m_StaticComboRecords.data() );
	}

	bool operator==( const ShaderFileCache_t& a ) const
	{
		return m_Name.data() == a.m_Name.data() && m_bVertexShader == a.m_bVertexShader;
	}
};


//-----------------------------------------------------------------------------
// Vertex + pixel shader manager
//-----------------------------------------------------------------------------
class CShaderManager
{
public:
	CShaderManager();
	~CShaderManager();

	// Methods of IShaderManager
	void				Init();
	void				Shutdown();
	// Sets which dynamic version of the vertex + pixel shader to use
	void				SetVertexShaderIndex( int vshIndex ) { m_nVertexShaderIndex = vshIndex; }
	void				SetPixelShaderIndex( int pshIndex ) { m_nPixelShaderIndex = pshIndex; }
	IShaderBuffer		*CompileShader( const char *pProgram, std::size_t nBufLen, const char *pShaderVersion );
	VertexShaderHandle_t CreateVertexShader( IShaderBuffer* pShaderBuffer );
	void				DestroyVertexShader( VertexShaderHandle_t hShader );
	PixelShaderHandle_t CreatePixelShader( IShaderBuffer* pShaderBuffer );
	void				DestroyPixelShader( PixelShaderHandle_t hShader );
	VertexShader_t		CreateVertexShader( const char *pVertexShaderFile, int nStaticVshIndex = 0, char *debugLabel = nullptr );
	PixelShader_t		CreatePixelShader( const char *pPixelShaderFile, int nStaticPshIndex = 0, char *debugLabel = nullptr );
	void				SetVertexShader( VertexShader_t shader );
	void				SetPixelShader( PixelShader_t shader );
	void				BindVertexShader( VertexShaderHandle_t shader );
	void				BindPixelShader( PixelShaderHandle_t shader );
	void				*GetCurrentVertexShader();
	void				*GetCurrentPixelShader();
	void				ResetShaderState();
	void				FlushShaders();
	void				ClearVertexAndPixelShaderRefCounts();
	void				PurgeUnusedVertexAndPixelShaders();
	void						SpewVertexAndPixelShaders();
	const char					*GetActiveVertexShaderName();
	const char					*GetActivePixelShaderName();
	bool						CreateDynamicCombos_Ver4( void *pContext, uint8 *pComboBuffer );
	bool						CreateDynamicCombos_Ver5( void *pContext, uint8 *pComboBuffer, char *debugLabel = nullptr );


	HardwareShader_t	GetVertexShader( VertexShader_t vs, int dynIdx );
	HardwareShader_t	GetPixelShader( PixelShader_t ps, int dynIdx );

	// Destroys all shaders
	void						DestroyAllShaders();

	void				AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics );

	void SetPixelShaderState_Internal( HardwareShader_t shader, DataCacheHandle_t hCachedShader  );
	void SetVertexShaderState_Internal( HardwareShader_t shader, DataCacheHandle_t hCachedShader );


private:
	struct ShaderStaticCombos_t
	{
		int					m_nCount;
		int					m_nNumDynamicCombosAfterSkips;

		HardwareShader_t	*m_pHardwareShaders;
		struct ShaderCreationData_t
		{
			std::vector< uint8 > 	ByteCode;
			uint32 				iCentroidMask;
		};

		ShaderCreationData_t *m_pCreationData;
	};
	
	struct ShaderLookup_t
	{
		std::string_view		m_Name;			// view into m_ShaderSymbolTable
		int						m_nStaticIndex;
		ShaderStaticCombos_t	m_ShaderStaticCombos;
		DWORD					m_Flags;
		int						m_nRefCount;
		ShaderFileCache_t		*m_pShaderFileCache;

		// for queued loading, bias an aligned optimal buffer forward to correct location
		int						m_nDataOffset;

		// diff version, valid during load only
		ShaderDictionaryEntry_t	*m_pComboDictionary;

		ShaderLookup_t()
		{
			m_Flags = 0;
			m_nRefCount = 0;
			m_ShaderStaticCombos.m_nCount = 0;
			m_ShaderStaticCombos.m_pHardwareShaders = 0;
			m_ShaderStaticCombos.m_pCreationData = 0;
			m_ShaderStaticCombos.m_nNumDynamicCombosAfterSkips = 0;
			m_pComboDictionary = nullptr;
		}
		void IncRefCount()
		{
			m_nRefCount++;
		}
		bool operator==( const ShaderLookup_t& a ) const
		{
			return m_Name.data() == a.m_Name.data() && m_nStaticIndex == a.m_nStaticIndex;
		}
	};

private:
	void					CreateStaticShaders();
	void					DestroyStaticShaders();

	// The low-level dx call to set the vertex shader state
	inline void				SetVertexShaderState( HardwareShader_t shader, DataCacheHandle_t hCachedShader = DC_INVALID_HANDLE )
	{
		if ( m_HardwareVertexShader != shader )
		{
			SetVertexShaderState_Internal( shader, hCachedShader );
		}
	}

	// The low-level dx call to set the pixel shader state
	inline void				SetPixelShaderState( HardwareShader_t shader, DataCacheHandle_t hCachedShader = DC_INVALID_HANDLE )
	{
		if ( m_HardwarePixelShader != shader )
		{
			SetPixelShaderState_Internal( shader, hCachedShader );
		}
	}

	typedef std::list< ShaderLookup_t > ShaderDict_t;

	// VertexShader_t/PixelShader_t handles are the addresses of dictionary entries
	static ShaderLookup_t	&ShaderLookup( intp hShader ) { return *(ShaderLookup_t *)hShader; }
	static intp				ShaderHandle( ShaderLookup_t &lookup ) { return (intp)&lookup; }

	// Destroy a particular vertex shader; returns the next entry
	ShaderDict_t::iterator	DestroyVertexShader( ShaderDict_t::iterator shader );
	// Destroy a particular pixel shader; returns the next entry
	ShaderDict_t::iterator	DestroyPixelShader( ShaderDict_t::iterator shader );

	bool					LoadAndCreateShaders( ShaderLookup_t &lookup, bool bVertexShader, char *debugLabel = nullptr );
	bool					DoesShaderCRCMatchSourceCode( const char *pFileName, uint32 crc32, uint32 &sourceCRC );
	FileHandle_t			OpenFileAndLoadHeader( const char *pFileName, ShaderHeader_t *pHeader );

	void					DisassembleShader( ShaderLookup_t *pLookup, int dynamicCombo, uint8 *pByteCode );
	void					WriteTranslatedFile( ShaderLookup_t *pLookup, int dynamicCombo, char *pFileContents, char *pFileExtension );

	// OSX only, no-op otherwise
	void					SaveShaderCache( char *cacheName );	// query GLM pair cache for all active shader pairs and write them to disk in named file
	bool					LoadShaderCache( char *cacheName );	// read named file, establish compiled shader sets for each vertex+static and pixel+static, then link pairs as listed in table

	// The current vertex and pixel shader index
	int m_nVertexShaderIndex;
	int m_nPixelShaderIndex;

	// lists: handles point at the entries, which must not move
	ShaderDict_t m_VertexShaderDict;
	ShaderDict_t m_PixelShaderDict;

	ks::intern_pool m_ShaderSymbolTable;

	// The current vertex and pixel shader
	HardwareShader_t	m_HardwareVertexShader;
	HardwareShader_t	m_HardwarePixelShader;

	// Entries are only added or purged all at once; a deque so lookups can point at them
	std::deque< ShaderFileCache_t > m_ShaderFileCache;

	// false, creates during init.
	// true, creates on access, helps reduce d3d memory for tools, but causes i/o hitches.
	bool m_bCreateShadersOnDemand;

#if defined( _DEBUG )
	// for debugging (can't resolve UtlSym)
	// need some history because 360 d3d has rips related to sequencing
	char	vshDebugName[MAX_SHADER_HISTORY][64];
	int		vshDebugIndex;
	char	pshDebugName[MAX_SHADER_HISTORY][64];
	int		pshDebugIndex;
#endif

};


//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
static CShaderManager s_ShaderManager;

// IShaderManager is a stateless facade that forwards to the one shader manager.
static IShaderManager s_ShaderManagerFacade;
IShaderManager *g_pShaderManager = &s_ShaderManagerFacade;
static CShaderManager &ShaderManagerImpl() { return s_ShaderManager; }
#include "ishadermanager_facade.inl"

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CShaderManager::CShaderManager() : 
	m_ShaderSymbolTable( true /* caseInsensitive */ )
{
	m_bCreateShadersOnDemand = false;

#ifdef _DEBUG
	vshDebugIndex = 0;
	pshDebugIndex = 0;
#endif
}

CShaderManager::~CShaderManager()
{
}

inline constexpr char REMOTE_SHADER_COMPILE_PORT[] = "20000";

//-----------------------------------------------------------------------------
// Syncs shader cache directory
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Initialization, shutdown
//-----------------------------------------------------------------------------
void CShaderManager::Init()
{
	// incompatible with the 360, violates loading system
	// only used by PC to help tools reduce d3d footprint
	m_bCreateShadersOnDemand = ( ShaderUtil()->InEditorMode() || CommandLine()->CheckParm( "-shadersondemand" ) );

	CreateStaticShaders();

}

void CShaderManager::Shutdown()
{


	DestroyAllShaders();
	DestroyStaticShaders();
}


//-----------------------------------------------------------------------------
// Compiles shaders
//-----------------------------------------------------------------------------
IShaderBuffer *CShaderManager::CompileShader( const char *pProgram, std::size_t nBufLen, const char *pShaderVersion )
{

	DevWarning( "ERROR: CompileShader called in a non-DYNAMIC_SHADER_COMPILE build!\n" );
	DebuggerBreak();
	return nullptr;

}


VertexShaderHandle_t CShaderManager::CreateVertexShader( IShaderBuffer* pShaderBuffer )
{
	// Create the vertex shader
	IDirect3DVertexShader9 *pVertexShader = nullptr;

	HRESULT hr = Dx9Device()->CreateVertexShader( (const DWORD*)pShaderBuffer->GetBits(), &pVertexShader );

	if ( FAILED( hr ) || !pVertexShader )
		return VERTEX_SHADER_HANDLE_INVALID;

	s_NumVertexShadersCreated++;
	RegisterVS( pShaderBuffer->GetBits(), pShaderBuffer->GetSize(), pVertexShader );

	// The handle is the D3D shader itself
	return (VertexShaderHandle_t)pVertexShader;
}

void CShaderManager::DestroyVertexShader( VertexShaderHandle_t hShader )
{
	if ( hShader == VERTEX_SHADER_HANDLE_INVALID )
		return;

	IDirect3DVertexShader9 *pVertexShader = (IDirect3DVertexShader9 *)hShader;

	UnregisterVS( pVertexShader );

	( pVertexShader->Release() );
}

PixelShaderHandle_t CShaderManager::CreatePixelShader( IShaderBuffer* pShaderBuffer )
{
	// Create the vertex shader
	IDirect3DPixelShader9 *pPixelShader = nullptr;
	HRESULT hr = Dx9Device()->CreatePixelShader( (const DWORD*)pShaderBuffer->GetBits(), &pPixelShader );

	if ( FAILED( hr ) || !pPixelShader )
		return PIXEL_SHADER_HANDLE_INVALID;

	s_NumPixelShadersCreated++;

	RegisterPS( pShaderBuffer->GetBits(), pShaderBuffer->GetSize(), pPixelShader );

	// The handle is the D3D shader itself
	return (PixelShaderHandle_t)pPixelShader;
}

void CShaderManager::DestroyPixelShader( PixelShaderHandle_t hShader )
{
	if ( hShader == PIXEL_SHADER_HANDLE_INVALID )
		return;

	IDirect3DPixelShader9 *pPixelShader = (IDirect3DPixelShader9 *)hShader;

	UnregisterPS( pPixelShader );

	( pPixelShader->Release() );
}


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
HardwareShader_t s_pIllegalMaterialPS = INVALID_HARDWARE_SHADER;

//-----------------------------------------------------------------------------
// Static methods
//-----------------------------------------------------------------------------
void CShaderManager::CreateStaticShaders()
{

	// GR - hack for illegal materials
	const DWORD psIllegalMaterial[] =
	{
			0xffff0101, 0x00000051, 0xa00f0000, 0x00000000, 0x3f800000, 0x00000000, 
			0x3f800000, 0x00000001, 0x800f0000, 0xa0e40000, 0x0000ffff
	};
	// create default shader
	Dx9Device()->CreatePixelShader( psIllegalMaterial, ( IDirect3DPixelShader9 ** )&s_pIllegalMaterialPS );
}

void CShaderManager::DestroyStaticShaders()
{
	// GR - invalid material hack
	// destroy internal shader
	if ( s_pIllegalMaterialPS != INVALID_HARDWARE_SHADER )
	{
		( ( IDirect3DPixelShader9 * )s_pIllegalMaterialPS )->Release();
		s_pIllegalMaterialPS = INVALID_HARDWARE_SHADER;
	}
}

//-----------------------------------------------------------------------------
// Open the shader file, optionally gets the header
//-----------------------------------------------------------------------------
FileHandle_t CShaderManager::OpenFileAndLoadHeader( const char *pFileName, ShaderHeader_t *pHeader )
{
	FileHandle_t fp = g_pFullFileSystem->Open( pFileName, "rb", "PLATFORM" );
	if ( fp == FILESYSTEM_INVALID_HANDLE )
	{
		return FILESYSTEM_INVALID_HANDLE;
	}

	if ( pHeader )
	{
		// read the header 
		g_pFullFileSystem->Read( pHeader, sizeof( ShaderHeader_t ), fp );

		switch ( pHeader->m_nVersion )
		{
			case 4:
				// version with combos done as diffs vs a reference combo
				// vsh/psh or older fxc
				break;

			case 5:
			case 6:
				// version with optimal dictionary and compressed combo block
				break;

			default:
				contract_assert( 0 );
				DevWarning( "Shader %s is the wrong version %d, expecting %d\n", pFileName, pHeader->m_nVersion, SHADER_VCS_VERSION_NUMBER );
				g_pFullFileSystem->Close( fp );
				return FILESYSTEM_INVALID_HANDLE;
		}
	}

	return fp;
}

//---------------------------------------------------------------------------------------------------------
// Writes text files named for looked-up shaders.  Used by GL shader translator to dump code for debugging
//---------------------------------------------------------------------------------------------------------
void CShaderManager::WriteTranslatedFile( ShaderLookup_t *pLookup, int dynamicCombo, char *pFileContents, char *pFileExtension )
{
	const char *pName = pLookup->m_Name.data();
	int nNumChars = V_strlen( pFileContents );

	char filename[MAX_PATH];
	std::sprintf( filename, "%s_%d_%d.%s", pName, pLookup->m_nStaticIndex, dynamicCombo, pFileExtension );
	g_pFullFileSystem->WriteFile( filename, "DEFAULT_WRITE_PATH", ks::byte_view( pFileContents, nNumChars ) );
}


//-----------------------------------------------------------------------------
// Disassemble a shader for debugging. Writes .asm files.
//-----------------------------------------------------------------------------
void CShaderManager::DisassembleShader( ShaderLookup_t *pLookup, int dynamicCombo, uint8 *pByteCode )
{
}

//-----------------------------------------------------------------------------
// Create dynamic combos
//-----------------------------------------------------------------------------
bool CShaderManager::CreateDynamicCombos_Ver4( void *pContext, uint8 *pComboBuffer )
{
	ks::ScratchArena<> scratch;
	ShaderLookup_t* pLookup = (ShaderLookup_t *)pContext;

	ShaderFileCache_t *pFileCache = pLookup->m_pShaderFileCache;
	ShaderHeader_t *pHeader = &pFileCache->m_Header;

	int nReferenceComboSizeForDiffs = ((ShaderHeader_t_v4 *)pHeader)->m_nDiffReferenceSize;

	uint8 *pReferenceShader = nullptr;
	uint8 *pDiffOutputBuffer = nullptr;
	if ( nReferenceComboSizeForDiffs )
	{
		// reference combo is *always* the largest combo, so safe worst case size for uncompression buffer
		pReferenceShader = (uint8 *)pFileCache->m_ReferenceCombo.data();
		pDiffOutputBuffer = (uint8 *)scratch.AllocBytes( nReferenceComboSizeForDiffs ); 
	}

	// build this shader's dynamic combos
	bool bOK = true;
	int nStartingOffset = 0;
	for ( int i = 0; i < pHeader->m_nDynamicCombos; i++ )
	{
		if ( pLookup->m_pComboDictionary[i].m_Offset == -1 )
		{
			// skipped
			continue;
		}

		if ( !nStartingOffset )
		{
			nStartingOffset = pLookup->m_pComboDictionary[i].m_Offset;
		}

		// offsets better be sequentially ascending
		contract_assert( nStartingOffset <= pLookup->m_pComboDictionary[i].m_Offset );

		if ( pLookup->m_pComboDictionary[i].m_Size <= 0 )
		{
			// skipped
			continue;
		}

		// get the right byte code from the monolithic buffer
		uint8 *pByteCode = (uint8 *)pComboBuffer + pLookup->m_nDataOffset + pLookup->m_pComboDictionary[i].m_Offset - nStartingOffset;
		int nByteCodeSize = pLookup->m_pComboDictionary[i].m_Size;

		if ( pReferenceShader )
		{
			// reference combo better be the largest combo, otherwise memory corruption
			contract_assert( nReferenceComboSizeForDiffs >= nByteCodeSize );

			// use the differencing algorithm to recover the full shader
			int nOriginalSize;
			ApplyDiffs( 
				pReferenceShader, 
				pByteCode,
				nReferenceComboSizeForDiffs,
				nByteCodeSize,
				nOriginalSize,
				pDiffOutputBuffer,
				nReferenceComboSizeForDiffs );

			pByteCode = pDiffOutputBuffer;
			nByteCodeSize = nOriginalSize;
		}

		HardwareShader_t hardwareShader = INVALID_HARDWARE_SHADER;

		if ( m_bCreateShadersOnDemand )
		{
			// cache the code off for later
			pLookup->m_ShaderStaticCombos.m_pCreationData[i].ByteCode.assign( nByteCodeSize, {} );
			V_memcpy( pLookup->m_ShaderStaticCombos.m_pCreationData[i].ByteCode.data(), pByteCode, nByteCodeSize );
			pLookup->m_ShaderStaticCombos.m_pCreationData[i].iCentroidMask = pFileCache->m_bVertexShader ? 0 : pHeader->m_nCentroidMask;
		}
		else
		{
			const char *pShaderName = pLookup->m_Name.data();

			if ( pFileCache->m_bVertexShader )
			{
				hardwareShader = CreateD3DVertexShader( reinterpret_cast< DWORD *>( pByteCode ), nByteCodeSize, pShaderName );
			}
			else
			{
				hardwareShader = CreateD3DPixelShader( reinterpret_cast< DWORD *>( pByteCode ), pHeader->m_nCentroidMask, nByteCodeSize, pShaderName );
			}

			if ( hardwareShader == INVALID_HARDWARE_SHADER )
			{
				contract_assert( 0 );
				bOK = false;
				break;
			}
		}
		pLookup->m_ShaderStaticCombos.m_pHardwareShaders[i] = hardwareShader;
	}

	delete [] pLookup->m_pComboDictionary;
	pLookup->m_pComboDictionary = nullptr;

	return bOK;
}

//-----------------------------------------------------------------------------
// Create dynamic combos
//-----------------------------------------------------------------------------
static uint32 NextULONG( uint8 * &pData )
{
	// handle unaligned read
	uint32 nRet;
	std::memcpy( &nRet, pData, sizeof( nRet ) );
	pData += sizeof( nRet );
	return nRet;
}
bool CShaderManager::CreateDynamicCombos_Ver5( void *pContext, uint8 *pComboBuffer, char *debugLabel )
{
	ShaderLookup_t* pLookup = (ShaderLookup_t *)pContext;
	ShaderFileCache_t *pFileCache = pLookup->m_pShaderFileCache;
	uint8 *pCompressedShaders = pComboBuffer + pLookup->m_nDataOffset;

	uint8 *pUnpackBuffer = new uint8[MAX_SHADER_UNPACKED_BLOCK_SIZE];

	char *debugLabelPtr = debugLabel;	// can be moved to point at something else if need be
	
	// now, loop through all blocks
	bool bOK = true;
	while ( bOK )
	{
		uint32 nBlockSize = NextULONG( pCompressedShaders );
		if ( nBlockSize == 0xffffffff )	
		{
			// any more blocks?
			break;
		}

		switch( nBlockSize  & 0xc0000000 )
		{
			case 0:											// bzip2
			{
				// uncompress
				uint32 nOutsize = MAX_SHADER_UNPACKED_BLOCK_SIZE;
				int nRslt = BZ2_bzBuffToBuffDecompress( 
					reinterpret_cast<char *>( pUnpackBuffer ),
					&nOutsize,
					reinterpret_cast<char *>( pCompressedShaders ),
					nBlockSize, 1, 0 );
				if ( nRslt < 0 )
				{
					// errors are negative for bzip
					contract_assert( 0 );
					DevWarning( "BZIP Error (%d) decompressing shader", nRslt );
					bOK = false;
				}
				
				pCompressedShaders += nBlockSize;
				nBlockSize = nOutsize;		// how much data there is
			}
			break;

			case 0x80000000:								// uncompressed
			{
				// not compressed, as is
				nBlockSize &= 0x3fffffff;
				std::memcpy( pUnpackBuffer, pCompressedShaders, nBlockSize );
				pCompressedShaders += nBlockSize;
			}
			break;

			case 0x40000000:								// lzma compressed
			{
				CLZMA lzDecoder;
				nBlockSize &= 0x3fffffff;
				
				std::size_t nOutsize = lzDecoder.Uncompress(
					reinterpret_cast<uint8 *>( pCompressedShaders ),
					pUnpackBuffer );
				pCompressedShaders += nBlockSize;
				nBlockSize = nOutsize;		// how much data there is
			}
			break;
			
			default:
			{
				contract_assert( 0 );
				Error(" unrecognized shader compression type = file corrupt?");
				bOK = false;
			}
		}
		
		uint8 *pReadPtr = pUnpackBuffer;
		while ( pReadPtr < pUnpackBuffer+nBlockSize )
		{
			uint32 nCombo_ID = NextULONG( pReadPtr );
			uint32 nShaderSize = NextULONG( pReadPtr );
			
			HardwareShader_t hardwareShader = INVALID_HARDWARE_SHADER;

			int iIndex = nCombo_ID;
			if ( iIndex >= pLookup->m_nStaticIndex )
				iIndex -= pLookup->m_nStaticIndex;			// ver5 stores combos as full combo, ver6 as dynamic combo # only
			if ( m_bCreateShadersOnDemand )
			{
				// cache the code off for later
				pLookup->m_ShaderStaticCombos.m_pCreationData[iIndex].ByteCode.assign( nShaderSize, {} );
				V_memcpy( pLookup->m_ShaderStaticCombos.m_pCreationData[iIndex].ByteCode.data(), pReadPtr, nShaderSize );
				pLookup->m_ShaderStaticCombos.m_pCreationData[iIndex].iCentroidMask = pFileCache->m_bVertexShader ? 0 : pFileCache->m_Header.m_nCentroidMask;
			}
			else
			{
				const char *pShaderName = pLookup->m_Name.data();

				if ( pFileCache->m_bVertexShader )
				{


					// pass binary code to d3d interface, on GL it will invoke the translator back to asm
					hardwareShader = CreateD3DVertexShader( reinterpret_cast< DWORD *>( pReadPtr ), nShaderSize, pShaderName, debugLabelPtr );
				}
				else
				{

					// pass binary code to d3d interface, on GL it will invoke the translator back to asm
					hardwareShader = CreateD3DPixelShader( reinterpret_cast< DWORD *>( pReadPtr ), pFileCache->m_Header.m_nCentroidMask, nShaderSize, pShaderName, debugLabelPtr );
				}
				if ( hardwareShader == INVALID_HARDWARE_SHADER )
				{
					DevWarning( "failed to create shader\n" );
					contract_assert( 0 );
					bOK = false;
					break;
				}

				pLookup->m_ShaderStaticCombos.m_nNumDynamicCombosAfterSkips++;
			}
			pLookup->m_ShaderStaticCombos.m_pHardwareShaders[iIndex] = hardwareShader;
			pReadPtr += nShaderSize;
		}
	}

	delete[] pUnpackBuffer;

	return bOK;
}


// Convert from a static combo/dynamic combo back into the combo values and spew.
void BitchAboutSkippedCombo( const char *pShaderName, int nStaticComboID, int nDynamicComboID )
{
	char path[MAX_PATH];
	V_strncpy( path, pShaderName, MAX_PATH );
	V_FileBase( path, path, MAX_PATH );
	const ShaderComboSemantics_t *const *ppSemantics = ks::find_ptr( s_ShaderComboInfoByName, path );
	if ( !ppSemantics )
	{
		DevWarning( "Can't find combo info for skipped combo!  Tell a programmer!!!\n" );
		return;
	}

	const ShaderComboSemantics_t *pSemantics = *ppSemantics;

	// The static combo id actually has the dynamic bits embedded in it, so we need to extract those first.
	for ( int i = 0; i < pSemantics->nDynamicShaderComboArrayCount; i++ )
	{
		int comboSize = pSemantics->pDynamicShaderComboArray[ i ].m_nComboMax - pSemantics->pDynamicShaderComboArray[ i ].m_nComboMin + 1;
		nStaticComboID /= comboSize;
	}

	DevWarning( "static combos: " );
	for ( int i = 0; i < pSemantics->nStaticShaderComboArrayCount; i++ )
	{
		int comboSize = pSemantics->pStaticShaderComboArray[i].m_nComboMax - pSemantics->pStaticShaderComboArray[i].m_nComboMin + 1;
		int comboVal = nStaticComboID % comboSize;
		if ( SHADER_COMBO_SPEW_VERBOSE || comboVal != 0 )
		{
			const char *pName = pSemantics->pStaticShaderComboArray[i].m_pComboName;
			DevWarning( "%s=%d ", pName, comboVal );
		}
		nStaticComboID /= comboSize;
	}
	DevWarning( "\n" );

	if ( nDynamicComboID != -1 )
	{
		DevWarning( "dynamic combos: " );
		for ( int i = 0; i < pSemantics->nDynamicShaderComboArrayCount; i++ ) 
		{
			int comboSize = pSemantics->pDynamicShaderComboArray[i].m_nComboMax - pSemantics->pDynamicShaderComboArray[i].m_nComboMin + 1;
			int comboVal = nDynamicComboID % comboSize;
			if ( SHADER_COMBO_SPEW_VERBOSE || comboVal != 0 )
			{
				const char *pName = pSemantics->pDynamicShaderComboArray[i].m_pComboName;
				DevWarning( "%s=%d ", pName, comboVal );
			}
			nDynamicComboID /= comboSize;
		}
		DevWarning( "\n" );
	}
}

void PrintComboDesc( const char *pShaderName, int nStaticComboID, int nDynamicComboID )
{
	char path[MAX_PATH];
	V_strncpy( path, pShaderName, MAX_PATH );
	V_FileBase( path, path, MAX_PATH );
	const ShaderComboSemantics_t *const *ppSemantics = ks::find_ptr( s_ShaderComboInfoByName, path );
	if ( !ppSemantics )
	{
		DevWarning( "Can't print combo desc for shader \"%s\"\n", pShaderName );
		return;
	}

	const ShaderComboSemantics_t *pSemantics = *ppSemantics;

	// The static combo id actually has the dynamic bits embedded in it, so we need to extract those first.
	for ( int i = 0; i < pSemantics->nDynamicShaderComboArrayCount; i++ )
	{
		int comboSize = pSemantics->pDynamicShaderComboArray[ i ].m_nComboMax - pSemantics->pDynamicShaderComboArray[ i ].m_nComboMin + 1;
		nStaticComboID /= comboSize;
	}

	Msg( "static combos: " );
	for ( int i = 0; i < pSemantics->nStaticShaderComboArrayCount; i++ )
	{
		int comboSize = pSemantics->pStaticShaderComboArray[i].m_nComboMax - pSemantics->pStaticShaderComboArray[i].m_nComboMin + 1;
		int comboVal = nStaticComboID % comboSize;
		if ( SHADER_COMBO_SPEW_VERBOSE || comboVal != 0 )
		{
			const char *pName = pSemantics->pStaticShaderComboArray[i].m_pComboName;
			Msg( "%s=%d ", pName, comboVal );
		}
		nStaticComboID /= comboSize;
	}
	Msg( "\n" );

	if ( nDynamicComboID != -1 )
	{
		Msg( "dynamic combos: " );
		for ( int i = 0; i < pSemantics->nDynamicShaderComboArrayCount; i++ )
		{
			int comboSize = pSemantics->pDynamicShaderComboArray[i].m_nComboMax - pSemantics->pDynamicShaderComboArray[i].m_nComboMin + 1;
			int comboVal = nDynamicComboID % comboSize;
			if ( SHADER_COMBO_SPEW_VERBOSE || comboVal != 0 )
			{
				const char *pName = pSemantics->pDynamicShaderComboArray[i].m_pComboName;
				Msg( "%s=%d ", pName, comboVal );
			}
			nDynamicComboID /= comboSize;
		}
		Msg( "\n" );
	}
}

//-----------------------------------------------------------------------------
// Loads all shaders
//-----------------------------------------------------------------------------
bool CShaderManager::LoadAndCreateShaders( ShaderLookup_t &lookup, bool bVertexShader, char *debugLabel )
{
	const char *pName = lookup.m_Name.data();

	// find it in the cache
	// a cache hit prevents costly i/o for static components, i.e. header, ref combo, etc.
	ShaderFileCache_t fileCacheLookup;
	fileCacheLookup.m_Name = lookup.m_Name;
	fileCacheLookup.m_bVertexShader = bVertexShader;
	auto fileCacheIt = std::find( m_ShaderFileCache.begin(), m_ShaderFileCache.end(), fileCacheLookup );
	// not found, create a new entry
	ShaderFileCache_t *pFileCache = ( fileCacheIt != m_ShaderFileCache.end() ) ? &*fileCacheIt : &m_ShaderFileCache.emplace_back();

	lookup.m_pShaderFileCache = pFileCache;

	// fetch from cache
	ShaderHeader_t *pHeader = &pFileCache->m_Header;

	FileHandle_t hFile = FILESYSTEM_INVALID_HANDLE;
	if ( pFileCache->IsValid() )
	{
		// using cached header, just open file, no read of header needed
		hFile = OpenFileAndLoadHeader( pFileCache->m_Filename.data(), nullptr );
		if ( hFile == FILESYSTEM_INVALID_HANDLE )
		{
			// shouldn't happen
			contract_assert( 0 );
			return false;
		}
	}
	else
	{
		V_memset( pHeader, 0, sizeof( ShaderHeader_t ) );

		// try the vsh/psh dir first
		char filename[MAX_PATH];
		V_snprintf( filename, MAX_PATH, ks::Concat( "shaders\\%s\\%s", SHADER_FNAME_EXTENSION ).c_str(), bVertexShader ? "vsh" : "psh", pName );
		hFile = OpenFileAndLoadHeader( filename, pHeader );
		if ( hFile == FILESYSTEM_INVALID_HANDLE )
		{
			// next, try the fxc dir
			V_snprintf( filename, MAX_PATH, ks::Concat( "shaders\\fxc\\%s", SHADER_FNAME_EXTENSION ).c_str(), pName );
			hFile = OpenFileAndLoadHeader( filename, pHeader );
			if ( hFile == FILESYSTEM_INVALID_HANDLE )
			{
				lookup.m_Flags |= SHADER_FAILED_LOAD;
				DevWarning( "Couldn't load %s shader %s\n", bVertexShader ? "vertex" : "pixel", pName );
				return false;
			}
		}
		else
		{
			lookup.m_Flags |= SHADER_IS_ASM;
		}

		lookup.m_Flags = pHeader->m_nFlags;

		pFileCache->m_Name = lookup.m_Name;
		pFileCache->m_Filename = m_ShaderSymbolTable.intern( filename );
		pFileCache->m_bVertexShader = bVertexShader;

		if ( pFileCache->IsOldVersion() )
		{ 
			int referenceComboSize = ((ShaderHeader_t_v4 *)pHeader)->m_nDiffReferenceSize;
			if ( referenceComboSize )
			{
				// cache the reference combo
				pFileCache->m_ReferenceCombo.resize( referenceComboSize );
				g_pFullFileSystem->Read( pFileCache->m_ReferenceCombo.data(), referenceComboSize, hFile );
			}
		}
		else
		{
			// cache the dictionary
			ks::ensure_size( pFileCache->m_StaticComboRecords, pHeader->m_nNumStaticCombos );
			g_pFullFileSystem->Read( pFileCache->m_StaticComboRecords.data(), pHeader->m_nNumStaticCombos * sizeof( StaticComboRecord_t ), hFile );
			if ( pFileCache->IsVersion6() )
			{
				// read static combo alias records
				int nNumDups;
				g_pFullFileSystem->Read( &nNumDups, sizeof( nNumDups ), hFile );
				if ( nNumDups )
				{
					ks::ensure_size( pFileCache->m_StaticComboDupRecords, nNumDups );
					g_pFullFileSystem->Read( pFileCache->m_StaticComboDupRecords.data(), nNumDups * sizeof( StaticComboAliasRecord_t ), hFile );
				}
			}

		}
	}

	// FIXME: should make lookup and ShaderStaticCombos_t are pool allocated.
	int i;
	lookup.m_ShaderStaticCombos.m_nCount = pHeader->m_nDynamicCombos;
	lookup.m_ShaderStaticCombos.m_pHardwareShaders = new HardwareShader_t[pHeader->m_nDynamicCombos];
	lookup.m_ShaderStaticCombos.m_nNumDynamicCombosAfterSkips = 0;
	if ( m_bCreateShadersOnDemand )
	{
		lookup.m_ShaderStaticCombos.m_pCreationData = new ShaderStaticCombos_t::ShaderCreationData_t[pHeader->m_nDynamicCombos];
	}
	for ( i = 0; i < pHeader->m_nDynamicCombos; i++ )
	{
		lookup.m_ShaderStaticCombos.m_pHardwareShaders[i] = INVALID_HARDWARE_SHADER;
	}

	int nStartingOffset = 0;
	int nEndingOffset = 0;

	if ( pFileCache->IsOldVersion() )
	{
		int nDictionaryOffset = sizeof( ShaderHeader_t ) + ((ShaderHeader_t_v4 *)pHeader)->m_nDiffReferenceSize;

		// read in shader's dynamic combos directory
		lookup.m_pComboDictionary = new ShaderDictionaryEntry_t[pHeader->m_nDynamicCombos];
		g_pFullFileSystem->Seek( hFile, nDictionaryOffset + lookup.m_nStaticIndex * sizeof( ShaderDictionaryEntry_t ), FILESYSTEM_SEEK_HEAD );
		if( !g_pFullFileSystem->Read( lookup.m_pComboDictionary, pHeader->m_nDynamicCombos * sizeof( ShaderDictionaryEntry_t ), hFile ) )
		{
			g_pFullFileSystem->Close( hFile );
			const char *pShaderName;
			pShaderName = pFileCache->m_Filename.data();
			DevWarning( "Shader '%s' - Cannot read, skipping.\n", pShaderName );
			return false;
		}

		// want single read of all this shader's dynamic combos into a target buffer
		// shaders are written sequentially, determine starting offset and length
		for ( i = 0; i < pHeader->m_nDynamicCombos; i++ )
		{
			if ( lookup.m_pComboDictionary[i].m_Offset == -1 )
			{
				// skipped
				continue;
			}

			// ensure offsets are in fact sequentially ascending 
			contract_assert( lookup.m_pComboDictionary[i].m_Offset >= nStartingOffset && lookup.m_pComboDictionary[i].m_Size >= 0 );

			if ( !nStartingOffset )
			{
				nStartingOffset = lookup.m_pComboDictionary[i].m_Offset;
			}
			nEndingOffset = lookup.m_pComboDictionary[i].m_Offset + lookup.m_pComboDictionary[i].m_Size;
		}
		if ( !nStartingOffset )
		{
			g_pFullFileSystem->Close( hFile );
			const char *pShaderName;
			pShaderName = pFileCache->m_Filename.data();
			DevWarning( "Shader '%s' - All dynamic combos skipped. This is bad!\n", pShaderName );
			contract_assert( 0 );
			return false;
		}
	}
	else
	{
		int nStaticComboIdx = pFileCache->FindCombo( lookup.m_nStaticIndex / pFileCache->m_Header.m_nDynamicCombos );
		if ( nStaticComboIdx == -1 )
		{
			g_pFullFileSystem->Close( hFile );
			lookup.m_Flags |= SHADER_FAILED_LOAD;
			const char *pShaderName;
			pShaderName = pFileCache->m_Filename.data();
			DevWarning( "*************************************************\n" );
			DevWarning( "Shader '%s' - Couldn't load combo %d of shader (dyn=%d)\n", pShaderName, lookup.m_nStaticIndex, pFileCache->m_Header.m_nDynamicCombos );
			BitchAboutSkippedCombo( pShaderName, lookup.m_nStaticIndex / pFileCache->m_Header.m_nDynamicCombos, -1 );
			DevWarning( "*************************************************\n" );
			contract_assert( 0 );
			return false;
		}

		nStartingOffset = pFileCache->m_StaticComboRecords[nStaticComboIdx].m_nFileOffset;
		nEndingOffset = pFileCache->m_StaticComboRecords[nStaticComboIdx+1].m_nFileOffset;
	}

	// align offsets for unbuffered optimal i/o - fastest i/o possible
	unsigned nOffsetAlign, nSizeAlign, nBufferAlign;
	g_pFullFileSystem->GetOptimalIOConstraints( hFile, &nOffsetAlign, &nSizeAlign, &nBufferAlign );
	unsigned int nAlignedOffset = AlignValue( ( nStartingOffset - nOffsetAlign ) + 1, nOffsetAlign );
	unsigned int nAlignedBytesToRead = AlignValue( nEndingOffset - nAlignedOffset, nSizeAlign );

	// used for adjusting provided buffer to actual data
	lookup.m_nDataOffset = nStartingOffset - nAlignedOffset;

	bool bOK = true;
	//printf("\n CShaderManager::LoadAndCreateShaders - reading %d bytes from file offset %d", nAlignedBytesToRead, nAlignedOffset);
	// single optimal read of all dynamic combos into monolithic buffer
	uint8 *pOptimalBuffer = (uint8 *)g_pFullFileSystem->AllocOptimalReadBuffer( hFile, nAlignedBytesToRead, nAlignedOffset );
	g_pFullFileSystem->Seek( hFile, nAlignedOffset, FILESYSTEM_SEEK_HEAD );
	if( g_pFullFileSystem->Read( pOptimalBuffer, nAlignedBytesToRead, hFile ) )
	{
		if ( pFileCache->IsOldVersion() )
		{
			bOK = CreateDynamicCombos_Ver4( &lookup, pOptimalBuffer );
		}
		else
		{
			bOK = CreateDynamicCombos_Ver5( &lookup, pOptimalBuffer, debugLabel );
		}
	}

	g_pFullFileSystem->FreeOptimalReadBuffer( pOptimalBuffer );

	g_pFullFileSystem->Close( hFile );

	if ( !bOK )
	{
		lookup.m_Flags |= SHADER_FAILED_LOAD;
	}

	return bOK;
}


//----------------------------------------------------------------------------------old code

//----------------------------------------------------------------------------------old code


//-----------------------------------------------------------------------------
// Purpose: compare two KeyValues by name
//-----------------------------------------------------------------------------
typedef KeyValues* PKEYVALUES;
int KeyValueNameCompare( const PKEYVALUES *pLeft, const PKEYVALUES *pRight )
{
	// Compare vertex shader name

	const char *pLeftString = (*pLeft)->GetString( "vs" );
	const char *pRightString = (*pRight)->GetString( "vs" );

	int nVSCompare = V_stricmp( pLeftString, pRightString );
	if ( nVSCompare > 0 )
		return 1;
	else if ( nVSCompare < 0 )
		return -1;

	// Compare pixel shader name
	pLeftString = (*pLeft)->GetString( "ps" );
	pRightString = (*pRight)->GetString( "ps" );

	int nPSCompare = V_stricmp( pLeftString, pRightString );
	if ( nPSCompare > 0 )
		return 1;
	else if ( nPSCompare < 0 )
		return -1;

	// Compare vs static index
	int nLeft  = (*pLeft)->GetInt( "vs_static" );
	int nRight = (*pRight)->GetInt( "vs_static" );
	if ( nLeft > nRight )
		return 1;
	else if ( nRight > nLeft )
		return -1;

	// Compare ps static index
	nLeft  = (*pLeft)->GetInt( "ps_static" );
	nRight = (*pRight)->GetInt( "ps_static" );
	if ( nLeft > nRight )
		return 1;
	else if ( nRight > nLeft )
		return -1;

	// Compare vs dynamic index
	nLeft  = (*pLeft)->GetInt( "vs_dynamic" );
	nRight = (*pRight)->GetInt( "vs_dynamic" );
	if ( nLeft > nRight )
		return 1;
	else if ( nRight > nLeft )
		return -1;

	// Compare ps dynamic index
	nLeft  = (*pLeft)->GetInt( "ps_dynamic" );
	nRight = (*pRight)->GetInt( "ps_dynamic" );
	if ( nLeft > nRight )
		return 1;
	else if ( nRight > nLeft )
		return -1;

	return 0; // exactly equal...this should never happen
}


void	CShaderManager::SaveShaderCache( char *cacheName )
{

}

bool	CShaderManager::LoadShaderCache( char *cacheName )
{
	return false;	// have to return a value on Windows build to appease compiler
}


	
//-----------------------------------------------------------------------------
// Creates and destroys vertex shaders
//-----------------------------------------------------------------------------
VertexShader_t CShaderManager::CreateVertexShader( const char *pFileName, int nStaticVshIndex, char *debugLabel )
{

	if ( !pFileName )
	{
		return INVALID_SHADER;
	}
	
	VertexShader_t shader;
	ShaderLookup_t lookup;
	lookup.m_Name = m_ShaderSymbolTable.intern( pFileName );
	lookup.m_nStaticIndex = nStaticVshIndex;
	auto it = std::find( m_VertexShaderDict.begin(), m_VertexShaderDict.end(), lookup );
	if ( it != m_VertexShaderDict.end() )
	{
		shader = ShaderHandle( *it );
	}
	else
	{
		//printf("\nCShaderManager::CreateVertexShader( filename = %s, staticVshIndex = %d - not in cache", pFileName, nStaticVshIndex );
	
		shader = ShaderHandle( m_VertexShaderDict.emplace_back( lookup ) );
		if ( !LoadAndCreateShaders( ShaderLookup( shader ), true, debugLabel ) )
		{
			return INVALID_SHADER;
		}
	}
	ShaderLookup( shader ).IncRefCount();
	return shader;
}

//-----------------------------------------------------------------------------
// Create pixel shader
//-----------------------------------------------------------------------------
PixelShader_t CShaderManager::CreatePixelShader( const char *pFileName, int nStaticPshIndex, char *debugLabel )
{

	if ( !pFileName )
	{
		return INVALID_SHADER;
	}
	
	PixelShader_t shader;
	ShaderLookup_t lookup;
	lookup.m_Name = m_ShaderSymbolTable.intern( pFileName );
	lookup.m_nStaticIndex = nStaticPshIndex;
	auto it = std::find( m_PixelShaderDict.begin(), m_PixelShaderDict.end(), lookup );
	if ( it != m_PixelShaderDict.end() )
	{
		shader = ShaderHandle( *it );
	}
	else
	{
		shader = ShaderHandle( m_PixelShaderDict.emplace_back( lookup ) );
		if ( !LoadAndCreateShaders( ShaderLookup( shader ), false, debugLabel ) )
		{
			return INVALID_SHADER;
		}
	}
	ShaderLookup( shader ).IncRefCount();
	return shader;
}

//-----------------------------------------------------------------------------
// Clear the refCounts to zero
//-----------------------------------------------------------------------------
void CShaderManager::ClearVertexAndPixelShaderRefCounts()
{
	for ( ShaderLookup_t &vshIndexLookup : m_VertexShaderDict )
	{
		VertexShader_t vshIndex = ShaderHandle( vshIndexLookup );
		ShaderLookup( vshIndex ).m_nRefCount = 0;
	}

	for ( ShaderLookup_t &pshIndexLookup : m_PixelShaderDict )
	{
		PixelShader_t pshIndex = ShaderHandle( pshIndexLookup );
		ShaderLookup( pshIndex ).m_nRefCount = 0;
	}
}

//-----------------------------------------------------------------------------
// Destroy all shaders that have no reference
//-----------------------------------------------------------------------------
void CShaderManager::PurgeUnusedVertexAndPixelShaders()
{
	
	// iterate vertex shaders
	for ( auto vshIndex = m_VertexShaderDict.begin(); vshIndex != m_VertexShaderDict.end(); )
	{
		contract_assert( vshIndex->m_nRefCount >= 0 );

		if ( vshIndex->m_nRefCount <= 0 )
			vshIndex = DestroyVertexShader( vshIndex );
		else
			++vshIndex;
	}

	// iterate pixel shaders
	for ( auto pshIndex = m_PixelShaderDict.begin(); pshIndex != m_PixelShaderDict.end(); )
	{
		contract_assert( pshIndex->m_nRefCount >= 0 );

		if ( pshIndex->m_nRefCount <= 0 )
			pshIndex = DestroyPixelShader( pshIndex );
		else
			++pshIndex;
	}
}



void* CShaderManager::GetCurrentVertexShader()
{
	return (void*)m_HardwareVertexShader;
}

void* CShaderManager::GetCurrentPixelShader()
{
	return (void*)m_HardwarePixelShader;
}


//-----------------------------------------------------------------------------
// The low-level dx call to set the vertex shader state
//-----------------------------------------------------------------------------
void CShaderManager::SetVertexShaderState_Internal( HardwareShader_t shader, DataCacheHandle_t hCachedShader )
{
	if ( m_HardwareVertexShader != shader )
	{
	 // hack hack hack

	VProfIncrementCounter<"vertex shader change", COUNTER_GROUP_DEFAULT>( 1 );
	Dx9Device()->SetVertexShader( (IDirect3DVertexShader9*)shader );
	m_HardwareVertexShader = shader;
	}
}

void CShaderManager::BindVertexShader( VertexShaderHandle_t hVertexShader )
{
	HardwareShader_t hHardwareShader = (HardwareShader_t)hVertexShader;
	SetVertexShaderState( hHardwareShader );
}


//-----------------------------------------------------------------------------
// Sets a particular vertex shader as the current shader
//-----------------------------------------------------------------------------
void CShaderManager::SetVertexShader( VertexShader_t shader )
{
	// Determine which vertex shader to use...
	if ( shader == INVALID_SHADER )
	{
		SetVertexShaderState( 0 );
		return;
	}

	int vshIndex = m_nVertexShaderIndex;
	contract_assert( vshIndex >= 0 );
	if( vshIndex < 0 )
	{
		vshIndex = 0;
	}

	ShaderLookup_t &vshLookup = ShaderLookup( shader );
//	DevWarning( "vsh: %s static: %d dynamic: %d\n", vshLookup.m_Name.data(),
//		vshLookup.m_nStaticIndex, m_nVertexShaderIndex );

	if ( vshLookup.m_Flags & SHADER_FAILED_LOAD )
	{
		contract_assert( 0 );
		return;
	}
#ifdef _DEBUG
	vshDebugIndex = (vshDebugIndex + 1) % MAX_SHADER_HISTORY;
	V_strncpy( vshDebugName[vshDebugIndex], vshLookup.m_Name.data(), sizeof( vshDebugName[0] ) );
#endif
	HardwareShader_t dxshader = vshLookup.m_ShaderStaticCombos.m_pHardwareShaders[vshIndex];

	if ( ( dxshader == INVALID_HARDWARE_SHADER ) && m_bCreateShadersOnDemand )
	{
		ShaderStaticCombos_t::ShaderCreationData_t *pCreationData = &vshLookup.m_ShaderStaticCombos.m_pCreationData[vshIndex];

		dxshader = CreateD3DVertexShader( ( DWORD * )pCreationData->ByteCode.data(), std::ssize( pCreationData->ByteCode ), vshLookup.m_Name.data() );

		vshLookup.m_ShaderStaticCombos.m_pHardwareShaders[vshIndex] = dxshader;
	}

	contract_assert( dxshader );

	if ( !dxshader )
	{
		static bool s_bFirst = true;
		if ( s_bFirst )
		{
			s_bFirst = false;

			DevWarning( "*************************************************\n" );
			DevWarning( "!!!!!Using invalid shader combo!!!!!  Consult a programmer and tell them to build debug materialsystem.dll and stdshader*.dll.  Run with \"mat_bufferprimitives 0\" and look for CMaterial in the call stack and see what m_pDebugName is.  You are likely using a shader combo that has been skipped.\n" );
			DevWarning( "Shader: %s static: %d dynamic: %d\n", vshLookup.m_Name.data(), vshLookup.m_nStaticIndex, m_nVertexShaderIndex );
			BitchAboutSkippedCombo( vshLookup.m_Name.data(), vshLookup.m_nStaticIndex, m_nVertexShaderIndex );
			DevWarning( "*************************************************\n" );
			contract_assert( 0 );
		}
	}

	SetVertexShaderState( dxshader );
}

//-----------------------------------------------------------------------------
// The low-level dx call to set the pixel shader state
//-----------------------------------------------------------------------------
void CShaderManager::SetPixelShaderState_Internal( HardwareShader_t shader, DataCacheHandle_t hCachedShader )
{
	if ( m_HardwarePixelShader != shader )
	{		
	VProfIncrementCounter<"pixel shader change", COUNTER_GROUP_DEFAULT>( 1 );
	Dx9Device()->SetPixelShader( (IDirect3DPixelShader*)shader );		
	m_HardwarePixelShader = shader;
	}
}

void CShaderManager::BindPixelShader( PixelShaderHandle_t hPixelShader )
{
	HardwareShader_t hHardwareShader = (HardwareShader_t)hPixelShader;
	SetPixelShaderState( hHardwareShader );
}

//-----------------------------------------------------------------------------
// Sets a particular pixel shader as the current shader
//-----------------------------------------------------------------------------
void CShaderManager::SetPixelShader( PixelShader_t shader )
{

	if ( shader == INVALID_SHADER )
	{
		SetPixelShaderState( 0 );
		return;
	}

	int pshIndex = m_nPixelShaderIndex;
	contract_assert( pshIndex >= 0 );
	ShaderLookup_t &pshLookup = ShaderLookup( shader );
	if ( pshIndex > pshLookup.m_ShaderStaticCombos.m_nCount )
	{
		SetPixelShaderState( 0 );
		DevWarning( "***** Invalid pixel shader index (out of range) for %s (%d of %d).\n", pshLookup.m_Name.data(), pshIndex, pshLookup.m_ShaderStaticCombos.m_nCount );
		contract_assert( 0 );
		return;
	}
	
//	DevWarning( "psh: %s static: %d dynamic: %d\n", lookup.m_Name.data(),
//		lookup.m_nStaticIndex, m_nPixelShaderIndex );
	if ( pshLookup.m_Flags & SHADER_FAILED_LOAD )
	{
		contract_assert( 0 );
		DevWarning( "***** Trying to set a pixel shader (%s) that failed loading!\n", pshLookup.m_Name.data() );
		return;
	}
	#ifdef _DEBUG
		pshDebugIndex = (pshDebugIndex + 1) % MAX_SHADER_HISTORY;
		V_strncpy( pshDebugName[pshDebugIndex], pshLookup.m_Name.data(), sizeof( pshDebugName[0] ) );
	#endif
	HardwareShader_t dxshader = pshLookup.m_ShaderStaticCombos.m_pHardwareShaders[pshIndex];

	if ( ( dxshader == INVALID_HARDWARE_SHADER ) && m_bCreateShadersOnDemand )
	{
		ShaderStaticCombos_t::ShaderCreationData_t *pCreationData = &pshLookup.m_ShaderStaticCombos.m_pCreationData[pshIndex];

		const char *pShaderName = pshLookup.m_Name.data();
		dxshader = CreateD3DPixelShader( ( DWORD * )pCreationData->ByteCode.data(), pCreationData->iCentroidMask, std::ssize( pCreationData->ByteCode ), pShaderName );

		pshLookup.m_ShaderStaticCombos.m_pHardwareShaders[pshIndex] = dxshader;
	}

	contract_assert( ( dxshader != INVALID_HARDWARE_SHADER ) && "Failed to set pixel shader." );
	if ( dxshader == INVALID_HARDWARE_SHADER )
	{
		static bool s_bFirst = true;
		if ( s_bFirst )
		{
			s_bFirst = false;

			DevWarning( "*************************************************\n" );
			DevWarning( "!!!!!Using invalid pixel shader combo!!!!!  Consult a programmer and tell them to build debug materialsystem.dll and stdshader*.dll.  Run with \"mat_bufferprimitives 0\" and look for CMaterial in the call stack and see what m_pDebugName is.  You are likely using a shader combo that has been skipped.\n" );
			DevWarning( "Shader: %s static: %d dynamic: %d\n", pshLookup.m_Name.data(), pshLookup.m_nStaticIndex, m_nPixelShaderIndex );
			BitchAboutSkippedCombo( pshLookup.m_Name.data(), pshLookup.m_nStaticIndex, m_nPixelShaderIndex );
			DevWarning( "*************************************************\n" );
			contract_assert( 0 );
		}
	}

	SetPixelShaderState( dxshader );
}

//-----------------------------------------------------------------------------
// Resets the shader state
//-----------------------------------------------------------------------------
void CShaderManager::ResetShaderState()
{
	// This will force the calls to SetVertexShader + SetPixelShader to actually set the state
	m_HardwareVertexShader = (HardwareShader_t)-1;
	m_HardwarePixelShader = (HardwareShader_t)-1;

	SetVertexShader( INVALID_SHADER );
	SetPixelShader( INVALID_SHADER );
}

//-----------------------------------------------------------------------------
// Destroy a particular vertex shader
//-----------------------------------------------------------------------------
CShaderManager::ShaderDict_t::iterator CShaderManager::DestroyVertexShader( ShaderDict_t::iterator shader )
{
	ShaderStaticCombos_t &combos = shader->m_ShaderStaticCombos;
	int i;
	for ( i = 0; i < combos.m_nCount; i++ )
	{
		if ( combos.m_pHardwareShaders[i] != INVALID_HARDWARE_SHADER )
		{
			IDirect3DVertexShader9* pShader = ( IDirect3DVertexShader9 * )combos.m_pHardwareShaders[i];
			UnregisterVS( pShader );
				pShader->Release();
		}
	}
	delete [] combos.m_pHardwareShaders;
	combos.m_pHardwareShaders = nullptr;

	if ( combos.m_pCreationData != nullptr )
	{
		delete [] combos.m_pCreationData;
		combos.m_pCreationData = nullptr;
	}

	return m_VertexShaderDict.erase( shader );
}

//-----------------------------------------------------------------------------
// Destroy a particular pixel shader
//-----------------------------------------------------------------------------
CShaderManager::ShaderDict_t::iterator CShaderManager::DestroyPixelShader( ShaderDict_t::iterator pixelShader )
{
	ShaderStaticCombos_t &combos = pixelShader->m_ShaderStaticCombos;
	int i;
	for ( i = 0; i < combos.m_nCount; i++ )
	{
		if ( combos.m_pHardwareShaders[i] != INVALID_HARDWARE_SHADER )
		{
			IDirect3DPixelShader* pShader = ( IDirect3DPixelShader * )combos.m_pHardwareShaders[i];
			UnregisterPS( pShader );
				pShader->Release();
		}
	}
	delete [] combos.m_pHardwareShaders;
	combos.m_pHardwareShaders = nullptr;

	if ( combos.m_pCreationData != nullptr )
	{
		delete [] combos.m_pCreationData;
		combos.m_pCreationData = nullptr;
	}

	return m_PixelShaderDict.erase( pixelShader );
}

HardwareShader_t CShaderManager::GetVertexShader( VertexShader_t vs, int dynIdx )
{
	ShaderLookup_t &vshLookup = ShaderLookup( vs );
	if ( !vshLookup.m_ShaderStaticCombos.m_pHardwareShaders )
	{
		Warning( "GetVertexShader: shader %s not loaded (m_pHardwareShaders is NULL)\n", vshLookup.m_Name.data() );
		return INVALID_HARDWARE_SHADER;
	}
	HardwareShader_t dxshader = vshLookup.m_ShaderStaticCombos.m_pHardwareShaders[dynIdx];
	return dxshader;
}

HardwareShader_t CShaderManager::GetPixelShader( PixelShader_t ps, int dynIdx )
{
	ShaderLookup_t &pshLookup = ShaderLookup( ps );
	if ( !pshLookup.m_ShaderStaticCombos.m_pHardwareShaders )
	{
		Warning( "GetPixelShader: shader %s not loaded (m_pHardwareShaders is NULL)\n", pshLookup.m_Name.data() );
		return INVALID_HARDWARE_SHADER;
	}
	HardwareShader_t dxshader = pshLookup.m_ShaderStaticCombos.m_pHardwareShaders[dynIdx];
	return dxshader;
}

//-----------------------------------------------------------------------------
// Destroys all shaders
//-----------------------------------------------------------------------------
void CShaderManager::DestroyAllShaders( void )
{

	for ( auto vshIndex = m_VertexShaderDict.begin(); vshIndex != m_VertexShaderDict.end(); )
	{
		contract_assert( vshIndex->m_nRefCount >= 0 );
		vshIndex = DestroyVertexShader( vshIndex );
	}

	for ( auto pshIndex = m_PixelShaderDict.begin(); pshIndex != m_PixelShaderDict.end(); )
	{
		contract_assert( pshIndex->m_nRefCount >= 0 );
		pshIndex = DestroyPixelShader( pshIndex );
	}

	// invalidate the file cache
	m_ShaderFileCache.clear();
}

//-----------------------------------------------------------------------------
// print all vertex and pixel shaders along with refcounts to the console
//-----------------------------------------------------------------------------
void CShaderManager::SpewVertexAndPixelShaders( void )
{
	// only spew a populated shader file cache
	Msg( "\nShader File Cache:\n" );
	for ( ShaderFileCache_t &cache : m_ShaderFileCache )
	{
		ShaderFileCache_t *pCache = &cache;
		Msg( "Total Combos:%9d Static:%9d Dynamic:%7d SeekTable:%7d Ver:%d '%s'\n", 
			pCache->m_Header.m_nTotalCombos, 
			pCache->m_Header.m_nDynamicCombos ? pCache->m_Header.m_nTotalCombos/pCache->m_Header.m_nDynamicCombos : 0,
			pCache->m_Header.m_nDynamicCombos,
			pCache->IsOldVersion() ? 0 : pCache->m_Header.m_nNumStaticCombos,
			pCache->m_Header.m_nVersion,
			pCache->m_Filename.data() );
	}
	Msg( "\n" );

	// spew vertex shader dictionary
	int totalVertexShaders = 0;
	int totalVertexShaderSets = 0;
	const char *pName;
	for ( ShaderLookup_t &vshIndexLookup : m_VertexShaderDict )
	{
		VertexShader_t vshIndex = ShaderHandle( vshIndexLookup );
		const ShaderLookup_t &lookup = ShaderLookup( vshIndex );
		pName = lookup.m_Name.data();
		Msg( "-- vsh 0x%8.8x: static combo:%9d dynamic combos:%6d refcount:%4d \"%s\"\n", (unsigned int)vshIndex,
			( int )lookup.m_nStaticIndex, ( int )lookup.m_ShaderStaticCombos.m_nNumDynamicCombosAfterSkips,
			lookup.m_nRefCount, pName );
		Msg( "   " );
		PrintComboDesc( pName, ( int )lookup.m_nStaticIndex, -1 );
		totalVertexShaders += lookup.m_ShaderStaticCombos.m_nNumDynamicCombosAfterSkips;
		totalVertexShaderSets++;
	}

	// spew pixel shader dictionary
	int totalPixelShaders = 0;
	int totalPixelShaderSets = 0;
	for ( ShaderLookup_t &pshIndexLookup : m_PixelShaderDict )
	{
		PixelShader_t pshIndex = ShaderHandle( pshIndexLookup );
		const ShaderLookup_t &lookup = ShaderLookup( pshIndex );
		pName = lookup.m_Name.data();
		Msg( "-- psh 0x%8.8x: static combo:%9d dynamic combos:%6d refcount:%4d \"%s\"\n", (unsigned int)pshIndex,
			( int )lookup.m_nStaticIndex, ( int )lookup.m_ShaderStaticCombos.m_nNumDynamicCombosAfterSkips,
			lookup.m_nRefCount, pName );
		Msg( "   " );
		PrintComboDesc( pName, ( int )lookup.m_nStaticIndex, -1 );
		totalPixelShaders += lookup.m_ShaderStaticCombos.m_nNumDynamicCombosAfterSkips;
		totalPixelShaderSets++;
	}

	Msg( "Total unique vertex shaders: %d\n", totalVertexShaders );
	Msg( "Total vertex shader sets: %d\n", totalVertexShaderSets );
	Msg( "Total unique pixel shaders: %d\n", totalPixelShaders );
	Msg( "Total pixel shader sets: %d\n", totalPixelShaderSets );
}

static void mat_spewvertexandpixelshaders( const CCommand &args );
static ConCommand mat_spewvertexandpixelshaders_command( "mat_spewvertexandpixelshaders", mat_spewvertexandpixelshaders, "Print all vertex and pixel shaders currently loaded to the console" );
static void mat_spewvertexandpixelshaders( const CCommand &args )
{
	( ( CShaderManager * )ShaderManager() )->SpewVertexAndPixelShaders();
}

const char *CShaderManager::GetActiveVertexShaderName()
{
#if !defined( _DEBUG )
	return "";
#else
	if ( !m_HardwareVertexShader )
	{
		return "NULL";
	}
	return vshDebugName[vshDebugIndex];
#endif
}

const char *CShaderManager::GetActivePixelShaderName()
{
#if !defined( _DEBUG )
	return "";
#else
	if ( !m_HardwarePixelShader )
	{
		return "NULL";
	}
	return pshDebugName[pshDebugIndex];
#endif
}

void CShaderManager::FlushShaders( void )
{
}

static void mat_shadercount( const CCommand &args );
static ConCommand mat_shadercount_command( "mat_shadercount", mat_shadercount, "display count of all shaders and reset that count" );
static void mat_shadercount( const CCommand &args )
{
	DevWarning( "Num Pixel Shaders = %d Vertex Shaders=%d\n", s_NumPixelShadersCreated, s_NumVertexShadersCreated );
	s_NumVertexShadersCreated = 0;
	s_NumPixelShadersCreated = 0;
}

void DestroyAllVertexAndPixelShaders( void )
{
	( ( CShaderManager * )ShaderManager() )->DestroyAllShaders();
}

void CShaderManager::AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics )
{
	s_ShaderComboInfoByName.try_emplace( pSemantics->pShaderName, pSemantics );
}
