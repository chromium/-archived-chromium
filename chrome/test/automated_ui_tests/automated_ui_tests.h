// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATED_UI_TESTS_AUTOMATED_UI_TESTS_H_
#define CHROME_TEST_AUTOMATED_UI_TESTS_AUTOMATED_UI_TESTS_H_

// This takes an input file of commands, which consist of a series of
// actions, and runs every command, reporting the status of each one
// to an output file once all the commands have been run.
//
// The input file should be an XML file that has a root of any name followed
// by a series of elements named "command" with child elements representing
// the various actions, in order, to be performed during each command. A
// command element can optionally include an "number" attribute to identify it.
//
// Example:
// <CommandList>
//  <command number="1"><NewTab/><Navigate/><OpenWindow/><Navigate/><Back/>
//  </command>
//  <command number="2"><NewTab/><Navigate/><Navigate/><Back/><Forward/>
//  </command>
//  <command number="3"><CloseTab/><OpenWindow/><NewTab/><Navigate/><CloseTab/>
//  </command>
// </CommandList>
//
// When the test is finished it will output results to the output file,
// overwriting any previous version of this file. The output file is an
// XML file which reports on each command, indicating whether it successfully
// ran and if there were any errors.
//
// Example: (actual output will probably contain more actions per command):
// <Report>
//  <Executed command_number="1"><NewTab/><Navigate/><result><success/>
//  </result> </Executed>
//  <Executed command_number="2"><Back/><Forward/><result><success/></result>
//  </Executed>
//  <Executed command_number="3"><CloseTab/><result>
//    <crash crash_dump="C:\a_crash.txt" command_completed="no"/></result>
//  </Executed>
//  </Report>
//
// A "crash" result will have two attributes, crash_dump, which points
// to the full path of crash dump associated with this crash, and
// command_completed which indicates whether or not the last
// action recorded was the final action of the command.
//
// Furthermore, each individual action may contain additional attributes
// to log non-fatal failures. If the attribute 'failed_to_complete="yes"'
// is present, then the action did not complete. If that attribute is present,
// an info, warning, or error attribute will also be present, and will contain
// a string describing the error. The presence of info means the failure was
// expected, probably due to a state making the action impossible to perform
// like trying to close the last remaining window. Warnings usually mean the
// action couldn't complete for an unknown and unexpected reason, but that the
// test state is probably fine. Errors are like warnings, but they mean the test
// state is probably incorrect, and more failures are likely to be caused by
// the same problem.
//
// Example of some failed actions:
// <CloseTab failed_to_complete="yes" info="would_have_exited_application"/>
// <Reload failed_to_complete="yes" warning="failed_to_apply_accelerator"/>
// <Star failed_to_complete="yes" error="browser_window_not_found"/>
//
//
// Switches
// --input : Specifies the input file, must be an absolute directory.
//          Default is "C:\automated_ui_tests.txt"
//
// --output : Specifies the output file, must be an absolute directory.
//           Default is "C:\automated_ui_tests_error_report.txt"
//
//
// Test reproduction options:
//
// If you're trying to reproduce the results from crash reports use the
// following switches
//
// --key : Specifies, via a comma delimited list, what actions to run. Examples:
//        --key=SetUp,ZoomPlus,Forward,History,Navigate,Back,TearDown
//        --key=SetUp,ZoomPlus
//        Note, the second key doesn't include a TearDown, that will
//        automatically be added if the result doesn't crash.
//
// --num-reproductions : Specifies the number of reproductions to run, the
//                       default is 1. Suggested use: run without this flag
//                       to see if we reproduce a crash, then run with the flag
//                       if there isn't a crash, to see if it might be a rare
//                       race condition that causes the crash.
//
//
// Debugging options:
//
// --debug : Will append each action that is performed to the output file, as
//          soon as the action is performed. If the program finishes, this file
//          will be overwritten with the normal results. This flag is used to
//          help debug the tests if they are crashing before they get a chance
//          to write their results to file.
//
// --wait-after-action : waits the specified amount of time (1s by default)
//                       after each action. Useful for debugging.
//

#include "chrome/test/ui/ui_test.h"

class AutomatedUITest : public UITest {
 protected:
  AutomatedUITest();
  ~AutomatedUITest();

  // Runs a reproduction of one set of actions, reporting whether they crash
  // or not.
  void RunReproduction();

  // Runs automated UI tests which are read from the input file.
  // Reports crashes to the output file.
  void RunAutomatedUITest();

  // Attempts to perform an action based on the input string. See below for
  // possible actions. Returns true if the action completes, false otherwise.
  bool DoAction(const std::string &action);

  // Actions ------------------------------------------------------------------

  // NOTE: This list is sorted alphabetically, so that we can easily detect
  // missing actions.

  // Activates back button in active window.
  // Returns true if call to activate the accelerator is successful.
  // XML element: <Back/>
  bool BackButton();

  // Changes the encoding of the page (the encoding is selected at random
  // from a list of encodings).
  // Returns true if call to activate the accelerator is successful.
  // XML element: <ChangeEncoding/>
  bool ChangeEncoding();

  // Uses accelerator to close the active tab if it isn't the only tab.
  // Returns false if active tab is the only tab, true otherwise.
  // XML element: <CloseTab/>
  bool CloseActiveTab();

  // Duplicates the current tab.
  // Returns true if call to activate the accelerator is successful.
  // XML element: <DuplicateTab/>
  bool DuplicateTab();

  // Opens one of the dialogs (chosen randomly) and exercises it.
  // XML element: <Dialog/>
  bool ExerciseDialog();

  // Activates "find in page" on the current page.
  // XML element: <FindInPage/>
  bool FindInPage();

  // Activates forward button in active window.
  // Returns true if call to activate the accelerator is successful.
  // XML element: <Forward/>
  bool ForwardButton();

  // Opens and focuses an OffTheRecord browser window.
  // XML element: <GoOffTheRecord/>
  bool GoOffTheRecord();

  // Navigates to the Home page.
  // Returns true if call to activate the accelerator is successful.
  // XML element: <Home/>
  bool Home();

  // Opens the JavaScriptConsole window. While it isn't modal, it takes focus
  // from the current browser window, so most of the test can't continue until
  // it is dismissed.
  // XML element: <JavaScriptConsole/>
  bool JavaScriptConsole();

  // Opens the JavaScriptDebugger window. While it isn't modal, it takes focus
  // from the current browser window, so most of the test can't continue until
  // it is dismissed.
  // XML element: <JavaScriptDebugger/>
  bool JavaScriptDebugger();

  // Navigates the activate tab to about:blank.
  // XML element: <Navigate/>
  // Optional Attributes: url="|address|" will navigate to |address|
  bool Navigate();

  // Opens a new tab in the active window using an accelerator.
  // Returns true if call to activate the accelerator is successful.
  // XML element: <NewTab/>
  bool NewTab();

  // Opens a new browser window by calling automation()->OpenNewBrowserWindow
  // Then activates the tab opened in the new window.
  // Returns true if window is successfully created.
  // XML element: <OpenWindow/>
  bool OpenAndActivateNewBrowserWindow();

  // Opens the About dialog. This dialog is modal so a majority of the test
  // can't be completed until it is dismissed.
  // XML element: <About/>
  bool OpenAboutDialog();

  // Opens the Clear Browsing Data dialog, this dialog is modal so a majority of
  // the test can't be completed until it is dismissed.
  // XML element: <ClearBrowsingData/>
  bool OpenClearBrowsingDataDialog();

  // Opens the Search Engines dialog. While it isn't modal, it takes focus from
  // the current browser window, so most of the test can't continue until it is
  // dismissed.
  // XML element: <EditSearchEngines/>
  bool OpenEditSearchEnginesDialog();

  // Opens the Import Settings dialog, this dialog is modal so a majority of
  // the test can't be completed until it is dismissed.
  // XML element: <ImportSettings/>
  bool OpenImportSettingsDialog();

  // Opens the Task Manager dialog. While it isn't modal, it takes focus from
  // the current browser window, so most of the test can't continue until it is
  // dismissed.
  // XML element: <TaskManager/>
  bool OpenTaskManagerDialog();

  // Opens the View Passwords dialog, this dialog is modal so a majority of
  // the test can't be completed until it is dismissed.
  // XML element: <ViewPasswords/>
  bool OpenViewPasswordsDialog();

  // Opens the Options dialog. While it isn't modal, it takes focus from
  // the current browser window, so most of the test can't continue until it is
  // dismissed.
  // XML element: <Options/>
  bool Options();

  // Simulates a page up key press on the active window.
  // XML element: <DownArrow/>
  bool PressDownArrow();

  // Simulates an enter key press on the active window.
  // XML element: <PressEnterKey/>
  bool PressEnterKey();

  // Simulates an escape key press on the active window.
  // XML element: <PressEscapeKey/>
  bool PressEscapeKey();

  // Simulates a page down key press on the active window.
  // XML element: <PageDown/>
  bool PressPageDown();

  // Simulates a page up key press on the active window.
  // XML element: <PageUp/>
  bool PressPageUp();

  // Simulates a space bar press on the active window.
  // XML element: <PressSpaceBar/>
  bool PressSpaceBar();

  // Simulates a tab key press on the active window.
  // XML element: <PressTabKey/>
  bool PressTabKey();

  // Simulates a page up key press on the active window.
  // XML element: <UpArrow/>
  bool PressUpArrow();

  // Reload the active tab. Returns false on failure.
  // XML element: <Reload/>
  bool ReloadPage();

  // Restores a previously closed tab.
  // Returns true if call to activate the accelerator is successful.
  // XML element: <RestoreTab/>
  bool RestoreTab();

  // Activates the next tab on the active browser window.
  // XML element: <SelectNextTab/>
  bool SelectNextTab();

  // Activates the previous tab on the active browser window.
  // XML element: <SelectPrevTab/>
  bool SelectPreviousTab();

  // Displays the bookmark bar.
  // Returns true if call to activate the accelerator is successful.
  // XML element: <ShowBookmarks/>
  bool ShowBookmarkBar();

  // Opens the Downloads page in the current active browser window.
  // XML element: <Downloads/>
  bool ShowDownloads();

  // Opens the History page in the current active browser window.
  // XML element: <History/>
  bool ShowHistory();

  // Stars the current page. This opens a dialog that may or may not be
  // dismissed.
  // XML element: <Star/>
  bool StarPage();

  // Views source of the current page.
  // Returns true if call to activate the accelerator is successful.
  // XML element: <ViewSource/>
  bool ViewSource();

  // Decreases the text size on the current active tab.
  // XML element: <ZoomMinus/>
  bool ZoomMinus();

  // Increases the text size on the current active tab.
  // XML element: <ZoomPlus/>
  bool ZoomPlus();

  // Test Dialog Actions ******************************************************
  // These are a special set of actions that perform multiple actions on a
  // specified dialog. They run kTestDialogActionsToRun actions randomly
  // chosen from test_dialog_possible_actions_ after opening the dialog. They
  // then always end with a PressEscapeKey action, to attempt to close the
  // dialog.
  //
  // The randomly performed actions are logged as child elements of the
  // TestDialog action. For example (for kTestDialogActionsToRun = 4):
  // <TestEditKeywords> <PressTabKey/><PressEnterKey/><DownArrow/>
  // <DownArrow/><PressEscapeKey/> </TestEditKeywords>

  // Opens About dialog and runs random actions on it.
  // XML element: <TestAboutChrome/>
  bool TestAboutChrome();

  // Opens Clear Browsing Data dialog and runs random actions on it.
  // XML element: <TestClearBrowsingData/>
  bool TestClearBrowsingData();

  // Opens Edit Keywords dialog and runs random actions on it.
  // XML element: <TestEditSearchEngines/>
  bool TestEditSearchEngines();

  // Opens Import Settings dialog and runs random actions on it.
  // XML element: <TestImportSettings/>
  bool TestImportSettings();

  // Opens Options dialog and runs random actions on it.
  // XML element: <TestOptions/>
  bool TestOptions();

  // Opens Task Manager and runs random actions on it.
  // This has the possibility of killing both the browser and renderer
  // processes, which will cause non-fatal errors for the remaining actions
  // in this command.
  // XML element: <TestTaskManager/>
  bool TestTaskManager();

  // Opens View Passwords dialog and runs random actions on it.
  // XML element: <TestViewPasswords/>
  bool TestViewPasswords();

  // End Test Dialog Actions **************************************************

  // Runs a limited set of actions designed to test dialogs. Will run
  // |num_actions| from the set defined in test_dialog_possible_actions_.
  bool FuzzyTestDialog(int num_actions);

  // Navigates to about:crash.
  // XML element: <Crash/>
  bool ForceCrash();

  // Drags the active tab. If |drag_out| is true, |drag_right| is ignored and
  // the tab is dragged vertically to remove it from the tabstrip. Otherwise,
  // If |drag_right| is true, if there is a tab to the right of the active tab,
  // the active tab is dragged to that tabs position. If |drag_right| is false,
  // if there is a tab to the left of the active tab, the active tab is dragged
  // to that tabs position. Returns false if the tab isn't dragged, or if an
  // attempt to drag out doesn't create a new window (likely because it was
  // dragged in to another window).
  // XML element (multiple elements use this):
  // <DragTabRight/> - Calls DragActiveTab(true, false) (attempt to drag right)
  // <DragTabLeft/> - Calls DragActiveTab(false, false) (attempt to drag left)
  // <DragTabOut/> - Calls DragActiveTab(false, true) (attempt to drag tab out)
  bool DragActiveTab(bool drag_right, bool drag_out);

  // Utility functions --------------------------------------------------------

  // Returns the WindowProxy associated with the given BrowserProxy
  // (transferring ownership of the pointer to the caller) and brings that
  // window to the top.
  WindowProxy* GetAndActivateWindowForBrowser(BrowserProxy* browser);

  // Runs the specified browser command in the current active browser.
  // See browser_commands.cc for the list of commands.
  // Returns true if the call is successful.
  // Returns false if the active window is not a browser window or if the
  // message to apply the accelerator fails.
  bool RunCommand(int browser_command);

  // Calls SimulateOSKeyPress on the active window. Simulates a key press at
  // the OS level. |key| is the key pressed  and |flags| specifies which
  // modifiers keys are also pressed (as defined in chrome/views/event.h).
  bool SimulateKeyPressInActiveWindow(wchar_t key, int flags);

  // Opens init file, reads it into the reader, and closes the file.
  // Returns false if there are any errors.
  bool InitXMLReader();

  // Closes the xml_writer and outputs the contents of its buffer to
  // the output file.
  bool WriteReportToFile();

  // Appends the provided string to the output file.
  void AppendToOutputFile(const std::string &append_string);

  // Logs a crash to the xml_writer in the form of:
  // <result><crash crash_dump="|crash_dump|" command_completed="yes/no"/>
  // </result>
  // crash_dump - Location of crash dump if applicable.
  // command_completed - True if all actions in the command were completed
  //                     before the crash occured.
  void LogCrashResult(const std::string &crash_dump, bool command_completed);

  // Logs a successful command to the xml_writer in the form of:
  // <result><success/><result/>
  void LogSuccessResult();

  // Adds the attribute "reason=|reason|" to the current element.
  // Used to log the reason for a given failure while performing an action.
  void LogActionFailureReason(const std::string &reason);

  // Adds the attribute 'info="|info|"' to the current element. Used when an
  // action could not complete for a non-serious issue. Usually because the
  // state of the test wouldn't allow for a particular action.
  void AddInfoAttribute(const std::string &info);

  // Adds the attribute "warning=|warning|" to the current element. Used when
  // an action could not complete because of a potentially troublesome issue.
  void AddWarningAttribute(const std::string &warning);

  // Adds the attribute "error=|error|" to the current element. Used when an
  // action could not complete due to an unexpected problem which might
  // invalidate the results of the entire command (not just the action).
  // This is usually used when the testing environment isn't acting as we'd
  // expect. For example, no chrome windows are focused, or key presses aren't
  // being registered.
  void AddErrorAttribute(const std::string &error);

  // Returns the full path of the crash dump. This is likely to be the
  // .txt file, not the actual crash dump. Although they do share
  // a common name.
  std::wstring GetMostRecentCrashDump();

  // Returns true if the test has produced any new crash logs.
  // A "new" crash log is one that was produced since DidCrash was last called
  // with |update_total_crashes| set to true.
  bool DidCrash(bool update_total_crashes);

  // Overridden so that UI Test doesn't set up when the tests start.
  // We use DoAction("SetUp") to set up, because it logs it and makes
  // it easier to check for crashes when we start the browser.
  virtual void SetUp() {}

  // Overridden so that UI Test doesn't close the browser (which is already
  // closed) at the end of the test.
  // We use DoAction("TearDown") to tear down, because it logs it and makes
  // it easier to check for crashes when we close the browser.
  virtual void TearDown() {}

 private:
  // Parses the init file.
  XmlReader init_reader_;

  // Builds the output file.
  XmlWriter xml_writer_;

  // Time the test was started. Used to find crash dumps.
  FILETIME test_start_time_;

  // Number of times the browser has crashed during this run.
  // Used to check for new crashes.
  int total_crashes_;

  // Used to init the init_reader_. It must exist as long as the reader does.
  std::string xml_init_file_;

  // If true, appends the commands to the output file as they are executed.
  // Used for debugging when automated_ui_tests.cc crashes before it outputs
  // results.
  bool debug_logging_enabled_;

  // A delay in second we wait for after each action.  Useful for debugging.
  int post_action_delay_;

  DISALLOW_COPY_AND_ASSIGN(AutomatedUITest);
};

#endif  // CHROME_TEST_AUTOMATED_UI_TESTS_AUTOMATED_UI_TESTS_H_
