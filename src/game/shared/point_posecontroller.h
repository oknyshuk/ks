
#include "reflect_annotations.h"
//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Controls the pose parameters of a model
//
//===========================================================================//


#define MAX_POSE_CONTROLLED_PROPS 4		// Number of entities by the same name that can be controlled


// Type of frequency modulations
enum PoseController_FModType_t
{
	POSECONTROLLER_FMODTYPE_NONE = 0,
	POSECONTROLLER_FMODTYPE_SINE,
	POSECONTROLLER_FMODTYPE_SQUARE,
	POSECONTROLLER_FMODTYPE_TRIANGLE,
	POSECONTROLLER_FMODTYPE_SAWTOOTH,
	POSECONTROLLER_FMODTYPE_NOISE,

	POSECONTROLLER_FMODTYPE_TOTAL,
};


#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// SERVER CLASS
//-----------------------------------------------------------------------------

#include "baseentity.h"


#define MAX_POSE_CYCLE_FREQUENCY 10.0f

#define MAX_POSE_FMOD_AMPLITUDE 10.0f

#define MAX_POSE_FMOD_RATE 10.0f

#define MAX_POSE_INTERPOLATION_TIME 10.0f

class [[= ks::reflect::NetTable{ .name = "DT_PoseController" } ]]
      CPoseController : public CBaseEntity
{
public:
	DECLARE_CLASS( CPoseController, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn( void );

	void Think( void );

	void BuildPropList( void );
	void BuildPoseIndexList( void );
	void SetPoseIndex( int i, int iValue );
	void SetCurrentPose( float fCurrentPoseValue );

	float GetPoseValue( void );

	void SetProp( CBaseAnimating *pProp );
	void SetPropName( const char *pName );
	void SetPoseParameterName( const char *pName );
	void SetPoseValue( float fValue );
	void SetInterpolationTime( float fValue );
	void SetInterpolationWrap( bool bWrap );
	void SetCycleFrequency( float fValue );
	void SetFModType( int nType );
	void SetFModTimeOffset( float fValue );
	void SetFModRate( float fValue );
	void SetFModAmplitude( float fValue );
	void RandomizeFMod( float fExtremeness );

	// Input handlers
	[[= ks::reflect::Input{ .name = "SetPoseParameterName", .type = FIELD_STRING } ]] void InputSetPoseParameterName( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetPoseValue", .type = FIELD_FLOAT } ]] void InputSetPoseValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetInterpolationTime", .type = FIELD_FLOAT } ]] void InputSetInterpolationTime( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetCycleFrequency", .type = FIELD_FLOAT } ]] void InputSetCycleFrequency( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFModType", .type = FIELD_INTEGER } ]] void InputSetFModType( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFModTimeOffset", .type = FIELD_FLOAT } ]] void InputSetFModTimeOffset( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFModRate", .type = FIELD_FLOAT } ]] void InputSetFModRate( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFModAmplitude", .type = FIELD_FLOAT } ]] void InputSetFModAmplitude( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "RandomizeFMod", .type = FIELD_FLOAT } ]] void InputRandomizeFMod( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "GetFMod", .type = FIELD_VOID } ]] void InputGetFMod( inputdata_t &inputdata );

private:

	CNetworkArray( EHANDLE, m_hProps, MAX_POSE_CONTROLLED_PROPS, [[= ks::reflect::Net{} ]] );				// Handles to controlled models
	CNetworkArray( unsigned char, m_chPoseIndex, MAX_POSE_CONTROLLED_PROPS, [[= ks::reflect::Net{ .bits = 5, .flags = SPROP_UNSIGNED } ]] );	// Pose parameter indices for each model

	bool		m_bDisablePropLookup;

	CNetworkVar( bool, m_bPoseValueParity, [[= ks::reflect::Net{} ]] );

	[[= ks::reflect::Key{ .name = "PropName" } ]] string_t	m_iszPropName;				// Targetname of the models to control
	[[= ks::reflect::Key{ .name = "PoseParameterName" } ]] string_t	m_iszPoseParameterName;		// Pose parameter name to control

	CNetworkVar( float, m_fPoseValue, [[= ks::reflect::Net{ .bits = 11, .low = 0.0f, .high = 1.0f } ]] [[= ks::reflect::Key{ .name = "PoseValue" } ]] );			// Normalized pose parameter value (maps to each pose parameter's min and max range)
	CNetworkVar( float, m_fInterpolationTime, [[= ks::reflect::Net{ .bits = 11, .low = 0.0f, .high = MAX_POSE_INTERPOLATION_TIME } ]] [[= ks::reflect::Key{ .name = "InterpolationTime" } ]] );	// Interpolation speed for client matching absolute pose values
	CNetworkVar( bool, m_bInterpolationWrap, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "InterpolationWrap" } ]] );	// Interpolation for the client wraps 0 to 1.

	CNetworkVar( float, m_fCycleFrequency, [[= ks::reflect::Net{ .bits = 11, .low = -MAX_POSE_CYCLE_FREQUENCY, .high = MAX_POSE_CYCLE_FREQUENCY } ]] [[= ks::reflect::Key{ .name = "CycleFrequency" } ]] );	// Cycles per second

	// Frequency modulation variables
	CNetworkVar( PoseController_FModType_t, m_nFModType, [[= ks::reflect::Net{ .bits = 3, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "FModType" } ]] );
	CNetworkVar( float, m_fFModTimeOffset, [[= ks::reflect::Net{ .bits = 11, .low = -1.0f, .high = 1.0f } ]] [[= ks::reflect::Key{ .name = "FModTimeOffset" } ]] );
	CNetworkVar( float, m_fFModRate, [[= ks::reflect::Net{ .bits = 11, .low = -MAX_POSE_FMOD_RATE, .high = MAX_POSE_FMOD_RATE } ]] [[= ks::reflect::Key{ .name = "FModRate" } ]] );
	CNetworkVar( float, m_fFModAmplitude, [[= ks::reflect::Net{ .bits = 11, .low = 0.0f, .high = MAX_POSE_FMOD_AMPLITUDE } ]] [[= ks::reflect::Key{ .name = "FModAmplitude" } ]] );
};


#else //#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// CLIENT CLASS
//-----------------------------------------------------------------------------

#include "c_baseentity.h"
#include "fx_interpvalue.h"


class [[= ks::reflect::NetTable{ .name = "DT_PoseController" } ]]
      C_PoseController : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_PoseController, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	virtual void	Spawn( void );
	virtual void	OnDataChanged( DataUpdateType_t updateType );

	virtual void	ClientThink( void );

private:

	void	UpdateModulation( void );
	void	UpdatePoseCycle( float fCycleAmount );
	void	SetCurrentPose( float fCurrentPoseValue );


	// Networked variables
	[[= ks::reflect::Net{} ]] EHANDLE						m_hProps[MAX_POSE_CONTROLLED_PROPS];
	[[= ks::reflect::Net{} ]] unsigned char				m_chPoseIndex[MAX_POSE_CONTROLLED_PROPS];
	[[= ks::reflect::Net{} ]] bool						m_bPoseValueParity;
	[[= ks::reflect::Net{} ]] float						m_fPoseValue;
	[[= ks::reflect::Net{} ]] float						m_fInterpolationTime;
	[[= ks::reflect::Net{} ]] bool						m_bInterpolationWrap;
	[[= ks::reflect::Net{} ]] float						m_fCycleFrequency;
	[[= ks::reflect::Net{} ]] PoseController_FModType_t	m_nFModType;
	[[= ks::reflect::Net{} ]] float						m_fFModTimeOffset;
	[[= ks::reflect::Net{} ]] float						m_fFModRate;
	[[= ks::reflect::Net{} ]] float						m_fFModAmplitude;
	bool	m_bOldPoseValueParity;

	float	m_fCurrentPoseValue;	// Actual pose value cycled by the frequency and modulation
	float	m_fCurrentFMod;			// The current fequency modulation amount (stored for noise walk)

	CInterpolatedValue	m_PoseTransitionValue;
};


#endif //#ifndef CLIENT_DLL
