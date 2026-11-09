// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include "rocketui/rmlui.h"
#include "rkconsole.h"

#include "rkhud_model.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"

#include "cdll_int.h"
#include "inputsystem/ButtonCode.h"
#include "inputsystem/iinputsystem.h"

#include <rocketui/rmlui.h>

// hud_console.rml reports edits to the input here (data-event-change) so the
// completion list can follow what is being typed.
static void BindConsole(Rml::DataModelConstructor &c) {
  c.BindEventCallback("console_input_changed",
                      [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) {
                        RkConsole().OnInputChange();
                      });
}
RK_HUD_SECTION(BindConsole);

static RocketConsole s_RocketConsole;

RocketConsole &RkConsole() { return s_RocketConsole; }

ConVar rocket_console_enabled("rocket_console_enabled", "1", FCVAR_ARCHIVE,
                              "Enable RocketUI console");

static void rocket_console_show( const CCommand &args );
static ConCommand rocket_console_show_command( "rocket_console_show", rocket_console_show, "Show the RocketUI console" );
static void rocket_console_show( const CCommand &args ) {
  RkConsole().Show();
}

static void rocket_console_hide( const CCommand &args );
static ConCommand rocket_console_hide_command( "rocket_console_hide", rocket_console_hide, "Hide the RocketUI console" );
static void rocket_console_hide( const CCommand &args ) {
  RkConsole().Hide();
}

static void rocket_console_toggle( const CCommand &args );
static ConCommand rocket_console_toggle_command( "rocket_console_toggle", rocket_console_toggle, "Toggle the RocketUI console" );
static void rocket_console_toggle( const CCommand &args ) {
  RkConsole().Toggle();
}

static void rocket_console_clear( const CCommand &args );
static ConCommand rocket_console_clear_command( "rocket_console_clear", rocket_console_clear, "Clear the RocketUI console" );
static void rocket_console_clear( const CCommand &args ) {
  RkConsole().Clear();
}

RocketConsole::RocketConsole()
    : m_pDocument(nullptr), m_elemOutput(nullptr), m_elemInput(nullptr),
      m_elemCompletionList(nullptr), m_bVisible(false),
      m_bInitialized(false), m_iHistoryPos(-1), m_iCompletionIndex(-1),
      m_bCompletionVisible(false), m_PrintColor(216, 222, 211, 255),
      m_DPrintColor(196, 181, 80, 255) {}

RocketConsole::~RocketConsole() { Shutdown(); }

// First refusal on input, before RmlUi sees it, installed for the lifetime of the
// console: the console owns its toggle key outright and swallows the keys it edits
// with. ESC is deliberately not here -- the engine consumes it before RocketUI is
// asked (keys.cpp -> HandleGameUIKey -> gameui_activate/gameui_hide), and
// CGameUI::OnGameUIActivated/Hidden is what raises and drops the pause menu.
static bool RocketUI_HandleInput(const InputEvent_t &event) {
  const ButtonCode_t key = (ButtonCode_t)event.m_nData;
  const bool bKeyboard = !IsMouseCode(key);

  switch (event.m_nType) {
  case IE_ButtonPressed:
  case IE_ButtonDoubleClicked:
  case IE_KeyCodeTyped: // repeat
    if (bKeyboard && RkConsole().HandleKeyInput(key, true))
      return true;
    break;

  case IE_ButtonReleased:
    if (bKeyboard && RkConsole().HandleKeyInput(key, false))
      return true;
    break;

  case IE_KeyTyped:
    if (RkConsole().HandleCharInput((wchar_t)event.m_nData))
      return true;
    break;

  default:
    break;
  }

  return false;
}

void RocketConsole::Initialize() {
  if (m_bInitialized)
    return;

  if (g_pCVar)
    g_pCVar->InstallConsoleDisplayFunc(this, ConsoleDisplayFn_t(std::cw<&RocketConsole::Display>, this));

  RocketUI()->SetInputHook(RocketUI_HandleInput);

  m_bInitialized = true;
}

void RocketConsole::Shutdown() {
  if (!m_bInitialized)
    return;

  UnloadDocument();

  RocketUI()->SetInputHook(nullptr);

  if (g_pCVar)
    g_pCVar->RemoveConsoleDisplayFunc(this);

  m_CommandHistory.clear(); m_CommandHistory.shrink_to_fit();
  m_CompletionList.clear(); m_CompletionList.shrink_to_fit();
  m_OutputBuffer.clear(); m_OutputBuffer.shrink_to_fit();

  m_bInitialized = false;
}

void RocketConsole::LoadCallback() { RkConsole().LoadDocument(); }

void RocketConsole::UnloadCallback() { RkConsole().UnloadDocument(); }

void RocketConsole::LoadDocument() {
  if (m_pDocument)
    return;

  m_pDocument = RkLoadDocument(ROCKET_CONTEXT_MENU, "hud_console.rml", LoadCallback,
                               UnloadCallback);
  if (!m_pDocument)
    return;

  // Get the textarea output element
  if (Rml::Element *outputElem = m_pDocument->GetElementById("console_output")) {
    m_elemOutput = rmlui_dynamic_cast<Rml::ElementFormControlTextArea *>(outputElem);
  }

  m_elemCompletionList = m_pDocument->GetElementById("completion_list");

  // Get the native input element
  if (Rml::Element *inputElem = m_pDocument->GetElementById("console_input")) {
    m_elemInput = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(inputElem);
  }

  // Replay buffered output to the textarea
  if (m_elemOutput && std::ssize( m_OutputBuffer ) > 0) {
    Rml::String allText;
    for (int i = 0; i < std::ssize( m_OutputBuffer ); i++) {
      allText += m_OutputBuffer[i].text.data();
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

  // Paint order is hud_console.rml's `z-index`, not this call: RmlUi only reorders
  // documents as a side effect of focus changing.
  m_pDocument->Show( Rml::ModalFlag::None, Rml::FocusFlag::Auto, Rml::ScrollFlag::None );
  m_bVisible = true;

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

  HideCompletionList();
}

void RocketConsole::Toggle() {
  if (m_bVisible)
    Hide();
  else
    Show();
}

void RocketConsole::Clear() {
  m_OutputBuffer.clear(); m_OutputBuffer.shrink_to_fit();
  m_szPendingOutput.clear();

  if (m_elemOutput) {
    m_elemOutput->SetValue("");
  }
}

std::string RocketConsole::GetInputValue() const {
  if (m_elemInput) {
    Rml::String val = m_elemInput->GetValue();
    return std::string(val.c_str());
  }
  return std::string("");
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

  // Note: the toggle key is deliberately not handled here. It is a normal engine
  // binding (`toggleconsole`), so it stays rebindable and has exactly one owner.
  bool isHandledKey = (key == KEY_ESCAPE || key == KEY_TAB ||
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
        if (!m_CompletionList.empty()) {
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
  std::string cmd = GetInputValue();
  if (std::ssize( cmd ) == 0)
    return;

  AddToHistory(cmd.data());

  // Echo
  char echo[1024];
  V_snprintf(echo, sizeof(echo), "] %s\n", cmd.data());
  ColorPrint(Color(255, 255, 200, 255), echo);

  // Execute
  engine->ClientCmd_Unrestricted(cmd.data());

  // Clear input
  SetInputValue("");
  m_iHistoryPos = -1;
}

void RocketConsole::AddToHistory(const char *command) {
  if (!command || !command[0])
    return;

  // Don't add duplicates
  if (!m_CommandHistory.empty() &&
      V_strcmp(m_CommandHistory[std::ssize( m_CommandHistory ) - 1].data(), command) ==
          0)
    return;

  m_CommandHistory.push_back( std::string(ks::nz( command )) );

  while (std::ssize( m_CommandHistory ) > 100)
    m_CommandHistory.erase( m_CommandHistory.begin() + 0 );
}

void RocketConsole::HistoryUp() {
  if (m_CommandHistory.empty())
    return;

  if (m_iHistoryPos == -1) {
    m_szSavedInput = GetInputValue();
    m_iHistoryPos = std::ssize( m_CommandHistory ) - 1;
  } else if (m_iHistoryPos > 0) {
    m_iHistoryPos--;
  }

  SetInputValueAndMoveCursorToEnd(m_CommandHistory[m_iHistoryPos].data());
}

void RocketConsole::HistoryDown() {
  if (m_iHistoryPos == -1)
    return;

  m_iHistoryPos++;

  if (m_iHistoryPos >= std::ssize( m_CommandHistory )) {
    m_iHistoryPos = -1;
    SetInputValueAndMoveCursorToEnd(m_szSavedInput.data());
  } else {
    SetInputValueAndMoveCursorToEnd(m_CommandHistory[m_iHistoryPos].data());
  }
}

void RocketConsole::RebuildCompletionList() {
  ClearCompletionList();

  std::string partial = GetInputValue();
  if (std::ssize( partial ) == 0)
    return;

  int partialLen = std::ssize( partial );

  // Check for command-specific completion
  const char *space = V_strstr(partial.data(), " ");
  if (space) {
    char cmdName[256];
    int cmdLen = space - partial.data();
    if (cmdLen >= (int)sizeof(cmdName))
      cmdLen = sizeof(cmdName) - 1;
    V_strncpy(cmdName, partial.data(), cmdLen + 1);

    ConCommand *cmd = g_pCVar->FindCommand(cmdName);
    if (cmd && cmd->CanAutoComplete()) {
      std::vector< std::string > suggestions;
      cmd->AutoCompleteSuggest(partial.data(), suggestions);

      for (int i = 0; i < std::ssize( suggestions ) && std::ssize( m_CompletionList ) < 20;
           i++) {
        CompletionItem item;
        item.name = suggestions[i];
        item.isCommand = true;
        m_CompletionList.push_back( item );
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
    if (V_strnicmp(name, partial.data(), partialLen) != 0)
      continue;

    CompletionItem item;
    item.name = ks::nz( name );
    item.isCommand = cmd->IsCommand();

    if (!item.isCommand) {
      ConVar *var = static_cast<ConVar *>(cmd);
      item.value = var->GetString();
    }

    m_CompletionList.push_back( item );

    if (std::ssize( m_CompletionList ) >= 20)
      break;
  }
}

void RocketConsole::ClearCompletionList() {
  m_CompletionList.clear(); m_CompletionList.shrink_to_fit();
  m_iCompletionIndex = -1;
}

void RocketConsole::ShowCompletionList() {
  m_bCompletionVisible = true;

  if (!m_elemCompletionList || !m_pDocument)
    return;

  while (m_elemCompletionList->GetNumChildren() > 0)
    m_elemCompletionList->RemoveChild(m_elemCompletionList->GetFirstChild());

  for (int i = 0; i < std::ssize( m_CompletionList ); i++) {
    Rml::ElementPtr item = m_pDocument->CreateElement("div");
    if (!item)
      continue;

    item->SetClass("completion_item", true);
    if (i == m_iCompletionIndex)
      item->SetClass("selected", true);

    char text[512];
    if (m_CompletionList[i].isCommand)
      V_snprintf(text, sizeof(text), "%s", m_CompletionList[i].name.data());
    else
      V_snprintf(text, sizeof(text), "%s = %s", m_CompletionList[i].name.data(),
                 m_CompletionList[i].value.data());

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
  if (m_CompletionList.empty())
    return;

  m_iCompletionIndex++;
  if (m_iCompletionIndex >= std::ssize( m_CompletionList ))
    m_iCompletionIndex = 0;

  ShowCompletionList();
}

void RocketConsole::PrevCompletion() {
  if (m_CompletionList.empty())
    return;

  m_iCompletionIndex--;
  if (m_iCompletionIndex < 0)
    m_iCompletionIndex = std::ssize( m_CompletionList ) - 1;

  ShowCompletionList();
}

void RocketConsole::ApplyCompletion() {
  if (m_iCompletionIndex < 0 || m_iCompletionIndex >= std::ssize( m_CompletionList ))
    return;

  std::string newVal = m_CompletionList[m_iCompletionIndex].name;
  if (!m_CompletionList[m_iCompletionIndex].isCommand)
    newVal += " ";

  SetInputValueAndMoveCursorToEnd(newVal.data());
  HideCompletionList();
}

void RocketConsole::OnInputChange() {
  // Update completions only if already visible (user pressed Tab)
  if (!m_bCompletionVisible)
    return;

  RebuildCompletionList();
  if (!m_CompletionList.empty()) {
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
  while (std::ssize( m_OutputBuffer ) >= MAX_OUTPUT_LINES)
    m_OutputBuffer.erase( m_OutputBuffer.begin() + 0 );

  OutputLine line;
  line.text = ks::nz( pMessage );
  line.color = clr;
  m_OutputBuffer.push_back( line );

  AddOutputLine(pMessage, clr);
}

void RocketConsole::Display(ConsoleDisplayOp_t op, const Color &clr, const char *pMessage) {
  switch (op) {
  case ConsoleDisplayOp_t::ColorPrint:
    ColorPrint(clr, pMessage);
    break;
  case ConsoleDisplayOp_t::Print:
    Print(pMessage);
    break;
  case ConsoleDisplayOp_t::DPrint:
    DPrint(pMessage);
    break;
  }
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
    for (int i = 0; i < std::ssize( m_OutputBuffer ) && offset < bufSize - 1; i++) {
      const char *text = m_OutputBuffer[i].text.c_str();
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
