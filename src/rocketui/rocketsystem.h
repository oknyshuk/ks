#ifndef ROCKETSYSTEM_H
#define ROCKETSYSTEM_H

#include <RmlUi/Core/SystemInterface.h>

#ifdef USE_SDL
#include <SDL_mouse.h>
#endif

class RocketSystem : public Rml::SystemInterface {
public:
  static RocketSystem m_Instance;

  SDL_Cursor *m_pCursors[SDL_NUM_SYSTEM_CURSORS];

  // Get the number of seconds elapsed since the start of the application
  double GetElapsedTime() override;

  // Log the specified message
  bool LogMessage(Rml::Log::Type type, const Rml::String &message) override;

  // Allocate various system cursors
  void InitCursors();
  // Free system cursors
  void FreeCursors();

  // Set mouse cursor
  void SetMouseCursor(const Rml::String &cursor_name) override;

  // Set clipboard text
  void SetClipboardText(const Rml::String &text) override;

  // Get clipboard text
  void GetClipboardText(Rml::String &text) override;
};

#endif // ROCKETSYSTEM_H
