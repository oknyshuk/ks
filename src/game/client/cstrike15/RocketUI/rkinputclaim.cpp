// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// The one definition of "the RmlUi layer owns the mouse".
//
// This used to be a reference count inside RocketUI that every panel bumped up
// and down by hand, mirrored by a private "am I grabbing?" bool per panel. Any
// path that hid a panel without walking its release branch left the count above
// zero forever, and UI_ActivateMouse() then re-showed the cursor and dropped SDL
// relative mode every frame - mouselook was dead for the rest of the session.
//
// So nothing is stored: RocketUI polls this predicate, which reads the panels'
// own live state. A panel that forgets to release cannot strand the game, and
// the worst a *missing* entry below can do is leave the cursor hidden while that
// panel is up, which the user can always escape.

#include "cbase.h"

#include <rocketui/rocketui.h>

#include "rkinputclaim.h"

#include "hud_element_helper.h"
#include "rkconsole.h"
#include "rkhud_buymenu.h"
#include "rkhud_chat.h"
#include "rkhud_loadingscreen.h"
#include "rkhud_pausemenu.h"
#include "rkhud_teammenu.h"
#include "rkmenu_main.h"
#include "rkpanel_options.h"
#include "rkpanel_play.h"

static bool RocketUI_UIOwnsInput()
{
    if ( RocketMainMenuDocument::OwnsInput() ||
         RocketPlayDocument::OwnsInput() ||
         RocketOptionsDocument::OwnsInput() ||
         RocketPauseMenuDocument::OwnsInput() ||
         RocketTeamMenuDocument::OwnsInput() ||
         RocketLoadingScreenDocument::OwnsInput() ||
         RkConsole().OwnsInput() )
        return true;

    // HUD elements only exist once the HUD has been created.
    RkHudChat *pChat = GET_HUDELEMENT( RkHudChat );
    if ( pChat && pChat->OwnsInput() )
        return true;

    RkHudBuyMenu *pBuyMenu = GET_HUDELEMENT( RkHudBuyMenu );
    if ( pBuyMenu && pBuyMenu->OwnsInput() )
        return true;

    return false;
}

void RocketUI_InstallInputClaimQuery( bool bInstall )
{
    if ( !RocketUI() )
    {
        if ( bInstall )
            Warning( "RocketUI is missing: UI panels will never get the mouse.\n" );
        return;
    }

    RocketUI()->SetInputClaimQuery( bInstall ? RocketUI_UIOwnsInput : nullptr );
}
