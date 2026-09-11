
#if !defined ( ENV_DOF_CONTROLLER_H )
#define ENV_DOF_CONTROLLER_H

#include "reflect_annotations.h"

struct DOFControlSettings_t
{
	// Near plane
	float	flNearBlurDepth;
	float	flNearBlurRadius;
	float	flNearFocusDistance;
	// Far plane
	float	flFarBlurDepth;
	float	flFarBlurRadius;
	float	flFarFocusDistance;
};

//-----------------------------------------------------------------------------
// Purpose: Entity that controls depth of field postprocessing
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_EnvDOFController" } ]]
      CEnvDOFController : public CPointEntity
{
	DECLARE_CLASS( CEnvDOFController, CPointEntity );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual void	Spawn( void );
	virtual void	Activate( void );
	virtual int		UpdateTransmitState( void );
	void			SetControllerState( DOFControlSettings_t setting );

	void	UpdateParamBlend( void );

	// Inputs
	[[= ks::reflect::Input{ .name = "SetNearBlurDepth", .type = FIELD_FLOAT } ]] void	InputSetNearBlurDepth( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetNearFocusDepth", .type = FIELD_FLOAT } ]] void	InputSetNearFocusDepth( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFarFocusDepth", .type = FIELD_FLOAT } ]] void	InputSetFarFocusDepth( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFarBlurDepth", .type = FIELD_FLOAT } ]] void	InputSetFarBlurDepth( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetNearBlurRadius", .type = FIELD_FLOAT } ]] void	InputSetNearBlurRadius( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFarBlurRadius", .type = FIELD_FLOAT } ]] void	InputSetFarBlurRadius( inputdata_t &inputdata );
	void	InputBlendDOFScale( inputdata_t &inputdata );
	
	[[= ks::reflect::Input{ .name = "SetFocusTarget", .type = FIELD_STRING } ]] void	InputSetFocusTarget( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFocusTargetRange", .type = FIELD_STRING } ]] void	InputSetFocusTargetRange( inputdata_t &inputdata );

private:
	[[= ks::reflect::Key{ .name = "focus_range" } ]] float	m_flFocusTargetRange;

	[[= ks::reflect::Key{ .name = "focus_target" } ]] string_t	m_strFocusTargetName;	// Name of the entity to focus on
	EHANDLE		m_hFocusTarget;

	CNetworkVar( bool, m_bDOFEnabled, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "enabled" } ]] );
	CNetworkVar( float, m_flNearBlurDepth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "near_blur" } ]] );
	CNetworkVar( float, m_flNearFocusDepth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "near_focus" } ]] );
	CNetworkVar( float, m_flFarFocusDepth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "far_focus" } ]] );
	CNetworkVar( float, m_flFarBlurDepth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "far_blur" } ]] );
	CNetworkVar( float, m_flNearBlurRadius, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "near_radius" } ]] );
	CNetworkVar( float, m_flFarBlurRadius, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "far_radius" } ]] );
};

#endif//  ENV_DOF_CONTROLLER_H
