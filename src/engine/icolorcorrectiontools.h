#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2003 Valve Corporation

class IColorOperation;

class IColorCorrectionTools
{
public:
	void		Init( void );
	void		Shutdown( void );

	bool		ShouldPause() const;

	void		GrabPreColorCorrectedFrame( int x, int y, int width, int height );

	void		UpdateColorCorrection( );

	void		SetFinalOperation( IColorOperation *pOp );
};

extern IColorCorrectionTools *colorcorrectiontools;
