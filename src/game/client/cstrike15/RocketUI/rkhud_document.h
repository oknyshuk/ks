// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#ifndef KISAKSTRIKE_RKHUD_DOCUMENT_H
#define KISAKSTRIKE_RKHUD_DOCUMENT_H

#include "hud_element_helper.h"
#include "hudelement.h"
#include "rkhud_model.h"

#include <rocketui/rocketui.h>

// Document lifetime for the RmlUi-backed HUD elements.
//
// Every one of these carried the same pair of free functions: fetch the element
// through GET_HUDELEMENT, warn if that failed, bail if already loaded, check the
// HUD context, load the .rml, warn if that failed, and on the way out Close() and
// null the handle. Nine copies, differing only in the names inside the strings.
//
// What is genuinely per-element -- what to cache from the document, which
// listeners to add, what to do per frame -- stays in the element as OnLoad,
// OnUnload and its own ShowPanel. Visibility and input are deliberately *not*
// managed here: they differ enough per element that a shared version would be a
// pile of flags.
//
// Usage:
//     class RkHudThing : public RkHudDocument<RkHudThing> { ... };
//     const char *RkHudThing::kDocument = "hud_thing.rml";
template <typename T>
class RkHudDocument : public CHudElement
{
public:
    explicit RkHudDocument( const char *pElementName ) : CHudElement( pElementName ) {}

    // CHudElement
    void LevelInit() override { Load(); }
    void LevelShutdown() override { Unload(); }

    Rml::ElementDocument *m_pDocument = nullptr;
    bool m_bVisible = false;

protected:
    // Defaults, hidden by T if it needs them.
    void OnLoad() {}
    void OnUnload() {}

    void Load()
    {
        if ( m_pDocument )
            return;

        m_pDocument = RkLoadDocument( ROCKET_CONTEXT_HUD, T::kDocument, &LoadThunk, &UnloadThunk );
        if ( !m_pDocument )
            return;

        static_cast<T *>( this )->OnLoad();
    }

    void Unload()
    {
        if ( !m_pDocument )
            return;

        static_cast<T *>( this )->OnUnload();
        m_pDocument->Close();
        m_pDocument = nullptr;
        m_bVisible = false;
    }

private:
    // `rocket_reload` hands documents back plain function pointers, so each
    // element needs its own pair. CRTP generates them.
    static void LoadThunk()
    {
        if ( T *element = GET_HUDELEMENT( T ) )
            element->Load();
    }
    static void UnloadThunk()
    {
        if ( T *element = GET_HUDELEMENT( T ) )
            element->Unload();
    }
};

#endif // KISAKSTRIKE_RKHUD_DOCUMENT_H
