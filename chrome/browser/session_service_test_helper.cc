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

#include "chrome/browser/session_service_test_helper.h"

#include "chrome/browser/session_backend.h"
#include "chrome/browser/session_service.h"
#include "chrome/common/scoped_vector.h"
#include "testing/gtest/include/gtest/gtest.h"

void SessionServiceTestHelper::RestoreSessionFromCommands(
    const std::vector<SessionCommand*>& commands,
    std::vector<SessionWindow*>* valid_windows) {
  service()->RestoreSessionFromCommands(commands, valid_windows);
}

void SessionServiceTestHelper::PrepareTabInWindow(const SessionID& window_id,
                                                  const SessionID& tab_id,
                                                  int visual_index,
                                                  bool select) {
  service()->SetTabWindow(window_id, tab_id);
  service()->SetTabIndexInWindow(window_id, tab_id, visual_index);
  if (select)
    service()->SetSelectedTabInWindow(window_id, visual_index);
}

// Be sure and null out service to force closing the file.
void SessionServiceTestHelper::ReadWindows(
    std::vector<SessionWindow*>* windows) {
  Time last_time;
  ScopedVector<SessionCommand> read_commands;
  backend()->ReadSessionImpl(false, &(read_commands.get()));
  RestoreSessionFromCommands(read_commands.get(), windows);
}

void SessionServiceTestHelper::AssertTabEquals(SessionID& window_id,
                                               SessionID& tab_id,
                                               int visual_index,
                                               int nav_index,
                                               int nav_count,
                                               const SessionTab& session_tab) {
  EXPECT_EQ(window_id.id(), session_tab.window_id.id());
  EXPECT_EQ(tab_id.id(), session_tab.tab_id.id());
  AssertTabEquals(visual_index, nav_index, nav_count, session_tab);
}

void SessionServiceTestHelper::AssertTabEquals(
    int visual_index,
    int nav_index,
    int nav_count,
    const SessionTab& session_tab) {
  EXPECT_EQ(visual_index, session_tab.tab_visual_index);
  EXPECT_EQ(nav_index, session_tab.current_navigation_index);
  ASSERT_EQ(nav_count, session_tab.navigations.size());
}

void SessionServiceTestHelper::AssertNavigationEquals(
    const TabNavigation& expected,
    const TabNavigation& actual) {
  EXPECT_EQ(expected.index, actual.index);
  EXPECT_TRUE(expected.url == actual.url);
  EXPECT_EQ(expected.title, actual.title);
  EXPECT_EQ(expected.state, actual.state);
  EXPECT_EQ(expected.transition, actual.transition);
  EXPECT_EQ(expected.type_mask, actual.type_mask);
}

void SessionServiceTestHelper::AssertSingleWindowWithSingleTab(
    const std::vector<SessionWindow*>& windows,
    int nav_count) {
  EXPECT_EQ(1, windows.size());
  EXPECT_EQ(1, windows[0]->tabs.size());
  EXPECT_EQ(nav_count, windows[0]->tabs[0]->navigations.size());
}

SessionBackend* SessionServiceTestHelper::backend() {
  return service_->backend_.get();
}
