#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math;

struct mnode_t;
struct mleaf_t;
struct mtexinfo_t;
typedef struct mtexdata_s mtexdata_t;
struct model_t;

struct dvis_t;
struct dworldlight_t;
struct decallist_t;

void	Map_VisClear( void );
void	Map_VisSetup( model_t *worldmodel, int visorigincount, const Vector origins[], bool forcenovis, unsigned int &returnFlags );
byte 	*Map_VisCurrent( void );
int		Map_VisCurrentCluster( void );
bool	Map_VisForceFullSky();
// reconstruct the ambient lighting for a leaf at the given position in worldspace
void	Mod_LeafAmbientColorAtPos( Vector *pOut, const Vector &pos, int leafIndex );

extern int			DecalListCreate( decallist_t *pList );

extern int		r_visframecount;

#include "modelloader.h"
