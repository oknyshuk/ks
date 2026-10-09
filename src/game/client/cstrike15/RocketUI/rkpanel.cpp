// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"

#include "rkpanel.h"

#include "rkhud_model.h"

#include <rocketui/rmlui.h>
import ks.core;
import std;

Rml::ElementDocument *RkPanelLoad( Rml::ElementDocument *&doc, RocketDesinationContext_t ctx,
                                   const char *file, LoadDocumentFn onLoad,
                                   UnloadDocumentFn onUnload )
{
    if ( doc )
        return doc;

    doc = RkLoadDocument( ctx, file, onLoad, onUnload );
    return doc;
}

void RkPanelUnload( Rml::ElementDocument *&doc, bool &visible )
{
    if ( doc )
    {
        doc->Close();
        doc = nullptr;
    }

    visible = false;
}

void RkPanelShow( Rml::ElementDocument *doc, bool &visible, bool show )
{
    if ( doc )
    {
        if ( show )
            doc->Show( Rml::ModalFlag::None, Rml::FocusFlag::Auto, Rml::ScrollFlag::None );
        else
            doc->Hide();
    }

    visible = show;
}
