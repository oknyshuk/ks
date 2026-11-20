// SPDX-License-Identifier: MIT
// Copyright (c) 2026-2026 oknyshuk

module;
#include <stddef.h>
#include <rocketui/rmlui.h>
#include <string.h>
#include <wchar.h>
#include <math.h>
#include <rocketui/rocketui.h>
#include <stdio.h>
#include <stdlib.h>
#include "inputsystem/ButtonCode.h"
#include "rocketui.h"
#include "rkhud_model.h"
#include "igameevents.h"
#include "../../hud_element_helper.h"
#include "localize/ilocalize.h"
#include "usermessages_schema.h"
#include "../../hud_macros.h"
#include "edict.h"
#include "client_class.h"
#include "igameresources.h"
#include "IGameUIFuncs.h"
#include "materialsystem/materialsystem_config.h"
#include "modes.h"
#include "filesystem.h"
module ks.client;

import std;
import ks.core.color;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.cvar;
import ks.tier1.iconvar;
import ks.tier1.icvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.string_ops;
import ks.tier1.strtools;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :sharedvar;
import :util;
import :cstrike15.RocketUI.rkconsole;
import :precache;
import :achievements_and_stats_interface;
import :baseentity;
import ks.game.gamemovement;
import :particlemgr;
import :noopanimcontroller;
import ks.core.threadtools;
import :cstrike15.cs_player;
import :cstrike15.cs_shareddefs;
import :cstrike15.cs_player_shared;
import :econ.econ_item_schema_minimal;
import ks.core.dbg;
import ks.core.vprof;
import ks.game.shareddefs;
import ks.core.basetypes;
import ks.core.types;
import ks.math.mathlib;
import ks.math.vector;
import ks.math.vector2d;
import ks.math.transform;
import ks.game.in_buttons;

using namespace ::ks::reflect;

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

ConVar rocket_console_enabled(side_registry(), "rocket_console_enabled", "1", FCVAR_ARCHIVE,
                              "Enable RocketUI console");

static void rocket_console_show( const CCommand &args );
static ConCommand rocket_console_show_command( side_registry(), "rocket_console_show", rocket_console_show, "Show the RocketUI console" );
static void rocket_console_show( const CCommand &args ) {
  RkConsole().Show();
}

static void rocket_console_hide( const CCommand &args );
static ConCommand rocket_console_hide_command( side_registry(), "rocket_console_hide", rocket_console_hide, "Hide the RocketUI console" );
static void rocket_console_hide( const CCommand &args ) {
  RkConsole().Hide();
}

static void rocket_console_toggle( const CCommand &args );
static ConCommand rocket_console_toggle_command( side_registry(), "rocket_console_toggle", rocket_console_toggle, "Toggle the RocketUI console" );
static void rocket_console_toggle( const CCommand &args ) {
  RkConsole().Toggle();
}

static void rocket_console_clear( const CCommand &args );
static ConCommand rocket_console_clear_command( side_registry(), "rocket_console_clear", rocket_console_clear, "Clear the RocketUI console" );
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

class CCSWeaponInfo;
class CHudElement;
class CBasePlayer;
class CCSPlayer;

using namespace ::ks::reflect;

DECLARE_HUDELEMENT( RkHudBuyMenu );

const char *RkHudBuyMenu::kDocument = "hud_buymenu.rml";

struct ItemListEntry
{
    Rml::String itemName;
    int itemPrice;

    bool operator==( const ItemListEntry & ) const = default;
};

// struct layout for data-binding model.
struct BuyMenuData
{
    int playerCash;
    int playerTeamNum;
    int buyTimeLeft; // seconds
    Rml::Vector<ItemListEntry> gearList;
    Rml::Vector<ItemListEntry> grenadeList;
    Rml::Vector<ItemListEntry> pistolList;
    Rml::Vector<ItemListEntry> rifleList;
    Rml::Vector<ItemListEntry> smgList;
    Rml::Vector<ItemListEntry> heavyList;

    bool operator==( const BuyMenuData & ) const = default;
} buyMenuData;

// Buying and closing are driven from the document (data-event-click / -keyup in
// hud_buymenu.rml). The click used to arrive as a bubbled mousedown on some inner
// span, from which this had to walk up to the .item container, QuerySelector the
// .item_name, read its inner RML back out and re-parse the weapon name -- all to
// recover the string the data model handed the document in the first place. The
// document now passes it straight back.
static void BuyItem( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments )
{
    if ( arguments.empty() )
        return;

    const Rml::String itemName = arguments[0].Get<Rml::String>();

    // Names arrive as WEAPON_AK47 / EQUIPMENT_KEVLAR; `buy` wants the tail.
    const char *underscore = V_strstr( itemName.c_str(), "_" );
    if ( !underscore )
        return;

    char command[512];
    V_snprintf( command, sizeof( command ), "buy %s", underscore + 1 );
    engine->ExecuteClientCmd( command );

    if ( RkHudBuyMenu *pBuyMenu = GET_HUDELEMENT( RkHudBuyMenu ) )
        pBuyMenu->UpdateBuyMenu();
}

// Any key closes the menu, the same clientside event the open path uses.
static void CloseBuyMenu( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & )
{
    CBasePlayer *localPlayer = CBasePlayer::GetLocalPlayer();
    IGameEvent *pEvent = gameeventmanager->CreateEvent( "buymenu_close" );
    if ( localPlayer && pEvent )
    {
        pEvent->SetInt( "userid", localPlayer->GetUserID() );
        gameeventmanager->FireEventClientSide( pEvent );
    }
}

static void BindBuyMenu( Rml::DataModelConstructor &c )
{
    if ( auto entry = c.RegisterStruct<ItemListEntry>() )
    {
        entry.RegisterMember( "item_name", &ItemListEntry::itemName );
        entry.RegisterMember( "item_price", &ItemListEntry::itemPrice );
    }
    c.RegisterArray<Rml::Vector<ItemListEntry>>();

    if ( auto h = c.RegisterStruct<BuyMenuData>() )
    {
        h.RegisterMember( "player_cash", &BuyMenuData::playerCash );
        h.RegisterMember( "player_teamnum", &BuyMenuData::playerTeamNum );
        h.RegisterMember( "buy_time_left", &BuyMenuData::buyTimeLeft );
        h.RegisterMember( "gear_list", &BuyMenuData::gearList );
        h.RegisterMember( "grenade_list", &BuyMenuData::grenadeList );
        h.RegisterMember( "pistol_list", &BuyMenuData::pistolList );
        h.RegisterMember( "rifle_list", &BuyMenuData::rifleList );
        h.RegisterMember( "smg_list", &BuyMenuData::smgList );
        h.RegisterMember( "heavy_list", &BuyMenuData::heavyList );
    }
    c.Bind( "buymenu", &buyMenuData );

    c.BindEventCallback( "buy_item", &BuyItem );
    c.BindEventCallback( "close_buymenu", &CloseBuyMenu );
}
RK_HUD_SECTION( BindBuyMenu );

// Updates the buy menu options via the game's weapon database.
// Called sparingly when buymenu is opened/interacted.
void RkHudBuyMenu::UpdateBuyMenu()
{
    CCSPlayer *localPlayer = CCSPlayer::GetLocalCSPlayer();
    if( !localPlayer )
        return;

    buyMenuData.gearList.clear();
    buyMenuData.grenadeList.clear();
    buyMenuData.pistolList.clear();
    buyMenuData.rifleList.clear();
    buyMenuData.smgList.clear();
    buyMenuData.heavyList.clear();

    for( int i = WEAPON_FIRST ; i < WEAPON_MAX; i++ )
    {
        if( localPlayer->CanAcquire( (CSWeaponID)i, AcquireMethod::Buy ) != AcquireResult::Allowed )
            continue;

        const CCSWeaponInfo* info = GetWeaponInfo( (CSWeaponID)i );
        if( !info )
            continue;

        ItemListEntry entry;
        entry.itemPrice = info->GetWeaponPrice();
        entry.itemName = WeaponIdAsString( (CSWeaponID)i );

        int weaponType = info->GetWeaponType();

        switch( weaponType )
        {
            case WEAPONTYPE_EQUIPMENT:
                buyMenuData.gearList.push_back( entry );
                break;
            case WEAPONTYPE_GRENADE:
                buyMenuData.grenadeList.push_back( entry );
                break;
            case WEAPONTYPE_PISTOL:
                buyMenuData.pistolList.push_back( entry );
                break;
            case WEAPONTYPE_RIFLE:
            case WEAPONTYPE_SNIPER_RIFLE:
                buyMenuData.rifleList.push_back( entry );
                break;
            case WEAPONTYPE_SUBMACHINEGUN:
                buyMenuData.smgList.push_back( entry );
                break;
            case WEAPONTYPE_SHOTGUN:
            case WEAPONTYPE_MACHINEGUN:
                buyMenuData.heavyList.push_back( entry );
                break;
            default:
            {
                continue;
            }
        }
    }

    RkHudDirty( "buymenu" );
}

// called 1x per frame
void RkHudBuyMenu::OnNewFrameBuyMenu()
{
    const BuyMenuData previous = buyMenuData;

    float timeIntoRound = CSGameRules()->GetRoundElapsedTime();
    float buyTime = CSGameRules()->GetBuyTimeLength();

    buyMenuData.buyTimeLeft = int(buyTime - timeIntoRound);

    CCSPlayer *localPlayer = CCSPlayer::GetLocalCSPlayer();
    if( localPlayer )
    {
        buyMenuData.playerCash = localPlayer->GetAccount();
        buyMenuData.playerTeamNum = localPlayer->GetTeamNumber();
    }

    // Cash and the buy timer change once a second at most.
    if( !( buyMenuData == previous ) )
        RkHudDirty( "buymenu" );
}

RkHudBuyMenu::RkHudBuyMenu(const char *value) : RkHudDocument( value )
{
    SetHiddenBits( /* HIDEHUD_MISCSTATUS */ 0 );
}

RkHudBuyMenu::~RkHudBuyMenu() noexcept
{
    StopListeningForAllEvents();

    Unload();
}

void RkHudBuyMenu::LevelInit()
{
    ListenForGameEvent( "buymenu_open" );
    ListenForGameEvent( "buymenu_close" );

    RkHudDocument::LevelInit();
}

void RkHudBuyMenu::ShowPanel(bool bShow, bool force)
{
    if( !m_pDocument )
        return;

    if( bShow )
    {
        if( !m_bVisible )
        {
            UpdateBuyMenu();
            m_pDocument->Show( Rml::ModalFlag::None, Rml::FocusFlag::Auto, Rml::ScrollFlag::None );
        }
        OnNewFrameBuyMenu();
    }
    else
    {
        if( m_bVisible )
        {
            m_pDocument->Hide();
        }
    }

    m_bVisible = bShow;
}

void RkHudBuyMenu::SetActive(bool bActive)
{
    ShowPanel( bActive, false );
    CHudElement::SetActive( bActive );
}

bool RkHudBuyMenu::ShouldDraw()
{
    // This element is opened/closed by clientside events
    // that we listen for and set m_bVisible manually via showpanel(true)

    return cl_drawhud.GetBool() && CSGameRules() && !CSGameRules()->IsBuyTimeElapsed() && m_bVisible && CHudElement::ShouldDraw();
}

void RkHudBuyMenu::FireGameEvent(IGameEvent *event)
{
    const char *type = event->GetName();

    if( !V_strcmp( "buymenu_open", type ) )
    {
        ShowPanel( true, false );
    }
    else if( !V_strcmp( "buymenu_close", type ) )
    {
        ShowPanel( false, false );
    }
}

class CHudElement;
class CBasePlayer;

using namespace ::ks::reflect;

DECLARE_HUDELEMENT( RkHudChat );

const char *RkHudChat::kDocument = "hud_chat.rml";

ConVar rocket_hud_chat_idle_opacity( side_registry(), "rocket_hud_chat_idle_opacity", "0.2", FCVAR_ARCHIVE, "The Opacity of the Chat while it is not active" );
ConVar rocket_hud_chat_active_opacity( side_registry(), "rocket_hud_chat_active_opacity", "0.7", FCVAR_ARCHIVE, "The Opacity of the Chat while typing/new message" );
ConVar rocket_hud_chat_max_entries( side_registry(), "rocket_hud_chat_max_entries", "1000", FCVAR_ARCHIVE, "Chat History Length" );

static void rocket_hud_chat_clear( const CCommand &args );
static ConCommand rocket_hud_chat_clear_command( side_registry(), "rocket_hud_chat_clear", rocket_hud_chat_clear, "Clears the Chat History", FCVAR_NONE );
static void rocket_hud_chat_clear( const CCommand &args )
{
    RkHudChat* pChat = GET_HUDELEMENT( RkHudChat );
    if( !pChat )
        return;
    pChat->ClearChatHistory();
}

static bool __MsgFunc_SayText2( const ks::net::CCSUsrMsg_SayText2 &msg )
{
    int paramSize = msg.params.size();
    int entID = msg.ent_idx;

    RkHudChat* pChat = GET_HUDELEMENT( RkHudChat );

    if( !pChat )
        return true;

    // from server
    if( entID == 0 && paramSize > 1 )
    {
        pChat->AddChatString( nullptr, msg.params[ 1 ].c_str(), RkHudChat::SERVER );
        return true;
    }

    CBasePlayer *speaker = UTIL_PlayerByIndex( entID );
    CBasePlayer *localPlayer = CBasePlayer::GetLocalPlayer( );

    if( !speaker || !localPlayer )
        return true;

    // params(0) = Name of player
    // params(1) = Message
    if( paramSize > 1 )
    {
        if( (speaker->GetTeamNumber() == localPlayer->GetTeamNumber()) )
            pChat->AddChatString( msg.params[ 0 ].c_str(), msg.params[ 1 ].c_str(),RkHudChat::FRIEND);
        else
            pChat->AddChatString( msg.params[ 0 ].c_str(), msg.params[ 1 ].c_str(),RkHudChat::FOE);
    }

    return true;
}
// converts all '\r' characters to '\n', so that the engine can deal with the properly
// returns a pointer to str
static void NullLastNewlineFromString( char *str )
{
    int s = strlen( str ) - 1;
    if ( s >= 0 )
    {
        if ( str[s] == '\n' || str[s] == '\r' )
            str[s] = 0;
    }
}

//-----------------------------------------------------------------------------
// Message handler for text messages
// displays a string, looking them up from the titles.txt file, which can be localised
// parameters:
//   byte:   message destination  ( HUD_PRINTCONSOLE, HUD_PRINTNOTIFY, HUD_PRINTCENTER, HUD_PRINTTALK )
//   string: message
// optional parameters:
//   string: message parameter 1
//   string: message parameter 2
//   string: message parameter 3
//   string: message parameter 4
// any string that starts with the character '#' is a message name, and is used to look up the real message in titles.txt
// the next ( optional) one to four strings are parameters for that string ( which can also be message names if they begin with '#')
//-----------------------------------------------------------------------------
static bool __MsgFunc_TextMsg( const ks::net::CCSUsrMsg_TextMsg &msg )
{
    char szString[2048] = {0};
    int dest = msg.msg_dst;

    wchar_t szBuf[5][256] = {};
    wchar_t outputBuf[256] = {};

    if( !cl_showtextmsg.GetBool() )
        return true;

    for( int i = 0; i < 4; i++ )
    {
        // Allow localizing player names
        if ( const char *pszEntIndex = StringAfterPrefix( msg.params[ i ].c_str(), "#ENTNAME[" ) )
        {
            int iEntIndex = V_atoi( pszEntIndex );
            wchar_t wszPlayerName[MAX_DECORATED_PLAYER_NAME_LENGTH] = {};
            if ( CCS_PlayerResource *pCSPR = ( CCS_PlayerResource* ) GameResources() )
            {
                pCSPR->GetDecoratedPlayerName( iEntIndex, wszPlayerName, sizeof( wszPlayerName ), ( EDecoratedPlayerNameFlag_t ) ( k_EDecoratedPlayerNameFlag_DontUseNameOfControllingPlayer | k_EDecoratedPlayerNameFlag_DontUseAssassinationTargetName ) );
            }
            if ( wszPlayerName[0] )
            {
                szString[0] = 0;
                V_wcscpy_safe( szBuf[ i ], wszPlayerName );
            }
            else if ( const char *pszEndBracket = V_strnchr( pszEntIndex, ']', 64 ) )
            {
                V_strcpy_safe( szString, pszEndBracket + 1 );
            }
            else
            {
                V_strcpy_safe( szString, msg.params[ i ].c_str() );
            }
        }
        else
        {
            V_strcpy_safe( szString, msg.params[ i ].c_str() );
        }

        if( szString[0] )
        {
            static char tmpStrBuf[1024];
            V_strncpy( tmpStrBuf, hudtextmessage->LookupString( szString, &dest ), sizeof(tmpStrBuf) );
            bool bTranslated = false;
            if( tmpStrBuf[0] == '#' ) // only translate parameters intended as localization tokens
            {
                const wchar_t *pBuf = g_pLocalize->Find( tmpStrBuf );
                if( pBuf )
                {
                    // copy pBuf into szBuf[i]
                    int nMaxChars = sizeof( szBuf[ i ] ) / sizeof( wchar_t );
                    wcsncpy( szBuf[ i ], pBuf, nMaxChars );
                    szBuf[ i ][ nMaxChars - 1 ] = 0;
                    bTranslated = true;
                }
            }

            if( !bTranslated )
            {
                if( i > 0 )
                {
                    NullLastNewlineFromString( tmpStrBuf );  // these strings are meant for substitution into the main strings, so cull the automatic end newlines
                }
                g_pLocalize->ConvertANSIToUnicode( tmpStrBuf, szBuf[ i ], sizeof( szBuf[ i ] ) );
            }
        }
    }

    //TODO: right now this is just ascii
    int len;
    RkHudChat* pChat;
    g_pLocalize->ConstructString( outputBuf, sizeof( outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
    g_pLocalize->ConvertUnicodeToANSI( outputBuf, szString, sizeof( szString) );
    len = strlen( szString );
    if ( len && szString[len-1] != '\n' && szString[len-1] != '\r' )
    {
        V_strncat( szString, "\n", sizeof( szString), 1 );
    }

    switch( dest )
    {
        case HUD_PRINTCENTER:
            // Center popup with RocketUI that deletes itself after 4.5 seconds
            new RocketPopupDocument( ConvertCRtoNL( szString ), 4.5f );
            break;
        case HUD_PRINTTALK:
            pChat = GET_HUDELEMENT( RkHudChat );
            if( !pChat )
                return true;

            pChat->AddChatString( nullptr, ConvertCRtoNL( szString ), RkHudChat::SERVER );
            break;
        case HUD_PRINTNOTIFY:
        case HUD_PRINTCONSOLE:
            Msg( "%s", ConvertCRtoNL( szString ) );
            break;
    }

    return true;
}

// hud_chat.rml routes its input's keydown here (data-event-keydown). Enter sends,
// escape drops out of message mode; every other key belongs to the text field.
// One line of chat. The sender only picks a colour, so it travels as the class
// name hud_chat.rml should put on the username.
struct ChatLine
{
    Rml::String username;
    Rml::String message;
    Rml::String sender_class;

    bool operator==( const ChatLine & ) const = default;
};
struct ChatData
{
    Rml::Vector<ChatLine> lines;

    bool operator==( const ChatData & ) const = default;
} chatData;

static void BindChat( Rml::DataModelConstructor &c )
{
    if ( auto line = c.RegisterStruct<ChatLine>() )
    {
        line.RegisterMember( "username", &ChatLine::username );
        line.RegisterMember( "message", &ChatLine::message );
        line.RegisterMember( "sender_class", &ChatLine::sender_class );
    }
    c.RegisterArray<Rml::Vector<ChatLine>>();

    if ( auto h = c.RegisterStruct<ChatData>() )
        h.RegisterMember( "lines", &ChatData::lines );
    c.Bind( "chat", &chatData );

    c.BindEventCallback( "chat_key", []( Rml::DataModelHandle, Rml::Event &ev, const Rml::VariantList & ) {
        RkHudChat *pChat = GET_HUDELEMENT( RkHudChat );
        if ( !pChat || !pChat->m_elemChatInput )
            return;

        const auto key = (Rml::Input::KeyIdentifier)ev.GetParameter<int>( "key_identifier", 0 );

        if ( key == Rml::Input::KI_RETURN )
        {
            auto *input = static_cast<Rml::ElementFormControl *>( pChat->m_elemChatInput );

            char sayBuffer[1024];
            V_snprintf( sayBuffer, sizeof( sayBuffer ), "%s \"%s\"",
                        pChat->GetMessageMode() == MM_SAY ? "say" : "say_team",
                        input->GetValue().c_str() );
            engine->ClientCmd_Unrestricted( sayBuffer );
            input->SetValue( "" );
        }
        else if ( key != Rml::Input::KI_ESCAPE )
        {
            return;
        }

        pChat->StopMessageMode();
    } );
}
RK_HUD_SECTION( BindChat );

// Called on program startup by a Macro with the HUD UI system

void RkHudChat::OnLoad()
{
    m_elemChatInput = m_pDocument->GetElementById( "chat_input" );
    if ( !m_elemChatInput )
    {
        Warning( "hud_chat.rml is missing 'chat_input'\n" );
        return;
    }

    // The chat is always on screen; only its opacity and input focus change.
    m_pDocument->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
}

void RkHudChat::OnUnload()
{
    if ( m_elemChatInput )
    m_elemChatInput = nullptr;
}

// HudElementHelper::CreateAllElements
RkHudChat::RkHudChat(const char *value) : RkHudDocument( value ),
    m_iMode( MM_NONE )
{
    SetHiddenBits( /* HIDEHUD_MISCSTATUS */ 0 );
}

// Chat's document is visible during normal gameplay (it draws chat history), so
// only actually typing counts as owning input.
RkHudChat::~RkHudChat() noexcept
{
    Unload();
}

void RkHudChat::LevelInit()
{
    HOOK_MESSAGE( SayText2 );
    HOOK_MESSAGE( TextMsg );

    RkHudDocument::LevelInit();
}

void RkHudChat::LevelShutdown()
{
    m_iMode = MM_NONE;

    RkHudDocument::LevelShutdown();
}

// this is called every frame, keep that in mind.
void RkHudChat::ShowPanel(bool bShow, bool force)
{
    if( !m_pDocument )
        return;

    if( bShow )
    {
        m_pDocument->SetProperty( "opacity", std::to_string(rocket_hud_chat_active_opacity.GetFloat()) );
    }
    else
    {
        // Do a fade out to the idle opacity level.
        float currentOpacity = m_pDocument->GetProperty("opacity")->Get<float>();
        if( currentOpacity > rocket_hud_chat_idle_opacity.GetFloat() )
            m_pDocument->SetProperty("opacity", std::to_string(currentOpacity - 0.0075f) );
    }
}

// Called every Frame by the hudsystem if ShouldDraw()
void RkHudChat::SetActive( bool bActive )
{
    ShowPanel( bActive, false );
    CHudElement::SetActive( bActive );
}

bool RkHudChat::ShouldDraw( void )
{
    if ( //IsTakingAFreezecamScreenshot() ||
            (CSGameRules() && CSGameRules()->IsPlayingTraining()) )
        return false;

    return cl_drawhud.GetBool() && (m_iMode != MM_NONE ) && CHudElement::ShouldDraw();
}

void RkHudChat::StartMessageMode( int mode )
{
    // Already in chat mode.
    if( ChatRaised() )
        return;
    
    if( GetHud().HudDisabled() )
        return;

    m_iMode = mode;

    // Claim the mouse for as long as we are typing (hud_chat.rml is otherwise
    // `pointer-events: none`, so it does not).
    m_pDocument->SetProperty( "pointer-events", "auto" );
    m_elemChatInput->Focus();
}

void RkHudChat::StopMessageMode()
{
    m_iMode = MM_NONE;

    m_pDocument->SetProperty( "pointer-events", "none" );
    m_elemChatInput->Blur();
}

bool RkHudChat::ChatRaised()
{
    return m_iMode != MM_NONE;
}

void RkHudChat::ClearChatHistory()
{
    chatData.lines.clear();
    RkHudDirty( "chat" );
}

void RkHudChat::AddChatString( const char *username, const char *message, MessageSender sender )
{
    if ( !message )
        return;

    ChatLine line;
    line.username = username ? ( Rml::String( username ) + ": " ) : Rml::String();
    line.message = message;
    switch ( sender )
    {
    case RkHudChat::SERVER: line.sender_class = "chat_username_server"; break;
    case RkHudChat::FRIEND: line.sender_class = "chat_username_friend"; break;
    case RkHudChat::FOE:    line.sender_class = "chat_username_foe"; break;
    }

    chatData.lines.push_back( line );

    // FIFO scrollback. The old version tore down *every* line once the cap was
    // reached; this drops just the oldest.
    const int cap = std::max( 1, rocket_hud_chat_max_entries.GetInt() );
    while ( (int)chatData.lines.size() > cap )
        chatData.lines.erase( chatData.lines.begin() );

    RkHudDirty( "chat" );

    // The document has to lay the new line out before its height can be scrolled
    // to. Only possible once the document exists -- messages can arrive before it.
    if ( !m_pDocument )
        return;

    m_pDocument->UpdateDocument();
    m_pDocument->SetScrollTop( m_pDocument->GetScrollHeight() - m_pDocument->GetClientHeight() );
}

void RkHudChat::AddChatString( const wchar_t *username, const wchar_t *message, MessageSender sender )
{
    //TODO: wide strings.
}

class CHudElement;
class CCSPlayer;
class CWeaponCSBase;

using namespace ::ks::reflect;

DECLARE_HUDELEMENT( RkHudCrosshair );

const char *RkHudCrosshair::kDocument = "hud_crosshair.rml";

// External convars defined in weapon_csbase.cpp and counterstrikeviewport.cpp
extern ConVar crosshair;
extern ConVar cl_crosshairsize;
extern ConVar cl_crosshairgap;
extern ConVar cl_crosshairthickness;
extern ConVar cl_crosshaircolor_r;
extern ConVar cl_crosshaircolor_g;
extern ConVar cl_crosshaircolor_b;
extern ConVar cl_crosshairalpha;
extern ConVar cl_crosshairdot;
extern ConVar cl_crosshair_drawoutline;
extern ConVar cl_crosshair_outlinethickness;

// Recoil tracking (baseplayer_shared.cpp)
extern ConVar view_recoil_tracking;

ConVar cl_crosshair_recoil( side_registry(), "cl_crosshair_recoil", "1", FCVAR_ARCHIVE, "Crosshair follows recoil" );

// What hud_crosshair.rml binds to. Whole pixels, because the document derives
// every rect from these with nothing but + - and *2 -- the same integers the
// legacy CHudCrosshair::DrawCrosshairRect calls used, so the lines still land on
// exact pixel boundaries. Layout lives in the .rml now; this is just the maths.
struct CrosshairData
{
    int x0, y0;             // top-left of the vertical/horizontal bars
    int cx, cy;             // aim centre, recoil included
    int gap, bar, thick, ol;
    Rml::String color = "rgba(0,255,0,255)", outline_color = "rgba(0,0,0,255)";
    bool outlined, dotted;

    bool operator==( const CrosshairData & ) const = default;
} crosshairData;

static void BindCrosshair( Rml::DataModelConstructor &c )
{
    if ( auto handle = c.RegisterStruct<CrosshairData>() )
    {
        handle.RegisterMember( "x0", &CrosshairData::x0 );
        handle.RegisterMember( "y0", &CrosshairData::y0 );
        handle.RegisterMember( "cx", &CrosshairData::cx );
        handle.RegisterMember( "cy", &CrosshairData::cy );
        handle.RegisterMember( "gap", &CrosshairData::gap );
        handle.RegisterMember( "bar", &CrosshairData::bar );
        handle.RegisterMember( "thick", &CrosshairData::thick );
        handle.RegisterMember( "ol", &CrosshairData::ol );
        handle.RegisterMember( "color", &CrosshairData::color );
        handle.RegisterMember( "outline_color", &CrosshairData::outline_color );
        handle.RegisterMember( "outlined", &CrosshairData::outlined );
        handle.RegisterMember( "dotted", &CrosshairData::dotted );
    }
    c.Bind( "crosshair", &crosshairData );
}
RK_HUD_SECTION( BindCrosshair );

RkHudCrosshair::RkHudCrosshair( const char *pElementName ) : RkHudDocument( pElementName )
{
    SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_CROSSHAIR );
}

RkHudCrosshair::~RkHudCrosshair() noexcept
{
    Unload();
}

void RkHudCrosshair::UpdateCrosshair()
{
    if( !m_pDocument )
        return;

    // Get screen dimensions and calculate center (same approach as original VGUI)
    Rml::Context *ctx = RocketUI()->AccessHudContext();
    if( !ctx )
        return;

    Rml::Vector2i dimensions = ctx->GetDimensions();
    int iCenterX = dimensions.x / 2;
    int iCenterY = dimensions.y / 2;

    // Offset crosshair center so it points where bullets actually go.
    //
    // The camera view applies two offsets (CalcViewAngles):
    //   eyeAngles += viewPunch                          (screen shake, cosmetic)
    //   eyeAngles += aimPunch * view_recoil_tracking    (partial recoil, default 0.45)
    //
    // Bullets land at: baseAngles + aimPunch  (full aim punch, no view punch).
    //
    // Screen center = camera direction, so without correction the crosshair
    // inherits both viewPunch and the partial aimPunch.  To make the crosshair
    // show the true bullet destination we must offset by:
    //   aimPunch * (1 - view_recoil_tracking) - viewPunch
    //
    // The viewPunch subtraction cancels the per-shot screen shake that rocks
    // the camera but doesn't affect bullet trajectory, eliminating the
    // visible jitter during spray.
    if( cl_crosshair_recoil.GetBool() )
    {
        CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
        if( pPlayer )
        {
            if( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
                pPlayer = ToCSPlayer( pPlayer->GetObserverTarget() );

            if( pPlayer )
            {
                QAngle aimPunch  = pPlayer->GetAimPunchAngle();
                QAngle viewPunch = pPlayer->GetViewPunchAngle();
                float flUntracked = 1.0f - view_recoil_tracking.GetFloat();

                // Net offset from screen center to bullet destination
                float flOffsetPitch = aimPunch[PITCH] * flUntracked - viewPunch[PITCH];
                float flOffsetYaw   = aimPunch[YAW]   * flUntracked - viewPunch[YAW];

                // Project angle offset to screen pixels.
                // GetFOV() is the base horizontal FOV for 4:3; the engine widens it
                // for other aspects but vertical FOV stays constant, so we derive
                // a single focal-length scale from screenHeight and the vertical FOV:
                //   vfov = 2 * atan(tan(baseFov/2) * 3/4)
                //   scale = screenHeight / (2 * tan(vfov/2))
                //         = screenHeight / (1.5 * tan(baseFov/2))
                float fov = pPlayer->GetFOV();
                float flScale = (float)dimensions.y / ( 1.5f * std::tanf( DEG2RAD( fov ) * 0.5f ) );

                // Pitch: negative = looking up → screen Y decreases
                // Yaw:   positive = looking left → screen X decreases
                iCenterX += (int)( -tanf( DEG2RAD( flOffsetYaw ) )   * flScale );
                iCenterY += (int)(  tanf( DEG2RAD( flOffsetPitch ) ) * flScale );
            }
        }
    }

    // Read convars
    float size = cl_crosshairsize.GetFloat();
    float gap = cl_crosshairgap.GetFloat();
    float thickness = cl_crosshairthickness.GetFloat();
    int r = cl_crosshaircolor_r.GetInt();
    int g = cl_crosshaircolor_g.GetInt();
    int b = cl_crosshaircolor_b.GetInt();
    int alpha = cl_crosshairalpha.GetInt();
    bool drawDot = cl_crosshairdot.GetBool();
    bool drawOutline = cl_crosshair_drawoutline.GetBool();
    float outlineThickness = cl_crosshair_outlinethickness.GetFloat();

    // Clamp values
    if( size < 0 ) size = 0;
    if( thickness < 0.5f ) thickness = 0.5f;
    alpha = clamp( alpha, 0, 255 );
    r = clamp( r, 0, 255 );
    g = clamp( g, 0, 255 );
    b = clamp( b, 0, 255 );
    outlineThickness = clamp( outlineThickness, 0.1f, 3.0f );

    // Scale values to pixels (matching original VGUI scaling)
    // Original uses YRES() macro which scales based on 480 base height
    float scale = dimensions.y / 480.0f;
    int iBarSize = (int)( size * scale );
    int iBarThickness = std::max( 1, (int)( thickness * scale ) );
    int iGap = (int)( gap * scale );
    // Outline thickness is NOT scaled in the original - it's raw pixels
    int iOutline = (int)( outlineThickness + 0.5f );

    // Publish for hud_crosshair.rml, which turns these into the ten rects. Only
    // handed over when something actually moved: with no recoil and no convar
    // edits that is one struct compare per frame instead of forty property
    // parses, and RmlUi re-evaluates the bindings only for what it is told is
    // dirty.
    CrosshairData next;
    next.cx = iCenterX;
    next.cy = iCenterY;
    next.thick = iBarThickness;
    next.bar = iBarSize;
    next.gap = iGap;
    next.ol = iOutline;
    next.x0 = iCenterX - iBarThickness / 2;
    next.y0 = iCenterY - iBarThickness / 2;
    next.outlined = drawOutline;
    next.dotted = drawDot;

    char buf[64];
    V_snprintf( buf, sizeof(buf), "rgba(%d,%d,%d,%d)", r, g, b, alpha );
    next.color = buf;
    V_snprintf( buf, sizeof(buf), "rgba(0,0,0,%d)", alpha );
    next.outline_color = buf;

    if( next == crosshairData )
        return;

    crosshairData = next;
    RkHudDirty( "crosshair" );
}

void RkHudCrosshair::ShowPanel( bool bShow, bool force )
{
    if( !m_pDocument )
        return;

    if( bShow )
    {
        if( !m_bVisible )
        {
            m_pDocument->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
        }

        UpdateCrosshair();
    }
    else
    {
        if( m_bVisible )
        {
            m_pDocument->Hide();
        }
    }

    m_bVisible = bShow;
}

void RkHudCrosshair::SetActive( bool bActive )
{
    ShowPanel( bActive, false );
    CHudElement::SetActive( bActive );
}

bool RkHudCrosshair::ShouldDraw()
{
    if( !crosshair.GetBool() )
        return false;

    CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
    if( !pPlayer )
        return false;

    // Handle spectating - get the target player if in first person spec
    if( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
    {
        pPlayer = ToCSPlayer( pPlayer->GetObserverTarget() );
        if( !pPlayer )
            return false;
    }
    else if( !pPlayer->IsAlive() )
    {
        // Dead and not spectating in first person
        return false;
    }

    // Check weapon-specific conditions
    CWeaponCSBase *pWeapon = pPlayer->GetActiveCSWeapon();
    if( pWeapon )
    {
        // Sniper rifles never show the regular crosshair (they use the scope HUD when scoped)
        if( pWeapon->GetWeaponType() == WEAPONTYPE_SNIPER_RIFLE )
            return false;
    }

    return cl_drawhud.GetBool() && CHudElement::ShouldDraw();
}

class CHudElement;
class CCSPlayer;
class CWeaponCSBase;

using namespace ::ks::reflect;

DECLARE_HUDELEMENT( RkHudInfoBar );

const char *RkHudInfoBar::kDocument = "hud_infobar.rml";

// Struct layout for data-binding model.
struct InfoBarData
{
    int hp;
    int armor;
    bool hasHelmet;
    int ammo;
    int ammoReserve;
    Rml::String fireModeString;
    Rml::String primaryString;
    Rml::String secondaryString;
    Rml::String knifeString;
    bool hasGrenade;
    bool hasFlash;
    bool hasFlashPair;
    bool hasDecoy;
    bool hasSmoke;
    bool hasFire;
    bool hasC4;

    bool operator==( const InfoBarData & ) const = default;
} infoBarData;

static void BindInfoBarData( Rml::DataModelConstructor &c )
{
    if ( auto h = c.RegisterStruct<InfoBarData>() )
    {
        h.RegisterMember( "hp", &InfoBarData::hp );
        h.RegisterMember( "armor", &InfoBarData::armor );
        h.RegisterMember( "ammo", &InfoBarData::ammo );
        h.RegisterMember( "ammo_reserve", &InfoBarData::ammoReserve );
        h.RegisterMember( "fire_mode_string", &InfoBarData::fireModeString );
        h.RegisterMember( "has_helmet", &InfoBarData::hasHelmet );
        h.RegisterMember( "primary_string", &InfoBarData::primaryString );
        h.RegisterMember( "secondary_string", &InfoBarData::secondaryString );
        h.RegisterMember( "knife_string", &InfoBarData::knifeString );
        h.RegisterMember( "has_grenade", &InfoBarData::hasGrenade );
        h.RegisterMember( "has_decoy", &InfoBarData::hasDecoy );
        h.RegisterMember( "has_flash", &InfoBarData::hasFlash );
        h.RegisterMember( "has_flash_pair", &InfoBarData::hasFlashPair );
        h.RegisterMember( "has_smoke", &InfoBarData::hasSmoke );
        h.RegisterMember( "has_fire", &InfoBarData::hasFire );
        h.RegisterMember( "has_c4", &InfoBarData::hasC4 );
    }
    c.Bind( "infobar", &infoBarData );
}
RK_HUD_SECTION( BindInfoBarData );

// Fills `infoBarData` from the player. Kept whole-struct so ShowPanel can tell
// whether anything actually changed before waking the data bindings.
static void UpdateInfoFromPlayer( const CCSPlayer &pPlayer )
{
    infoBarData.hp = pPlayer.GetHealth();
    infoBarData.armor = pPlayer.ArmorValue();
    infoBarData.hasHelmet = false;
    if( pPlayer.HasHelmet() )
        infoBarData.hasHelmet = true;

    infoBarData.fireModeString = " ";
    infoBarData.primaryString = " ";
    infoBarData.secondaryString = " ";
    infoBarData.knifeString = " ";
    infoBarData.hasGrenade = false;
    //infoBarData.hasFlash = false;
    //infoBarData.hasFlashPair = false;
    infoBarData.hasDecoy = false;
    infoBarData.hasSmoke = false;
    infoBarData.hasFire = false;
    infoBarData.hasC4 = false;

    int flashbangAmount = 0;
    for( int i = 0; i < MAX_WEAPONS; i++ )
    {
        CWeaponCSBase *weapon = (CWeaponCSBase*)pPlayer.GetWeapon(i);
        if( !weapon )
            continue;

        int slot = weapon->GetSlot();
        const char *name;

        switch( slot )
        {
            case WEAPON_SLOT_RIFLE:
                name = V_strstr(weapon->GetName(), "_");
                if( name && name[0] )
                    infoBarData.primaryString = name+1;
                break;
            case WEAPON_SLOT_PISTOL:
                name = V_strstr(weapon->GetName(), "_");
                if( name && name[0] )
                    infoBarData.secondaryString = name+1;
                break;
            case WEAPON_SLOT_KNIFE:
                name = V_strstr(weapon->GetName(), "_");
                if( name && name[0] )
                    infoBarData.knifeString = name+1;
                break;
            case WEAPON_SLOT_GRENADES:
            {
                int weaponID = weapon->GetCSWeaponID();
                switch( weaponID )
                {
                    case WEAPON_HEGRENADE:
                        infoBarData.hasGrenade = true;
                        break;
                    case WEAPON_FLASHBANG:
                        flashbangAmount++;
                        break;
                    case WEAPON_SMOKEGRENADE:
                        infoBarData.hasSmoke = true;
                        break;
                    case WEAPON_MOLOTOV:
                        infoBarData.hasFire = true;
                        break;
                    case WEAPON_INCGRENADE:
                        infoBarData.hasFire = true;
                        break;
                    case WEAPON_DECOY:
                        infoBarData.hasDecoy = true;
                    default:
                        break;
                }
                break;
            }
            case WEAPON_SLOT_C4:
                infoBarData.hasC4 = true;
                break;
            default:
                break;
        }
    }
    infoBarData.hasFlash = ( flashbangAmount == 1 );
    infoBarData.hasFlashPair = ( flashbangAmount == 2 );

    CWeaponCSBase *activeWeapon = pPlayer.GetActiveCSWeapon();
    if( activeWeapon )
    {
        infoBarData.ammo = activeWeapon->Clip1();
        infoBarData.ammoReserve = activeWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );
        if( activeWeapon->IsFullAuto() )
            infoBarData.fireModeString = "AUTO";
        else if( activeWeapon->IsInBurstMode() )
            infoBarData.fireModeString = "BURST";
        else
            infoBarData.fireModeString = "SINGLE";
    }
}

RkHudInfoBar::RkHudInfoBar(const char *value) : RkHudDocument( value )
{
    SetHiddenBits( /* HIDEHUD_MISCSTATUS */ 0 );
}

RkHudInfoBar::~RkHudInfoBar() noexcept
{
    Unload();
}

// Called every frame by the HUD system (CHud::DoElementThink -> SetActive).
void RkHudInfoBar::ShowPanel(bool bShow, bool force)
{
    if( !m_pDocument )
        return;

    if( !bShow )
    {
        if( m_bVisible )
            m_pDocument->Hide();
        m_bVisible = false;
        return;
    }

    if( !m_bVisible )
        m_pDocument->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
    m_bVisible = true;

    CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();

    // observing someone? switch to that player.
    if( pPlayer && pPlayer->IsObserver() &&
        ( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE || pPlayer->GetObserverMode() == OBS_MODE_CHASE ) )
        pPlayer = ToCSPlayer( pPlayer->GetObserverTarget() );

    if( !pPlayer )
        return;

    const InfoBarData previous = infoBarData;
    UpdateInfoFromPlayer( *pPlayer );

    // The HUD this drives changes a few times a second at most. Dirtying
    // unconditionally made RmlUi re-evaluate every binding and re-shape the text
    // on all of them, every frame.
    if( !( infoBarData == previous ) )
        RkHudDirty( "infobar" );
}

void RkHudInfoBar::SetActive(bool bActive)
{
    ShowPanel( bActive, false );
    CHudElement::SetActive( bActive );
}

bool RkHudInfoBar::ShouldDraw()
{
    CCSPlayer *localPlayer = CCSPlayer::GetLocalCSPlayer();

    return localPlayer &&
    cl_drawhud.GetBool() &&
    ( localPlayer->IsAlive() || ( localPlayer->IsObserver() && localPlayer->GetObserverMode() == OBS_MODE_IN_EYE || localPlayer->GetObserverMode() == OBS_MODE_CHASE ) ) &&
    CHudElement::ShouldDraw();
}

class CHudElement;
class CCSPlayer;

using namespace ::ks::reflect;

DECLARE_HUDELEMENT( RkHudKillfeed );

const char *RkHudKillfeed::kDocument = "hud_killfeed.rml";

ConVar rocket_hud_killfeed_linger_time( side_registry(), "rocket_hud_killfeed_linger_time", "5", FCVAR_ARCHIVE, "How long in seconds to keep each killfeed entry on screen." );

struct KillfeedEntry
{
    Rml::String attackerName;
    Rml::String gunName;
    Rml::String victimName;
    bool headshot;
    bool wallbang;
    float noticeSpawnTime;
};

// Struct layout for data-binding model.
struct KillFeedData
{
    std::deque<KillfeedEntry> entries;
} killFeedData;

static void BindKillfeed( Rml::DataModelConstructor &c )
{
    if ( auto entry = c.RegisterStruct<KillfeedEntry>() )
    {
        entry.RegisterMember( "attacker_name", &KillfeedEntry::attackerName );
        entry.RegisterMember( "gun_name", &KillfeedEntry::gunName );
        entry.RegisterMember( "victim_name", &KillfeedEntry::victimName );
        entry.RegisterMember( "headshot", &KillfeedEntry::headshot );
        entry.RegisterMember( "wallbang", &KillfeedEntry::wallbang );
    }
    c.RegisterArray<std::deque<KillfeedEntry>>();

    if ( auto h = c.RegisterStruct<KillFeedData>() )
        h.RegisterMember( "entries", &KillFeedData::entries );
    c.Bind( "killfeed", &killFeedData );
}
RK_HUD_SECTION( BindKillfeed );

void RkHudKillfeed::OnPlayerDeath( IGameEvent *event )
{
    KillfeedEntry entry;
    int nAttacker = engine->GetPlayerForUserID( event->GetInt( "attacker" ) );
    int nVictim = engine->GetPlayerForUserID( event->GetInt( "userid" ) );

    CCSPlayer* attacker = ToCSPlayer( ClientEntityList().GetBaseEntity( nAttacker ) );
    CCSPlayer* victim = ToCSPlayer( ClientEntityList().GetBaseEntity( nVictim ) );

    if( !attacker || !victim )
        return;

    entry.victimName = victim->GetPlayerName();
    entry.attackerName = attacker->GetPlayerName();
    entry.gunName = event->GetString( "weapon" );
    entry.headshot = ( event->GetInt( "headshot" ) > 0 );
    entry.wallbang = ( event->GetInt( "penetrated" ) > 0 );

    entry.noticeSpawnTime = gpGlobals->curtime;

    killFeedData.entries.push_back( entry );

    RkHudDirty( "killfeed" );
}

// called every frame
void RkHudKillfeed::CheckForOldEntries()
{
    if( killFeedData.entries.empty() )
        return;

    // pop off the first guy, then we're done. This gets called often enough to not matter about the rest.
    if( (gpGlobals->curtime - killFeedData.entries.front().noticeSpawnTime) > rocket_hud_killfeed_linger_time.GetFloat() )
    {
        killFeedData.entries.pop_front();
        RkHudDirty( "killfeed" );
    }
}

// The killfeed has no open/close of its own: it is up whenever the HUD is.
void RkHudKillfeed::OnLoad()
{
    ShowPanel( true, false );
}

RkHudKillfeed::RkHudKillfeed(const char *value) : RkHudDocument( value )
{
    SetHiddenBits( /* HIDEHUD_MISCSTATUS */ 0 );
}

RkHudKillfeed::~RkHudKillfeed() noexcept
{
    StopListeningForAllEvents();

    Unload();
}

void RkHudKillfeed::LevelInit()
{
    ListenForGameEvent( "player_death" );

    RkHudDocument::LevelInit();
}

void RkHudKillfeed::LevelShutdown()
{
    killFeedData.entries.clear();

    RkHudDocument::LevelShutdown();
}

void RkHudKillfeed::ShowPanel(bool bShow, bool force)
{
    if( !m_pDocument )
        return;

    if( bShow )
    {
        if( !m_bVisible )
        {
            m_pDocument->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
        }
        CheckForOldEntries();
    }
    else
    {
        if( m_bVisible )
        {
            m_pDocument->Hide();
        }
    }

    m_bVisible = bShow;
}

void RkHudKillfeed::SetActive(bool bActive)
{
    ShowPanel( bActive, false );
    CHudElement::SetActive( bActive );
}

bool RkHudKillfeed::ShouldDraw()
{
    return cl_drawhud.GetBool() && CHudElement::ShouldDraw();
}

void RkHudKillfeed::FireGameEvent(IGameEvent *event)
{
    // We only listen for "player_death"
    OnPlayerDeath( event );
}

using namespace ::ks::reflect;

Rml::ElementDocument *RocketLoadingScreenDocument::m_pInstance = nullptr;
bool RocketLoadingScreenDocument::m_bVisible = false;

// hud_loadingscreen.rml dismisses itself on any click (data-event-mousedown).
static void BindLoadingScreen( Rml::DataModelConstructor &c )
{
    c.BindEventCallback( "dismiss_loadingscreen", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & ) {
        RocketLoadingScreenDocument::ShowPanel( false );
    } );
}
RK_HUD_SECTION( BindLoadingScreen );

void RocketLoadingScreenDocument::LoadDialog()
{
    RkPanelLoad( m_pInstance, ROCKET_CONTEXT_HUD, "hud_loadingscreen.rml", LoadDialog, UnloadDialog );
}

void RocketLoadingScreenDocument::UnloadDialog()
{
    RkPanelUnload( m_pInstance, m_bVisible );
}

void RocketLoadingScreenDocument::ShowPanel(bool bShow, bool immediate)
{
    if( bShow )
        LoadDialog();

    // Dismissing the loading screen is what joins the game and raises team select.
    const bool bWasVisible = m_bVisible;

    RkPanelShow( m_pInstance, m_bVisible, bShow );

    if( !bShow && bWasVisible )
    {
        engine->ClientCmd_Unrestricted( "joingame" );
        RocketTeamMenuDocument::ShowPanel( true );
    }
}

using namespace ::ks::reflect;

// Function-local so registration works whatever order the translation units
// initialise in.
static std::vector<RkHudSectionFn> &Sections()
{
    static std::vector<RkHudSectionFn> sections;
    return sections;
}

RkHudSection::RkHudSection( RkHudSectionFn bindFunc )
{
    if ( bindFunc )
        Sections().push_back( bindFunc );
}

// One model per context, same schema in each. HUD documents load into the HUD
// context and panels into whichever context is current, so both carry it.
//
// The bookkeeping is here rather than asking RmlUi: Context::GetDataModel logs an
// error when the model is absent, which is exactly the case we need to test for.
static Rml::DataModelConstructor EnsureModel( Rml::Context *ctx )
{
    if ( !ctx )
        return {};

    struct Bound
    {
        Rml::Context *ctx = nullptr;
    };
    static Bound s_bound[2];

    for ( const Bound &bound : s_bound )
        if ( bound.ctx == ctx )
            return ctx->GetDataModel( "hud" );

    // Not allow_missing_variables: every section is bound below before any
    // document loads, so a name a document cannot resolve is an authoring typo
    // and should say so loudly instead of silently rendering nothing.
    Rml::DataModelConstructor model = ctx->CreateDataModel( "hud" );
    if ( !model )
    {
        Warning( "RocketUI: couldn't create the UI data model\n" );
        return {};
    }

    for ( Bound &bound : s_bound )
    {
        if ( !bound.ctx )
        {
            bound.ctx = ctx;
            break;
        }
    }

    for ( RkHudSectionFn bindFunc : Sections() )
        bindFunc( model );

    return model;
}

Rml::DataModelConstructor RkHudModel()
{
    if ( !RocketUI() )
        return {};

    // Both contexts get the schema; the HUD one is what callers work with.
    EnsureModel( RocketUI()->AccessMenuContext() );
    return EnsureModel( RocketUI()->AccessHudContext() );
}

Rml::ElementDocument *RkLoadDocument( RocketDesinationContext_t ctx, const char *file,
                                      LoadDocumentFn onLoad, UnloadDocumentFn onUnload )
{
    if ( !RocketUI() )
        return nullptr;

    // Before the document, not after: the model has to be in place for the
    // document's bindings to resolve as it loads.
    RkHudModel();

    Rml::ElementDocument *doc = RocketUI()->LoadDocumentFile( ctx, file, onLoad, onUnload );
    if ( !doc )
        Warning( "RocketUI: couldn't load %s\n", file );

    return doc;
}

void RkHudDirty( const char *section )
{
    if ( !RocketUI() )
        return;

    // A section can be on screen in either context (panels load into whichever
    // is current), so both are told.
    for ( Rml::Context *ctx : { RocketUI()->AccessHudContext(), RocketUI()->AccessMenuContext() } )
    {
        if ( Rml::DataModelConstructor model = EnsureModel( ctx ) )
            model.GetModelHandle().DirtyVariable( section );
    }
}

using namespace ::ks::reflect;

Rml::ElementDocument *RocketPauseMenuDocument::m_pInstance = nullptr;
bool RocketPauseMenuDocument::m_bVisible = false;

// The engine keeps its own GameUI-visible flag, and while it is set UI_ActivateMouse()
// deactivates the mouse every frame -- no mouse look, even with nothing on screen. So a
// panel that means "back to the game" must not just hide itself: it asks the engine to
// dismiss the GameUI, and CGameUI::OnGameUIHidden takes our documents down. One-way
// flow (engine -> documents) instead of two half-wired state machines.
void RocketUI_ReturnToGame()
{
    if ( engine->IsInGame() && engineui && engineui->IsGameUIVisible() )
        engine->ClientCmd_Unrestricted( "gameui_hide\n" );
}

// The buttons name their own action in hud_pausemenu.rml (data-event-click), so
// there is no listener walking every <button> and no chain of id comparisons.
static void BindPauseMenu( Rml::DataModelConstructor &c )
{
    c.BindEventCallback( "pm_resume", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & ) {
        RocketUI_ReturnToGame();
    } );
    c.BindEventCallback( "pm_choose_team", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & ) {
        RocketTeamMenuDocument::ShowPanel( true );
        RocketPauseMenuDocument::ShowPanel( false );
    } );
    c.BindEventCallback( "pm_options", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & ) {
        RocketOptionsDocument::ShowPanel( true );
        RocketPauseMenuDocument::ShowPanel( false );
    } );
    c.BindEventCallback( "pm_disconnect", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & ) {
        engine->ClientCmd_Unrestricted( "disconnect" );
    } );
}
RK_HUD_SECTION( BindPauseMenu );

void RocketPauseMenuDocument::LoadDialog()
{
    RkPanelLoad( m_pInstance, ROCKET_CONTEXT_HUD, "hud_pausemenu.rml", LoadDialog, UnloadDialog );
}

void RocketPauseMenuDocument::UnloadDialog()
{
    RkPanelUnload( m_pInstance, m_bVisible );
}

void RocketPauseMenuDocument::ShowPanel(bool bShow, bool immediate)
{
    if( bShow )
    {
        LoadDialog();

        // The chat and the buy menu are modal in their own right: ESC belongs to
        // them while either is up.
        RkHudChat *pChat = GET_HUDELEMENT( RkHudChat );
        RkHudBuyMenu *pBuyMenu = GET_HUDELEMENT( RkHudBuyMenu );
        if( !pChat || !pBuyMenu )
            return;
        if( pChat->ChatRaised() || pBuyMenu->m_bVisible || !pBuyMenu->m_pDocument )
            return;
    }

    RkPanelShow( m_pInstance, m_bVisible, bShow );
}

class CHudElement;
class CBasePlayer;
class CCSPlayer;

using namespace ::ks::reflect;

DECLARE_HUDELEMENT( RkHudRadar );

const char *RkHudRadar::kDocument = "hud_radar.rml";
DECLARE_HUD_MESSAGE( RkHudRadar, ProcessSpottedEntityUpdate );

ConVar rocket_hud_radar_info_linger_time( side_registry(), "rocket_hud_radar_info_linger_time", "3", FCVAR_ARCHIVE, "How long in seconds does the data stay visible after an update" );
ConVar rocket_hud_radar_scale( side_registry(), "rocket_hud_radar_scale", "0.15", FCVAR_ARCHIVE, "scale for radar" );

// What hud_radar.rml binds to. One blip per player slot; the document positions
// and colours them, so this file only does the projection maths.
struct RadarBlip
{
    int x = 0, y = 0;
    bool ct = false;    // false = T; picks the .player-ct / .player-t colour
    bool shown = false;

    bool operator==( const RadarBlip & ) const = default;
};
struct RadarData
{
    int width = 0, height = 0;
    Rml::Vector<RadarBlip> blips;

    bool operator==( const RadarData & ) const = default;
} radarData;

static void BindRadar( Rml::DataModelConstructor &c )
{
    if ( auto blip = c.RegisterStruct<RadarBlip>() )
    {
        blip.RegisterMember( "x", &RadarBlip::x );
        blip.RegisterMember( "y", &RadarBlip::y );
        blip.RegisterMember( "ct", &RadarBlip::ct );
        blip.RegisterMember( "shown", &RadarBlip::shown );
    }
    c.RegisterArray<Rml::Vector<RadarBlip>>();

    if ( auto handle = c.RegisterStruct<RadarData>() )
    {
        handle.RegisterMember( "width", &RadarData::width );
        handle.RegisterMember( "height", &RadarData::height );
        handle.RegisterMember( "blips", &RadarData::blips );
    }
    // One blip per player slot for the lifetime of the model: data-for only
    // instances elements when the array *size* changes, so a fixed size means the
    // blips are created once and only ever moved afterwards.
    radarData.blips.assign( MAX_PLAYERS, RadarBlip{} );
    c.Bind( "radar", &radarData );
}
RK_HUD_SECTION( BindRadar );

static void RadarSizeChanged( ConVar *pConvar, const char *szOldValue, float fOldValue )
{
    RkHudRadar *pRadar = GET_HUDELEMENT( RkHudRadar );
    if( !pRadar )
    {
        Warning( "Couldn't grab hud radar to update size!\n" );
        return;
    }
    pRadar->UpdateRadarSize();
}
ConVar rocket_hud_radar_height( side_registry(), "rocket_hud_radar_height", "400", FCVAR_ARCHIVE, "height in pixels for the radar", RadarSizeChanged );
ConVar rocket_hud_radar_width( side_registry(), "rocket_hud_radar_width", "400", FCVAR_ARCHIVE, "width in pixels for the radar", RadarSizeChanged );

bool RkHudRadar::MsgFunc_ProcessSpottedEntityUpdate(const ks::net::CCSUsrMsg_ProcessSpottedEntityUpdate &msg)
{
    if( msg.new_update )
    {
        // Clear everything.
        //for( int i = 0; i < MAX_PLAYERS; i++ )
        //    m_spottedPlayers[i].timelastSpotted = 0.0f;
    }

    for( int i = 0; i < msg.entity_updates.size(); i++ )
    {
        const auto &update = msg.entity_updates[ i ];

        int entID = update.entity_idx;
        // make sure this is a valid id for any type of entity.
        if( entID < 1 || entID >= MAX_EDICTS )
            continue;

        // these are sent from the server as /4
        int x = update.origin_x * 4;
        int y = update.origin_y * 4;
        int z = update.origin_z * 4;
        int yaw = update.angle_y;

        const char *szEntClassName;
        int classID = update.class_id;
        for ( ClientClass *pCur = g_pClientClassHead; pCur; pCur = pCur->m_pNext )
        {
            if( pCur->m_ClassID == classID )
            {
                szEntClassName = pCur->GetName();
                break;
            }
        }

        if( !szEntClassName )
        {
            Warning( "Unknown entity class received in ProcessSpottedEntityUpdate.\n" );
        }

        // Clients are unaware of the defuser class type, so we need to flag defuse entities manually
        if( update.defuser )
        {
            // TODO: this is the defuser!
        }
        else if( !V_strcmp( "CCSPlayer", szEntClassName ) )
        {
            // out of bounds!
            if( entID < 1 || entID > MAX_PLAYERS )
                return true;

            // subtract 1 to convert 1-64 to 0-63
            SpottedInfo &playerInfo = m_spottedPlayers[ (entID-1) ];
            playerInfo.entId = update.entity_idx;
            playerInfo.originX = x;
            playerInfo.originY = y;
            playerInfo.originZ = z;
            playerInfo.angleYaw = yaw;
            if( update.player_has_defuser.has_value() )
            {
                playerInfo.playerWithDefuser = true;
                // TODO: set defuser pos
            }
            if( update.player_has_c4 )
            {
                playerInfo.playerWithC4 = true;
                // TODO: set bomb pos
            }
            playerInfo.timelastSpotted = gpGlobals->curtime;
        }
        else if( !V_strcmp( "CC4", szEntClassName ) || !V_strcmp( "CPlantedC4", szEntClassName ) )
        {
            // TODO: set bomb pos
        }
        else if( !V_strcmp( "CHostage", szEntClassName ) )
        {
            // TODO: set hostage pos
        }
    }

    return true;
}

// https://www.unknowncheats.me/forum/general-programming-and-reversing/135529-implement-simple-radar.html
static inline void RotatePoint( float x, float y, float centerX, float centerY, float angle, float *outX, float *outY )
{
    angle = DEG2RAD( angle );

    float cosTheta = std::cosf( angle );
    float sinTheta = std::sinf( angle );

    *outX = (cosTheta * ( x - centerX )) - (sinTheta * ( y - centerY ));
    *outY = (sinTheta * ( x - centerX )) + (cosTheta * ( y - centerY ));
    *outX += centerX;
    *outY += centerY;
}

void RkHudRadar::UpdateRadarSize()
{
    if( !m_pDocument )
        return;

    m_radarWidth = rocket_hud_radar_width.GetFloat();
    m_radarHeight = rocket_hud_radar_height.GetFloat();
    m_radarCenterX = ( m_radarWidth / 2.0f );
    m_radarCenterY = ( m_radarHeight / 2.0f );

    // hud_radar.rml sizes itself off these; blips are children, so their
    // coordinates stay relative to the radar box.
    radarData.width = (int)m_radarWidth;
    radarData.height = (int)m_radarHeight;
    RkHudDirty( "radar" );
}

void RkHudRadar::UpdateRadarFrame()
{
    CCSPlayer *activePlayer = CCSPlayer::GetLocalCSPlayer();
    if( !activePlayer )
        return;

    // observing someone? switch to that player.
    if( activePlayer->IsObserver() && (activePlayer->GetObserverMode() == OBS_MODE_IN_EYE || activePlayer->GetObserverMode() == OBS_MODE_CHASE) )
        activePlayer = ToCSPlayer(activePlayer->GetObserverTarget());

    if( !activePlayer )
        return;

    IGameResources *gr = GameResources();
    float currTime = gpGlobals->curtime;
    const RadarData previous = radarData;

    // The Radar packets are designed to supplement an existing regular radar.
    // We will do a regular radar, but with the dormant check, see if we have some recent maphack data from server.
    for( int i = 1; i <= MAX_PLAYERS; i++ )
    {
        // array index for the blip
        int index = i - 1;
        RadarBlip &blip = radarData.blips[index];

        if( i == engine->GetLocalPlayer() )
        {
            blip.shown = false;
            continue;
        }

        CBasePlayer *player = UTIL_PlayerByIndex( i );
        const SpottedInfo &playerInfo = m_spottedPlayers[index];

        if( !player || !player->IsAlive() )
        {
            blip.shown = false;
            continue;
        }

        // if dormant and we dont have any recent information from the server
        if( player->IsDormant() && ( ( currTime - playerInfo.timelastSpotted ) > rocket_hud_radar_info_linger_time.GetFloat() ) )
        {
            blip.shown = false;
            continue;
        }

        // At this point we either have a visible player or recent server radar data.
        // GameResources knows the team even while the entity is dormant.
        blip.ct = gr && gr->GetTeam( i ) == TEAM_CT;

        float originDiffX;
        float originDiffY;

        if( player->IsDormant() )
        {
            originDiffX = activePlayer->GetAbsOrigin().x - playerInfo.originX;
            originDiffY = activePlayer->GetAbsOrigin().y - playerInfo.originY;
        }
        else
        {
           originDiffX = activePlayer->GetAbsOrigin().x - player->GetAbsOrigin().x;
           originDiffY = activePlayer->GetAbsOrigin().y - player->GetAbsOrigin().y;
        }

        originDiffX *= rocket_hud_radar_scale.GetFloat();
        originDiffY *= rocket_hud_radar_scale.GetFloat();
        originDiffX *= -1; // x goes other way

        // add the center of the radar.
        originDiffX += m_radarCenterX;
        originDiffY += m_radarCenterY;

        float rotatedX;
        float rotatedY;
        RotatePoint( originDiffX, originDiffY, m_radarCenterX, m_radarCenterY, activePlayer->EyeAngles().y - 90.0f, &rotatedX, &rotatedY );

        // these guys are off the radar. Go ahead and hide them.
        if( rotatedX > m_radarWidth || rotatedX < 0 || rotatedY > m_radarHeight || rotatedY < 0 )
        {
            blip.shown = false;
            continue;
        }

        blip.x = int( rotatedX );
        blip.y = int( rotatedY );
        blip.shown = true;
    }

    // Everyone moves at once or not at all, so one compare covers the lot.
    if( !( radarData == previous ) )
        RkHudDirty( "radar" );
}

void RkHudRadar::OnLoad()
{
    UpdateRadarSize();
}

RkHudRadar::RkHudRadar(const char *value) : RkHudDocument( value )
{
    SetHiddenBits( /* HIDEHUD_MISCSTATUS */ 0 );
}

RkHudRadar::~RkHudRadar() noexcept
{
    Unload();
}

void RkHudRadar::LevelInit()
{
    RkHudDocument::LevelInit();

    HOOK_HUD_MESSAGE( RkHudRadar, ProcessSpottedEntityUpdate );
}

void RkHudRadar::ShowPanel(bool bShow, bool force)
{
    if( !m_pDocument )
        return;

    if( bShow )
    {
        if( !m_bVisible )
        {
            m_pDocument->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
        }
        UpdateRadarFrame();
    }
    else
    {
        if( m_bVisible )
        {
            m_pDocument->Hide();
        }
    }

    m_bVisible = bShow;
}

void RkHudRadar::SetActive(bool bActive)
{
    ShowPanel( bActive, false );
    CHudElement::SetActive( bActive );
}

bool RkHudRadar::ShouldDraw()
{
    CCSPlayer *localPlayer = CCSPlayer::GetLocalCSPlayer();

    return localPlayer &&
           cl_drawhud.GetBool() &&
           ( localPlayer->IsAlive() || ( localPlayer->IsObserver() && localPlayer->GetObserverMode() == OBS_MODE_IN_EYE || localPlayer->GetObserverMode() == OBS_MODE_CHASE ) ) &&
           CHudElement::ShouldDraw();
}

class CHudElement;
class CCSPlayer;

using namespace ::ks::reflect;

DECLARE_HUDELEMENT( RkHudRoundTimer );

const char *RkHudRoundTimer::kDocument = "hud_roundtimer.rml";

// Struct layout for data-binding model.
struct RoundTimerData
{
    int MinutesLeft;
    int SecondsLeft;
    bool bombPlanted;
    int ctScore;
    int tScore;

    bool operator==( const RoundTimerData & ) const = default;
} roundTimerData;

static void BindRoundTimerData( Rml::DataModelConstructor &c )
{
    if ( auto h = c.RegisterStruct<RoundTimerData>() )
    {
        h.RegisterMember( "minutes_left", &RoundTimerData::MinutesLeft );
        h.RegisterMember( "seconds_left", &RoundTimerData::SecondsLeft );
        h.RegisterMember( "bomb_planted", &RoundTimerData::bombPlanted );
        h.RegisterMember( "ct_score", &RoundTimerData::ctScore );
        h.RegisterMember( "t_score", &RoundTimerData::tScore );
    }
    c.Bind( "roundtimer", &roundTimerData );
}
RK_HUD_SECTION( BindRoundTimerData );

RkHudRoundTimer::RkHudRoundTimer(const char *value) : RkHudDocument( value )
{
    SetHiddenBits( /* HIDEHUD_MISCSTATUS */ 0 );
}

RkHudRoundTimer::~RkHudRoundTimer() noexcept
{
    Unload();
}

void RkHudRoundTimer::ShowPanel(bool bShow, bool force)
{
    if( !m_pDocument )
        return;

    if( bShow )
    {
        if( !m_bVisible )
        {
            m_pDocument->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
        }

        const RoundTimerData previous = roundTimerData;

        int remainingTime;
        if ( CSGameRules()->IsFreezePeriod() )
        {
            // countdown to the start of the round while we're in freeze period
            remainingTime = (int)std::ceil( CSGameRules()->GetRoundStartTime() - gpGlobals->curtime );
        }
        else
        {
            remainingTime = (int)std::ceil( CSGameRules()->GetRoundRemainingTime() );
        }
        roundTimerData.SecondsLeft = remainingTime % 60;
        roundTimerData.MinutesLeft = remainingTime / 60;
        roundTimerData.ctScore = g_PR->GetTeamScore( TEAM_CT );
        roundTimerData.tScore = g_PR->GetTeamScore( TEAM_TERRORIST );
        roundTimerData.bombPlanted = CSGameRules()->m_bBombPlanted;

        // A clock that ticks once a second, polled every frame.
        if( !( roundTimerData == previous ) )
            RkHudDirty( "roundtimer" );
    }
    else
    {
        if( m_bVisible )
        {
            m_pDocument->Hide();
        }
    }

    m_bVisible = bShow;
}

void RkHudRoundTimer::SetActive(bool bActive)
{
    ShowPanel( bActive, false );
    CHudElement::SetActive( bActive );
}

bool RkHudRoundTimer::ShouldDraw()
{
    CCSPlayer *localPlayer = CCSPlayer::GetLocalCSPlayer();

    if( !localPlayer || localPlayer->GetTeamNumber() == TEAM_UNASSIGNED )
        return false;

    return cl_drawhud.GetBool() && CHudElement::ShouldDraw();
}

class CHudElement;
class CBaseViewModel;
class CCSPlayer;
class CWeaponCSBase;

using namespace ::ks::reflect;

DECLARE_HUDELEMENT_DEPTH( RkHudScope, 70 );

const char *RkHudScope::kDocument = "hud_scope.rml";

extern ConVar cl_crosshair_sniper_width;
ConVar cl_crosshair_sniper_show_normal_inaccuracy( side_registry(), "cl_crosshair_sniper_show_normal_inaccuracy", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_SS, "Include standing inaccuracy when determining sniper crosshair blur" );

// What hud_scope.rml binds to. Whole pixels, already clamped: the viewport rect
// and the shot-spread blur are weapon physics, worked out below, while what a
// rect *looks like* stays in the document.
struct ScopeData
{
    int screen_w, screen_h;
    int scope_x, scope_y, scope_w, scope_h, scope_x2;
    int top_h, bottom_y, bottom_h, left_w, right_w;
    int core_x, core_y, thickness;   // sharp cross (steady)
    int blur_x, blur_y, blur;        // soft sprite cross (moving)
    Rml::String opacity = "1";
    bool steady;

    bool operator==( const ScopeData & ) const = default;
} scopeData;

static void BindScope( Rml::DataModelConstructor &c )
{
    if ( auto h = c.RegisterStruct<ScopeData>() )
    {
        h.RegisterMember( "screen_w", &ScopeData::screen_w );
        h.RegisterMember( "screen_h", &ScopeData::screen_h );
        h.RegisterMember( "scope_x", &ScopeData::scope_x );
        h.RegisterMember( "scope_y", &ScopeData::scope_y );
        h.RegisterMember( "scope_w", &ScopeData::scope_w );
        h.RegisterMember( "scope_h", &ScopeData::scope_h );
        h.RegisterMember( "scope_x2", &ScopeData::scope_x2 );
        h.RegisterMember( "top_h", &ScopeData::top_h );
        h.RegisterMember( "bottom_y", &ScopeData::bottom_y );
        h.RegisterMember( "bottom_h", &ScopeData::bottom_h );
        h.RegisterMember( "left_w", &ScopeData::left_w );
        h.RegisterMember( "right_w", &ScopeData::right_w );
        h.RegisterMember( "core_x", &ScopeData::core_x );
        h.RegisterMember( "core_y", &ScopeData::core_y );
        h.RegisterMember( "thickness", &ScopeData::thickness );
        h.RegisterMember( "blur_x", &ScopeData::blur_x );
        h.RegisterMember( "blur_y", &ScopeData::blur_y );
        h.RegisterMember( "blur", &ScopeData::blur );
        h.RegisterMember( "opacity", &ScopeData::opacity );
        h.RegisterMember( "steady", &ScopeData::steady );
    }
    c.Bind( "scope", &scopeData );
}
RK_HUD_SECTION( BindScope );

RkHudScope::RkHudScope( const char *pElementName ) : RkHudDocument( pElementName ),
    m_fAnimInset( 1.0f ),
    m_fLineSpreadDistance( 1.0f )
{
    SetHiddenBits( HIDEHUD_PLAYERDEAD );
    SetIgnoreGlobalHudDisable( true );
}

RkHudScope::~RkHudScope() noexcept
{
    Unload();
}

void RkHudScope::UpdateScope()
{
    if( !m_pDocument )
        return;

    CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
    if( !pPlayer )
        return;

    if( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
    {
        pPlayer = ToCSPlayer( pPlayer->GetObserverTarget() );
        if( !pPlayer )
            return;
    }

    CWeaponCSBase *pWeapon = pPlayer->GetActiveCSWeapon();
    if( !pWeapon || pWeapon->GetWeaponType() != WEAPONTYPE_SNIPER_RIFLE )
        return;

    Rml::Context *ctx = RocketUI()->AccessHudContext();
    if( !ctx )
        return;

    Rml::Vector2i dim = ctx->GetDimensions();
    int screenWide = dim.x;
    int screenTall = dim.y;

    const float kScopeMinFOV = 25.0f;
    float flTargetFOVForZoom = std::max<std::common_type_t<int, float>>( pWeapon->GetZoomFOV( pWeapon->GetCSZoomLevel() ), kScopeMinFOV );

    // Reset animation when not scoped
    if( pPlayer->GetFOV() == pPlayer->GetDefaultFOV() && !pPlayer->m_bIsScoped )
    {
        m_fAnimInset = 2;
        m_fLineSpreadDistance = 20;
    }

    if( flTargetFOVForZoom == pPlayer->GetDefaultFOV() || !pPlayer->m_bIsScoped )
        return;

    CBaseViewModel *baseViewModel = pPlayer->GetViewModel( 0 );
    if( !baseViewModel )
        return;
    CPredictedViewModel *viewModel = dynamic_cast<CPredictedViewModel *>( baseViewModel );
    if( !viewModel )
        return;

    float fHalfFov = DEG2RAD( flTargetFOVForZoom ) * 0.5f;
    float fInaccuracyIn640x480Pixels = 320.0f / std::tanf( fHalfFov );

    float fWeaponInaccuracy = pWeapon->GetInaccuracy() + pWeapon->GetSpread();
    if( !cl_crosshair_sniper_show_normal_inaccuracy.GetBool() )
        fWeaponInaccuracy -= pWeapon->GetInaccuracyStand( Secondary_Mode ) + pWeapon->GetSpread();
    fWeaponInaccuracy = std::max<std::common_type_t<float, int>>( fWeaponInaccuracy, 0 );

    float fRawSpreadDistance = fWeaponInaccuracy * fInaccuracyIn640x480Pixels;
    float fSpreadDistance = clamp( fRawSpreadDistance, 0.0f, 100.0f );

    // Animate blur
    float flInsetGoal = fSpreadDistance * ( 0.4f / 30.0f );
    m_fAnimInset = Approach( flInsetGoal, m_fAnimInset, std::fabsf( ( flInsetGoal - m_fAnimInset ) * gpGlobals->frametime ) * 19.0f );
    m_fLineSpreadDistance = RemapValClamped( gpGlobals->frametime * 140.0f, 0.0f, 1.0f, m_fLineSpreadDistance, fRawSpreadDistance );

    float flAccuracyFishtail = pWeapon->GetAccuracyFishtail();
    int offsetX = (int)( viewModel->GetBobState().m_flRawLateralBob * ( screenTall / 14.0f ) + flAccuracyFishtail );
    int offsetY = (int)( viewModel->GetBobState().m_flRawVerticalBob * ( screenTall / 14.0f ) );

    float flInacDisplayBlur = m_fAnimInset * 0.04f;
    if( flInacDisplayBlur > 0.22f )
        flInacDisplayBlur = 0.22f;

    // Calculate scope bounds (same as original CHudScope)
    int inset = (int)( ( screenTall / 14.0f ) + ( flInacDisplayBlur * ( screenTall * 0.5f ) ) );

    // y1,y2 = top and bottom of scope viewport
    // x1,x2 = left and right of scope viewport (centered, square based on height)
    int y1 = inset + offsetY;
    int x1 = ( screenWide - screenTall ) / 2 + inset + offsetX;
    int y2 = screenTall - inset + offsetY;
    int x2 = screenWide - ( ( screenWide - screenTall ) / 2 + inset ) + offsetX;

    int centerX = ( screenWide / 2 ) + offsetX;
    int centerY = ( screenTall / 2 ) + offsetY;

    // --- publish -----------------------------------------------------------
    // The clamps stay here: std::max(y1,0) and friends are logic about a viewport
    // that can be pushed off-screen by the bob, not styling.
    ScopeData next;
    next.screen_w = screenWide;
    next.screen_h = screenTall;
    next.scope_x = x1;
    next.scope_y = y1;
    next.scope_w = x2 - x1;
    next.scope_h = y2 - y1;
    next.scope_x2 = x2;
    next.top_h = std::max( y1, 0 );
    next.bottom_y = y2;
    next.bottom_h = std::max( screenTall - y2, 0 );
    next.left_w = std::max( x1, 0 );
    next.right_w = std::max( screenWide - x2, 0 );

    // Reticle: faithful port of the legacy CHudScope -- a hard toggle between a
    // thin sharp solid line (steady) and Valve's scope_line_blur sprite (moving),
    // the sprite widening + fading with shot spread. The sprite's soft alpha
    // profile (vs a solid bar) is what keeps the viewport from darkening.
    int thickness = cl_crosshair_sniper_width.GetInt();
    if( thickness < 1 )
        thickness = 1;

    float fBlurWidth = std::powf( m_fLineSpreadDistance, 0.75f );
    float fScreenBlurWidth = fBlurWidth * screenTall / 640.0f;
    next.steady = ( fScreenBlurWidth <= thickness + 0.5f );

    float a = next.steady ? ( ( fBlurWidth < 1.0f ) ? 1.0f : 1.0f / fBlurWidth )
                          : ( ( fBlurWidth < 1.8f ) ? 1.0f : 1.8f / fBlurWidth );
    a = clamp( std::sqrtf( a ), 140.0f / 255.0f, 1.0f );
    char op[16];
    V_snprintf( op, sizeof(op), "%.3f", a );
    next.opacity = op;

    int bw = (int)( 2.0f * fScreenBlurWidth );
    if( bw < 3 )
        bw = 3; // keep the soft sprite from sampling to nothing near the toggle

    next.thickness = thickness;
    next.core_x = centerX - thickness / 2;
    next.core_y = centerY - thickness / 2;
    next.blur = bw;
    next.blur_x = centerX - bw / 2;
    next.blur_y = centerY - bw / 2;

    if( next == scopeData )
        return;

    scopeData = next;
    RkHudDirty( "scope" );
}

void RkHudScope::ShowPanel( bool bShow, bool force )
{
    if( !m_pDocument )
        return;

    if( bShow )
    {
        if( !m_bVisible )
            m_pDocument->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );

        UpdateScope();
    }
    else
    {
        if( m_bVisible )
            m_pDocument->Hide();
    }

    m_bVisible = bShow;
}

void RkHudScope::SetActive( bool bActive )
{
    ShowPanel( bActive, false );
    CHudElement::SetActive( bActive );
}

bool RkHudScope::ShouldDraw()
{
    CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
    if( !pPlayer )
        return false;

    if( pPlayer->GetObserverInterpState() == CCSPlayer::OBSERVER_INTERP_TRAVELING )
        return false;

    if( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
    {
        pPlayer = ToCSPlayer( pPlayer->GetObserverTarget() );
        if( !pPlayer )
            return false;
    }
    else if( pPlayer->GetObserverMode() != OBS_MODE_NONE )
    {
        return false;
    }

    CWeaponCSBase *pWeapon = pPlayer->GetActiveCSWeapon();
    if( !pWeapon || pWeapon->GetWeaponType() != WEAPONTYPE_SNIPER_RIFLE )
        return false;

    if( !pPlayer->m_bIsScoped )
        return false;

    return CHudElement::ShouldDraw();
}

class CHudElement;
class CCSPlayer;

using namespace ::ks::reflect;

DECLARE_HUDELEMENT( RkHudScoreboard );

const char *RkHudScoreboard::kDocument = "hud_scoreboard.rml";

// Struct layout for data-binding model.
struct PlayerEntry
{
    int entid;
    Rml::String name;
    int teamnum;

    int cash;
    int kills;
    int deaths;
    int assists;
    int ping;
    bool alive;

    bool operator==( const PlayerEntry & ) const = default;
};
struct ScoreboardData
{
    int tScore;
    int ctScore;
    int numSpecs;
    Rml::Vector<PlayerEntry> ctPlayers;
    Rml::Vector<PlayerEntry> tPlayers;

    bool operator==( const ScoreboardData & ) const = default;
} scoreboardData;

static void BindScoreboard( Rml::DataModelConstructor &c )
{
    if ( auto entry = c.RegisterStruct<PlayerEntry>() )
    {
        entry.RegisterMember( "entid", &PlayerEntry::entid );
        entry.RegisterMember( "name", &PlayerEntry::name );
        entry.RegisterMember( "teamnum", &PlayerEntry::teamnum );
        entry.RegisterMember( "cash", &PlayerEntry::cash );
        entry.RegisterMember( "kills", &PlayerEntry::kills );
        entry.RegisterMember( "deaths", &PlayerEntry::deaths );
        entry.RegisterMember( "assists", &PlayerEntry::assists );
        entry.RegisterMember( "ping", &PlayerEntry::ping );
        entry.RegisterMember( "alive", &PlayerEntry::alive );
    }
    c.RegisterArray<Rml::Vector<PlayerEntry>>();

    if ( auto h = c.RegisterStruct<ScoreboardData>() )
    {
        h.RegisterMember( "t_score", &ScoreboardData::tScore );
        h.RegisterMember( "ct_score", &ScoreboardData::ctScore );
        h.RegisterMember( "num_specs", &ScoreboardData::numSpecs );
        h.RegisterMember( "ctplayers", &ScoreboardData::ctPlayers );
        h.RegisterMember( "tplayers", &ScoreboardData::tPlayers );
    }
    c.Bind( "scoreboard", &scoreboardData );
}
RK_HUD_SECTION( BindScoreboard );

// Rows are data-driven; these two are the containers the update needs to reach.
void RkHudScoreboard::OnLoad()
{
    m_elemCtSection = m_pDocument->GetElementById( "ct" );
    m_elemTSection = m_pDocument->GetElementById( "t" );
    if ( !m_elemCtSection || !m_elemTSection )
        Warning( "hud_scoreboard.rml is missing the 'ct'/'t' sections\n" );
}

RkHudScoreboard::RkHudScoreboard(const char *value) : RkHudDocument( value )
{
    SetHiddenBits( /* HIDEHUD_MISCSTATUS */ 0 );
}

RkHudScoreboard::~RkHudScoreboard() noexcept
{
    Unload();
}

void RkHudScoreboard::Update()
{
    IGameResources *gr = GameResources();
    if( !gr )
        return;

    const ScoreboardData previous = scoreboardData;

    // Rebuilt from scratch rather than reconciled in place. The old version
    // searched both team arrays for an existing row, erased on team change while
    // iterating, and patched the loop counter to compensate -- for at most 64
    // rows that are only looked at while the board is held open. It also stopped
    // at `i < maxClients`, so the last slot never appeared.
    scoreboardData.ctPlayers.clear();
    scoreboardData.tPlayers.clear();

    for( int i = 1; i <= gpGlobals->maxClients; i++ )
    {
        if( !gr->IsConnected( i ) )
            continue;

        const int team = gr->GetTeam( i );
        if( team != TEAM_CT && team != TEAM_TERRORIST )
            continue;   // spectators don't get a row

        CCSPlayer *player = ToCSPlayer( UTIL_PlayerByIndex( i ) );
        if( !player )
            continue;

        PlayerEntry entry = {};
        entry.entid = i;
        entry.teamnum = team;
        entry.name = player->IsBot() ? ( Rml::String( "BOT " ) + gr->GetPlayerName( i ) )
                                     : Rml::String( gr->GetPlayerName( i ) );
        entry.cash = player->GetAccount();
        entry.kills = gr->GetKills( i );
        entry.deaths = gr->GetDeaths( i );
        entry.assists = g_PR->GetAssists( i );
        entry.ping = gr->GetPing( i );
        entry.alive = player->IsAlive();

        ( team == TEAM_CT ? scoreboardData.ctPlayers : scoreboardData.tPlayers )
            .push_back( entry );
    }

    scoreboardData.numSpecs = GetGlobalTeam( TEAM_SPECTATOR )->GetNumPlayers();
    scoreboardData.ctScore = gr->GetTeamScore( TEAM_CT );
    scoreboardData.tScore = gr->GetTeamScore( TEAM_TERRORIST );

    // Re-scanned every frame the board is held open; the numbers on it change on
    // kills and buys. Comparing the rows is far cheaper than rebuilding them.
    if( !( scoreboardData == previous ) )
        RkHudDirty( "scoreboard" );
}

// this is called every frame, keep that in mind.
void RkHudScoreboard::ShowPanel( bool bShow, bool force )
{
    if( !m_pDocument )
        return;

    if( bShow )
    {
        // Update the information every frame while the scoreboard is open.
        Update();

        if( !m_bVisible )
        {
            m_pDocument->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
        }
    }
    else
    {
        if( m_bVisible )
        {
            m_pDocument->Hide();
        }
    }

    m_bVisible = bShow;
}

void RkHudScoreboard::SetActive(bool bActive)
{
    ShowPanel( bActive, false );
    CHudElement::SetActive( bActive );
}

bool RkHudScoreboard::ShouldDraw()
{
    int buttons = input->GetButtonBits( false );

    if( !(buttons & IN_SCORE) )
        return false;

    return cl_drawhud.GetBool() && CHudElement::ShouldDraw();
}

class CBasePlayer;

using namespace ::ks::reflect;

Rml::ElementDocument *RocketTeamMenuDocument::m_pInstance = nullptr;
bool RocketTeamMenuDocument::m_bVisible = false;
RocketTeamMenuEventListener* RocketTeamMenuDocument::m_pEventListener = nullptr;

// One action with the team as an argument, named by the buttons themselves in
// hud_teammenu.rml (data-event-click="jointeam( 3 )").
static void BindTeamMenu( Rml::DataModelConstructor &c )
{
    c.BindEventCallback( "jointeam", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments ) {
        if ( arguments.empty() )
            return;

        RocketTeamMenuDocument::ShowPanel( false );

        char command[32];
        V_snprintf( command, sizeof( command ), "jointeam %d", arguments[0].Get<int>() );
        engine->ClientCmd_Unrestricted( command );
    } );
}
RK_HUD_SECTION( BindTeamMenu );

void RocketTeamMenuEventListener::StartAlwaysListenEvents()
{
    ListenForGameEvent( "jointeam_failed" );
    ListenForGameEvent( "player_spawned" );
    ListenForGameEvent( "teamchange_pending" );
}

void RocketTeamMenuEventListener::StopAlwaysListenEvents()
{
    StopListeningForAllEvents();
}

//listen for a few events related to joining teams.
void RocketTeamMenuEventListener::FireGameEvent(IGameEvent *event)
{
    const char *type = event->GetName();

    if( !V_strcmp( type, "jointeam_failed" ) )
    {
        ConMsg("oy vey the jointeam failed\n");
    }
    else if( !V_strcmp( type, "player_spawned" ) )
    {
        CBasePlayer *localPlayer = CBasePlayer::GetLocalPlayer();
        // If this was us.
        if( localPlayer && localPlayer->GetUserID() == event->GetInt( "userid" ) )
        {
            RocketTeamMenuDocument::ShowPanel( false, false );
        }
    }
    else if( !V_strcmp( type, "teamchange_pending" ) )
    {
        RocketTeamMenuDocument::ShowPanel( false, false );
    }
}

void RocketTeamMenuDocument::LoadDialog()
{
    if( m_pInstance || !RkPanelLoad( m_pInstance, ROCKET_CONTEXT_HUD, "hud_teammenu.rml", LoadDialog, UnloadDialog ) )
        return;

    // Joining can fail or be pre-empted; the menu closes itself when it does.
    m_pEventListener = new RocketTeamMenuEventListener;
    m_pEventListener->StartAlwaysListenEvents();
}

void RocketTeamMenuDocument::UnloadDialog()
{
    if( m_pEventListener )
    {
        m_pEventListener->StopListeningForAllEvents();
        delete m_pEventListener;
        m_pEventListener = nullptr;
    }

    RkPanelUnload( m_pInstance, m_bVisible );
}

void RocketTeamMenuDocument::ShowPanel(bool bShow, bool immediate)
{
    if( bShow )
        LoadDialog();

    RkPanelShow( m_pInstance, m_bVisible, bShow );
}

using namespace ::ks::reflect;

Rml::ElementDocument *RocketMainMenuDocument::m_pInstance = nullptr;
bool RocketMainMenuDocument::showing = false;

// menu.rml names these on the buttons (data-event-click).
static void BindMainMenu( Rml::DataModelConstructor &c )
{
    c.BindEventCallback( "show_options", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & ) {
        RocketOptionsDocument::ShowPanel( true );
    } );
    c.BindEventCallback( "show_play", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & ) {
        RocketPlayDocument::ShowPanel( true );
    } );
}
RK_HUD_SECTION( BindMainMenu );

void RocketMainMenuDocument::LoadDialog()
{
    RkPanelLoad( m_pInstance, ROCKET_CONTEXT_MENU, "menu.rml", LoadDialog, UnloadDialog );
}

void RocketMainMenuDocument::UnloadDialog()
{
    RkPanelUnload( m_pInstance, showing );
}

void RocketMainMenuDocument::RestorePanel()
{
    // Re-show the main menu (ShowPanel loads it first if it isn't up yet).
    ShowPanel( true );
}

// Reached before the first frame in some paths, so it loads for the caller.
void RocketMainMenuDocument::ShowPanel(bool bShow, bool immediate)
{
    if( bShow )
        LoadDialog();

    RkPanelShow( m_pInstance, showing, bShow );
}

using namespace ::ks::reflect;

Rml::ElementDocument *RkPanelLoad( Rml::ElementDocument *&doc, RocketDesinationContext_t ctx,
                                   const char *file, LoadDocumentFn onLoad,
                                   UnloadDocumentFn onUnload )
{
    if ( doc )
        return doc;

    doc = RkLoadDocument( ctx, file, onLoad, onUnload );
    return doc;
}

void RkPanelUnload( Rml::ElementDocument *&doc, bool &visible )
{
    if ( doc )
    {
        doc->Close();
        doc = nullptr;
    }

    visible = false;
}

void RkPanelShow( Rml::ElementDocument *doc, bool &visible, bool show )
{
    if ( doc )
    {
        if ( show )
            doc->Show( Rml::ModalFlag::None, Rml::FocusFlag::Auto, Rml::ScrollFlag::None );
        else
            doc->Hide();
    }

    visible = show;
}

using namespace ::ks::reflect;

extern IGameUIFuncs *gameuifuncs;

Rml::ElementDocument *RocketOptionsDocument::m_pInstance = nullptr;
bool RocketOptionsDocument::m_bVisible = false;
bool RocketOptionsDocument::m_bPopulating = false;

// Push the current ConVar value into one control. The mirror of
// ApplyConVarFromControl, which reads it back out on change.
static void PopulateControlFromConVar( Rml::Element *elem )
{
    const Rml::String convarName = elem->GetAttribute<Rml::String>( "data-convar", "" );
    if ( convarName.empty() )
        return;

    ConVarRef cvar( convarName.c_str() );
    if ( !cvar.IsValid() )
        return;

    const Rml::String tag = elem->GetTagName();
    const Rml::String type = elem->GetAttribute<Rml::String>( "type", "" );
    char valBuf[32];

    if ( tag == "select" )
    {
        V_snprintf( valBuf, sizeof( valBuf ), "%d", cvar.GetInt() );
        if ( auto *sel = rmlui_dynamic_cast<Rml::ElementFormControlSelect *>( elem ) )
            sel->SetValue( Rml::String( valBuf ) );
    }
    else if ( type == "checkbox" )
    {
        if ( cvar.GetBool() )
            elem->SetAttribute( "checked", "" );
        else
            elem->RemoveAttribute( "checked" );
    }
    else if ( type == "range" )
    {
        V_snprintf( valBuf, sizeof( valBuf ), "%.2f", cvar.GetFloat() );
        if ( auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>( elem ) )
            input->SetValue( Rml::String( valBuf ) );

        char labelId[64];
        V_snprintf( labelId, sizeof( labelId ), "%s_val", elem->GetId().c_str() );
        if ( Rml::ElementDocument *doc = elem->GetOwnerDocument() )
            if ( Rml::Element *label = doc->GetElementById( labelId ) )
                label->SetInnerRML( Rml::String( valBuf ) );
    }
}

// Walk the document once instead of naming fifteen controls by hand.
static void PopulateConVarControls( Rml::Element *elem )
{
    if ( !elem )
        return;

    PopulateControlFromConVar( elem );

    for ( int i = 0; i < elem->GetNumChildren(); i++ )
        PopulateConVarControls( elem->GetChild( i ) );
}

// Forward declaration - defined below PopulateControls
static void ApplyVideoMode( Rml::ElementDocument *doc );

// Every control in panel_options.rml names what it does (data-event-click /
// data-event-change), so the two Rml::EventListener subclasses that used to sit
// here -- one walking a chain of `id == "..."` comparisons for the tabs and
// buttons, the other another chain for the settings -- are gone. What is left is
// the part that was never dispatch: the settings that are not a single ConVar.

// A control whose value *is* one ConVar says so with data-convar, and this is
// both halves of that: read on populate, written on change. Adding such an option
// is an attribute in the .rml and nothing here.
static void ApplyConVarFromControl( Rml::Element *target, const Rml::String &value )
{
    const Rml::String convarName = target->GetAttribute<Rml::String>( "data-convar", "" );
    if ( convarName.empty() )
        return;

    ConVarRef cvar( convarName.c_str() );
    if ( !cvar.IsValid() )
        return;

    const Rml::String tag = target->GetTagName();
    const Rml::String type = target->GetAttribute<Rml::String>( "type", "" );

    if ( tag == "select" )
    {
        cvar.SetValue( atoi( value.c_str() ) );
    }
    else if ( type == "checkbox" )
    {
        cvar.SetValue( target->HasAttribute( "checked" ) ? 1 : 0 );
    }
    else if ( type == "range" )
    {
        const float fVal = (float)atof( value.c_str() );
        cvar.SetValue( fVal );

        // Sliders show their number in a sibling <id>_val.
        char labelId[64];
        V_snprintf( labelId, sizeof( labelId ), "%s_val", target->GetId().c_str() );
        if ( Rml::ElementDocument *doc = target->GetOwnerDocument() )
        {
            if ( Rml::Element *label = doc->GetElementById( labelId ) )
            {
                char valBuf[32];
                V_snprintf( valBuf, sizeof( valBuf ), "%.2f", fVal );
                label->SetInnerRML( Rml::String( valBuf ) );
            }
        }
    }
}

static void BindOptions( Rml::DataModelConstructor &c )
{
    using Args = const Rml::VariantList &;

    c.BindEventCallback( "options_tab", []( Rml::DataModelHandle, Rml::Event &ev, Args args ) {
        if ( !args.empty() )
            RocketOptionsDocument::SwitchTab( args[0].Get<Rml::String>().c_str() );
    } );
    c.BindEventCallback( "options_close", []( Rml::DataModelHandle, Rml::Event &, Args ) {
        RocketOptionsDocument::ShowPanel( false );
    } );
    c.BindEventCallback( "options_save", []( Rml::DataModelHandle, Rml::Event &, Args ) {
        Msg( "Saving settings.\n" );
        engine->ClientCmd_Unrestricted( "mat_savechanges" );
        engine->ClientCmd_Unrestricted( "host_writeconfig" );
    } );

    // Resolution and display mode go through mat_setvideomode together; changing
    // the mode also re-filters the resolution list (windowed hides modes larger
    // than the desktop).
    c.BindEventCallback( "apply_video_mode", []( Rml::DataModelHandle, Rml::Event &ev, Args ) {
        if ( RocketOptionsDocument::m_bPopulating )
            return;
        Rml::Element *target = ev.GetTargetElement();
        if ( !target )
            return;
        ApplyVideoMode( target->GetOwnerDocument() );
        if ( target->GetId() == "display_mode" )
            RocketOptionsDocument::PopulateResolution();
    } );

    // Settings that are not one ConVar.
    c.BindEventCallback( "apply_snd_quality", []( Rml::DataModelHandle, Rml::Event &ev, Args ) {
        if ( RocketOptionsDocument::m_bPopulating )
            return;
        const int quality = atoi( ev.GetParameter<Rml::String>( "value", "" ).c_str() );
        ConVarRef sndPitch( "Snd_PitchQuality" );
        ConVarRef dspSlow( "dsp_slow_cpu" );
        ConVarRef dspStereo( "dsp_enhance_stereo" );
        dspSlow.SetValue( quality == 0 );
        sndPitch.SetValue( quality >= 2 );
        if ( dspStereo.IsValid() )
            dspStereo.SetValue( 0 );
    } );

    c.BindEventCallback( "apply_closedcaptions", []( Rml::DataModelHandle, Rml::Event &ev, Args ) {
        if ( RocketOptionsDocument::m_bPopulating )
            return;
        const int ccMode = atoi( ev.GetParameter<Rml::String>( "value", "" ).c_str() );
        ConVarRef ccSub( "cc_subtitles" );
        ccSub.SetValue( ccMode == 2 ? 1 : 0 );

        char cmd[64];
        V_snprintf( cmd, sizeof( cmd ), "closecaption %d", ccMode ? 1 : 0 );
        engine->ClientCmd_Unrestricted( cmd );
    } );

    // Reverse mouse is the *sign* of m_pitch, acceleration is m_customaccel 0/3.
    c.BindEventCallback( "apply_m_pitch", []( Rml::DataModelHandle, Rml::Event &ev, Args ) {
        if ( RocketOptionsDocument::m_bPopulating )
            return;
        Rml::Element *target = ev.GetTargetElement();
        ConVarRef pitch( "m_pitch" );
        if ( !target || !pitch.IsValid() )
            return;
        float absVal = std::fabsf( pitch.GetFloat() );
        if ( absVal < 0.0001f )
            absVal = 0.022f;
        pitch.SetValue( target->HasAttribute( "checked" ) ? -absVal : absVal );
    } );

    c.BindEventCallback( "apply_m_customaccel", []( Rml::DataModelHandle, Rml::Event &ev, Args ) {
        if ( RocketOptionsDocument::m_bPopulating )
            return;
        Rml::Element *target = ev.GetTargetElement();
        ConVarRef accel( "m_customaccel" );
        if ( target && accel.IsValid() )
            accel.SetValue( target->HasAttribute( "checked" ) ? 3 : 0 );
    } );

    // Everything else: one control, one ConVar, named by data-convar.
    c.BindEventCallback( "apply_convar", []( Rml::DataModelHandle, Rml::Event &ev, Args ) {
        if ( RocketOptionsDocument::m_bPopulating )
            return;
        if ( Rml::Element *target = ev.GetTargetElement() )
            ApplyConVarFromControl( target, ev.GetParameter<Rml::String>( "value", "" ) );
    } );
}
RK_HUD_SECTION( BindOptions );

void RocketOptionsDocument::SwitchTab( const char *tabId )
{
    if( !m_pInstance )
        return;

    static const char *tabIds[] = { "tab-video", "tab-audio", "tab-mouse" };
    static const char *pageIds[] = { "page-video", "page-audio", "page-mouse" };

    for( int i = 0; i < 3; i++ )
    {
        Rml::Element *tab = m_pInstance->GetElementById( tabIds[i] );
        Rml::Element *page = m_pInstance->GetElementById( pageIds[i] );

        if( !V_strcmp( tabId, tabIds[i] ) )
        {
            if( tab ) tab->SetClass( "active", true );
            if( page ) page->SetClass( "visible", true );
        }
        else
        {
            if( tab ) tab->SetClass( "active", false );
            if( page ) page->SetClass( "visible", false );
        }
    }
}

void RocketOptionsDocument::PopulateResolution()
{
    if( !m_pInstance || !gameuifuncs )
        return;

    Rml::Element *elem = m_pInstance->GetElementById( "resolution" );
    if( !elem )
        return;

    auto *sel = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( elem );
    if( !sel )
        return;

    // Get current render resolution
    int curWidth = 0, curHeight = 0;
    if( materials )
        materials->GetBackBufferDimensions( curWidth, curHeight );

    // Get desktop resolution of the current display
    int desktopW = 0, desktopH = 0;
    gameuifuncs->GetDesktopResolution( desktopW, desktopH );

    // Check if windowed — only filter by desktop resolution in windowed mode
    // In fullscreen (Wayland FULLSCREEN_DESKTOP), resolution is internal render
    // resolution so any mode should be available
    bool bWindowed = false;
    if( materials )
        bWindowed = materials->GetCurrentConfigForVideoCard().Windowed();

    // Get available video modes from the engine
    vmode_t *pModes = nullptr;
    int nModes = 0;
    gameuifuncs->GetVideoModes( &pModes, &nModes );

    // Clear all existing options from the selectbox widget
    sel->RemoveAll();

    int selectedIdx = -1;
    int bestMatch = std::numeric_limits<int>::max();
    int optionCount = 0;

    // Check if current backbuffer resolution is in the mode list.
    // The engine may have auto-adjusted to a display's native resolution
    // (e.g. 1440p) that wasn't in the driver's init-time mode list.
    bool bBackbufferInList = false;
    for( int i = 0; i < nModes; i++ )
    {
        if( pModes[i].width == curWidth && pModes[i].height == curHeight )
        {
            bBackbufferInList = true;
            break;
        }
    }

    // Build options from the driver's mode list (already sorted ascending by engine).
    // Insert the current backbuffer resolution at its sorted position if missing.
    Rml::String optionsRml;
    bool bInsertedBackbuffer = bBackbufferInList;
    for( int i = 0; i < nModes; i++ )
    {
        int w = pModes[i].width;
        int h = pModes[i].height;

        // Don't show modes bigger than the desktop for windowed mode (matches VGUI)
        if( bWindowed && desktopW > 0 && desktopH > 0 && ( w > desktopW || h > desktopH ) )
            continue;

        // Insert backbuffer resolution at its sorted position
        if( !bInsertedBackbuffer && curWidth > 0 && curHeight > 0 &&
            ( w > curWidth || ( w == curWidth && h > curHeight ) ) )
        {
            char opt[128];
            V_snprintf( opt, sizeof(opt), "<option value=\"%dx%d\">%dx%d</option>", curWidth, curHeight, curWidth, curHeight );
            optionsRml += opt;
            selectedIdx = optionCount;
            optionCount++;
            bInsertedBackbuffer = true;
        }

        char opt[128];
        V_snprintf( opt, sizeof(opt), "<option value=\"%dx%d\">%dx%d</option>", w, h, w, h );
        optionsRml += opt;

        int diff = abs( w - curWidth ) + abs( h - curHeight );
        if( diff < bestMatch )
        {
            bestMatch = diff;
            selectedIdx = optionCount;
        }
        optionCount++;
    }

    // Backbuffer resolution is higher than all modes — append at end
    if( !bInsertedBackbuffer && curWidth > 0 && curHeight > 0 )
    {
        char opt[128];
        V_snprintf( opt, sizeof(opt), "<option value=\"%dx%d\">%dx%d</option>", curWidth, curHeight, curWidth, curHeight );
        optionsRml += opt;
        selectedIdx = optionCount;
        optionCount++;
    }

    // Inject as RML — options become DOM children, then MoveChildren() moves them
    // to the selectbox widget (same path as static RML options)
    elem->SetInnerRML( optionsRml );

    if( selectedIdx >= 0 )
        sel->SetSelection( selectedIdx );
}

void RocketOptionsDocument::PopulateDisplayMode()
{
    if( !m_pInstance || !materials )
        return;

    const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

    Rml::Element *elem = m_pInstance->GetElementById( "display_mode" );
    if( !elem )
        return;

    auto *sel = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( elem );
    if( !sel )
        return;

    sel->SetValue( config.Windowed() ? "1" : "0" );
}

// Apply resolution + display mode via mat_setvideomode
static void ApplyVideoMode( Rml::ElementDocument *doc )
{
    if( !doc )
        return;

    // Read resolution select
    Rml::Element *resElem = doc->GetElementById( "resolution" );
    Rml::Element *modeElem = doc->GetElementById( "display_mode" );
    if( !resElem || !modeElem )
        return;

    auto *resSel = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( resElem );
    auto *modeSel = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( modeElem );
    if( !resSel || !modeSel )
        return;

    Rml::String resValue = resSel->GetValue();
    Rml::String modeValue = modeSel->GetValue();

    // Parse "WxH" from resolution value
    int w = 0, h = 0;
    if( sscanf( resValue.c_str(), "%dx%d", &w, &h ) != 2 || w <= 0 || h <= 0 )
        return;

    int windowed = atoi( modeValue.c_str() );

    char cmd[128];
    V_snprintf( cmd, sizeof(cmd), "mat_setvideomode %d %d %d", w, h, windowed );
    engine->ClientCmd_Unrestricted( cmd );
}

void RocketOptionsDocument::PopulateControls()
{
    if( !m_pInstance )
        return;

    // Suppress change events while syncing UI to current state
    m_bPopulating = true;

    // Video settings - resolution and display mode
    PopulateResolution();
    PopulateDisplayMode();

    // Every control that names a ConVar with data-convar, in one pass.
    PopulateConVarControls( m_pInstance );

    // Audio settings - composite: sound quality
    {
        ConVarRef sndPitch( "Snd_PitchQuality" );
        ConVarRef dspSlow( "dsp_slow_cpu" );
        int quality = 0; // Low
        if( dspSlow.IsValid() && !dspSlow.GetBool() )
            quality = 1; // Medium
        if( sndPitch.IsValid() && sndPitch.GetBool() )
            quality = 2; // High

        Rml::Element *elem = m_pInstance->GetElementById( "snd_quality" );
        if( elem )
        {
            auto *sel = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( elem );
            if( sel )
            {
                char valBuf[8];
                V_snprintf( valBuf, sizeof(valBuf), "%d", quality );
                sel->SetValue( Rml::String( valBuf ) );
            }
        }
    }

    // Audio settings - composite: closed captions
    {
        ConVarRef closecaption( "closecaption" );
        ConVarRef ccSub( "cc_subtitles" );
        int ccMode = 0; // Disabled
        if( closecaption.IsValid() && closecaption.GetBool() )
        {
            if( ccSub.IsValid() && ccSub.GetBool() )
                ccMode = 2; // Subtitles Only
            else
                ccMode = 1; // Subtitles & Sound Effects
        }

        Rml::Element *elem = m_pInstance->GetElementById( "closedcaptions" );
        if( elem )
        {
            auto *sel = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( elem );
            if( sel )
            {
                char valBuf[8];
                V_snprintf( valBuf, sizeof(valBuf), "%d", ccMode );
                sel->SetValue( Rml::String( valBuf ) );
            }
        }
    }

    // Mouse settings - reverse mouse (positive m_pitch = not reversed)
    {
        ConVarRef pitch( "m_pitch" );
        Rml::Element *elem = m_pInstance->GetElementById( "m_pitch" );
        if( elem && pitch.IsValid() )
        {
            if( pitch.GetFloat() < 0.0f )
                elem->SetAttribute( "checked", "" );
            else
                elem->RemoveAttribute( "checked" );
        }
    }

    // Mouse settings - acceleration (m_customaccel: 0=off, 3=on)
    {
        ConVarRef accel( "m_customaccel" );
        Rml::Element *elem = m_pInstance->GetElementById( "m_customaccel" );
        if( elem && accel.IsValid() )
        {
            if( accel.GetInt() != 0 )
                elem->SetAttribute( "checked", "" );
            else
                elem->RemoveAttribute( "checked" );
        }
    }

    m_bPopulating = false;
}

void RocketOptionsDocument::LoadDialog()
{
    // Transient panel: rebuild here so it lands in the context current now.
    //
    // The controls fire `change` as they initialise, and those events reach us
    // through the document itself (data-event-change) rather than a listener
    // attached afterwards -- so the guard has to be up before the load, or the
    // RML's default values would be written into the ConVars. PopulateControls
    // drops it again.
    m_bPopulating = true;
    UnloadDialog();

    if( !RkPanelLoad( m_pInstance, ROCKET_CONTEXT_CURRENT, "panel_options.rml", LoadDialog, UnloadDialog ) )
    {
        m_bPopulating = false;
        return;
    }

    PopulateControls();
}

void RocketOptionsDocument::UnloadDialog()
{
    RkPanelUnload( m_pInstance, m_bVisible );
}

void RocketOptionsDocument::ShowPanel( bool bShow, bool immediate )
{
    if( bShow )
        LoadDialog();   // rebuilds the panel, re-reading the ConVars

    RkPanelShow( m_pInstance, m_bVisible, bShow );
}

using namespace ::ks::reflect;

Rml::ElementDocument *RocketPlayDocument::m_pInstance = nullptr;
bool RocketPlayDocument::m_bVisible = false;

// panel_play.rml names these on its buttons (data-event-click).
static void BindPlayPanel( Rml::DataModelConstructor &c )
{
    c.BindEventCallback( "start_server", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & ) {
        RocketPlayDocument::StartServer();
    } );
    c.BindEventCallback( "close_play", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & ) {
        RocketPlayDocument::ShowPanel( false );
    } );
}
RK_HUD_SECTION( BindPlayPanel );

void RocketPlayDocument::PopulateMapList()
{
    if( !m_pInstance )
        return;

    Rml::Element *elem = m_pInstance->GetElementById( "maplist" );
    if( !elem )
        return;

    auto *sel = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( elem );
    if( !sel )
        return;

    sel->RemoveAll();

    // Scan for .bsp files in the maps/ directory, same as VGUI's
    // CCreateMultiplayerGameServerPage::LoadMaps
    FileFindHandle_t findHandle = 0;
    const char *pszFilename = g_pFullFileSystem->FindFirst( "maps/*.bsp", &findHandle );

    Rml::String optionsRml;
    int count = 0;

    while( pszFilename )
    {
        char mapname[256];
        V_strncpy( mapname, pszFilename, sizeof(mapname) );

        // Strip .bsp extension
        char *ext = V_strstr( mapname, ".bsp" );
        if( ext )
            *ext = 0;

        char opt[256];
        V_snprintf( opt, sizeof(opt), "<option value=\"%s\">%s</option>", mapname, mapname );
        optionsRml += opt;
        count++;

        pszFilename = g_pFullFileSystem->FindNext( findHandle );
    }
    g_pFullFileSystem->FindClose( findHandle );

    if( count > 0 )
    {
        elem->SetInnerRML( optionsRml );
        sel->SetSelection( 0 );
    }
}

void RocketPlayDocument::StartServer()
{
    if( !m_pInstance )
        return;

    // Get map selection
    Rml::Element *mapElem = m_pInstance->GetElementById( "maplist" );
    if( !mapElem )
        return;

    auto *mapSel = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( mapElem );
    if( !mapSel )
        return;

    Rml::String mapName = mapSel->GetValue();
    if( mapName.empty() )
        return;

    // Get max players
    Rml::Element *playersElem = m_pInstance->GetElementById( "maxplayers" );
    int maxPlayers = 16;
    if( playersElem )
    {
        auto *playersSel = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( playersElem );
        if( playersSel )
            maxPlayers = atoi( playersSel->GetValue().c_str() );
    }

    // Get hostname
    Rml::Element *hostnameElem = m_pInstance->GetElementById( "hostname" );
    Rml::String hostName = "Counter-Strike: Global Offensive";
    if( hostnameElem )
    {
        auto *hostnameInput = rmlui_dynamic_cast<Rml::ElementFormControlInput*>( hostnameElem );
        if( hostnameInput )
        {
            Rml::String val = hostnameInput->GetValue();
            if( !val.empty() )
                hostName = val;
        }
    }

    // Get password
    Rml::Element *passwordElem = m_pInstance->GetElementById( "sv_password" );
    Rml::String password;
    if( passwordElem )
    {
        auto *passwordInput = rmlui_dynamic_cast<Rml::ElementFormControlInput*>( passwordElem );
        if( passwordInput )
            password = passwordInput->GetValue();
    }

    // Build the command string, matching VGUI's CCreateMultiplayerGameDialog::OnOK
    char szMapCommand[1024];
    V_snprintf( szMapCommand, sizeof(szMapCommand),
        "disconnect\nwait\nwait\nsv_lan 1\nsetmaster enable\nmaxplayers %i\nsv_password \"%s\"\nhostname \"%s\"\nprogress_enable\nmap %s\n",
        maxPlayers,
        password.c_str(),
        hostName.c_str(),
        mapName.c_str()
    );

    // Hide; this runs inside the panel's own data-event callback, so it must not
    // close (see rkpanel.h).
    ShowPanel( false );

    engine->ClientCmd_Unrestricted( szMapCommand );
}

void RocketPlayDocument::LoadDialog()
{
    // Transient panel: rebuild here so it lands in the context current now.
    UnloadDialog();

    if( !RkPanelLoad( m_pInstance, ROCKET_CONTEXT_CURRENT, "panel_play.rml", LoadDialog, UnloadDialog ) )
        return;

    PopulateMapList();
}

void RocketPlayDocument::UnloadDialog()
{
    RkPanelUnload( m_pInstance, m_bVisible );
}

void RocketPlayDocument::ShowPanel( bool bShow, bool immediate )
{
    if( bShow )
        LoadDialog();   // rebuilds the panel, re-scanning the maps folder

    RkPanelShow( m_pInstance, m_bVisible, bShow );
}

using namespace ::ks::reflect;

static void rocket_test_popup( const CCommand &args );
static ConCommand rocket_test_popup_command( side_registry(), "rocket_test_popup", rocket_test_popup, "Create a Test Popup in RocketUI" );
static void rocket_test_popup( const CCommand &args )
{
	if ( args.ArgC() != 3 )
	{
        Msg( "Example usage: rocket_test_popup <messages> <timeout>" );
	}
	else
    {
        float timeout = V_atof(args.ArgV()[2]);
        const char *text = args.ArgV()[1];

        Msg( "Created new Popup...(%f) seconds\n", timeout);
        new RocketPopupDocument( text, timeout );
    }
}

void RocketPopupDocument::ProcessEvent(Rml::Event &event)
{
    if( event.GetId() == Rml::EventId::Animationend )
    {
        // Animation is over, this means it's time to close
        delete this;
    }
}

RocketPopupDocument::RocketPopupDocument(const char *text, float timeout)
{
    V_strncpy( m_textBuffer, text, MAX_POPUP_TEXT );
    m_fTimeout = timeout;

    m_pInstance = RkLoadDocument( ROCKET_CONTEXT_CURRENT, "panel_popup.rml" );
    m_pInstance->AddEventListener(Rml::EventId::Animationend, this );
    m_pInstance->GetElementById("text")->SetInnerRML( m_textBuffer );
    m_pInstance->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
    m_pInstance->Animate("opacity", Rml::Property(0.0f, Rml::Unit::NUMBER), timeout, Rml::Tween(Rml::Tween::Quadratic));
}

RocketPopupDocument::~RocketPopupDocument()
{
    m_pInstance->RemoveEventListener( Rml::EventId::Animationend, this );
    m_pInstance->Close();
    m_pInstance = nullptr;
}
