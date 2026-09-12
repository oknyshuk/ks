#ifndef ENV_CASCADE_LIGHT_H
#define ENV_CASCADE_LIGHT_H

#include "reflect_annotations.h"


//------------------------------------------------------------------------------
// Purpose : Sunlight shadow control entity
//------------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_CascadeLight", .base = false } ]]
      CCascadeLight : public CBaseEntity
{
public:
	DECLARE_CLASS( CCascadeLight, CBaseEntity );

	CCascadeLight();
	virtual ~CCascadeLight();

	void Spawn( void );
	void Release( void );
	void OnActivate();
	void OnDeactivate();

	bool KeyValue( const char *szKeyName, const char *szValue );
	virtual bool GetKeyValue( const char *szKeyName, char *szValue, int iMaxLen );
	int  UpdateTransmitState();

	inline const Vector &GetShadowDirection() const { return m_shadowDirection; }
	inline const Vector &GetEnvLightShadowDirection() const { return m_envLightShadowDirection; }
	// Inputs
	[[= ks::reflect::Input{ .name = "SetAngles", .type = FIELD_STRING } ]] void	InputSetAngles( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "LightColor", .type = FIELD_COLOR32 } ]] void	InputSetLightColor( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "LightColorScale", .type = FIELD_INTEGER } ]] void	InputSetLightColorScale( inputdata_t &inputdata );

	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	static void SetEnvLightShadowPitch( float flPitch );
	static void SetEnvLightShadowAngles( const QAngle &angles );
	static void SetLightColor( int r, int g, int b, int a );
	void SetEnabled( bool bEnable );

private:
	CNetworkVector( m_shadowDirection, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_envLightShadowDirection, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );

	CNetworkVar( bool, m_bEnabled, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "enabled" } ]] );
	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool m_bStartDisabled;
	CNetworkVar( bool, m_bUseLightEnvAngles, [[= ks::reflect::Net{} ]] );

	CNetworkColor32( m_LightColor, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WireSide::Send>{} ]] );
	CNetworkVar( int, m_LightColorScale, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Proxy<SendProxy_Int32ToInt32, ks::reflect::WireSide::Send>{} ]] );
	CNetworkVar( float, m_flMaxShadowDist, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );

	void UpdateEnvLight();

	static float m_flEnvLightShadowPitch;
	static QAngle m_EnvLightShadowAngles;
	static bool m_bEnvLightShadowValid;
	static color32 m_EnvLightColor;
	static int m_EnvLightColorScale;
};

extern CCascadeLight *g_pCascadeLight;

#endif // #ifndef ENV_CASCADE_LIGHT_H
