#include "rkpanel_play.h"

#include "cbase.h"
#include "cdll_client_int.h"
#include "filesystem.h"
#include "tier1/convar.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>

Rml::ElementDocument *RocketPlayDocument::m_pInstance = nullptr;
bool RocketPlayDocument::m_bVisible = false;
bool RocketPlayDocument::m_bGrabbingInput = false;

class RkPlayClickListener : public Rml::EventListener
{
public:
    void ProcessEvent( Rml::Event &event ) override
    {
        Rml::Element *current = event.GetCurrentElement();
        if( !current )
            return;

        const Rml::String &id = current->GetId();

        if( id == "start" )
        {
            RocketPlayDocument::StartServer();
        }
        else if( id == "close" )
        {
            RocketPlayDocument::ShowPanel( false );
            RocketPlayDocument::UnloadDialog();
        }
    }
};

static RkPlayClickListener playClickListener;

RocketPlayDocument::RocketPlayDocument()
{
}

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
    FileFindHandle_t findHandle = NULL;
    const char *pszFilename = g_pFullFileSystem->FindFirst( "maps/*.bsp", &findHandle );

    Rml::String optionsRml;
    int count = 0;

    while( pszFilename )
    {
        char mapname[256];
        Q_strncpy( mapname, pszFilename, sizeof(mapname) );

        // Strip .bsp extension
        char *ext = Q_strstr( mapname, ".bsp" );
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
    Q_snprintf( szMapCommand, sizeof(szMapCommand),
        "disconnect\nwait\nwait\nsv_lan 1\nsetmaster enable\nmaxplayers %i\nsv_password \"%s\"\nhostname \"%s\"\nprogress_enable\nmap %s\n",
        maxPlayers,
        password.c_str(),
        hostName.c_str(),
        mapName.c_str()
    );

    // Close the panel before starting the server
    ShowPanel( false );
    UnloadDialog();

    engine->ClientCmd_Unrestricted( szMapCommand );
}

static void AttachClickListener( Rml::ElementDocument *doc, const char *elementId )
{
    Rml::Element *elem = doc->GetElementById( elementId );
    if( elem )
        elem->AddEventListener( Rml::EventId::Click, &playClickListener );
}

static void DetachClickListener( Rml::ElementDocument *doc, const char *elementId )
{
    Rml::Element *elem = doc->GetElementById( elementId );
    if( elem )
        elem->RemoveEventListener( Rml::EventId::Click, &playClickListener );
}

void RocketPlayDocument::LoadDialog()
{
    if( !m_pInstance )
    {
        m_pInstance = RocketUI()->LoadDocumentFile( ROCKET_CONTEXT_CURRENT, "panel_play.rml", RocketPlayDocument::LoadDialog, RocketPlayDocument::UnloadDialog );
        if( !m_pInstance )
        {
            Error( "Couldn't create rocketui play panel!\n" );
            /* Exit */
        }

        AttachClickListener( m_pInstance, "start" );
        AttachClickListener( m_pInstance, "close" );

        PopulateMapList();
    }
}

void RocketPlayDocument::UnloadDialog()
{
    if( m_pInstance )
    {
        DetachClickListener( m_pInstance, "start" );
        DetachClickListener( m_pInstance, "close" );

        m_pInstance->Close();
        m_pInstance = nullptr;

        if( m_bGrabbingInput )
        {
            RocketUI()->DenyInputToGame( false, "PlayPanel" );
            m_bGrabbingInput = false;
        }
    }

    m_bVisible = false;
}

void RocketPlayDocument::ShowPanel( bool bShow, bool immediate )
{
    if( bShow )
    {
        if( !m_pInstance )
            LoadDialog();

        // Refresh map list each time panel is shown
        PopulateMapList();

        m_pInstance->Show();

        if( !m_bGrabbingInput )
        {
            RocketUI()->DenyInputToGame( true, "PlayPanel" );
            m_bGrabbingInput = true;
        }
    }
    else
    {
        if( m_pInstance )
            m_pInstance->Hide();

        if( m_bGrabbingInput )
        {
            RocketUI()->DenyInputToGame( false, "PlayPanel" );
            m_bGrabbingInput = false;
        }
    }

    m_bVisible = bShow;
}
