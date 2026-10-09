// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include "rocketui/rmlui.h"
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
#include "rkmenu_main.h"

#include "rkpanel.h"

#include "rkhud_model.h"

#include <rocketui/rmlui.h>
import ks.tier1;
import std;

#include "rkpanel_options.h"
#include "rkpanel_play.h"

Rml::ElementDocument *RocketMainMenuDocument::m_pInstance = nullptr;
bool RocketMainMenuDocument::showing = false;

// menu.rml names these on the buttons (data-event-click).
static void BindMainMenu( Rml::DataModelConstructor &c )
{
    c.BindEventCallback( "show_options", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & ) {
        RocketOptionsDocument::ShowPanel( true );
    } );
    c.BindEventCallback( "show_play", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & ) {
        RocketPlayDocument::ShowPanel( true );
    } );
}
RK_HUD_SECTION( BindMainMenu );

void RocketMainMenuDocument::LoadDialog()
{
    RkPanelLoad( m_pInstance, ROCKET_CONTEXT_MENU, "menu.rml", LoadDialog, UnloadDialog );
}

void RocketMainMenuDocument::UnloadDialog()
{
    RkPanelUnload( m_pInstance, showing );
}

void RocketMainMenuDocument::RestorePanel()
{
    // Re-show the main menu (ShowPanel loads it first if it isn't up yet).
    ShowPanel( true );
}

// Reached before the first frame in some paths, so it loads for the caller.
void RocketMainMenuDocument::ShowPanel(bool bShow, bool immediate)
{
    if( bShow )
        LoadDialog();

    RkPanelShow( m_pInstance, showing, bShow );
}
