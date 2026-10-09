// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "gamerules.h"
#include "cs_custom_texture_saver.h"

bool CCSCustomTextureSaver::Init()
{
	m_materials.clear();
	m_bHasJob = false;
	return true;
}

void CCSCustomTextureSaver::AddMaterialToWatch( CustomMaterialGenerationData_t embroiderMaterial )
{
	m_materials.push_back( embroiderMaterial );
	m_bHasJob = true;
}

void CCSCustomTextureSaver::Update( float frameTime )
{
	if ( m_bHasJob == false )
		return;

	int count = std::ssize( m_materials );
	
	if ( count == 0 )
	{
		m_bHasJob = false;
		return;
	}
	
	std::vector< CustomMaterialGenerationData_t > completedMaterials;

	for ( int i = 0; i < count; ++i )
	{
		int completionCount = 0;

		for ( int j = 0; j < m_materials[i].nTex; j++ )
		{
			completionCount += ( m_materials[i].pCustomMaterial->GetTexture( j )->GenerationComplete() );
		}
		if ( completionCount == 3 )
		{
			count--;
			completedMaterials.push_back( m_materials[i] );
			m_materials.erase( m_materials.begin() + i );
			i--;
		}
	}

	for ( int i = 0; i < std::ssize( completedMaterials ); i++ )
	{
		for ( int j = 0; j < completedMaterials[i].nTex; j++ )
		{
			if ( g_pFullFileSystem->FileExists( completedMaterials[i].fileNames[j], "MOD" ) &&  !g_pFullFileSystem->IsFileWritable( completedMaterials[i].fileNames[j], "MOD" ) )
			{
				// Skip read only files
				continue;
			}

			IVTFTexture *pVTFTexture = completedMaterials[i].pCustomMaterial->GetTexture( j )->GetResultVTF();

			ks::byte_writer buf;
			pVTFTexture->Serialize(buf);
					
			FileHandle_t f = g_pFullFileSystem->Open( completedMaterials[i].fileNames[j], "wb", nullptr );

			if ( f != FILESYSTEM_INVALID_HANDLE )
			{
				g_pFullFileSystem->Write( buf.data(), buf.size(), f );
				g_pFullFileSystem->Close(f);
			}
			DevMsg( "Saved VTF %s\n", completedMaterials[i].fileNames[j] );

			char szFullVTFPath[ MAX_PATH ];
			g_pFullFileSystem->RelativePathToFullPath( completedMaterials[i].fileNames[j], "MOD", szFullVTFPath, sizeof( szFullVTFPath ) );
		}
		// TODO: add saved files to perforce
	}
}
