#include "rocketsystem.h"
#include "rocketuiimpl.h"

#include <SDL3/SDL.h>

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

// Every system cursor SDL offers, indexed by its own enum: the whole array is
// cheap and it means SetMouseCursor is a lookup rather than a chain.
void RocketSystem::InitCursors() {
  for (int i = 0; i < SDL_SYSTEM_CURSOR_COUNT; i++)
    m_pCursors[i] = SDL_CreateSystemCursor((SDL_SystemCursor)i);
}

void RocketSystem::FreeCursors() {
  for (SDL_Cursor *&cursor : m_pCursors) {
    SDL_DestroyCursor(cursor);
    cursor = nullptr;
  }
}

// RCSS `cursor` values, which are ours to name, mapped onto SDL's set.
void RocketSystem::SetMouseCursor(const Rml::String &cursor_name) {
  struct Mapping {
    const char *name;
    SDL_SystemCursor cursor;
  };
  static constexpr Mapping kCursors[] = {
      {"move", SDL_SYSTEM_CURSOR_MOVE},
      {"pointer", SDL_SYSTEM_CURSOR_POINTER},
      {"resize", SDL_SYSTEM_CURSOR_NWSE_RESIZE},
      {"cross", SDL_SYSTEM_CURSOR_CROSSHAIR},
      {"text", SDL_SYSTEM_CURSOR_TEXT},
      {"unavailable", SDL_SYSTEM_CURSOR_NOT_ALLOWED},
      {"wait", SDL_SYSTEM_CURSOR_WAIT},
      {"progress", SDL_SYSTEM_CURSOR_PROGRESS},
  };

  SDL_SystemCursor wanted = SDL_SYSTEM_CURSOR_DEFAULT;
  for (const Mapping &entry : kCursors)
    if (cursor_name == entry.name)
      wanted = entry.cursor;

  SDL_SetCursor(m_pCursors[wanted]);
}

void RocketSystem::SetClipboardText(const Rml::String &text) {
  SDL_SetClipboardText(text.c_str());
}

void RocketSystem::GetClipboardText(Rml::String &text) {
  char *clipboard = SDL_GetClipboardText();
  if (clipboard) {
    text = clipboard;
    SDL_free(clipboard);
  } else {
    text.clear();
  }
}
