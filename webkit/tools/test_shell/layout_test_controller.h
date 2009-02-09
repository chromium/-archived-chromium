// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
  LayoutTestController class:
  Bound to a JavaScript window.layoutTestController object using the
  CppBoundClass::BindToJavascript(), this allows layout tests that are run in
  the test_shell (or, in principle, any web page loaded into a client app built
  with this class) to control various aspects of how the tests are run and what
  sort of output they produce.
*/

#ifndef WEBKIT_TOOLS_TEST_SHELL_LAYOUT_TEST_CONTROLLER_H_
#define WEBKIT_TOOLS_TEST_SHELL_LAYOUT_TEST_CONTROLLER_H_

#include <queue>

#include "base/timer.h"
#include "webkit/glue/cpp_bound_class.h"

class TestShell;

class LayoutTestController : public CppBoundClass {
 public:
  // Builds the property and method lists needed to bind this class to a JS
  // object.
  LayoutTestController(TestShell* shell);

  // This function sets a flag that tells the test_shell to dump pages as
  // plain text, rather than as a text representation of the renderer's state.
  // It takes no arguments, and ignores any that may be present.
  void dumpAsText(const CppArgumentList& args, CppVariant* result);

  // This function sets a flag that tells the test_shell to print a line of
  // descriptive test for each editing command.  It takes no arguments, and
  // ignores any that may be present.
  void dumpEditingCallbacks(const CppArgumentList& args, CppVariant* result);

  // This function sets a flag that tells the test_shell to print a line of
  // descriptive test for each frame load callback.  It takes no arguments, and
  // ignores any that may be present.
  void dumpFrameLoadCallbacks(const CppArgumentList& args, CppVariant* result);

  // This function sets a flag that tells the test_shell to print out a text
  // representation of the back/forward list.  It ignores all args.
  void dumpBackForwardList(const CppArgumentList& args, CppVariant* result);

  // This function sets a flag that tells the test_shell to print out the
  // scroll offsets of the child frames.  It ignores all args.
  void dumpChildFrameScrollPositions(const CppArgumentList& args, CppVariant* result);

  // This function sets a flag that tells the test_shell to recursively
  // dump all frames as plain text if the dumpAsText flag is set.
  // It takes no arguments, and ignores any that may be present.
  void dumpChildFramesAsText(const CppArgumentList& args, CppVariant* result);

  // This function sets a flag that tells the test_shell to dump all calls
  // to window.status().
  // It takes no arguments, and ignores any that may be present.
  void dumpWindowStatusChanges(const CppArgumentList& args, CppVariant* result);

  // When called with a boolean argument, this sets a flag that controls
  // whether content-editable elements accept editing focus when an editing
  // attempt is made. It ignores any additional arguments.
  void setAcceptsEditing(const CppArgumentList& args, CppVariant* result);

  // Functions for dealing with windows.  By default we block all new windows.
  void windowCount(const CppArgumentList& args, CppVariant* result);
  void setCanOpenWindows(const CppArgumentList& args, CppVariant* result);
  void setCloseRemainingWindowsWhenComplete(const CppArgumentList& args, CppVariant* result);

  // By default, tests end when page load is complete.  These methods are used
  // to delay the completion of the test until notifyDone is called.
  void waitUntilDone(const CppArgumentList& args, CppVariant* result);
  void notifyDone(const CppArgumentList& args, CppVariant* result);

  // Methods for adding actions to the work queue.  Used in conjunction with
  // waitUntilDone/notifyDone above.
  void queueBackNavigation(const CppArgumentList& args, CppVariant* result);
  void queueForwardNavigation(const CppArgumentList& args, CppVariant* result);
  void queueReload(const CppArgumentList& args, CppVariant* result);
  void queueScript(const CppArgumentList& args, CppVariant* result);
  void queueLoad(const CppArgumentList& args, CppVariant* result);

  // Although this is named "objC" to match the Mac version, it actually tests
  // the identity of its two arguments in C++.
  void objCIdentityIsEqual(const CppArgumentList& args, CppVariant* result);

  // Gives focus to the window.
  void setWindowIsKey(const CppArgumentList& args, CppVariant* result);

  // Method that controls whether pressing Tab key cycles through page elements
  // or inserts a '\t' char in text area
  void setTabKeyCyclesThroughElements(const CppArgumentList& args, CppVariant* result);

  // Passes through to WebPreferences which allows the user to have a custom
  // style sheet.
  void setUserStyleSheetEnabled(const CppArgumentList& args, CppVariant* result);
  void setUserStyleSheetLocation(const CppArgumentList& args, CppVariant* result);

  // Puts Webkit in "dashboard compatibility mode", which is used in obscure
  // Mac-only circumstances. It's not really necessary, and will most likely
  // never be used by Chrome, but some layout tests depend on its presence.
  void setUseDashboardCompatibilityMode(const CppArgumentList& args, CppVariant* result);

  // Causes navigation actions just printout the intended navigation instead
  // of taking you to the page. This is used for cases like mailto, where you
  // don't actually want to open the mail program.
  void setCustomPolicyDelegate(const CppArgumentList& args, CppVariant* result);

  // Converts a URL starting with file:///tmp/ to the local mapping.
  void pathToLocalResource(const CppArgumentList& args, CppVariant* result);

  // Sets a bool such that when a drag is started, we fill the drag clipboard
  // with a fake file object.
  void addFileToPasteboardOnDrag(const CppArgumentList& args, CppVariant* result);

  // Executes an internal command (superset of document.execCommand() commands).
  void execCommand(const CppArgumentList& args, CppVariant* result);

  // Checks if an internal command is currently available.
  void isCommandEnabled(const CppArgumentList& args, CppVariant* result);

  // Set the WebPreference that controls webkit's popup blocking.
  void setPopupBlockingEnabled(const CppArgumentList& args, CppVariant* result);

  // If true, causes provisional frame loads to be stopped for the remainder of
  // the test.
  void setStopProvisionalFrameLoads(const CppArgumentList& args,
                                    CppVariant* result);

  // Enable or disable smart insert/delete.  This is enabled by default.
  void setSmartInsertDeleteEnabled(const CppArgumentList& args,
                                   CppVariant* result);

  // Enable or disable trailing whitespace selection on double click.
  void setSelectTrailingWhitespaceEnabled(const CppArgumentList& args,
                                          CppVariant* result);

  void pauseAnimationAtTimeOnElementWithId(const CppArgumentList& args,
                                           CppVariant* result);
  void pauseTransitionAtTimeOnElementWithId(const CppArgumentList& args,
                                            CppVariant* result);
  void elementDoesAutoCompleteForElementWithId(const CppArgumentList& args,
                                               CppVariant* result);
  void numberOfActiveAnimations(const CppArgumentList& args,
                                CppVariant* result);

  // The following are only stubs.  TODO(pamg): Implement any of these that
  // are needed to pass the layout tests.
  void dumpAsWebArchive(const CppArgumentList& args, CppVariant* result);
  void dumpTitleChanges(const CppArgumentList& args, CppVariant* result);
  void dumpResourceLoadCallbacks(const CppArgumentList& args, CppVariant* result);
  void setMainFrameIsFirstResponder(const CppArgumentList& args, CppVariant* result);
  void dumpSelectionRect(const CppArgumentList& args, CppVariant* result);
  void display(const CppArgumentList& args, CppVariant* result);
  void testRepaint(const CppArgumentList& args, CppVariant* result);
  void repaintSweepHorizontally(const CppArgumentList& args, CppVariant* result);
  void clearBackForwardList(const CppArgumentList& args, CppVariant* result);
  void keepWebHistory(const CppArgumentList& args, CppVariant* result);
  void storeWebScriptObject(const CppArgumentList& args, CppVariant* result);
  void accessStoredWebScriptObject(const CppArgumentList& args, CppVariant* result);
  void objCClassNameOf(const CppArgumentList& args, CppVariant* result);
  void addDisallowedURL(const CppArgumentList& args, CppVariant* result);
  void setCallCloseOnWebViews(const CppArgumentList& args, CppVariant* result);
  void setPrivateBrowsingEnabled(const CppArgumentList& args, CppVariant* result);

  // The fallback method is called when a nonexistent method is called on
  // the layout test controller object.
  // It is usefull to catch typos in the JavaScript code (a few layout tests
  // do have typos in them) and it allows the script to continue running in
  // that case (as the Mac does).
  void fallbackMethod(const CppArgumentList& args, CppVariant* result);

 public:
  // The following methods are not exposed to JavaScript.
  void SetWorkQueueFrozen(bool frozen) { work_queue_.set_frozen(frozen); }

  bool ShouldDumpAsText() { return dump_as_text_; }
  bool ShouldDumpEditingCallbacks() { return dump_editing_callbacks_; }
  bool ShouldDumpFrameLoadCallbacks() { return dump_frame_load_callbacks_; }
  void SetShouldDumpFrameLoadCallbacks(bool value) {
    dump_frame_load_callbacks_ = value;
  }
  bool ShouldDumpResourceLoadCallbacks() {
    return dump_resource_load_callbacks_;
  }
  bool ShouldDumpStatusCallbacks() {
    return dump_window_status_changes_;
  }
  bool ShouldDumpBackForwardList() { return dump_back_forward_list_; }
  bool ShouldDumpTitleChanges() { return dump_title_changes_; }
  bool ShouldDumpChildFrameScrollPositions() {
    return dump_child_frame_scroll_positions_;
  }
  bool ShouldDumpChildFramesAsText() {
    return dump_child_frames_as_text_;
  }
  bool AcceptsEditing() { return accepts_editing_; }
  bool CanOpenWindows() { return can_open_windows_; }
  bool ShouldAddFileToPasteboard() { return should_add_file_to_pasteboard_; }
  bool StopProvisionalFrameLoads() { return stop_provisional_frame_loads_; }

  // Called by the webview delegate when the toplevel frame load is done.
  void LocationChangeDone();

  // Reinitializes all static values.  The Reset() method should be called
  // before the start of each test (currently from
  // TestShell::RunFileTest).
  void Reset();

  // A single item in the work queue.
  class WorkItem {
   public:
    virtual ~WorkItem() {}
    virtual void Run(TestShell* shell) = 0;
  };

  // Used to clear the value of shell_ from test_shell_tests.
  static void ClearShell() { shell_ = NULL; }

 private:
  friend class WorkItem;

  // Helper class for managing events queued by methods like queueLoad or
  // queueScript.
  class WorkQueue {
   public:
    virtual ~WorkQueue();
    void ProcessWorkSoon();

    // Reset the state of the class between tests.
    void Reset();

    void AddWork(WorkItem* work);

    void set_frozen(bool frozen) { frozen_ = frozen; }
    bool empty() { return queue_.empty(); }

   private:
    void ProcessWork();

    base::OneShotTimer<WorkQueue> timer_;
    std::queue<WorkItem*> queue_;
    bool frozen_;
  };

  // Non-owning pointer.  The LayoutTestController is owned by the host.
  static TestShell* shell_;

  // If true, the test_shell will produce a plain text dump rather than a
  // text representation of the renderer.
  static bool dump_as_text_;

  // If true, the test_shell will write a descriptive line for each editing
  // command.
  static bool dump_editing_callbacks_;

  // If true, the test_shell will output a descriptive line for each frame
  // load callback.
  static bool dump_frame_load_callbacks_;

  // If true, the test_shell will output a descriptive line for each resource
  // load callback.
  static bool dump_resource_load_callbacks_;

  // If true, the test_shell will produce a dump of the back forward list as
  // well.
  static bool dump_back_forward_list_;

  // If true, the test_shell will print out the child frame scroll offsets as
  // well.
  static bool dump_child_frame_scroll_positions_;

  // If true and if dump_as_text_ is true, the test_shell will recursively
  // dump all frames as plain text.
  static bool dump_child_frames_as_text_;

  // If true, the test_shell will dump all changes to window.status.
  static bool dump_window_status_changes_;

  // If true, output a message when the page title is changed.
  static bool dump_title_changes_;

  // If true, the element will be treated as editable.  This value is returned
  // from various editing callbacks that are called just before edit operations
  // are allowed.
  static bool accepts_editing_;

  // If true, new windows can be opened via javascript or by plugins.  By
  // default, set to false and can be toggled to true using
  // setCanOpenWindows().
  static bool can_open_windows_;

  // When reset is called, go through and close all but the main test shell
  // window.  By default, set to true but toggled to false using
  // setCloseRemainingWindowsWhenComplete().
  static bool close_remaining_windows_;

  // If true and a drag starts, adds a file to the drag&drop clipboard.
  static bool should_add_file_to_pasteboard_;

  // If true, stops provisional frame loads during the
  // DidStartProvisionalLoadForFrame callback.
  static bool stop_provisional_frame_loads_;

  // If true, don't dump output until notifyDone is called.
  static bool wait_until_done_;

  // To prevent infinite loops, only the first page of a test can add to a
  // work queue (since we may well come back to that same page).
  static bool work_queue_frozen_;


  static WorkQueue work_queue_;

  static CppVariant globalFlag_;

  // Bound variable counting the number of top URLs visited.
  static CppVariant webHistoryItemCount_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_LAYOUT_TEST_CONTROLLER_H_

