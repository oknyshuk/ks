#ifndef _INCLUDED_C_ENV_AMBIENT_LIGHT_H
#define _INCLUDED_C_ENV_AMBIENT_LIGHT_H

#include "reflect_annotations.h"

#include "c_spatialentity.h"

//------------------------------------------------------------------------------
// Purpose : Ambient light controller entity with radial falloff
//------------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_EnvAmbientLight" } ]]
      [[= ks::reflect::From<"m_Value", ks::reflect::Net{ .wire = "m_vecColor" }>{} ]]
      C_EnvAmbientLight : public C_SpatialEntityTemplate<Vector>
{
public:
	DECLARE_CLASS( C_EnvAmbientLight, C_SpatialEntityTemplate<Vector> );
	DECLARE_CLIENTCLASS();

	virtual void ApplyAccumulation( void );
	virtual void ClientThink( void );

	void SetColor( const Vector &vecColor, float flLerpTime = 0 );
	Vector GetTargetColor() { return m_vecTargetColor * 255.0f; }

protected:
	virtual void AddToPersonalSpatialEntityMgr( void );
	virtual void RemoveFromPersonalSpatialEntityMgr( void );

	Vector m_vecStartColor;
	Vector m_vecTargetColor;
	CountdownTimer m_colorTimer;
};

#endif // _INCLUDED_C_ENV_AMBIENT_LIGHT_H
