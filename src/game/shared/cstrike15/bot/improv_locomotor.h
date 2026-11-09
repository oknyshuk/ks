#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "nav.h"

class CNavArea;
class CNavLadder;

// improv_locomotor.h
// Interface for moving Improvs along computed paths
// Author: Michael Booth, July 2004

// TODO: Remove duplicate methods from CImprov, and update CImprov to use this class

/**
 * A locomotor owns the movement of an Improv.
 * Each method forwards to the concrete class (CHostage or CChicken) according to the Kind
 * (see hostage/cs_simple_hostage.cpp).
 */
class CImprovLocomotor
{
public:
	enum class Kind
	{
		Hostage,	///< CHostage
		Chicken,	///< CChicken
	};

	enum MoveToFailureType
	{
		FAIL_INVALID_PATH,
		FAIL_STUCK,
		FAIL_FELL_OFF,
	};

	const Vector & GetCentroid( void ) const;		
	const Vector & GetFeet( void ) const;		///< return position of "feet" - point below centroid of improv at feet level
	const Vector & GetEyes( void ) const;		
	float GetMoveAngle( void ) const;		///< return direction of movement
	CNavArea * GetLastKnownArea( void ) const;		
	bool GetSimpleGroundHeightWithFloor( const Vector &pos, float *height, Vector *normal = nullptr );		///< find "simple" ground height, treating current nav area as part of the floor
	void Crouch( void );		
	void StandUp( void );		///< "un-crouch"
	bool IsCrouching( void ) const;		
	void Jump( void );		///< initiate a jump
	bool IsJumping( void ) const;		
	void Run( void );		///< set movement speed to running
	void Walk( void );		///< set movement speed to walking
	bool IsRunning( void ) const;		
	void StartLadder( const CNavLadder *ladder, NavTraverseType how, const Vector &approachPos, const Vector &departPos );		///< invoked when a ladder is encountered while following a path
	bool TraverseLadder( const CNavLadder *ladder, NavTraverseType how, const Vector &approachPos, const Vector &departPos, float deltaT );		///< traverse given ladder
	bool IsUsingLadder( void ) const;		
	void TrackPath( const Vector &pathGoal, float deltaT );		///< move along path by following "pathGoal"
	void OnMoveToSuccess( const Vector &goal );		///< invoked when an improv reaches its MoveTo goal
	void OnMoveToFailure( const Vector &goal, MoveToFailureType reason );		///< invoked when an improv fails to reach a MoveTo goal

protected:
	explicit CImprovLocomotor( Kind kind ) : m_improvKind( kind ) { }

private:
	Kind m_improvKind;
};
