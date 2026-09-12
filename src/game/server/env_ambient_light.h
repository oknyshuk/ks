#ifndef _INCLUDED_ENV_AMBIENT_LIGHT_H
#define _INCLUDED_ENV_AMBIENT_LIGHT_H

#include "reflect_annotations.h"

#include "spatialentity.h"

//------------------------------------------------------------------------------
// Purpose : Ambient light controller entity
//------------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_EnvAmbientLight" } ]]
      CEnvAmbientLight : public CSpatialEntity
{
	DECLARE_CLASS( CEnvAmbientLight, CSpatialEntity );

public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn( void );

	[[= ks::reflect::Input{ .name = "SetColor", .type = FIELD_COLOR32 } ]] void InputSetColor(inputdata_t &inputdata);
	void SetColor( const Vector &vecColor );

private:
	[[= ks::reflect::Key{ .name = "Color" } ]] color32	m_Color;

	CNetworkVector( m_vecColor, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );
};

#endif // _INCLUDED_ENV_AMBIENT_LIGHT_H
