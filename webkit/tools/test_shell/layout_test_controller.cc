// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This file contains the definition for LayoutTestController.

#include <vector>

#include "webkit/tools/test_shell/layout_test_controller.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/path_service.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webview.h"
#include "webkit/tools/test_shell/test_shell.h"

using std::string;
using std::wstring;

namespace {

// Stops the test from running and prints a brief warning to stdout.  Called
// when the timer for loading a layout test expires.
VOID CALLBACK TestTimeout(HWND hwnd, UINT msg, UINT_PTR timer_id, DWORD ms) {
  reinterpret_cast<TestShell*>(timer_id)->TestFinished();
  // Print a warning to be caught by the layout-test script.
  puts("#TEST_TIMED_OUT\n");
}

}


TestShell* LayoutTestController::shell_ = NULL;
bool LayoutTestController::dump_as_text_ = false;
bool LayoutTestController::dump_editing_callbacks_ = false;
bool LayoutTestController::dump_frame_load_callbacks_ = false;
bool LayoutTestController::dump_resource_load_callbacks_ = false;
bool LayoutTestController::dump_back_forward_list_ = false;
bool LayoutTestController::dump_child_frame_scroll_positions_ = false;
bool LayoutTestController::dump_child_frames_as_text_ = false;
bool LayoutTestController::dump_title_changes_ = false;
bool LayoutTestController::accepts_editing_ = true;
bool LayoutTestController::wait_until_done_ = false;
bool LayoutTestController::can_open_windows_ = false;
bool LayoutTestController::close_remaining_windows_ = true;
bool LayoutTestController::should_add_file_to_pasteboard_ = false;
LayoutTestController::WorkQueue LayoutTestController::work_queue_;
CppVariant LayoutTestController::globalFlag_;

LayoutTestController::LayoutTestController(TestShell* shell) {
  // Set static shell_ variable since we can't do it in an initializer list. 
  // We also need to be careful not to assign shell_ to new windows which are
  // temporary.
  if (NULL == shell_)
    shell_ = shell;

  // Initialize the map that associates methods of this class with the names
  // they will use when called by JavaScript.  The actual binding of those
  // names to their methods will be done by calling BindToJavaScript() (defined
  // by CppBoundClass, the parent to LayoutTestController).
  BindMethod("dumpAsText", &LayoutTestController::dumpAsText);
  BindMethod("dumpChildFrameScrollPositions", &LayoutTestController::dumpChildFrameScrollPositions);
  BindMethod("dumpChildFramesAsText", &LayoutTestController::dumpChildFramesAsText);
  BindMethod("dumpEditingCallbacks", &LayoutTestController::dumpEditingCallbacks);
  BindMethod("dumpBackForwardList", &LayoutTestController::dumpBackForwardList);
  BindMethod("dumpFrameLoadCallbacks", &LayoutTestController::dumpFrameLoadCallbacks);
  BindMethod("dumpResourceLoadCallbacks", &LayoutTestController::dumpResourceLoadCallbacks);
  BindMethod("dumpTitleChanges", &LayoutTestController::dumpTitleChanges);
  BindMethod("setAcceptsEditing", &LayoutTestController::setAcceptsEditing);
  BindMethod("waitUntilDone", &LayoutTestController::waitUntilDone);
  BindMethod("notifyDone", &LayoutTestController::notifyDone);
  BindMethod("queueReload", &LayoutTestController::queueReload);
  BindMethod("queueScript", &LayoutTestController::queueScript);
  BindMethod("queueLoad", &LayoutTestController::queueLoad);
  BindMethod("queueBackNavigation", &LayoutTestController::queueBackNavigation);
  BindMethod("queueForwardNavigation", &LayoutTestController::queueForwardNavigation);
  BindMethod("windowCount", &LayoutTestController::windowCount);
  BindMethod("setCanOpenWindows", &LayoutTestController::setCanOpenWindows);
  BindMethod("setCloseRemainingWindowsWhenComplete", &LayoutTestController::setCloseRemainingWindowsWhenComplete);
  BindMethod("objCIdentityIsEqual", &LayoutTestController::objCIdentityIsEqual);
  BindMethod("setWindowIsKey", &LayoutTestController::setWindowIsKey);
  BindMethod("setTabKeyCyclesThroughElements", &LayoutTestController::setTabKeyCyclesThroughElements);
  BindMethod("setUserStyleSheetLocation", &LayoutTestController::setUserStyleSheetLocation);
  BindMethod("setUserStyleSheetEnabled", &LayoutTestController::setUserStyleSheetEnabled);
  BindMethod("pathToLocalResource", &LayoutTestController::pathToLocalResource);
  BindMethod("addFileToPasteboardOnDrag", &LayoutTestController::addFileToPasteboardOnDrag);
  BindMethod("execCommand", &LayoutTestController::execCommand);

  // The following are stubs.
  BindMethod("dumpAsWebArchive", &LayoutTestController::dumpAsWebArchive);
  BindMethod("setMainFrameIsFirstResponder", &LayoutTestController::setMainFrameIsFirstResponder);
  BindMethod("dumpSelectionRect", &LayoutTestController::dumpSelectionRect);
  BindMethod("display", &LayoutTestController::display);
  BindMethod("testRepaint", &LayoutTestController::testRepaint);
  BindMethod("repaintSweepHorizontally", &LayoutTestController::repaintSweepHorizontally);
  BindMethod("clearBackForwardList", &LayoutTestController::clearBackForwardList);
  BindMethod("keepWebHistory", &LayoutTestController::keepWebHistory);
  BindMethod("storeWebScriptObject", &LayoutTestController::storeWebScriptObject);
  BindMethod("accessStoredWebScriptObject", &LayoutTestController::accessStoredWebScriptObject);
  BindMethod("objCClassNameOf", &LayoutTestController::objCClassNameOf);
  BindMethod("addDisallowedURL", &LayoutTestController::addDisallowedURL);
  BindMethod("setCallCloseOnWebViews", &LayoutTestController::setCallCloseOnWebViews);
  BindMethod("setPrivateBrowsingEnabled", &LayoutTestController::setPrivateBrowsingEnabled);
  BindMethod("setUseDashboardCompatibilityMode", &LayoutTestController::setUseDashboardCompatibilityMode);
  BindMethod("setCustomPolicyDelegate", &LayoutTestController::setCustomPolicyDelegate);

  // This typo (missing 'i') is intentional as it matches the typo in the layout test
  // see: LayoutTests/fast/canvas/fill-stroke-clip-reset-path.html.
  // If Apple ever fixes this, we'll need to update it.
  BindMethod("setUseDashboardCompatiblityMode", &LayoutTestController::setUseDashboardCompatibilityMode);

  // The fallback method is called when an unknown method is invoked.
  BindFallbackMethod(&LayoutTestController::fallbackMethod);

  // Shared property used by a number of layout tests in
  // LayoutTests\http\tests\security\dataURL.
  BindProperty("globalFlag", &globalFlag_);
}

LayoutTestController::WorkQueue::~WorkQueue() {
  Reset();
}

void LayoutTestController::WorkQueue::ProcessWork() {
  // Quit doing work once a load is in progress.
  while (!queue_.empty() && !shell_->delegate()->top_loading_frame()) {
    queue_.front()->Run(shell_);
    delete queue_.front();
    queue_.pop();
  }

  if (!shell_->delegate()->top_loading_frame() && !wait_until_done_) {
    shell_->TestFinished();
  }
}

void LayoutTestController::WorkQueue::Reset() {
  frozen_ = false;
  while (!queue_.empty()) {
    delete queue_.front();
    queue_.pop();
  }
}

void LayoutTestController::WorkQueue::AddWork(WorkItem* work) {
  if (frozen_) {
    delete work;
    return;
  }
  queue_.push(work);
}

void LayoutTestController::dumpAsText(const CppArgumentList& args, 
                                                   CppVariant* result) {
  dump_as_text_ = true;
  result->SetNull();
}

void LayoutTestController::dumpEditingCallbacks(
    const CppArgumentList& args, CppVariant* result) {
  dump_editing_callbacks_ = true;
  result->SetNull();
}

void LayoutTestController::dumpBackForwardList(
    const CppArgumentList& args, CppVariant* result) {
  dump_back_forward_list_ = true;
  result->SetNull();
}

void LayoutTestController::dumpFrameLoadCallbacks(
    const CppArgumentList& args, CppVariant* result) {
  dump_frame_load_callbacks_ = true;
  result->SetNull();
}

void LayoutTestController::dumpResourceLoadCallbacks(
    const CppArgumentList& args, CppVariant* result) {
  dump_resource_load_callbacks_ = true;
  result->SetNull();
}

void LayoutTestController::dumpChildFrameScrollPositions(
    const CppArgumentList& args, CppVariant* result) {
  dump_child_frame_scroll_positions_ = true;
  result->SetNull();
}

void LayoutTestController::dumpChildFramesAsText(
    const CppArgumentList& args, CppVariant* result) {
  dump_child_frames_as_text_ = true;
  result->SetNull();
}

void LayoutTestController::dumpTitleChanges(
    const CppArgumentList& args, CppVariant* result) {
  dump_title_changes_ = true;
  result->SetNull();
}

void LayoutTestController::setAcceptsEditing(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool())
    accepts_editing_ = args[0].value.boolValue;
  result->SetNull();
}

void LayoutTestController::waitUntilDone(
    const CppArgumentList& args, CppVariant* result) {
  // Set a timer in case something hangs.  We use a custom timer rather than
  // the one managed by the message loop so we can kill it when the load
  // finishes successfully.
  if (!::IsDebuggerPresent()) {
    UINT_PTR timer_id = reinterpret_cast<UINT_PTR>(shell_);
    SetTimer(shell_->mainWnd(), timer_id, shell_->GetFileTestTimeout(),
             &TestTimeout);
  }
  wait_until_done_ = true;
  result->SetNull();
}

void LayoutTestController::notifyDone(
    const CppArgumentList& args, CppVariant* result) {
  if (!shell_->interactive() && wait_until_done_ &&
      !shell_->delegate()->top_loading_frame() && work_queue_.empty()) {
    shell_->TestFinished();
  }
  wait_until_done_ = false;
  result->SetNull();
}

class WorkItemBackForward : public LayoutTestController::WorkItem {
 public:
  WorkItemBackForward(int distance) : distance_(distance) {}
  void Run(TestShell* shell) {
    shell->GoBackOrForward(distance_);
  }
 private:
  int distance_;
};

void LayoutTestController::queueBackNavigation(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isNumber())
    work_queue_.AddWork(new WorkItemBackForward(-args[0].ToInt32()));
  result->SetNull();
}

void LayoutTestController::queueForwardNavigation(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isNumber())
    work_queue_.AddWork(new WorkItemBackForward(args[0].ToInt32()));
  result->SetNull();
}

class WorkItemReload : public LayoutTestController::WorkItem {
 public:
  void Run(TestShell* shell) {
    shell->Reload();
  }
};

void LayoutTestController::queueReload(
    const CppArgumentList& args, CppVariant* result) {
  work_queue_.AddWork(new WorkItemReload);
  result->SetNull();
}

class WorkItemScript : public LayoutTestController::WorkItem {
 public:
  WorkItemScript(const string& script) : script_(script) {}
  void Run(TestShell* shell) {
    wstring url = L"javascript:" + UTF8ToWide(script_);
    shell->LoadURL(url.c_str());
  }
 private:
  string script_;
};

void LayoutTestController::queueScript(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isString())
    work_queue_.AddWork(new WorkItemScript(args[0].ToString()));
  result->SetNull();
}

class WorkItemLoad : public LayoutTestController::WorkItem {
 public:
  WorkItemLoad(const GURL& url, const string& target)
      : url_(url), target_(target) {}
  void Run(TestShell* shell) {
    shell->LoadURLForFrame(UTF8ToWide(url_.spec()).c_str(),
                           UTF8ToWide(target_).c_str());
  }
 private:
  GURL url_;
  string target_;
};

void LayoutTestController::queueLoad(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isString()) {
    GURL current_url = shell_->webView()->GetMainFrame()->GetURL();
    GURL full_url = current_url.Resolve(args[0].ToString());

    string target = "";
    if (args.size() > 1 && args[1].isString())
      target = args[1].ToString();

    work_queue_.AddWork(new WorkItemLoad(full_url, target));
  }
  result->SetNull();
}

void LayoutTestController::objCIdentityIsEqual(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() < 2) {
    // This is the best we can do to return an error.
    result->SetNull();
    return;
  }
  result->Set(args[0].isEqual(args[1]));
}

void LayoutTestController::Reset() {
  if (shell_) {
    shell_->webView()->MakeTextStandardSize();
    shell_->webView()->SetTabKeyCyclesThroughElements(true);
  }
  dump_as_text_ = false;
  dump_editing_callbacks_ = false;
  dump_frame_load_callbacks_ = false;
  dump_resource_load_callbacks_ = false;
  dump_back_forward_list_ = false;
  dump_child_frame_scroll_positions_ = false;
  dump_child_frames_as_text_ = false;
  dump_title_changes_ = false;
  accepts_editing_ = true;
  wait_until_done_ = false;
  can_open_windows_ = false;
  should_add_file_to_pasteboard_ = false;
  globalFlag_.Set(false);

  if (close_remaining_windows_) {
    // Iterate through the window list and close everything except the original
    // shell.  We don't want to delete elements as we're iterating, so we copy
    // to a temp vector first.
    WindowList* windows = TestShell::windowList();
    std::vector<HWND> windows_to_delete;
    for (WindowList::iterator i = windows->begin(); i != windows->end(); ++i) {
      if (*i != shell_->mainWnd())
        windows_to_delete.push_back(*i);
    }
    DCHECK(windows_to_delete.size() + 1 == windows->size());
    for (size_t i = 0; i < windows_to_delete.size(); ++i) {
      DestroyWindow(windows_to_delete[i]);
    }
    DCHECK(windows->size() == 1);
  } else {
    // Reset the value
    close_remaining_windows_ = true;
  }
  work_queue_.Reset();
}

void LayoutTestController::LocationChangeDone() {
  // no more new work after the first complete load.
  work_queue_.set_frozen(true);

  if (!wait_until_done_)
    work_queue_.ProcessWork();
}

void LayoutTestController::setCanOpenWindows(
    const CppArgumentList& args, CppVariant* result) {
  can_open_windows_ = true;
  result->SetNull();
}

void LayoutTestController::setTabKeyCyclesThroughElements(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool()) {
    shell_->webView()->SetTabKeyCyclesThroughElements(args[0].ToBoolean());
  }
  result->SetNull();
}

void LayoutTestController::windowCount(
    const CppArgumentList& args, CppVariant* result) {
  int num_windows = static_cast<int>(TestShell::windowList()->size());
  result->Set(num_windows);
}

void LayoutTestController::setCloseRemainingWindowsWhenComplete(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool()) {
    close_remaining_windows_ = args[0].value.boolValue;
  }
  result->SetNull();
}

void LayoutTestController::setWindowIsKey(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool()) {
    shell_->SetFocus(shell_->webViewHost(), args[0].value.boolValue);
  }
  result->SetNull();
}

void LayoutTestController::setUserStyleSheetEnabled(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool()) {
    shell_->delegate()->SetUserStyleSheetEnabled(args[0].value.boolValue);
  }

  result->SetNull();
}

void LayoutTestController::setUserStyleSheetLocation(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isString()) {
    GURL location(TestShell::RewriteLocalUrl(args[0].ToString()));
    shell_->delegate()->SetUserStyleSheetLocation(location);
  }

  result->SetNull();
}

void LayoutTestController::execCommand(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isString()) {
    std::string command = args[0].ToString();
    std::string value("");

    // Ignore the second parameter (which is userInterface)
    // since this command emulates a manual action.
    if (args.size() >= 3 && args[2].isString())
      value = args[2].ToString();

    // Note: webkit's version does not return the boolean, so neither do we.
    shell_->webView()->GetFocusedFrame()->ExecuteCoreCommandByName(command,
                                                                   value);
  }
  result->SetNull();
}

void LayoutTestController::setUseDashboardCompatibilityMode(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool()) {
    shell_->delegate()->SetDashboardCompatibilityMode(args[0].value.boolValue);
  }

  result->SetNull();
}

void LayoutTestController::setCustomPolicyDelegate(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool()) {
    shell_->delegate()->SetCustomPolicyDelegate(args[0].value.boolValue);
  }

  result->SetNull();
}

void LayoutTestController::pathToLocalResource(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
  if (args.size() <= 0 || !args[0].isString())
    return;

  std::string url = args[0].ToString();
  // Some layout tests use file://// which we resolve as a UNC path.  Normalize
  // them to just file:///.
  while (StartsWithASCII(url, "file:////", false)) {
    url = url.substr(0, 8) + url.substr(9);
  }
  GURL location(TestShell::RewriteLocalUrl(url));
  result->Set(location.spec());
}

void LayoutTestController::addFileToPasteboardOnDrag(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
  should_add_file_to_pasteboard_ = true;
}

//
// Unimplemented stubs
//

void LayoutTestController::dumpAsWebArchive(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::setMainFrameIsFirstResponder(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::dumpSelectionRect(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::display(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::testRepaint(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::repaintSweepHorizontally(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::clearBackForwardList(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::keepWebHistory(
    const CppArgumentList& args,  CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::storeWebScriptObject(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::accessStoredWebScriptObject(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::objCClassNameOf(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}
void LayoutTestController::addDisallowedURL(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}
void LayoutTestController::setCallCloseOnWebViews(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}
void LayoutTestController::setPrivateBrowsingEnabled(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::fallbackMethod(
    const CppArgumentList& args, CppVariant* result) {
  std::wstring message(L"JavaScript ERROR: unknown method called on LayoutTestController");
  if (shell_->interactive()) {
    logging::LogMessage("CONSOLE:", 0).stream() << message;
  } else {
    printf("CONSOLE MESSAGE: %S\n", message.c_str());
  }
  result->SetNull();
}
