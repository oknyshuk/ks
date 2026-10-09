#pragma once
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include <rocketui/rmlui.h>

#include <rocketui/rocketui.h>

#include <rocketui/rmlui.h>

inline constexpr auto MAX_POPUP_TEXT = 256;

class RocketPopupDocument : Rml::EventListener
{
public:
    RocketPopupDocument( const char *text, float timeout = 5.0f );
    virtual ~RocketPopupDocument();
    bool IsActive() { return m_pInstance != nullptr; }
    bool IsVisible() { return m_bVisible; }
    Rml::ElementDocument *GetInstance() { return m_pInstance; }

    void ProcessEvent( Rml::Event &event ) override;
private:
    Rml::ElementDocument *m_pInstance;
    char m_textBuffer[MAX_POPUP_TEXT];

    float m_fTimeout;
    bool m_bVisible;
    bool m_bGrabbingInput;
};
