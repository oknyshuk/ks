#ifndef KISAKSTRIKE_RKCONSOLE_H
#define KISAKSTRIKE_RKCONSOLE_H

#include "icvar.h"
#include "color.h"
#include "tier1/utlvector.h"
#include "tier1/utlstring.h"
#include "GameEventListener.h"
#include <rocketui/rocketui.h>
#include <RmlUi/Core/Types.h>

namespace Rml {
    class ElementDocument;
    class Element;
    class ElementFormControlInput;
    class ElementFormControlTextArea;
}

class RocketConsole : public IConsoleDisplayFunc, public CGameEventListener
{
public:
    static RocketConsole& Instance();

    RocketConsole();
    ~RocketConsole();

    void Initialize();
    void Shutdown();

    void Show();
    void Hide();
    void Toggle();
    bool IsVisible() const { return m_bVisible; }

    void Clear();

    // IConsoleDisplayFunc implementation
    virtual void ColorPrint(const Color& clr, const char* pMessage) override;
    virtual void Print(const char* pMessage) override;
    virtual void DPrint(const char* pMessage) override;
    virtual void GetConsoleText(char* pchText, size_t bufSize) const override;

    // Input handling - called from RocketUI input system
    bool HandleKeyInput(int key, bool down);
    bool HandleCharInput(wchar_t ch);

    // CGameEventListener
    virtual void FireGameEvent(IGameEvent *event);

private:
    void LoadDocument();
    void UnloadDocument();

    // Input helpers (native input handles editing, we handle special keys)
    CUtlString GetInputValue() const;
    void SetInputValue(const char* text);
    void Submit();
    bool IsInputFocused() const;

    // History
    void AddToHistory(const char* command);
    void HistoryUp();
    void HistoryDown();

    // Autocomplete
    void RebuildCompletionList();
    void ClearCompletionList();
    void ShowCompletionList();
    void HideCompletionList();
    void NextCompletion();
    void PrevCompletion();
    void ApplyCompletion();

    // Output
    void AddOutputLine(const char* text, const Color& color);
    void FlushOutput();
    void ScrollToBottom();

    static void LoadCallback();
    static void UnloadCallback();

private:
    Rml::ElementDocument* m_pDocument;
    Rml::ElementFormControlTextArea* m_elemOutput;  // Readonly textarea for output
    Rml::ElementFormControlInput* m_elemInput;      // Native input element
    Rml::Element* m_elemCompletionList;

    bool m_bVisible;
    bool m_bGrabbingInput;
    bool m_bInitialized;

    // Command history
    CUtlVector<CUtlString> m_CommandHistory;
    int m_iHistoryPos;  // -1 = not browsing history
    CUtlString m_szSavedInput;  // Saved when browsing history

    // Autocomplete
    struct CompletionItem {
        CUtlString name;
        CUtlString value;
        bool isCommand;
    };
    CUtlVector<CompletionItem> m_CompletionList;
    int m_iCompletionIndex;
    bool m_bCompletionVisible;

    // Output buffer (for replay on document load)
    struct OutputLine {
        CUtlString text;
        Color color;
    };
    CUtlVector<OutputLine> m_OutputBuffer;
    static const int MAX_OUTPUT_LINES = 200;
    static const size_t MAX_OUTPUT_SIZE = 20 * 1024;  // ~20KB text limit (~2MB geometry)

    Color m_PrintColor;
    Color m_DPrintColor;

    // Pending output to batch updates
    Rml::String m_szPendingOutput;
};

RocketConsole& RkConsole();

#endif // KISAKSTRIKE_RKCONSOLE_H
