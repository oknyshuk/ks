#ifndef KISAKSTRIKE_RKHUD_SCOREBOARD_H
#define KISAKSTRIKE_RKHUD_SCOREBOARD_H

#include <rocketui/rocketui.h>
#include "rkhud_document.h"
#include <rocketui/rmlui.h>

extern ConVar cl_drawhud;

class RkHudScoreboard : public RkHudDocument<RkHudScoreboard> {
public:
    static const char *kDocument;

    explicit RkHudScoreboard(const char *value);
    virtual ~RkHudScoreboard();

    // Overrides from CHudElement
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);
    void OnLoad();

    // Some precached elements from the instance.
    Rml::Element *m_elemCtSection;
    Rml::Element *m_elemTSection;


private:
    void Update();
};

#endif //KISAKSTRIKE_RKHUD_SCOREBOARD_H
