// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/libxml_utils.h"
#include "chrome/common/win_util.h"
#include "chrome/test/automated_ui_tests/automated_ui_tests.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/views/view.h"
#include "googleurl/src/gurl.h"

namespace {

const wchar_t* const kReproSwitch = L"key";

const wchar_t* const kReproRepeatSwitch = L"num-reproductions";

const wchar_t* const kInputFilePathSwitch = L"input";

const wchar_t* const kOutputFilePathSwitch = L"output";

const wchar_t* const kDebugModeSwitch = L"debug";

const wchar_t* const kWaitSwitch = L"wait-after-action";

const wchar_t* const kDefaultInputFilePath = L"C:\\automated_ui_tests.txt";

const wchar_t* const kDefaultOutputFilePath
  = L"C:\\automated_ui_tests_error_report.txt";

const int kDebuggingTimeoutMsec = 5000;

// How many commands to run when testing a dialog box.
const int kTestDialogActionsToRun = 7;

}  // namespace

// This subset of commands is used to test dialog boxes, which aren't likely
// to respond to most other commands.
const std::string kTestDialogPossibleActions[] = {
  // See FuzzyTestDialog for details on why Enter and SpaceBar must appear first
  // in this list.
  "PressEnterKey",
  "PressSpaceBar",
  "PressTabKey",
  "DownArrow"
};

// The list of dialogs that can be shown.
const std::string kDialogs[] = {
  "About",
  "Options",
  "TaskManager",
  "JavaScriptDebugger",
  "JavaScriptConsole",
  "ClearBrowsingData",
  "ImportSettings",
  "EditSearchEngines",
  "ViewPasswords"
};

AutomatedUITest::AutomatedUITest()
    : total_crashes_(0),
      debug_logging_enabled_(false),
      post_action_delay_(0) {
  show_window_ = true;
  GetSystemTimeAsFileTime(&test_start_time_);
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (parsed_command_line.HasSwitch(kDebugModeSwitch))
    debug_logging_enabled_ = true;
  if (parsed_command_line.HasSwitch(kWaitSwitch)) {
    std::wstring str = parsed_command_line.GetSwitchValue(kWaitSwitch);
    if (str.empty()) {
      post_action_delay_ = 1;
    } else {
      post_action_delay_ = static_cast<int>(StringToInt64(str));
    }
  }
}

AutomatedUITest::~AutomatedUITest() {}

void AutomatedUITest::RunReproduction() {
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  xml_writer_.StartWriting();
  xml_writer_.StartElement("Report");
  std::string action_string =
      WideToASCII(parsed_command_line.GetSwitchValue(kReproSwitch));

  int64 num_reproductions = 1;
  if (parsed_command_line.HasSwitch(kReproRepeatSwitch)) {
    std::wstring num_reproductions_string =
        parsed_command_line.GetSwitchValue(kReproRepeatSwitch);
    std::string test = WideToASCII(num_reproductions_string);
    num_reproductions = StringToInt64(num_reproductions_string);
  }
  std::vector<std::string> actions;
  SplitString(action_string, L',', &actions);
  bool did_crash = false;
  bool command_complete = false;

  for (int64 i = 0; i < num_reproductions && !did_crash; ++i) {
    bool did_teardown = false;
    xml_writer_.StartElement("Executed");
    for (size_t j = 0; j < actions.size(); ++j) {
      DoAction(actions[j]);
      if (DidCrash(true)) {
        did_crash = true;
        if (j >= (actions.size() - 1))
          command_complete = true;
        break;
      }
      if (LowerCaseEqualsASCII(actions[j], "teardown"))
        did_teardown = true;
    }

    // Force proper teardown after each run, if it didn't already happen. But
    // don't teardown after crashes.
    if (!did_teardown && !did_crash)
      DoAction("TearDown");

    xml_writer_.EndElement();  // End "Executed" element.
  }

  if (did_crash) {
    std::string crash_dump = WideToASCII(GetMostRecentCrashDump());
    std::string result =
        "*** Crash dump produced. See result file for more details. Dump = ";
    result.append(crash_dump);
    result.append(" ***\n");
    printf("%s", result.c_str());
    LogCrashResult(crash_dump, command_complete);
    EXPECT_TRUE(false) << "Crash detected.";
  } else {
    printf("*** No crashes. See result file for more details. ***\n");
    LogSuccessResult();
  }

  WriteReportToFile();
}


void AutomatedUITest::RunAutomatedUITest() {
  ASSERT_TRUE(InitXMLReader()) << "Error initializing XMLReader";
  xml_writer_.StartWriting();
  xml_writer_.StartElement("Report");

  while (init_reader_.Read()) {
    init_reader_.SkipToElement();
    std::string node_name = init_reader_.NodeName();
    if (LowerCaseEqualsASCII(node_name, "command")) {
      bool no_errors = true;
      xml_writer_.StartElement("Executed");
      std::string command_number;
      if (init_reader_.NodeAttribute("number", &command_number)) {
        xml_writer_.AddAttribute("command_number", command_number);
      }
      xml_writer_.StopIndenting();

      // Starts the browser, logging it as an action.
      DoAction("SetUp");

      // Record the depth of the root of the command subtree, then advance to
      // the first element in preparation for parsing.
      int start_depth = init_reader_.Depth();
      ASSERT_TRUE(init_reader_.Read()) << "Malformed XML file.";
      init_reader_.SkipToElement();

      // Check for a crash right after startup.
      if (DidCrash(true)) {
        LogCrashResult(WideToASCII(GetMostRecentCrashDump()), false);
        // Try and start up again.
        CloseBrowserAndServer();
        LaunchBrowserAndServer();
        if (DidCrash(true)) {
          no_errors = false;
          // We crashed again, so skip to the end of the this command.
          while (init_reader_.Depth() != start_depth) {
            ASSERT_TRUE(init_reader_.Read()) << "Malformed XML file.";
          }
        } else {
          // We didn't crash, so end the old element, logging a crash for that.
          // Then start a new element to log this command.
          xml_writer_.StartIndenting();
          xml_writer_.EndElement();
          xml_writer_.StartElement("Executed");
          xml_writer_.AddAttribute("command_number", command_number);
          xml_writer_.StopIndenting();
          xml_writer_.StartElement("SetUp");
          xml_writer_.EndElement();
        }
      }
      // Parse the command, performing the specified actions and checking
      // for a crash after each one.
      while (init_reader_.Depth() != start_depth) {
        node_name = init_reader_.NodeName();

        DoAction(node_name);

        // Advance to the next element
        ASSERT_TRUE(init_reader_.Read()) << "Malformed XML file.";
        init_reader_.SkipToElement();
        if (DidCrash(true)) {
          no_errors = false;
          // This was the last action if we've returned to the initial depth
          // of the command subtree.
          bool wasLastAction = init_reader_.Depth() == start_depth;
          LogCrashResult(WideToASCII(GetMostRecentCrashDump()), wasLastAction);
          // Skip to the beginning of the next command.
          while (init_reader_.Depth() != start_depth) {
            ASSERT_TRUE(init_reader_.Read()) << "Malformed XML file.";
          }
        }
      }

      if (no_errors) {
        // If there were no previous crashes, log our tear down and check for
        // a crash, log success for the entire command if this doesn't crash.
        DoAction("TearDown");
        if (DidCrash(true))
          LogCrashResult(WideToASCII(GetMostRecentCrashDump()), true);
        else
          LogSuccessResult();
      } else {
        // If there was a previous crash, just tear down without logging, so
        // that we know what the last command was before we crashed.
        CloseBrowserAndServer();
      }

      xml_writer_.StartIndenting();
      xml_writer_.EndElement();  // End "Executed" element.
    }
  }
  // The test is finished so write our report.
  WriteReportToFile();
}

bool AutomatedUITest::DoAction(const std::string & action) {
  bool did_complete_action = false;
  xml_writer_.StartElement(action);
  if (debug_logging_enabled_)
    AppendToOutputFile(action);

  if (LowerCaseEqualsASCII(action, "about")) {
    did_complete_action = OpenAboutDialog();
  } else if (LowerCaseEqualsASCII(action, "back")) {
    did_complete_action = BackButton();
  } else if (LowerCaseEqualsASCII(action, "changeencoding")) {
    did_complete_action = ChangeEncoding();
  } else if (LowerCaseEqualsASCII(action, "closetab")) {
    did_complete_action = CloseActiveTab();
  } else if (LowerCaseEqualsASCII(action, "clearbrowsingdata")) {
    did_complete_action = OpenClearBrowsingDataDialog();
  } else if (LowerCaseEqualsASCII(action, "crash")) {
    did_complete_action = ForceCrash();
  } else if (LowerCaseEqualsASCII(action, "dialog")) {
    did_complete_action = ExerciseDialog();
  } else if (LowerCaseEqualsASCII(action, "downarrow")) {
    did_complete_action = PressDownArrow();
  } else if (LowerCaseEqualsASCII(action, "downloads")) {
    did_complete_action = ShowDownloads();
  } else if (LowerCaseEqualsASCII(action, "dragtableft")) {
    did_complete_action = DragActiveTab(false, false);
  } else if (LowerCaseEqualsASCII(action, "dragtabout")) {
    did_complete_action = DragActiveTab(false, true);
  } else if (LowerCaseEqualsASCII(action, "dragtabright")) {
    did_complete_action = DragActiveTab(true, false);
  } else if (LowerCaseEqualsASCII(action, "duplicatetab")) {
    did_complete_action = DuplicateTab();
  } else if (LowerCaseEqualsASCII(action, "editsearchengines")) {
    did_complete_action = OpenEditSearchEnginesDialog();
  } else if (LowerCaseEqualsASCII(action, "findinpage")) {
    did_complete_action = FindInPage();
  } else if (LowerCaseEqualsASCII(action, "forward")) {
    did_complete_action = ForwardButton();
  } else if (LowerCaseEqualsASCII(action, "goofftherecord")) {
    did_complete_action = GoOffTheRecord();
  } else if (LowerCaseEqualsASCII(action, "history")) {
    did_complete_action = ShowHistory();
  } else if (LowerCaseEqualsASCII(action, "home")) {
    did_complete_action = Home();
  } else if (LowerCaseEqualsASCII(action, "importsettings")) {
    did_complete_action = OpenImportSettingsDialog();
  } else if (LowerCaseEqualsASCII(action, "javascriptconsole")) {
    did_complete_action = JavaScriptConsole();
  } else if (LowerCaseEqualsASCII(action, "javascriptdebugger")) {
    did_complete_action = JavaScriptDebugger();
  } else if (LowerCaseEqualsASCII(action, "navigate")) {
    did_complete_action = Navigate();
  } else if (LowerCaseEqualsASCII(action, "newtab")) {
    did_complete_action = NewTab();
  } else if (LowerCaseEqualsASCII(action, "openwindow")) {
    did_complete_action = OpenAndActivateNewBrowserWindow();
  } else if (LowerCaseEqualsASCII(action, "options")) {
    did_complete_action = Options();
  } else if (LowerCaseEqualsASCII(action, "pagedown")) {
    did_complete_action = PressPageDown();
  } else if (LowerCaseEqualsASCII(action, "pageup")) {
    did_complete_action = PressPageUp();
  } else if (LowerCaseEqualsASCII(action, "pressenterkey")) {
    did_complete_action = PressEnterKey();
  } else if (LowerCaseEqualsASCII(action, "pressescapekey")) {
    did_complete_action = PressEscapeKey();
  } else if (LowerCaseEqualsASCII(action, "pressspacebar")) {
    did_complete_action = PressSpaceBar();
  } else if (LowerCaseEqualsASCII(action, "presstabkey")) {
    did_complete_action = PressTabKey();
  } else if (LowerCaseEqualsASCII(action, "reload")) {
    did_complete_action = ReloadPage();
  } else if (LowerCaseEqualsASCII(action, "restoretab")) {
    did_complete_action = RestoreTab();
  } else if (LowerCaseEqualsASCII(action, "selectnexttab")) {
    did_complete_action = SelectNextTab();
  } else if (LowerCaseEqualsASCII(action, "selectprevtab")) {
    did_complete_action = SelectPreviousTab();
  } else if (LowerCaseEqualsASCII(action, "showbookmarks")) {
    did_complete_action = ShowBookmarkBar();
  } else if (LowerCaseEqualsASCII(action, "setup")) {
    LaunchBrowserAndServer();
    did_complete_action = true;
  } else if (LowerCaseEqualsASCII(action, "sleep")) {
    // This is for debugging, it probably shouldn't be used real tests.
    Sleep(kDebuggingTimeoutMsec);
    did_complete_action = true;
  } else if (LowerCaseEqualsASCII(action, "star")) {
    did_complete_action = StarPage();
  } else if (LowerCaseEqualsASCII(action, "taskmanager")) {
    did_complete_action = OpenTaskManagerDialog();
  } else if (LowerCaseEqualsASCII(action, "teardown")) {
    CloseBrowserAndServer();
    did_complete_action = true;
  } else if (LowerCaseEqualsASCII(action, "testaboutchrome")) {
    did_complete_action = TestAboutChrome();
  } else if (LowerCaseEqualsASCII(action, "testclearbrowsingdata")) {
    did_complete_action = TestClearBrowsingData();
  } else if (LowerCaseEqualsASCII(action, "testeditsearchengines")) {
    did_complete_action = TestEditSearchEngines();
  } else if (LowerCaseEqualsASCII(action, "testimportsettings")) {
    did_complete_action = TestImportSettings();
  } else if (LowerCaseEqualsASCII(action, "testoptions")) {
    did_complete_action = TestOptions();
  } else if (LowerCaseEqualsASCII(action, "testtaskmanager")) {
    did_complete_action = TestTaskManager();
  } else if (LowerCaseEqualsASCII(action, "testviewpasswords")) {
    did_complete_action = TestViewPasswords();
  } else if (LowerCaseEqualsASCII(action, "uparrow")) {
    did_complete_action = PressUpArrow();
  } else if (LowerCaseEqualsASCII(action, "viewpasswords")) {
    did_complete_action = OpenViewPasswordsDialog();
  } else if (LowerCaseEqualsASCII(action, "viewsource")) {
    did_complete_action = ViewSource();
  } else if (LowerCaseEqualsASCII(action, "zoomplus")) {
    did_complete_action = ZoomPlus();
  } else if (LowerCaseEqualsASCII(action, "zoomminus")) {
    did_complete_action = ZoomMinus();
  } else {
    NOTREACHED() << "Unknown command passed into DoAction: "
                 << action.c_str();
  }

  if (!did_complete_action)
    xml_writer_.AddAttribute("failed_to_complete", "yes");
  xml_writer_.EndElement();

  if (post_action_delay_)
    ::Sleep(1000 * post_action_delay_);

  return did_complete_action;
}

bool AutomatedUITest::OpenAndActivateNewBrowserWindow() {
  if (!automation()->OpenNewBrowserWindow(SW_SHOWNORMAL)) {
    AddWarningAttribute("failed_to_open_new_browser_window");
    return false;
  }
  int num_browser_windows;
  automation()->GetBrowserWindowCount(&num_browser_windows);
  // Get the most recently opened browser window and activate the tab
  // in order to activate this browser window.
  scoped_ptr<BrowserProxy> browser(
    automation()->GetBrowserWindow(num_browser_windows - 1));
  if (browser.get() == NULL) {
    AddErrorAttribute("browser_window_not_found");
    return false;
  }
  bool is_timeout;
  if (!browser->ActivateTabWithTimeout(0, action_max_timeout_ms(),
                                       &is_timeout)) {
    AddWarningAttribute("failed_to_activate_tab");
    return false;
  }
  return true;
}

bool AutomatedUITest::BackButton() {
  return RunCommand(IDC_BACK);
}

bool AutomatedUITest::ChangeEncoding() {
  // Get the encoding list that is used to populate the UI (encoding menu)
  const std::vector<int>* encoding_ids =
      CharacterEncoding::GetCurrentDisplayEncodings(
          L"ISO-8859-1,windows-1252", L"");
  DCHECK(encoding_ids);
  DCHECK(!encoding_ids->empty());
  unsigned len = static_cast<unsigned>(encoding_ids->size());

  // The vector will contain mostly IDC values for encoding commands plus a few
  // menu separators (0 values). If we hit a separator we just retry.
  int index = base::RandInt(0, len);
  while ((*encoding_ids)[index] == 0) {
    index = base::RandInt(0, len);
  }

  return RunCommand((*encoding_ids)[index]);
}

bool AutomatedUITest::CloseActiveTab() {
  bool return_value = false;
  scoped_ptr<BrowserProxy> browser(automation()->GetLastActiveBrowserWindow());
  if (browser.get() == NULL) {
    AddErrorAttribute("browser_window_not_found");
    return false;
  }
  int browser_windows_count;
  int tab_count;
  bool is_timeout;
  browser->GetTabCountWithTimeout(&tab_count,
                                  action_max_timeout_ms(),
                                  &is_timeout);
  automation()->GetBrowserWindowCount(&browser_windows_count);
  // Avoid quitting the application by not closing the last window.
  if (tab_count > 1) {
    int new_tab_count;
    return_value = browser->RunCommand(IDC_CLOSE_TAB);
    // Wait for the tab to close before we continue.
    if (!browser->WaitForTabCountToChange(tab_count,
                                          &new_tab_count,
                                          action_max_timeout_ms())) {
      AddWarningAttribute("tab_count_failed_to_change");
      return false;
    }
  } else if (tab_count == 1 && browser_windows_count > 1) {
    int new_window_count;
    return_value = browser->RunCommand(IDC_CLOSE_TAB);
    // Wait for the window to close before we continue.
    if (!automation()->WaitForWindowCountToChange(browser_windows_count,
                                                  &new_window_count,
                                                  action_max_timeout_ms())) {
      AddWarningAttribute("window_count_failed_to_change");
      return false;
    }
  } else {
    AddInfoAttribute("would_have_exited_application");
    return false;
  }
  return return_value;
}

bool AutomatedUITest::DuplicateTab() {
  return RunCommand(IDC_DUPLICATE_TAB);
}

bool AutomatedUITest::FindInPage() {
  return RunCommand(IDC_FIND);
}

bool AutomatedUITest::ForwardButton() {
  return RunCommand(IDC_FORWARD);
}

bool AutomatedUITest::GoOffTheRecord() {
  return RunCommand(IDC_NEW_INCOGNITO_WINDOW);
}

bool AutomatedUITest::Home() {
  return RunCommand(IDC_HOME);
}

bool AutomatedUITest::JavaScriptConsole() {
  return RunCommand(IDC_JS_CONSOLE);
}

bool AutomatedUITest::JavaScriptDebugger() {
  return RunCommand(IDC_DEBUGGER);
}

bool AutomatedUITest::Navigate() {
  scoped_ptr<BrowserProxy> browser(automation()->GetLastActiveBrowserWindow());
  if (browser.get() == NULL) {
    AddErrorAttribute("browser_window_not_found");
    return false;
  }
  bool did_timeout;
  scoped_ptr<TabProxy> tab(
      browser->GetActiveTabWithTimeout(action_max_timeout_ms(), &did_timeout));
  // TODO(devint): This might be masking a bug. I can't think of many
  // valid cases where we would get a browser window, but not be able
  // to return an active tab. Yet this has happened and has triggered crashes.
  // Investigate this.
  if (tab.get() == NULL) {
    AddErrorAttribute("active_tab_not_found");
    return false;
  }
  std::string url = "about:blank";
  if (init_reader_.NodeAttribute("url", &url)) {
    xml_writer_.AddAttribute("url", url);
  }
  GURL test_url(url);
  did_timeout = false;
  tab->NavigateToURLWithTimeout(test_url,
                                command_execution_timeout_ms(),
                                &did_timeout);

  if (did_timeout) {
    AddWarningAttribute("timeout");
    return false;
  }
  return true;
}

bool AutomatedUITest::NewTab() {
  scoped_ptr<BrowserProxy> browser(automation()->GetLastActiveBrowserWindow());
  if (browser.get() == NULL) {
    AddErrorAttribute("browser_window_not_found");
    return false;
  }
  int old_tab_count;
  int new_tab_count;
  bool is_timeout;
  browser->GetTabCountWithTimeout(&old_tab_count,
                                  action_max_timeout_ms(),
                                  &is_timeout);
  // Apply accelerator and wait for a new tab to open, if either
  // fails, return false. Apply Accelerator takes care of logging its failure.
  bool return_value = RunCommand(IDC_NEW_TAB);
  if (!browser->WaitForTabCountToChange(old_tab_count,
                                        &new_tab_count,
                                        action_max_timeout_ms())) {
    AddWarningAttribute("tab_count_failed_to_change");
    return false;
  }
  return return_value;
}

bool AutomatedUITest::OpenAboutDialog() {
  return RunCommand(IDC_ABOUT);
}

bool AutomatedUITest::OpenClearBrowsingDataDialog() {
  return RunCommand(IDC_CLEAR_BROWSING_DATA);
}

bool AutomatedUITest::OpenEditSearchEnginesDialog() {
  return RunCommand(IDC_EDIT_SEARCH_ENGINES);
}

bool AutomatedUITest::OpenImportSettingsDialog() {
  return RunCommand(IDC_IMPORT_SETTINGS);
}

bool AutomatedUITest::OpenTaskManagerDialog() {
  return RunCommand(IDC_TASK_MANAGER);
}

bool AutomatedUITest::OpenViewPasswordsDialog() {
  return RunCommand(IDC_VIEW_PASSWORDS);
}

bool AutomatedUITest::Options() {
  return RunCommand(IDC_OPTIONS);
}

bool AutomatedUITest::PressDownArrow() {
  return SimulateKeyPressInActiveWindow(VK_DOWN, 0);
}

bool AutomatedUITest::PressEnterKey() {
  return SimulateKeyPressInActiveWindow(VK_RETURN, 0);
}

bool AutomatedUITest::PressEscapeKey() {
  return SimulateKeyPressInActiveWindow(VK_ESCAPE, 0);
}

bool AutomatedUITest::PressPageDown() {
  return SimulateKeyPressInActiveWindow(VK_PRIOR, 0);
}

bool AutomatedUITest::PressPageUp() {
  return SimulateKeyPressInActiveWindow(VK_NEXT, 0);
}

bool AutomatedUITest::PressSpaceBar() {
  return SimulateKeyPressInActiveWindow(VK_SPACE, 0);
}

bool AutomatedUITest::PressTabKey() {
  return SimulateKeyPressInActiveWindow(VK_TAB, 0);
}

bool AutomatedUITest::PressUpArrow() {
  return SimulateKeyPressInActiveWindow(VK_UP, 0);
}

bool AutomatedUITest::ReloadPage() {
  return RunCommand(IDC_RELOAD);
}

bool AutomatedUITest::RestoreTab() {
  return RunCommand(IDC_RESTORE_TAB);
}

bool AutomatedUITest::SelectNextTab() {
  return RunCommand(IDC_SELECT_NEXT_TAB);
}

bool AutomatedUITest::SelectPreviousTab() {
  return RunCommand(IDC_SELECT_PREVIOUS_TAB);
}

bool AutomatedUITest::ShowBookmarkBar() {
  return RunCommand(IDC_SHOW_BOOKMARK_BAR);
}

bool AutomatedUITest::ShowDownloads() {
  return RunCommand(IDC_SHOW_DOWNLOADS);
}

bool AutomatedUITest::ShowHistory() {
  return RunCommand(IDC_SHOW_HISTORY);
}

bool AutomatedUITest::StarPage() {
  return RunCommand(IDC_STAR);
}

bool AutomatedUITest::ViewSource() {
  return RunCommand(IDC_VIEW_SOURCE);
}

bool AutomatedUITest::ZoomMinus() {
  return RunCommand(IDC_ZOOM_MINUS);
}

bool AutomatedUITest::ZoomPlus() {
  return RunCommand(IDC_ZOOM_PLUS);
}

bool AutomatedUITest::TestAboutChrome() {
  DoAction("About");
  return FuzzyTestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::TestClearBrowsingData() {
  DoAction("ClearBrowsingData");
  return FuzzyTestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::TestEditSearchEngines() {
  DoAction("EditSearchEngines");
  return FuzzyTestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::TestImportSettings() {
  DoAction("ImportSettings");
  return FuzzyTestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::TestTaskManager() {
  DoAction("TaskManager");
  return FuzzyTestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::TestOptions() {
  DoAction("Options");
  return FuzzyTestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::TestViewPasswords() {
  DoAction("ViewPasswords");
  return FuzzyTestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::ExerciseDialog() {
  int index = base::RandInt(0, arraysize(kDialogs) - 1);
  return DoAction(kDialogs[index]) && FuzzyTestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::FuzzyTestDialog(int num_actions) {
  bool return_value = true;

  for (int i = 0; i < num_actions; i++) {
    // We want to make sure the first action performed on the dialog is not
    // Space or Enter because focus is likely on the Close button. Both Space
    // and Enter would close the dialog without performing more actions. We
    // rely on the fact that those two actions are first in the array and set
    // the lower bound to 2 if i == 0 to skip those two actions.
    int action_index = base::RandInt(i == 0 ? 2 : 0,
                                     arraysize(kTestDialogPossibleActions)
                                         - 1);
    return_value = return_value &&
                   DoAction(kTestDialogPossibleActions[action_index]);
    if (DidCrash(false))
      break;
  }
  return DoAction("PressEscapeKey") && return_value;
}

bool AutomatedUITest::ForceCrash() {
  scoped_ptr<BrowserProxy> browser(automation()->GetLastActiveBrowserWindow());
  if (browser.get() == NULL) {
    AddErrorAttribute("browser_window_not_found");
    return false;
  }
  scoped_ptr<TabProxy> tab(browser->GetActiveTab());
  GURL test_url("about:crash");
  bool did_timeout;
  tab->NavigateToURLWithTimeout(test_url, kDebuggingTimeoutMsec, &did_timeout);
  if (!did_timeout) {
    AddInfoAttribute("expected_crash");
    return false;
  }
  return true;
}

bool AutomatedUITest::DragActiveTab(bool drag_right, bool drag_out) {
  scoped_ptr<BrowserProxy> browser(automation()->GetLastActiveBrowserWindow());
  scoped_ptr<WindowProxy> window(
      GetAndActivateWindowForBrowser(browser.get()));
  if (window.get() == NULL) {
    AddErrorAttribute("active_window_not_found");
    return false;
  }
  bool is_timeout;

  if (browser.get() == NULL) {
    AddErrorAttribute("browser_window_not_found");
    return false;
  }
  int tab_count;
  browser->GetTabCountWithTimeout(&tab_count,
                                  action_max_timeout_ms(),
                                  &is_timeout);
  // As far as we're concerned, if we can't get a view for a tab, it doesn't
  // exist, so cap tab_count at the number of tab view ids there are.
  tab_count = std::min(tab_count, VIEW_ID_TAB_LAST - VIEW_ID_TAB_0);

  int tab_index;
  if (!browser->GetActiveTabIndexWithTimeout(&tab_index,
                                             action_max_timeout_ms(),
                                             &is_timeout)) {
    AddWarningAttribute("no_active_tab");
    return false;
  }

  gfx::Rect dragged_tab_bounds;
  if (!window->GetViewBoundsWithTimeout(VIEW_ID_TAB_0 + tab_index,
                                        &dragged_tab_bounds, false,
                                        action_max_timeout_ms(),
                                        &is_timeout)) {
    AddWarningAttribute("no_tab_view_found");
    return false;
  }

  // Click on the center of the tab, and drag it to the left or the right.
  POINT dragged_tab_point(dragged_tab_bounds.CenterPoint().ToPOINT());
  POINT destination_point(dragged_tab_point);

  int window_count;
  if (drag_out) {
    destination_point.y += 3*dragged_tab_bounds.height();
    automation()->GetBrowserWindowCount(&window_count);
  } else if (drag_right) {
    if (tab_index >= (tab_count-1)) {
      AddInfoAttribute("index_cant_be_moved");
      return false;
    }
    destination_point.x += 2*dragged_tab_bounds.width()/3;
  } else {
    if (tab_index <= 0) {
      AddInfoAttribute("index_cant_be_moved");
      return false;
    }
    destination_point.x -= 2*dragged_tab_bounds.width()/3;
  }

  if (!browser->SimulateDragWithTimeout(dragged_tab_point,
                                        destination_point,
                                        views::Event::EF_LEFT_BUTTON_DOWN,
                                        action_max_timeout_ms(),
                                        &is_timeout, false)) {
    AddWarningAttribute("failed_to_simulate_drag");
    return false;
  }

  // If we try to drag the tab out and the window we drag from contains more
  // than just the dragged tab, we would expect the window count to increase
  // because the dragged tab should open in a new window. If not, we probably
  // just dragged into another tabstrip.
  if (drag_out && tab_count > 1) {
      int new_window_count;
      automation()->GetBrowserWindowCount(&new_window_count);
      if (new_window_count == window_count) {
        AddInfoAttribute("no_new_browser_window");
        return false;
      }
  }
  return true;
}

WindowProxy* AutomatedUITest::GetAndActivateWindowForBrowser(
    BrowserProxy* browser) {
  WindowProxy* window = automation()->GetWindowForBrowser(browser);

  bool did_timeout;
  if (!browser->BringToFrontWithTimeout(action_max_timeout_ms(), &did_timeout)) {
    AddWarningAttribute("failed_to_bring_window_to_front");
    return NULL;
  }
  return window;
}

bool AutomatedUITest::RunCommand(int browser_command) {
  scoped_ptr<BrowserProxy> browser(automation()->GetLastActiveBrowserWindow());
  if (browser.get() == NULL) {
    AddErrorAttribute("browser_window_not_found");
    return false;
  }
  if (!browser->RunCommand(browser_command)) {
    AddWarningAttribute("failure_running_browser_command");
    return false;
  }
  return true;
}

bool AutomatedUITest::SimulateKeyPressInActiveWindow(wchar_t key, int flags) {
  scoped_ptr<WindowProxy> window(automation()->GetActiveWindow());
  if (window.get() == NULL) {
    AddErrorAttribute("active_window_not_found");
    return false;
  }
  if (!window->SimulateOSKeyPress(key, flags)) {
    AddWarningAttribute("failure_simulating_key_press");
    return false;
  }
  return true;
}

bool AutomatedUITest::InitXMLReader() {
  std::wstring input_path;
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (parsed_command_line.HasSwitch(kInputFilePathSwitch))
    input_path = parsed_command_line.GetSwitchValue(kInputFilePathSwitch);
  else
    input_path = kDefaultInputFilePath;

  if (!file_util::ReadFileToString(input_path, &xml_init_file_))
    return false;
  return init_reader_.Load(xml_init_file_);
}

bool AutomatedUITest::WriteReportToFile() {
  std::ofstream error_file;
  std::wstring path;
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (parsed_command_line.HasSwitch(kOutputFilePathSwitch))
    path = parsed_command_line.GetSwitchValue(kOutputFilePathSwitch);
  else
    path = kDefaultOutputFilePath;

  if (!path.empty())
    error_file.open(path.c_str(), std::ios::out);

  // Closes all open elements and free the writer. This is required
  // in order to retrieve the contents of the buffer.
  xml_writer_.StopWriting();
  error_file << xml_writer_.GetWrittenString();
  error_file.close();
  return true;
}

void AutomatedUITest::AppendToOutputFile(const std::string &append_string) {
  std::ofstream error_file;
  std::wstring path;
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (parsed_command_line.HasSwitch(kOutputFilePathSwitch))
    path = parsed_command_line.GetSwitchValue(kOutputFilePathSwitch);
  else
    path = kDefaultOutputFilePath;

  if (!path.empty())
    error_file.open(path.c_str(), std::ios::out | std::ios_base::app);

  error_file << append_string << " ";
  error_file.close();
}

void AutomatedUITest::LogCrashResult(const std::string &crash_dump,
                                     bool command_completed) {
  xml_writer_.StartElement("result");
  xml_writer_.StartElement("crash");
  xml_writer_.AddAttribute("crash_dump", crash_dump);
  if (command_completed)
    xml_writer_.AddAttribute("command_completed", "yes");
  else
    xml_writer_.AddAttribute("command_completed", "no");
  xml_writer_.EndElement();
  xml_writer_.EndElement();
}

void AutomatedUITest::LogSuccessResult() {
  xml_writer_.StartElement("result");
  xml_writer_.StartElement("success");
  xml_writer_.EndElement();
  xml_writer_.EndElement();
}

void AutomatedUITest::AddInfoAttribute(const std::string &info) {
  xml_writer_.AddAttribute("info", info);
}

void AutomatedUITest::AddWarningAttribute(const std::string &warning) {
  xml_writer_.AddAttribute("warning", warning);
}

void AutomatedUITest::AddErrorAttribute(const std::string &error) {
  xml_writer_.AddAttribute("error", error);
}

std::wstring AutomatedUITest::GetMostRecentCrashDump() {
  std::wstring crash_dump_path;
  int file_count = 0;
  FILETIME most_recent_file_time;
  std::wstring most_recent_file_name;
  WIN32_FIND_DATA find_file_data;

  PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dump_path);
  // All files in the given directory.
  std::wstring filename_spec = crash_dump_path + L"\\*";
  HANDLE find_handle = FindFirstFile(filename_spec.c_str(), &find_file_data);
  if (find_handle != INVALID_HANDLE_VALUE) {
    most_recent_file_time = find_file_data.ftCreationTime;
    most_recent_file_name = find_file_data.cFileName;
    do {
      // Don't count current or parent directories.
      if ((wcscmp(find_file_data.cFileName, L"..") == 0) ||
          (wcscmp(find_file_data.cFileName, L".") == 0))
        continue;

      long result = CompareFileTime(&find_file_data.ftCreationTime,
                                    &most_recent_file_time);

      // File was created on or after the current most recent file.
      if ((result == 1) || (result == 0)) {
        most_recent_file_time = find_file_data.ftCreationTime;
        most_recent_file_name = find_file_data.cFileName;
      }
    } while (FindNextFile(find_handle, &find_file_data));
    FindClose(find_handle);
  }

  if (most_recent_file_name.empty()) {
    return L"";
  } else {
    file_util::AppendToPath(&crash_dump_path, most_recent_file_name);
    return crash_dump_path;
  }
}

bool AutomatedUITest::DidCrash(bool update_total_crashes) {
  std::wstring crash_dump_path;
  PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dump_path);
  // Each crash creates two dump files, so we divide by two here.
  int actual_crashes = file_util::CountFilesCreatedAfter(
    crash_dump_path, test_start_time_) / 2;

  // If there are more crash dumps than the total dumps which we have recorded
  // then this is a new crash.
  if (actual_crashes > total_crashes_) {
    if (update_total_crashes)
      total_crashes_ = actual_crashes;
    return true;
  } else {
    return false;
  }
}

TEST_F(AutomatedUITest, TheOneAndOnlyTest) {
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (parsed_command_line.HasSwitch(kReproSwitch))
    RunReproduction();
  else
    RunAutomatedUITest();
}

