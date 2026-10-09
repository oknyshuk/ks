#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

#include "reflect_annotations.h"
#include "dt_recv.h"
#include "client_class.h"
#include "c_baseentity.h"

class [[= ks::reflect::NetTable{ .name = "DT_FuncBrush" } ]]
      C_FuncBrush : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_FuncBrush, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	void OnDataChanged( DataUpdateType_t type );

private:
};
