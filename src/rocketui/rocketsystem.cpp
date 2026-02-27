#include "rocketsystem.h"
#include "rocketuiimpl.h"

#include <SDL3/SDL.h>

RocketSystem RocketSystem::m_Instance;

double RocketSystem::GetElapsedTime() {
  return (double)RocketUIImpl::m_Instance.GetTime();
}

bool RocketSystem::LogMessage(Rml::Log::Type type, const Rml::String &message) {
  bool ret = false;
  if (type == Rml::Log::LT_ERROR)
    ret = true;

  fprintf(stderr, "[RocketUI] %s\n", message.c_str());

  return ret;
}

void RocketSystem::InitCursors() {
#ifdef USE_SDL
  m_pCursors[SDL_SYSTEM_CURSOR_DEFAULT] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
  m_pCursors[SDL_SYSTEM_CURSOR_TEXT] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
  m_pCursors[SDL_SYSTEM_CURSOR_WAIT] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
  m_pCursors[SDL_SYSTEM_CURSOR_CROSSHAIR] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
  m_pCursors[SDL_SYSTEM_CURSOR_PROGRESS] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_PROGRESS);
  m_pCursors[SDL_SYSTEM_CURSOR_NWSE_RESIZE] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE);
  m_pCursors[SDL_SYSTEM_CURSOR_NESW_RESIZE] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NESW_RESIZE);
  m_pCursors[SDL_SYSTEM_CURSOR_EW_RESIZE] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE);
  m_pCursors[SDL_SYSTEM_CURSOR_NS_RESIZE] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE);
  m_pCursors[SDL_SYSTEM_CURSOR_MOVE] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE);
  m_pCursors[SDL_SYSTEM_CURSOR_NOT_ALLOWED] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NOT_ALLOWED);
  m_pCursors[SDL_SYSTEM_CURSOR_POINTER] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
#endif
}

void RocketSystem::FreeCursors() {
#ifdef USE_SDL
  SDL_DestroyCursor(m_pCursors[SDL_SYSTEM_CURSOR_DEFAULT]);
  SDL_DestroyCursor(m_pCursors[SDL_SYSTEM_CURSOR_TEXT]);
  SDL_DestroyCursor(m_pCursors[SDL_SYSTEM_CURSOR_WAIT]);
  SDL_DestroyCursor(m_pCursors[SDL_SYSTEM_CURSOR_CROSSHAIR]);
  SDL_DestroyCursor(m_pCursors[SDL_SYSTEM_CURSOR_PROGRESS]);
  SDL_DestroyCursor(m_pCursors[SDL_SYSTEM_CURSOR_NWSE_RESIZE]);
  SDL_DestroyCursor(m_pCursors[SDL_SYSTEM_CURSOR_NESW_RESIZE]);
  SDL_DestroyCursor(m_pCursors[SDL_SYSTEM_CURSOR_EW_RESIZE]);
  SDL_DestroyCursor(m_pCursors[SDL_SYSTEM_CURSOR_NS_RESIZE]);
  SDL_DestroyCursor(m_pCursors[SDL_SYSTEM_CURSOR_MOVE]);
  SDL_DestroyCursor(m_pCursors[SDL_SYSTEM_CURSOR_NOT_ALLOWED]);
  SDL_DestroyCursor(m_pCursors[SDL_SYSTEM_CURSOR_POINTER]);
#endif
}

void RocketSystem::SetMouseCursor(const Rml::String &cursor_name) {
#ifdef USE_SDL
  if (cursor_name == "move")
    SDL_SetCursor(m_pCursors[SDL_SYSTEM_CURSOR_CROSSHAIR]);
  else if (cursor_name == "pointer")
    SDL_SetCursor(m_pCursors[SDL_SYSTEM_CURSOR_POINTER]);
  else if (cursor_name == "resize")
    SDL_SetCursor(m_pCursors[SDL_SYSTEM_CURSOR_MOVE]);
  else if (cursor_name == "cross")
    SDL_SetCursor(m_pCursors[SDL_SYSTEM_CURSOR_CROSSHAIR]);
  else if (cursor_name == "text")
    SDL_SetCursor(m_pCursors[SDL_SYSTEM_CURSOR_TEXT]);
  else if (cursor_name == "unavailable")
    SDL_SetCursor(m_pCursors[SDL_SYSTEM_CURSOR_NOT_ALLOWED]);
  else
    SDL_SetCursor(m_pCursors[SDL_SYSTEM_CURSOR_DEFAULT]);
#endif
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
