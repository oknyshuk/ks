#include "rkconsole.h"
#include "cbase.h"

#include "cdll_int.h"
#include "inputsystem/ButtonCode.h"
#include "inputsystem/iinputsystem.h"
#include "tier1/convar.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Elements/ElementFormControlTextArea.h>

// Event listener for console input changes
class ConsoleInputListener : public Rml::EventListener {
public:
  void ProcessEvent(Rml::Event &event) override {
    if (event.GetId() == Rml::EventId::Change) {
      RkConsole().OnInputChange();
    }
  }
};

static ConsoleInputListener s_InputListener;

static RocketConsole s_RocketConsole;

RocketConsole &RocketConsole::Instance() { return s_RocketConsole; }

RocketConsole &RkConsole() { return s_RocketConsole; }

ConVar rocket_console_enabled("rocket_console_enabled", "1", FCVAR_ARCHIVE,
                              "Enable RocketUI console");

CON_COMMAND(rocket_console_show, "Show the RocketUI console") {
  RkConsole().Show();
}

CON_COMMAND(rocket_console_hide, "Hide the RocketUI console") {
  RkConsole().Hide();
}

CON_COMMAND(rocket_console_toggle, "Toggle the RocketUI console") {
  RkConsole().Toggle();
}

CON_COMMAND(rocket_console_clear, "Clear the RocketUI console") {
  RkConsole().Clear();
}

RocketConsole::RocketConsole()
    : m_pDocument(nullptr), m_elemOutput(nullptr), m_elemInput(nullptr),
      m_elemCompletionList(nullptr), m_bVisible(false), m_bGrabbingInput(false),
      m_bInitialized(false), m_iHistoryPos(-1), m_iCompletionIndex(-1),
      m_bCompletionVisible(false), m_PrintColor(216, 222, 211, 255),
      m_DPrintColor(196, 181, 80, 255) {}

RocketConsole::~RocketConsole() { Shutdown(); }

static bool ConsoleKeyInputHandler(int buttonCode, bool down) {
  return RkConsole().HandleKeyInput(buttonCode, down);
}

static bool ConsoleCharInputHandler(wchar_t ch) {
  return RkConsole().HandleCharInput(ch);
}

void RocketConsole::Initialize() {
  if (m_bInitialized)
    return;

  if (g_pCVar)
    g_pCVar->InstallConsoleDisplayFunc(this);

  // Register input handlers with RocketUI
  RocketUI()->RegisterConsoleHandlers(ConsoleKeyInputHandler,
                                      ConsoleCharInputHandler);

  // Listen for map changes to clear console (reduces geometry buffer pressure)
  ListenForGameEvent("game_newmap");

  m_bInitialized = true;
}

void RocketConsole::Shutdown() {
  if (!m_bInitialized)
    return;

  UnloadDocument();

  // Unregister input handlers
  RocketUI()->RegisterConsoleHandlers(nullptr, nullptr);

  // Stop listening for game events
  StopListeningForAllEvents();

  if (g_pCVar)
    g_pCVar->RemoveConsoleDisplayFunc(this);

  m_CommandHistory.Purge();
  m_CompletionList.Purge();
  m_OutputBuffer.Purge();

  m_bInitialized = false;
}

void RocketConsole::LoadCallback() { RkConsole().LoadDocument(); }

void RocketConsole::UnloadCallback() { RkConsole().UnloadDocument(); }

void RocketConsole::LoadDocument() {
  if (m_pDocument)
    return;

  m_pDocument = RocketUI()->LoadDocumentFile(
      ROCKET_CONTEXT_MENU, "hud_console.rml", LoadCallback, UnloadCallback);

  if (!m_pDocument) {
    Warning("RocketConsole: Failed to load hud_console.rml\n");
    return;
  }

  // Get the textarea output element
  if (Rml::Element *outputElem = m_pDocument->GetElementById("console_output")) {
    m_elemOutput = rmlui_dynamic_cast<Rml::ElementFormControlTextArea *>(outputElem);
  }

  m_elemCompletionList = m_pDocument->GetElementById("completion_list");

  // Get the native input element
  if (Rml::Element *inputElem = m_pDocument->GetElementById("console_input")) {
    m_elemInput = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(inputElem);
    if (m_elemInput) {
      // Listen for input changes to auto-update completions
      m_elemInput->AddEventListener(Rml::EventId::Change, &s_InputListener);
    }
  }

  // Replay buffered output to the textarea
  if (m_elemOutput && m_OutputBuffer.Count() > 0) {
    Rml::String allText;
    for (int i = 0; i < m_OutputBuffer.Count(); i++) {
      allText += m_OutputBuffer[i].text.Get();
    }

    // Apply same truncation as FlushOutput to ensure consistency
    if (allText.size() > MAX_OUTPUT_SIZE) {
      size_t removeBytes = allText.size() - MAX_OUTPUT_SIZE;
      size_t cutPos = allText.find('\n', removeBytes);
      if (cutPos != Rml::String::npos)
        allText.erase(0, cutPos + 1);
      else
        allText.erase(0, removeBytes);
    }

    m_elemOutput->SetValue(allText);
  }

  // Clear pending output since it was replayed from m_OutputBuffer
  // (both contain the same messages, so clearing prevents duplicates)
  m_szPendingOutput.clear();

  m_pDocument->Hide();
}

void RocketConsole::UnloadDocument() {
  if (!m_pDocument)
    return;

  if (m_bGrabbingInput) {
    RocketUI()->DenyInputToGame(false, "RocketConsole");
    RocketUI()->SetInputContext(nullptr);
    m_bGrabbingInput = false;
  }

  // Remove event listener before closing
  if (m_elemInput) {
    m_elemInput->RemoveEventListener(Rml::EventId::Change, &s_InputListener);
  }

  m_pDocument->Close();
  m_pDocument = nullptr;
  m_elemOutput = nullptr;
  m_elemInput = nullptr;
  m_elemCompletionList = nullptr;
  m_bVisible = false;
}

void RocketConsole::Show() {
  if (!rocket_console_enabled.GetBool())
    return;

  if (!m_bInitialized)
    Initialize();

  if (!m_pDocument)
    LoadDocument();

  if (!m_pDocument)
    return;

  m_pDocument->Show();
  m_bVisible = true;

  if (!m_bGrabbingInput) {
    RocketUI()->DenyInputToGame(true, "RocketConsole");
    RocketUI()->EnableCursor(true);
    RocketUI()->SetInputContext(RocketUI()->AccessMenuContext());
    m_bGrabbingInput = true;
  }

  // Focus the input element
  if (m_elemInput)
    m_elemInput->Focus();

  // Flush any pending output and scroll to bottom
  FlushOutput();
}

void RocketConsole::Hide() {
  if (!m_pDocument)
    return;

  m_pDocument->Hide();
  m_bVisible = false;

  if (m_bGrabbingInput) {
    RocketUI()->DenyInputToGame(false, "RocketConsole");
    RocketUI()->SetInputContext(nullptr);
    m_bGrabbingInput = false;
  }

  HideCompletionList();
}

void RocketConsole::Toggle() {
  if (m_bVisible)
    Hide();
  else
    Show();
}

void RocketConsole::Clear() {
  m_OutputBuffer.Purge();
  m_szPendingOutput.clear();

  if (m_elemOutput) {
    m_elemOutput->SetValue("");
  }
}

void RocketConsole::FireGameEvent(IGameEvent *event) {
  const char *name = event->GetName();
  if (V_strcmp(name, "game_newmap") == 0) {
    // Clear console on map change to reduce geometry buffer pressure
    Clear();
  }
}

CUtlString RocketConsole::GetInputValue() const {
  if (m_elemInput) {
    Rml::String val = m_elemInput->GetValue();
    return CUtlString(val.c_str());
  }
  return CUtlString("");
}

void RocketConsole::SetInputValue(const char *text) {
  if (m_elemInput) {
    m_elemInput->SetValue(text ? text : "");
  }
}

void RocketConsole::MoveCursorToEnd() {
  if (m_elemInput) {
    Rml::String val = m_elemInput->GetValue();
    int len = (int)val.size();
    m_elemInput->SetSelectionRange(len, len);
  }
}

void RocketConsole::SetInputValueAndMoveCursorToEnd(const char *text) {
  SetInputValue(text);
  MoveCursorToEnd();
}

// Check if input field has focus
bool RocketConsole::IsInputFocused() const {
  if (m_elemInput && m_pDocument) {
    Rml::Element *focused = m_pDocument->GetFocusLeafNode();
    return (focused == m_elemInput);
  }
  return false;
}

// Key input handler - returns true if consumed
bool RocketConsole::HandleKeyInput(int buttonCode, bool down) {
  if (!m_bVisible)
    return false;

  ButtonCode_t key = (ButtonCode_t)buttonCode;
  bool inputFocused = IsInputFocused();

  // Determine if this is a key we handle (need to consume both down and up)
  bool isHandledKey = (key == KEY_ESCAPE || key == KEY_BACKQUOTE || key == KEY_TAB ||
                       (inputFocused && (key == KEY_ENTER || key == KEY_PAD_ENTER ||
                                         key == KEY_UP || key == KEY_DOWN)));

  // Consume key-up for handled keys, let RmlUi see key-up for others (needed for key repeat)
  if (!down)
    return isHandledKey;

  // Key-down processing
  switch (key) {
  case KEY_ESCAPE:
    if (m_bCompletionVisible)
      HideCompletionList();
    else
      Hide();
    return true;

  case KEY_BACKQUOTE:
    Hide();
    return true;

  case KEY_ENTER:
  case KEY_PAD_ENTER:
    if (!inputFocused)
      return false;
    if (m_bCompletionVisible && m_iCompletionIndex >= 0)
      ApplyCompletion();
    else
      Submit();
    HideCompletionList();
    return true;

  case KEY_TAB:
    if (m_elemInput) {
      m_elemInput->Focus();
      if (m_bCompletionVisible)
        NextCompletion();
      else {
        RebuildCompletionList();
        if (m_CompletionList.Count() > 0) {
          m_iCompletionIndex = 0;
          ShowCompletionList();
        }
      }
    }
    return true;

  case KEY_UP:
    if (!inputFocused)
      return false;
    if (m_bCompletionVisible)
      PrevCompletion();
    else
      HistoryUp();
    return true;

  case KEY_DOWN:
    if (!inputFocused)
      return false;
    if (m_bCompletionVisible)
      NextCompletion();
    else
      HistoryDown();
    return true;

  default:
    return false;
  }
}

// Character input handler - let RmlUi handle most, we just filter backtick
bool RocketConsole::HandleCharInput(wchar_t ch) {
  if (!m_bVisible)
    return false;

  // Block backtick from being typed
  if (ch == '`' || ch == '~')
    return true;

  // Let RmlUi handle normal characters
  return false;
}

void RocketConsole::Submit() {
  CUtlString cmd = GetInputValue();
  if (cmd.Length() == 0)
    return;

  AddToHistory(cmd.Get());

  // Echo
  char echo[1024];
  V_snprintf(echo, sizeof(echo), "] %s\n", cmd.Get());
  ColorPrint(Color(255, 255, 200, 255), echo);

  // Execute
  engine->ClientCmd_Unrestricted(cmd.Get());

  // Clear input
  SetInputValue("");
  m_iHistoryPos = -1;
}

void RocketConsole::AddToHistory(const char *command) {
  if (!command || !command[0])
    return;

  // Don't add duplicates
  if (m_CommandHistory.Count() > 0 &&
      V_strcmp(m_CommandHistory[m_CommandHistory.Count() - 1].Get(), command) ==
          0)
    return;

  m_CommandHistory.AddToTail(CUtlString(command));

  while (m_CommandHistory.Count() > 100)
    m_CommandHistory.Remove(0);
}

void RocketConsole::HistoryUp() {
  if (m_CommandHistory.Count() == 0)
    return;

  if (m_iHistoryPos == -1) {
    m_szSavedInput = GetInputValue();
    m_iHistoryPos = m_CommandHistory.Count() - 1;
  } else if (m_iHistoryPos > 0) {
    m_iHistoryPos--;
  }

  SetInputValueAndMoveCursorToEnd(m_CommandHistory[m_iHistoryPos].Get());
}

void RocketConsole::HistoryDown() {
  if (m_iHistoryPos == -1)
    return;

  m_iHistoryPos++;

  if (m_iHistoryPos >= m_CommandHistory.Count()) {
    m_iHistoryPos = -1;
    SetInputValueAndMoveCursorToEnd(m_szSavedInput.Get());
  } else {
    SetInputValueAndMoveCursorToEnd(m_CommandHistory[m_iHistoryPos].Get());
  }
}

void RocketConsole::RebuildCompletionList() {
  ClearCompletionList();

  CUtlString partial = GetInputValue();
  if (partial.Length() == 0)
    return;

  int partialLen = partial.Length();

  // Check for command-specific completion
  const char *space = V_strstr(partial.Get(), " ");
  if (space) {
    char cmdName[256];
    int cmdLen = space - partial.Get();
    if (cmdLen >= (int)sizeof(cmdName))
      cmdLen = sizeof(cmdName) - 1;
    V_strncpy(cmdName, partial.Get(), cmdLen + 1);

    ConCommand *cmd = g_pCVar->FindCommand(cmdName);
    if (cmd && cmd->CanAutoComplete()) {
      CUtlVector<CUtlString> suggestions;
      cmd->AutoCompleteSuggest(partial.Get(), suggestions);

      for (int i = 0; i < suggestions.Count() && m_CompletionList.Count() < 20;
           i++) {
        CompletionItem item;
        item.name = suggestions[i];
        item.isCommand = true;
        m_CompletionList.AddToTail(item);
      }
      return;
    }
  }

  // Iterate cvars/commands
  ICvar::Iterator iter(g_pCVar);
  for (iter.SetFirst(); iter.IsValid(); iter.Next()) {
    ConCommandBase *cmd = iter.Get();

    if (cmd->IsFlagSet(FCVAR_DEVELOPMENTONLY) || cmd->IsFlagSet(FCVAR_HIDDEN))
      continue;

    const char *name = cmd->GetName();
    if (V_strnicmp(name, partial.Get(), partialLen) != 0)
      continue;

    CompletionItem item;
    item.name = name;
    item.isCommand = cmd->IsCommand();

    if (!item.isCommand) {
      ConVar *var = static_cast<ConVar *>(cmd);
      item.value = var->GetString();
    }

    m_CompletionList.AddToTail(item);

    if (m_CompletionList.Count() >= 20)
      break;
  }
}

void RocketConsole::ClearCompletionList() {
  m_CompletionList.Purge();
  m_iCompletionIndex = -1;
}

void RocketConsole::ShowCompletionList() {
  m_bCompletionVisible = true;

  if (!m_elemCompletionList || !m_pDocument)
    return;

  while (m_elemCompletionList->GetNumChildren() > 0)
    m_elemCompletionList->RemoveChild(m_elemCompletionList->GetFirstChild());

  for (int i = 0; i < m_CompletionList.Count(); i++) {
    Rml::ElementPtr item = m_pDocument->CreateElement("div");
    if (!item)
      continue;

    item->SetClass("completion_item", true);
    if (i == m_iCompletionIndex)
      item->SetClass("selected", true);

    char text[512];
    if (m_CompletionList[i].isCommand)
      V_snprintf(text, sizeof(text), "%s", m_CompletionList[i].name.Get());
    else
      V_snprintf(text, sizeof(text), "%s = %s", m_CompletionList[i].name.Get(),
                 m_CompletionList[i].value.Get());

    item->SetInnerRML(text);
    m_elemCompletionList->AppendChild(std::move(item));
  }

  m_elemCompletionList->SetProperty("display", "block");
}

void RocketConsole::HideCompletionList() {
  m_bCompletionVisible = false;
  ClearCompletionList();

  if (m_elemCompletionList) {
    while (m_elemCompletionList->GetNumChildren() > 0)
      m_elemCompletionList->RemoveChild(m_elemCompletionList->GetFirstChild());
    m_elemCompletionList->SetProperty("display", "none");
  }
}

void RocketConsole::NextCompletion() {
  if (m_CompletionList.Count() == 0)
    return;

  m_iCompletionIndex++;
  if (m_iCompletionIndex >= m_CompletionList.Count())
    m_iCompletionIndex = 0;

  ShowCompletionList();
}

void RocketConsole::PrevCompletion() {
  if (m_CompletionList.Count() == 0)
    return;

  m_iCompletionIndex--;
  if (m_iCompletionIndex < 0)
    m_iCompletionIndex = m_CompletionList.Count() - 1;

  ShowCompletionList();
}

void RocketConsole::ApplyCompletion() {
  if (m_iCompletionIndex < 0 || m_iCompletionIndex >= m_CompletionList.Count())
    return;

  CUtlString newVal = m_CompletionList[m_iCompletionIndex].name;
  if (!m_CompletionList[m_iCompletionIndex].isCommand)
    newVal += " ";

  SetInputValueAndMoveCursorToEnd(newVal.Get());
  HideCompletionList();
}

void RocketConsole::OnInputChange() {
  // Update completions only if already visible (user pressed Tab)
  if (!m_bCompletionVisible)
    return;

  RebuildCompletionList();
  if (m_CompletionList.Count() > 0) {
    m_iCompletionIndex = 0;
    ShowCompletionList();
  } else {
    HideCompletionList();
  }
}

void RocketConsole::ColorPrint(const Color &clr, const char *pMessage) {
  if (!pMessage || !pMessage[0])
    return;

  // Keep buffer for replay on document load (limited size)
  while (m_OutputBuffer.Count() >= MAX_OUTPUT_LINES)
    m_OutputBuffer.Remove(0);

  OutputLine line;
  line.text = pMessage;
  line.color = clr;
  m_OutputBuffer.AddToTail(line);

  AddOutputLine(pMessage, clr);
}

void RocketConsole::Print(const char *pMessage) {
  ColorPrint(m_PrintColor, pMessage);
}

void RocketConsole::DPrint(const char *pMessage) {
  ColorPrint(m_DPrintColor, pMessage);
}

void RocketConsole::GetConsoleText(char *pchText, size_t bufSize) const {
  if (!pchText || bufSize == 0)
    return;

  pchText[0] = '\0';

  if (m_elemOutput) {
    Rml::String text = m_elemOutput->GetValue();
    V_strncpy(pchText, text.c_str(), bufSize);
  } else {
    // Fallback to buffer if document not loaded
    size_t offset = 0;
    for (int i = 0; i < m_OutputBuffer.Count() && offset < bufSize - 1; i++) {
      const char *text = m_OutputBuffer[i].text.Get();
      size_t len = V_strlen(text);
      if (offset + len >= bufSize - 1)
        len = bufSize - 1 - offset;
      V_memcpy(pchText + offset, text, len);
      offset += len;
    }
    pchText[offset] = '\0';
  }
}

void RocketConsole::AddOutputLine(const char *text, const Color & /*color*/) {
  if (!text)
    return;

  // Append to pending output
  m_szPendingOutput += text;

  // Flush immediately if console is visible to ensure output appears
  if (m_bVisible && m_elemOutput) {
    FlushOutput();
  }
}

void RocketConsole::FlushOutput() {
  if (m_szPendingOutput.empty() || !m_elemOutput)
    return;

  // Check if we're already at the bottom before modifying content
  // Only auto-scroll if user is already at the bottom (like VGUI console)
  float scrollTop = m_elemOutput->GetScrollTop();
  float scrollHeight = m_elemOutput->GetScrollHeight();
  float clientHeight = m_elemOutput->GetClientHeight();
  bool wasAtBottom = (scrollTop + clientHeight >= scrollHeight - 1.0f);

  Rml::String current = m_elemOutput->GetValue();
  current += m_szPendingOutput;
  m_szPendingOutput.clear();

  // Truncate if too large (keep last ~200KB)
  if (current.size() > MAX_OUTPUT_SIZE) {
    size_t removeBytes = current.size() - MAX_OUTPUT_SIZE;
    // Find next newline after removeBytes to cut cleanly
    size_t cutPos = current.find('\n', removeBytes);
    if (cutPos != Rml::String::npos)
      current.erase(0, cutPos + 1);
    else
      current.erase(0, removeBytes);
  }

  m_elemOutput->SetValue(current);

  // Only scroll to bottom if user was already at the bottom
  if (wasAtBottom) {
    ScrollToBottom();
  }
}

void RocketConsole::ScrollToBottom() {
  if (m_elemOutput) {
    // Scroll textarea to bottom
    m_elemOutput->SetScrollTop(m_elemOutput->GetScrollHeight());
  }
}
