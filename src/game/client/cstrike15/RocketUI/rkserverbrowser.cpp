//========================================================================//
//
// rkserverbrowser.cpp - PLACEHOLDER (not yet built)
//
// The legacy VGUI serverbrowser module (src/serverbrowser, ~9K LOC) has been
// removed as part of retiring VGUI in favor of RmlUi ("RocketUI").
//
// This is where the RmlUi server browser will live. To implement it:
//   1. Author an .rml layout + .rcss (server list, filter/tab bar, join/refresh
//      buttons) alongside the other RocketUI documents.
//   2. Add an RkServerBrowser controller (DECLARE like the other rk* elements)
//      that owns the document and wires the buttons to the queries below.
//   3. Drive server/matchmaking queries via the master-server / matchmaking
//      interfaces (internet/LAN/history/favorites lists, per-server info,
//      password + join flow). See the old serverbrowser git history for the
//      query plumbing (IServerBrowser / ISteamMatchmakingServers usage).
//   4. Point the menu's "OpenServerBrowser" command at this document instead of
//      the removed g_VModuleLoader.ActivateModule("Servers") path
//      (see cstrike15basepanel.cpp::OnOpenServerBrowser).
//   5. Add this file (and any siblings) to game/client/wscript once real.
//
//========================================================================//
