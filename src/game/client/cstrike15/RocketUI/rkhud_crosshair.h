#ifndef KISAKSTRIKE_RKHUD_CROSSHAIR_H
#define KISAKSTRIKE_RKHUD_CROSSHAIR_H

#include <rocketui/rocketui.h>
#include "hudelement.h"

extern ConVar cl_drawhud;

class RkHudCrosshair : public CHudElement {
public:
    explicit RkHudCrosshair(const char *pElementName);
    virtual ~RkHudCrosshair();

    // Overrides from CHudElement
    void LevelInit(void);
    virtual void LevelShutdown(void);
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);

    Rml::ElementDocument *m_pDocument;
    bool m_bVisible;

    // Crosshair line elements
    Rml::Element *m_lineTop;
    Rml::Element *m_lineBottom;
    Rml::Element *m_lineLeft;
    Rml::Element *m_lineRight;
    Rml::Element *m_dot;

    // Outline elements
    Rml::Element *m_olTop;
    Rml::Element *m_olBottom;
    Rml::Element *m_olLeft;
    Rml::Element *m_olRight;
    Rml::Element *m_olDot;

private:
    void UpdateCrosshair();
};

#endif //KISAKSTRIKE_RKHUD_CROSSHAIR_H
