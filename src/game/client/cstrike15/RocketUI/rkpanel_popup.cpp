// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include "rocketui/rmlui.h"
#include "rkpanel_popup.h"

#include "rkhud_model.h"

#include "string_t.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "npcevent.h"
#include "debugoverlay_shared.h"
#include "cdll_client_int.h" // extern globals to interfaces like engineclient

#include <rocketui/rmlui.h>


static void rocket_test_popup( const CCommand &args );
static ConCommand rocket_test_popup_command( "rocket_test_popup", rocket_test_popup, "Create a Test Popup in RocketUI" );
static void rocket_test_popup( const CCommand &args )
{
	if ( args.ArgC() != 3 )
	{
        Msg( "Example usage: rocket_test_popup <messages> <timeout>" );
	}
	else
    {
        float timeout = V_atof(args.ArgV()[2]);
        const char *text = args.ArgV()[1];

        Msg( "Created new Popup...(%f) seconds\n", timeout);
        new RocketPopupDocument( text, timeout );
    }
}

void RocketPopupDocument::ProcessEvent(Rml::Event &event)
{
    if( event.GetId() == Rml::EventId::Animationend )
    {
        // Animation is over, this means it's time to close
        delete this;
    }
}

RocketPopupDocument::RocketPopupDocument(const char *text, float timeout)
{
    V_strncpy( m_textBuffer, text, MAX_POPUP_TEXT );
    m_fTimeout = timeout;

    m_pInstance = RkLoadDocument( ROCKET_CONTEXT_CURRENT, "panel_popup.rml" );
    m_pInstance->AddEventListener(Rml::EventId::Animationend, this );
    m_pInstance->GetElementById("text")->SetInnerRML( m_textBuffer );
    m_pInstance->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
    m_pInstance->Animate("opacity", Rml::Property(0.0f, Rml::Unit::NUMBER), timeout, Rml::Tween(Rml::Tween::Quadratic));
}

RocketPopupDocument::~RocketPopupDocument()
{
    m_pInstance->RemoveEventListener( Rml::EventId::Animationend, this );
    m_pInstance->Close();
    m_pInstance = nullptr;
}
