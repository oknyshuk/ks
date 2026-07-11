// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#ifndef KISAKSTRIKE_RKHUD_SCOPE_H
#define KISAKSTRIKE_RKHUD_SCOPE_H

#include <rocketui/rocketui.h>
#include "hudelement.h"

extern ConVar cl_drawhud;

class RkHudScope : public CHudElement {
public:
    explicit RkHudScope(const char *pElementName);
    virtual ~RkHudScope();

    void LevelInit(void);
    virtual void LevelShutdown(void);
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);

    Rml::ElementDocument *m_pDocument;
    bool m_bVisible;

    // Fill elements (black areas around scope viewport)
    Rml::Element *m_fillTop;
    Rml::Element *m_fillBottom;
    Rml::Element *m_fillLeft;
    Rml::Element *m_fillRight;

    // Centered square whose corners scope_arc masks into a circle
    Rml::Element *m_scope;

    // Reticle elements: 0=v-core, 1=h-core (sharp, steady),
    //                   2=v-blur, 3=h-blur (soft sprite, moving)
    Rml::Element *m_reticle[4];

private:
    void UpdateScope();

    // Animation state
    float m_fAnimInset;
    float m_fLineSpreadDistance;
};

#endif //KISAKSTRIKE_RKHUD_SCOPE_H
