// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#ifndef KISAKSTRIKE_RKHUD_MODEL_H
#define KISAKSTRIKE_RKHUD_MODEL_H

#include <rocketui/rocketui.h>
#include <rocketui/rmlui.h>

// The HUD's one data model.
//
// Every hud_*.rml says data-model="hud" and reads its own section, e.g.
// crosshair.x0 or infobar.hp. Before this there were eight models, each with its
// own create/remove/handle plumbing repeated in the element that owned it, and
// each element could only ever see its own data.
//
// A section registers a bind function from its own file with RK_HUD_SECTION, so
// the schema still lives next to the code that fills it. All sections are bound
// when the model is created, which happens before any document loads, so a
// document may read across sections.
//
// The model deliberately outlives documents: it binds file-scope statics, so
// level changes and `rocket_reload` can close every document without touching it.

using RkHudSectionFn = void ( * )( Rml::DataModelConstructor & );

struct RkHudSection
{
    explicit RkHudSection( RkHudSectionFn bindFunc );
};

// Register a section's bind function. Place at file scope:
//     static void BindCrosshair( Rml::DataModelConstructor &c ) { ... }
//     RK_HUD_SECTION( BindCrosshair );
#define RK_HUD_SECTION( fn ) static const RkHudSection g_##fn##_registration( fn )

// The HUD model, created (and all sections bound) on first use. Invalid if there
// is no HUD context yet.
Rml::DataModelConstructor RkHudModel();

// Load a document that reads the model, ensuring the model exists in the target
// context first. Every UI document goes through here: RocketUI::LoadDocumentFile
// happily loads a document whose data-model is missing, which renders every
// binding as a blank and logs "Could not locate data model 'hud'" -- which is
// exactly what happened when a panel was opened before any HUD element had ever
// loaded, i.e. at the main menu.
Rml::ElementDocument *RkLoadDocument( RocketDesinationContext_t ctx, const char *file,
                                      LoadDocumentFn onLoad = nullptr,
                                      UnloadDocumentFn onUnload = nullptr );

// Mark one section dirty by the name it was bound under. Only the views reading
// that section are re-evaluated, so one element updating does not cost the rest.
void RkHudDirty( const char *section );

#endif // KISAKSTRIKE_RKHUD_MODEL_H
