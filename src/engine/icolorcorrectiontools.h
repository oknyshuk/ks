#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2003 Valve Corporation

class IColorOperation;

class IColorCorrectionTools
{
public:
	virtual void		Init( void ) = 0;
	virtual void		Shutdown( void ) = 0;

	virtual bool		ShouldPause() const = 0;

	virtual void		GrabPreColorCorrectedFrame( int x, int y, int width, int height ) =  0;

	virtual void		UpdateColorCorrection( ) =  0;

	virtual void		SetFinalOperation( IColorOperation *pOp ) = 0;
};

extern IColorCorrectionTools *colorcorrectiontools;
