// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include "rocketsystem.h"
#include "rocketuiimpl.h"
import ks.core.dbg;


RocketSystem RocketSystem::m_Instance;

double RocketSystem::GetElapsedTime() {
  return (double)RocketUIImpl::m_Instance.GetTime();
}

bool RocketSystem::LogMessage(Rml::Log::Type type, const Rml::String &message) {
  // Through tier0, so RmlUi's own document/RCSS/font errors land in the console and
  // in csgo/console.log rather than only on stderr.
  if (type == Rml::Log::LT_ERROR || type == Rml::Log::LT_ASSERT)
    Warning("[RocketUI] %s\n", message.c_str());
  else
    Msg("[RocketUI] %s\n", message.c_str());

  return true; // continue execution; false would break into the debugger
}

// One cursor per kind: the whole array is cheap and it means SetMouseCursor is
// a lookup rather than a chain.
void RocketSystem::InitCursors() {
  for (int i = 0; i < ks::desktop::cursor_kind_count; i++)
    m_pCursors[i] = ks::desktop::create_cursor((ks::desktop::cursor_kind)i);
}

void RocketSystem::FreeCursors() {
  for (ks::desktop::cursor *&cursor : m_pCursors) {
    ks::desktop::destroy_cursor(cursor);
    cursor = nullptr;
  }
}

// RCSS `cursor` values, which are ours to name, mapped onto the desktop's set.
void RocketSystem::SetMouseCursor(const Rml::String &cursor_name) {
  struct Mapping {
    const char *name;
    ks::desktop::cursor_kind cursor;
  };
  static constexpr Mapping kCursors[] = {
      {"move", ks::desktop::cursor_kind::move},
      {"pointer", ks::desktop::cursor_kind::pointer},
      {"resize", ks::desktop::cursor_kind::diagonal_resize},
      {"cross", ks::desktop::cursor_kind::crosshair},
      {"text", ks::desktop::cursor_kind::text},
      {"unavailable", ks::desktop::cursor_kind::not_allowed},
      {"wait", ks::desktop::cursor_kind::wait},
      {"progress", ks::desktop::cursor_kind::progress},
  };

  ks::desktop::cursor_kind wanted = ks::desktop::cursor_kind::arrow;
  for (const Mapping &entry : kCursors)
    if (cursor_name == entry.name)
      wanted = entry.cursor;

  ks::desktop::use_cursor(m_pCursors[(int)wanted]);
}

void RocketSystem::SetClipboardText(const Rml::String &text) {
  ks::desktop::set_clipboard(text.c_str());
}

void RocketSystem::GetClipboardText(Rml::String &text) {
  char clipboard[16 * 1024];
  ks::desktop::clipboard(clipboard, sizeof(clipboard));
  text = clipboard;
}
