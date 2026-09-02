// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include "cbase.h"

#include "rkhud_model.h"

#include <rocketui/rocketui.h>

#include <vector>

// Function-local so registration works whatever order the translation units
// initialise in.
static std::vector<RkHudSectionFn> &Sections()
{
    static std::vector<RkHudSectionFn> sections;
    return sections;
}

RkHudSection::RkHudSection( RkHudSectionFn bindFunc )
{
    if ( bindFunc )
        Sections().push_back( bindFunc );
}

// One model per context, same schema in each. HUD documents load into the HUD
// context and panels into whichever context is current, so both carry it.
//
// The bookkeeping is here rather than asking RmlUi: Context::GetDataModel logs an
// error when the model is absent, which is exactly the case we need to test for.
static Rml::DataModelConstructor EnsureModel( Rml::Context *ctx )
{
    if ( !ctx )
        return {};

    struct Bound
    {
        Rml::Context *ctx = nullptr;
    };
    static Bound s_bound[2];

    for ( const Bound &bound : s_bound )
        if ( bound.ctx == ctx )
            return ctx->GetDataModel( "hud" );

    // Not allow_missing_variables: every section is bound below before any
    // document loads, so a name a document cannot resolve is an authoring typo
    // and should say so loudly instead of silently rendering nothing.
    Rml::DataModelConstructor model = ctx->CreateDataModel( "hud" );
    if ( !model )
    {
        Warning( "RocketUI: couldn't create the UI data model\n" );
        return {};
    }

    for ( Bound &bound : s_bound )
    {
        if ( !bound.ctx )
        {
            bound.ctx = ctx;
            break;
        }
    }

    for ( RkHudSectionFn bindFunc : Sections() )
        bindFunc( model );

    return model;
}

Rml::DataModelConstructor RkHudModel()
{
    if ( !RocketUI() )
        return {};

    // Both contexts get the schema; the HUD one is what callers work with.
    EnsureModel( RocketUI()->AccessMenuContext() );
    return EnsureModel( RocketUI()->AccessHudContext() );
}

Rml::ElementDocument *RkLoadDocument( RocketDesinationContext_t ctx, const char *file,
                                      LoadDocumentFn onLoad, UnloadDocumentFn onUnload )
{
    if ( !RocketUI() )
        return nullptr;

    // Before the document, not after: the model has to be in place for the
    // document's bindings to resolve as it loads.
    RkHudModel();

    Rml::ElementDocument *doc = RocketUI()->LoadDocumentFile( ctx, file, onLoad, onUnload );
    if ( !doc )
        Warning( "RocketUI: couldn't load %s\n", file );

    return doc;
}

void RkHudDirty( const char *section )
{
    if ( !RocketUI() )
        return;

    // A section can be on screen in either context (panels load into whichever
    // is current), so both are told.
    for ( Rml::Context *ctx : { RocketUI()->AccessHudContext(), RocketUI()->AccessMenuContext() } )
    {
        if ( Rml::DataModelConstructor model = EnsureModel( ctx ) )
            model.GetModelHandle().DirtyVariable( section );
    }
}
