#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: When embroidered patches are generated from sticker kits, we must wait
// for the textures to be generated before saving to disk.

import ks.tier1;
#include "igamesystem.h"
#include "materialsystem/icustommaterialmanager.h"
import std;
#include "datacache/imdlcache.h"

struct CustomMaterialGenerationData_t
{
	ICustomMaterial *pCustomMaterial;
	char fileNames[COMBINER_MAX_TEXTURES_PER_MATERIAL][MAX_PATH];
	const char *pchChangeListName;
	int nTex;
};

class CCSCustomTextureSaver : public CAutoGameSystemPerFrame
{
public:
	CCSCustomTextureSaver() { m_bHasJob = false;  }
	~CCSCustomTextureSaver() {}

	virtual bool Init();
	virtual void Shutdown() {};
	void AddMaterialToWatch( CustomMaterialGenerationData_t embroiderMaterial );

private:

	virtual void Update( float frametime );

	std::vector< CustomMaterialGenerationData_t > m_materials;

	bool m_bHasJob;

};
