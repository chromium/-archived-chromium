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

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_NAVIGATION_CONTROLLER_H__
#define WEBKIT_TOOLS_TEST_SHELL_TEST_NAVIGATION_CONTROLLER_H__

#include <vector>
#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "webkit/glue/weburlrequest.h"
#include "webkit/tools/test_shell/temp/navigation_controller_base.h"
#include "webkit/tools/test_shell/temp/navigation_entry.h"
#include "webkit/tools/test_shell/temp/page_transition_types.h"

class GURL;
class TestShell;
class WebHistoryItem;

// Associated with browser-initated navigations to hold tracking data.
class TestShellExtraRequestData : public WebRequest::ExtraData {
 public:
  TestShellExtraRequestData(int32 pending_page_id,
                            PageTransition::Type transition)
      : WebRequest::ExtraData(),
        pending_page_id(pending_page_id),
        transition_type(transition),
        request_committed(false) {
  }

  // Contains the page_id for this navigation or -1 if there is none yet.
  int32 pending_page_id;

  // Contains the transition type that the browser specified when it
  // initiated the load.
  PageTransition::Type transition_type;

  // True if we have already processed the "DidCommitLoad" event for this
  // request.  Used by session history.
  bool request_committed;
};

// Same as TestNavigationEntry, but caches the HistoryItem.
class TestNavigationEntry : public NavigationEntry {
 public:
  TestNavigationEntry();
  TestNavigationEntry(int page_id,
                      const GURL& url,
                      const std::wstring& title,
                      PageTransition::Type transition,
                      const std::wstring& target_frame);

  ~TestNavigationEntry();

  // We don't care about tab contents types, so just pick one and use it
  // everywhere.
  static TabContentsType GetTabContentsType() {
    return 0;
  }

  void SetContentState(const std::string& state);
  WebHistoryItem* GetHistoryItem() const;

  const std::wstring& GetTargetFrame() const { return target_frame_; }

private:
  mutable scoped_refptr<WebHistoryItem> cached_history_item_;
  std::wstring target_frame_;
};

// Test shell's NavigationController.  The goal is to be as close to the Chrome
// version as possible.
class TestNavigationController : public NavigationControllerBase {
 public:
  TestNavigationController(TestShell* shell);
  ~TestNavigationController();

  virtual void Reset();

 private:
  virtual int GetMaxPageID() const { return max_page_id_; }
  virtual void NavigateToPendingEntry(bool reload);
  virtual void NotifyNavigationStateChanged();

  TestShell* shell_;
  int max_page_id_;

  DISALLOW_EVIL_CONSTRUCTORS(TestNavigationController);
};

#endif // WEBKIT_TOOLS_TEST_SHELL_TEST_NAVIGATION_CONTROLLER_H__
