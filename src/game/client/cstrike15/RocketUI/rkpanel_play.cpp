// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include "rocketui/rmlui.h"
#include "rkpanel_play.h"

#include "rkpanel.h"

#include "rkhud_model.h"

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
#include "cdll_client_int.h"
#include "filesystem.h"

#include <rocketui/rmlui.h>
import ks.tier1;
import std;
#include "rkhud_pausemenu.h"
#include "rkmenu_main.h"

Rml::ElementDocument *RocketPlayDocument::m_pInstance = nullptr;
bool RocketPlayDocument::m_bVisible = false;

// panel_play.rml names these on its buttons (data-event-click).
static void BindPlayPanel( Rml::DataModelConstructor &c )
{
    c.BindEventCallback( "start_server", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & ) {
        RocketPlayDocument::StartServer();
    } );
    c.BindEventCallback( "close_play", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & ) {
        RocketPlayDocument::ShowPanel( false );
    } );
}
RK_HUD_SECTION( BindPlayPanel );

void RocketPlayDocument::PopulateMapList()
{
    if( !m_pInstance )
        return;

    Rml::Element *elem = m_pInstance->GetElementById( "maplist" );
    if( !elem )
        return;

    auto *sel = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( elem );
    if( !sel )
        return;

    sel->RemoveAll();

    // Scan for .bsp files in the maps/ directory, same as VGUI's
    // CCreateMultiplayerGameServerPage::LoadMaps
    FileFindHandle_t findHandle = 0;
    const char *pszFilename = g_pFullFileSystem->FindFirst( "maps/*.bsp", &findHandle );

    Rml::String optionsRml;
    int count = 0;

    while( pszFilename )
    {
        char mapname[256];
        V_strncpy( mapname, pszFilename, sizeof(mapname) );

        // Strip .bsp extension
        char *ext = V_strstr( mapname, ".bsp" );
        if( ext )
            *ext = 0;

        char opt[256];
        V_snprintf( opt, sizeof(opt), "<option value=\"%s\">%s</option>", mapname, mapname );
        optionsRml += opt;
        count++;

        pszFilename = g_pFullFileSystem->FindNext( findHandle );
    }
    g_pFullFileSystem->FindClose( findHandle );

    if( count > 0 )
    {
        elem->SetInnerRML( optionsRml );
        sel->SetSelection( 0 );
    }
}

void RocketPlayDocument::StartServer()
{
    if( !m_pInstance )
        return;

    // Get map selection
    Rml::Element *mapElem = m_pInstance->GetElementById( "maplist" );
    if( !mapElem )
        return;

    auto *mapSel = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( mapElem );
    if( !mapSel )
        return;

    Rml::String mapName = mapSel->GetValue();
    if( mapName.empty() )
        return;

    // Get max players
    Rml::Element *playersElem = m_pInstance->GetElementById( "maxplayers" );
    int maxPlayers = 16;
    if( playersElem )
    {
        auto *playersSel = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( playersElem );
        if( playersSel )
            maxPlayers = atoi( playersSel->GetValue().c_str() );
    }

    // Get hostname
    Rml::Element *hostnameElem = m_pInstance->GetElementById( "hostname" );
    Rml::String hostName = "Counter-Strike: Global Offensive";
    if( hostnameElem )
    {
        auto *hostnameInput = rmlui_dynamic_cast<Rml::ElementFormControlInput*>( hostnameElem );
        if( hostnameInput )
        {
            Rml::String val = hostnameInput->GetValue();
            if( !val.empty() )
                hostName = val;
        }
    }

    // Get password
    Rml::Element *passwordElem = m_pInstance->GetElementById( "sv_password" );
    Rml::String password;
    if( passwordElem )
    {
        auto *passwordInput = rmlui_dynamic_cast<Rml::ElementFormControlInput*>( passwordElem );
        if( passwordInput )
            password = passwordInput->GetValue();
    }

    // Build the command string, matching VGUI's CCreateMultiplayerGameDialog::OnOK
    char szMapCommand[1024];
    V_snprintf( szMapCommand, sizeof(szMapCommand),
        "disconnect\nwait\nwait\nsv_lan 1\nsetmaster enable\nmaxplayers %i\nsv_password \"%s\"\nhostname \"%s\"\nprogress_enable\nmap %s\n",
        maxPlayers,
        password.c_str(),
        hostName.c_str(),
        mapName.c_str()
    );

    // Hide; this runs inside the panel's own data-event callback, so it must not
    // close (see rkpanel.h).
    ShowPanel( false );

    engine->ClientCmd_Unrestricted( szMapCommand );
}

void RocketPlayDocument::LoadDialog()
{
    // Transient panel: rebuild here so it lands in the context current now.
    UnloadDialog();

    if( !RkPanelLoad( m_pInstance, ROCKET_CONTEXT_CURRENT, "panel_play.rml", LoadDialog, UnloadDialog ) )
        return;

    PopulateMapList();
}

void RocketPlayDocument::UnloadDialog()
{
    RkPanelUnload( m_pInstance, m_bVisible );
}

void RocketPlayDocument::ShowPanel( bool bShow, bool immediate )
{
    if( bShow )
        LoadDialog();   // rebuilds the panel, re-scanning the maps folder

    RkPanelShow( m_pInstance, m_bVisible, bShow );
}
