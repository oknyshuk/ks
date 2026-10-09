#pragma once
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include <rocketui/rmlui.h>

import ks.desktop;

class RocketSystem : public Rml::SystemInterface {
public:
  static RocketSystem m_Instance;

  ks::desktop::cursor *m_pCursors[ks::desktop::cursor_kind_count];

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
