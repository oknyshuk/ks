//========= Copyright (c) 1996-2010, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CFuncInstanceIoProxy : public CBaseEntity
{
public:
	DECLARE_CLASS( CFuncInstanceIoProxy, CBaseEntity );

	// Input handlers
	[[= ks::reflect::Input{ .name = "OnProxyRelay1", .type = FIELD_STRING } ]] void InputProxyRelay1( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay2", .type = FIELD_STRING } ]] void InputProxyRelay2( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay3", .type = FIELD_STRING } ]] void InputProxyRelay3( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay4", .type = FIELD_STRING } ]] void InputProxyRelay4( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay5", .type = FIELD_STRING } ]] void InputProxyRelay5( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay6", .type = FIELD_STRING } ]] void InputProxyRelay6( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay7", .type = FIELD_STRING } ]] void InputProxyRelay7( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay8", .type = FIELD_STRING } ]] void InputProxyRelay8( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay9", .type = FIELD_STRING } ]] void InputProxyRelay9( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay10", .type = FIELD_STRING } ]] void InputProxyRelay10( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay11", .type = FIELD_STRING } ]] void InputProxyRelay11( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay12", .type = FIELD_STRING } ]] void InputProxyRelay12( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay13", .type = FIELD_STRING } ]] void InputProxyRelay13( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay14", .type = FIELD_STRING } ]] void InputProxyRelay14( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay15", .type = FIELD_STRING } ]] void InputProxyRelay15( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay16", .type = FIELD_STRING } ]] void InputProxyRelay16( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay17", .type = FIELD_STRING } ]] void InputProxyRelay17( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay18", .type = FIELD_STRING } ]] void InputProxyRelay18( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay19", .type = FIELD_STRING } ]] void InputProxyRelay19( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay20", .type = FIELD_STRING } ]] void InputProxyRelay20( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay21", .type = FIELD_STRING } ]] void InputProxyRelay21( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay22", .type = FIELD_STRING } ]] void InputProxyRelay22( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay23", .type = FIELD_STRING } ]] void InputProxyRelay23( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay24", .type = FIELD_STRING } ]] void InputProxyRelay24( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay25", .type = FIELD_STRING } ]] void InputProxyRelay25( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay26", .type = FIELD_STRING } ]] void InputProxyRelay26( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay27", .type = FIELD_STRING } ]] void InputProxyRelay27( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay28", .type = FIELD_STRING } ]] void InputProxyRelay28( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay29", .type = FIELD_STRING } ]] void InputProxyRelay29( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay30", .type = FIELD_STRING } ]] void InputProxyRelay30( inputdata_t &inputdata );
;

	DECLARE_DATADESC();

private:

	[[= ks::reflect::Key{ .name = "OnProxyRelay1" } ]] COutputEvent m_OnProxyRelay1;
	[[= ks::reflect::Key{ .name = "OnProxyRelay2" } ]] COutputEvent m_OnProxyRelay2;
	[[= ks::reflect::Key{ .name = "OnProxyRelay3" } ]] COutputEvent m_OnProxyRelay3;
	[[= ks::reflect::Key{ .name = "OnProxyRelay4" } ]] COutputEvent m_OnProxyRelay4;
	[[= ks::reflect::Key{ .name = "OnProxyRelay5" } ]] COutputEvent m_OnProxyRelay5;
	[[= ks::reflect::Key{ .name = "OnProxyRelay6" } ]] COutputEvent m_OnProxyRelay6;
	[[= ks::reflect::Key{ .name = "OnProxyRelay7" } ]] COutputEvent m_OnProxyRelay7;
	[[= ks::reflect::Key{ .name = "OnProxyRelay8" } ]] COutputEvent m_OnProxyRelay8;
	[[= ks::reflect::Key{ .name = "OnProxyRelay9" } ]] COutputEvent m_OnProxyRelay9;
	[[= ks::reflect::Key{ .name = "OnProxyRelay10" } ]] COutputEvent m_OnProxyRelay10;
	[[= ks::reflect::Key{ .name = "OnProxyRelay11" } ]] COutputEvent m_OnProxyRelay11;
	[[= ks::reflect::Key{ .name = "OnProxyRelay12" } ]] COutputEvent m_OnProxyRelay12;
	[[= ks::reflect::Key{ .name = "OnProxyRelay13" } ]] COutputEvent m_OnProxyRelay13;
	[[= ks::reflect::Key{ .name = "OnProxyRelay14" } ]] COutputEvent m_OnProxyRelay14;
	[[= ks::reflect::Key{ .name = "OnProxyRelay15" } ]] COutputEvent m_OnProxyRelay15;
	[[= ks::reflect::Key{ .name = "OnProxyRelay16" } ]] COutputEvent m_OnProxyRelay16;
	[[= ks::reflect::Key{ .name = "OnProxyRelay17" } ]] COutputEvent m_OnProxyRelay17;
	[[= ks::reflect::Key{ .name = "OnProxyRelay18" } ]] COutputEvent m_OnProxyRelay18;
	[[= ks::reflect::Key{ .name = "OnProxyRelay19" } ]] COutputEvent m_OnProxyRelay19;
	[[= ks::reflect::Key{ .name = "OnProxyRelay20" } ]] COutputEvent m_OnProxyRelay20;
	[[= ks::reflect::Key{ .name = "OnProxyRelay21" } ]] COutputEvent m_OnProxyRelay21;
	[[= ks::reflect::Key{ .name = "OnProxyRelay22" } ]] COutputEvent m_OnProxyRelay22;
	[[= ks::reflect::Key{ .name = "OnProxyRelay23" } ]] COutputEvent m_OnProxyRelay23;
	[[= ks::reflect::Key{ .name = "OnProxyRelay24" } ]] COutputEvent m_OnProxyRelay24;
	[[= ks::reflect::Key{ .name = "OnProxyRelay25" } ]] COutputEvent m_OnProxyRelay25;
	[[= ks::reflect::Key{ .name = "OnProxyRelay26" } ]] COutputEvent m_OnProxyRelay26;
	[[= ks::reflect::Key{ .name = "OnProxyRelay27" } ]] COutputEvent m_OnProxyRelay27;
	[[= ks::reflect::Key{ .name = "OnProxyRelay28" } ]] COutputEvent m_OnProxyRelay28;
	[[= ks::reflect::Key{ .name = "OnProxyRelay29" } ]] COutputEvent m_OnProxyRelay29;
	[[= ks::reflect::Key{ .name = "OnProxyRelay30" } ]] COutputEvent m_OnProxyRelay30;

};

LINK_ENTITY_TO_CLASS( func_instance_io_proxy, CFuncInstanceIoProxy );

IMPLEMENT_REFLECT_DATAMAP( CFuncInstanceIoProxy )

//------------------------------------------------------------------------------
// Purpose : Route the incomming to the outgoing proxy messages. 
//------------------------------------------------------------------------------
void CFuncInstanceIoProxy::InputProxyRelay1( inputdata_t &inputdata )
{
	m_OnProxyRelay1.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay2( inputdata_t &inputdata )
{
	m_OnProxyRelay2.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay3( inputdata_t &inputdata )
{
	m_OnProxyRelay3.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay4( inputdata_t &inputdata )
{
	m_OnProxyRelay4.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay5( inputdata_t &inputdata )
{
	m_OnProxyRelay5.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay6( inputdata_t &inputdata )
{
	m_OnProxyRelay6.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay7( inputdata_t &inputdata )
{
	m_OnProxyRelay7.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay8( inputdata_t &inputdata )
{
	m_OnProxyRelay8.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay9( inputdata_t &inputdata )
{
	m_OnProxyRelay9.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay10( inputdata_t &inputdata )
{
	m_OnProxyRelay10.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay11( inputdata_t &inputdata )
{
	m_OnProxyRelay11.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay12( inputdata_t &inputdata )
{
	m_OnProxyRelay12.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay13( inputdata_t &inputdata )
{
	m_OnProxyRelay13.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay14( inputdata_t &inputdata )
{
	m_OnProxyRelay14.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay15( inputdata_t &inputdata )
{
	m_OnProxyRelay15.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay16( inputdata_t &inputdata )
{
	m_OnProxyRelay16.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay17( inputdata_t &inputdata )
{
	m_OnProxyRelay17.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay18( inputdata_t &inputdata )
{
	m_OnProxyRelay18.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay19( inputdata_t &inputdata )
{
	m_OnProxyRelay19.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay20( inputdata_t &inputdata )
{
	m_OnProxyRelay20.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay21( inputdata_t &inputdata )
{
	m_OnProxyRelay21.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay22( inputdata_t &inputdata )
{
	m_OnProxyRelay22.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay23( inputdata_t &inputdata )
{
	m_OnProxyRelay23.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay24( inputdata_t &inputdata )
{
	m_OnProxyRelay24.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay25( inputdata_t &inputdata )
{
	m_OnProxyRelay25.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay26( inputdata_t &inputdata )
{
	m_OnProxyRelay26.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay27( inputdata_t &inputdata )
{
	m_OnProxyRelay27.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay28( inputdata_t &inputdata )
{
	m_OnProxyRelay28.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay29( inputdata_t &inputdata )
{
	m_OnProxyRelay29.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay30( inputdata_t &inputdata )
{
	m_OnProxyRelay30.FireOutput( inputdata.pActivator, inputdata.pCaller );
	DevWarning( "Maximun Proxy Messages used - ask a programmer for more.\n" );
}

