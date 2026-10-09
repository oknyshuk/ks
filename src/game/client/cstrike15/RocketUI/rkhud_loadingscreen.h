#pragma once
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include <rocketui/rmlui.h>

#include <rocketui/rocketui.h>

class RocketLoadingScreenDocument
{
protected:
    static Rml::ElementDocument *m_pInstance;

public:
    static void LoadDialog( void );
    static void UnloadDialog( void );
    static void ShowPanel( bool bShow, bool immediate = false );
    static bool IsActive() { return m_pInstance != nullptr; }
    static bool IsVisible() { return m_bVisible; }
    static Rml::ElementDocument *GetInstance() { return m_pInstance; }
private:
    static bool m_bVisible;
};
