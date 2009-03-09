// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service_test_helper.h"

#include "chrome/browser/sessions/session_backend.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/common/scoped_vector.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

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
  backend()->ReadLastSessionCommandsImpl(&(read_commands.get()));
  RestoreSessionFromCommands(read_commands.get(), windows);
}

void SessionServiceTestHelper::AssertTabEquals(SessionID& window_id,
                                               SessionID& tab_id,
                                               int visual_index,
                                               int nav_index,
                                               size_t nav_count,
                                               const SessionTab& session_tab) {
  EXPECT_EQ(window_id.id(), session_tab.window_id.id());
  EXPECT_EQ(tab_id.id(), session_tab.tab_id.id());
  AssertTabEquals(visual_index, nav_index, nav_count, session_tab);
}

void SessionServiceTestHelper::AssertTabEquals(
    int visual_index,
    int nav_index,
    size_t nav_count,
    const SessionTab& session_tab) {
  EXPECT_EQ(visual_index, session_tab.tab_visual_index);
  EXPECT_EQ(nav_index, session_tab.current_navigation_index);
  ASSERT_EQ(nav_count, session_tab.navigations.size());
}

void SessionServiceTestHelper::AssertNavigationEquals(
    const TabNavigation& expected,
    const TabNavigation& actual) {
  EXPECT_TRUE(expected.url() == actual.url());
  EXPECT_EQ(expected.referrer(), actual.referrer());
  EXPECT_EQ(expected.title(), actual.title());
  EXPECT_EQ(expected.state(), actual.state());
  EXPECT_EQ(expected.transition(), actual.transition());
  EXPECT_EQ(expected.type_mask(), actual.type_mask());
}

void SessionServiceTestHelper::AssertSingleWindowWithSingleTab(
    const std::vector<SessionWindow*>& windows,
    size_t nav_count) {
  ASSERT_EQ(1U, windows.size());
  EXPECT_EQ(1U, windows[0]->tabs.size());
  EXPECT_EQ(nav_count, windows[0]->tabs[0]->navigations.size());
}

SessionBackend* SessionServiceTestHelper::backend() {
  return service_->backend();
}

