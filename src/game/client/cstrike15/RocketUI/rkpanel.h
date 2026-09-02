// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#ifndef KISAKSTRIKE_RKPANEL_H
#define KISAKSTRIKE_RKPANEL_H

#include <rocketui/rocketui.h>

// The shared half of a menu panel's lifecycle.
//
// Six panels (main menu, pause menu, team menu, loading screen, options, play)
// each had their own copy of: load the document if it isn't up, warn if that
// failed, Close() and forget it on the way out, and Show()/Hide() while tracking a
// visible flag. The panels keep their own statics -- these take them by reference,
// so nothing about how a panel is declared or reached has to change.
//
// What stays with the panel is what differs: which controls to populate, and
// whether it is allowed to open at all.

// Loads the document if needed. Returns it, or nullptr if the load failed; the
// caller's pointer is updated either way. onLoad/onUnload are the hot-reload pair
// `rocket_reload` calls.
Rml::ElementDocument *RkPanelLoad( Rml::ElementDocument *&doc, RocketDesinationContext_t ctx,
                                   const char *file, LoadDocumentFn onLoad,
                                   UnloadDocumentFn onUnload );

// Closes and forgets the document. Safe to call when it is not loaded.
void RkPanelUnload( Rml::ElementDocument *&doc, bool &visible );

// Shows or hides an already-loaded document. Panels that own the mouse are asked
// about it through their own OwnsInput(), which reads `visible`.
void RkPanelShow( Rml::ElementDocument *doc, bool &visible, bool show );

#endif // KISAKSTRIKE_RKPANEL_H
