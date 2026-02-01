#include "rocketsystem.h"
#include "rocketuiimpl.h"

#include <SDL.h>

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
  m_pCursors[SDL_SYSTEM_CURSOR_ARROW] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
  m_pCursors[SDL_SYSTEM_CURSOR_IBEAM] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
  m_pCursors[SDL_SYSTEM_CURSOR_WAIT] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
  m_pCursors[SDL_SYSTEM_CURSOR_CROSSHAIR] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
  m_pCursors[SDL_SYSTEM_CURSOR_WAITARROW] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAITARROW);
  m_pCursors[SDL_SYSTEM_CURSOR_SIZENWSE] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
  m_pCursors[SDL_SYSTEM_CURSOR_SIZENESW] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
  m_pCursors[SDL_SYSTEM_CURSOR_SIZEWE] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
  m_pCursors[SDL_SYSTEM_CURSOR_SIZENS] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
  m_pCursors[SDL_SYSTEM_CURSOR_SIZEALL] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
  m_pCursors[SDL_SYSTEM_CURSOR_NO] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO);
  m_pCursors[SDL_SYSTEM_CURSOR_HAND] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
#endif
}

void RocketSystem::FreeCursors() {
#ifdef USE_SDL
  SDL_FreeCursor(m_pCursors[SDL_SYSTEM_CURSOR_ARROW]);
  SDL_FreeCursor(m_pCursors[SDL_SYSTEM_CURSOR_IBEAM]);
  SDL_FreeCursor(m_pCursors[SDL_SYSTEM_CURSOR_WAIT]);
  SDL_FreeCursor(m_pCursors[SDL_SYSTEM_CURSOR_CROSSHAIR]);
  SDL_FreeCursor(m_pCursors[SDL_SYSTEM_CURSOR_WAITARROW]);
  SDL_FreeCursor(m_pCursors[SDL_SYSTEM_CURSOR_SIZENWSE]);
  SDL_FreeCursor(m_pCursors[SDL_SYSTEM_CURSOR_SIZENESW]);
  SDL_FreeCursor(m_pCursors[SDL_SYSTEM_CURSOR_SIZEWE]);
  SDL_FreeCursor(m_pCursors[SDL_SYSTEM_CURSOR_SIZENS]);
  SDL_FreeCursor(m_pCursors[SDL_SYSTEM_CURSOR_SIZEALL]);
  SDL_FreeCursor(m_pCursors[SDL_SYSTEM_CURSOR_NO]);
  SDL_FreeCursor(m_pCursors[SDL_SYSTEM_CURSOR_HAND]);
#endif
}

void RocketSystem::SetMouseCursor(const Rml::String &cursor_name) {
#ifdef USE_SDL
  if (cursor_name == "move")
    SDL_SetCursor(m_pCursors[SDL_SYSTEM_CURSOR_CROSSHAIR]);
  else if (cursor_name == "pointer")
    SDL_SetCursor(m_pCursors[SDL_SYSTEM_CURSOR_HAND]);
  else if (cursor_name == "resize")
    SDL_SetCursor(m_pCursors[SDL_SYSTEM_CURSOR_SIZEALL]);
  else if (cursor_name == "cross")
    SDL_SetCursor(m_pCursors[SDL_SYSTEM_CURSOR_CROSSHAIR]);
  else if (cursor_name == "text")
    SDL_SetCursor(m_pCursors[SDL_SYSTEM_CURSOR_IBEAM]);
  else if (cursor_name == "unavailable")
    SDL_SetCursor(m_pCursors[SDL_SYSTEM_CURSOR_NO]);
  else
    SDL_SetCursor(m_pCursors[SDL_SYSTEM_CURSOR_ARROW]);
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
