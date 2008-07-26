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

#include <windows.h>

#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/session_backend.h"
#include "chrome/browser/session_service.h"
#include "chrome/browser/session_service_test_helper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/scoped_vector.h"
#include "chrome/common/stl_util-inl.h"
#include "testing/gtest/include/gtest/gtest.h"

class SessionServiceTest : public testing::Test {
 public:
  SessionServiceTest() : window_bounds(0, 1, 2, 3) {}

 protected:
  virtual void SetUp() {
    wchar_t b[32];
    _itow_s(static_cast<int>(GetTickCount()), b, arraysize(b), 10);

    PathService::Get(base::DIR_TEMP, &path_);
    file_util::AppendToPath(&path_, L"SessionTestDirs");
    file_util::CreateDirectory(path_);
    file_util::AppendToPath(&path_, b);

    helper_.set_service(new SessionService(path_));

    service()->SetWindowType(window_id, BrowserType::TABBED_BROWSER);
    service()->SetWindowBounds(window_id, window_bounds, false);
  }

  virtual void TearDown() {
    helper_.set_service(NULL);
    file_util::Delete(path_, true);
  }

  void UpdateNavigation(const SessionID& window_id,
                        const SessionID& tab_id,
                        const TabNavigation& navigation,
                        bool select) {
    NavigationEntry entry(TAB_CONTENTS_UNKNOWN_TYPE);
    entry.SetURL(navigation.url);
    entry.SetTitle(navigation.title);
    entry.SetContentState(navigation.state);
    entry.SetTransitionType(navigation.transition);
    entry.SetHasPostData(navigation.type_mask & TabNavigation::HAS_POST_DATA);
    service()->UpdateTabNavigation(window_id, tab_id, navigation.index, entry);
    if (select)
      service()->SetSelectedNavigationIndex(window_id, tab_id,
                                            navigation.index);
  }

  void ReadWindows(std::vector<SessionWindow*>* windows) {
    // Forces closing the file.
    helper_.set_service(NULL);

    helper_.set_service(new SessionService(path_));
    helper_.ReadWindows(windows);
  }

  SessionService* service() { return helper_.service(); }

  SessionBackend* backend() { return helper_.backend(); }

  const gfx::Rect window_bounds;

  SessionID window_id;

  // Path used in testing.
  std::wstring path_;

  SessionServiceTestHelper helper_;
};

TEST_F(SessionServiceTest, Basic) {
  SessionID tab_id;
  ASSERT_NE(window_id.id(), tab_id.id());

  TabNavigation nav1(0, GURL("http://google.com"), L"abc", "def",
                     PageTransition::QUALIFIER_MASK);
  nav1.type_mask = TabNavigation::HAS_POST_DATA;

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(1, windows->size());
  ASSERT_TRUE(window_bounds == windows[0]->bounds);
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());
  ASSERT_EQ(1, windows[0]->tabs.size());
  ASSERT_EQ(BrowserType::TABBED_BROWSER, windows[0]->type);

  SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);

  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

TEST_F(SessionServiceTest, ClosingTabStaysClosed) {
  SessionID tab_id;
  SessionID tab2_id;
  ASSERT_NE(tab_id.id(), tab2_id.id());

  TabNavigation nav1(0, GURL("http://google.com"), L"abc", "def",
                     PageTransition::QUALIFIER_MASK);
  TabNavigation nav2(0, GURL("http://google2.com"), L"abcd", "defg",
                     PageTransition::AUTO_BOOKMARK);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  helper_.PrepareTabInWindow(window_id, tab2_id, 1, false);
  UpdateNavigation(window_id, tab2_id, nav2, true);
  service()->TabClosed(window_id, tab2_id);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(1, windows->size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());
  ASSERT_EQ(1, windows[0]->tabs.size());

  SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);

  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

TEST_F(SessionServiceTest, Pruning) {
  SessionID tab_id;

  TabNavigation nav1(0, GURL("http://google.com"), L"abc", "def",
                     PageTransition::QUALIFIER_MASK);
  TabNavigation nav2(0, GURL("http://google2.com"), L"abcd", "defg",
                     PageTransition::AUTO_BOOKMARK);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  for (int i = 0; i < 6; ++i) {
    TabNavigation& nav = (i % 2) == 0 ? nav1 : nav2;
    nav.index = i;
    UpdateNavigation(window_id, tab_id, nav, true);
  }
  service()->TabNavigationPathPruned(window_id, tab_id, 3);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(1, windows->size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(1, windows[0]->tabs.size());

  SessionTab* tab = windows[0]->tabs[0];
  // We left the selected index at 5, then pruned. When rereading the
  // index should get reset to last valid navigation, which is 2.
  helper_.AssertTabEquals(window_id, tab_id, 0, 2, 3, *tab);

  nav1.index = 0;
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
  nav2.index = 1;
  helper_.AssertNavigationEquals(nav2, tab->navigations[1]);
  nav1.index = 2;
  helper_.AssertNavigationEquals(nav1, tab->navigations[2]);
}

TEST_F(SessionServiceTest, TwoWindows) {
  SessionID window2_id;
  SessionID tab1_id;
  SessionID tab2_id;

  TabNavigation nav1(0, GURL("http://google.com"), L"abc", "def",
                     PageTransition::QUALIFIER_MASK);
  TabNavigation nav2(0, GURL("http://google2.com"), L"abcd", "defg",
                     PageTransition::AUTO_BOOKMARK);

  helper_.PrepareTabInWindow(window_id, tab1_id, 0, true);
  UpdateNavigation(window_id, tab1_id, nav1, true);

  const gfx::Rect window2_bounds(3, 4, 5, 6);
  service()->SetWindowType(window2_id, BrowserType::TABBED_BROWSER);
  service()->SetWindowBounds(window2_id, window2_bounds, true);
  helper_.PrepareTabInWindow(window2_id, tab2_id, 0, true);
  UpdateNavigation(window2_id, tab2_id, nav2, true);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(2, windows->size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(0, windows[1]->selected_tab_index);
  ASSERT_EQ(1, windows[0]->tabs.size());
  ASSERT_EQ(1, windows[1]->tabs.size());

  SessionTab* rt1;
  SessionTab* rt2;
  if (windows[0]->window_id.id() == window_id.id()) {
    ASSERT_EQ(window2_id.id(), windows[1]->window_id.id());
    ASSERT_FALSE(windows[0]->is_maximized);
    ASSERT_TRUE(windows[1]->is_maximized);
    rt1 = windows[0]->tabs[0];
    rt2 = windows[1]->tabs[0];
  } else {
    ASSERT_EQ(window2_id.id(), windows[0]->window_id.id());
    ASSERT_EQ(window_id.id(), windows[1]->window_id.id());
    ASSERT_TRUE(windows[0]->is_maximized);
    ASSERT_FALSE(windows[1]->is_maximized);
    rt1 = windows[1]->tabs[0];
    rt2 = windows[0]->tabs[0];
  }
  SessionTab* tab = rt1;
  helper_.AssertTabEquals(window_id, tab1_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);

  tab = rt2;
  helper_.AssertTabEquals(window2_id, tab2_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav2, tab->navigations[0]);
}

TEST_F(SessionServiceTest, WindowWithNoTabsGetsPruned) {
  SessionID window2_id;
  SessionID tab1_id;
  SessionID tab2_id;

  TabNavigation nav1(0, GURL("http://google.com"), L"abc", "def",
                     PageTransition::QUALIFIER_MASK);

  helper_.PrepareTabInWindow(window_id, tab1_id, 0, true);
  UpdateNavigation(window_id, tab1_id, nav1, true);

  const gfx::Rect window2_bounds(3, 4, 5, 6);
  service()->SetWindowType(window2_id, BrowserType::TABBED_BROWSER);
  service()->SetWindowBounds(window2_id, window2_bounds, false);
  helper_.PrepareTabInWindow(window2_id, tab2_id, 0, true);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(1, windows->size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(1, windows[0]->tabs.size());
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());

  SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab1_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

TEST_F(SessionServiceTest, ClosingWindowDoesntCloseTabs) {
  SessionID tab_id;
  SessionID tab2_id;
  ASSERT_NE(tab_id.id(), tab2_id.id());

  TabNavigation nav1(0, GURL("http://google.com"), L"abc", "def",
                     PageTransition::QUALIFIER_MASK);
  TabNavigation nav2(0, GURL("http://google2.com"), L"abcd", "defg",
                     PageTransition::AUTO_BOOKMARK);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  helper_.PrepareTabInWindow(window_id, tab2_id, 1, false);
  UpdateNavigation(window_id, tab2_id, nav2, true);

  service()->WindowClosing(window_id);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(1, windows->size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());
  ASSERT_EQ(2, windows[0]->tabs.size());

  SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);

  tab = windows[0]->tabs[1];
  helper_.AssertTabEquals(window_id, tab2_id, 1, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav2, tab->navigations[0]);
}

TEST_F(SessionServiceTest, WindowCloseCommittedAfterNavigate) {
  SessionID window2_id;
  SessionID tab_id;
  SessionID tab2_id;
  ASSERT_NE(window2_id.id(), window_id.id());

  service()->SetWindowType(window2_id, BrowserType::TABBED_BROWSER);
  service()->SetWindowBounds(window2_id, window_bounds, false);

  TabNavigation nav1(0, GURL("http://google.com"), L"abc", "def",
                     PageTransition::QUALIFIER_MASK);
  TabNavigation nav2(0, GURL("http://google2.com"), L"abcd", "defg",
                     PageTransition::AUTO_BOOKMARK);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  helper_.PrepareTabInWindow(window2_id, tab2_id, 0, false);
  UpdateNavigation(window2_id, tab2_id, nav2, true);

  service()->WindowClosing(window2_id);
  service()->TabClosed(window2_id, tab2_id);
  service()->WindowClosed(window2_id);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(1, windows->size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());
  ASSERT_EQ(1, windows[0]->tabs.size());

  SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

// Makes sure we don't track popups.
TEST_F(SessionServiceTest, IgnorePopups) {
  SessionID window2_id;
  SessionID tab_id;
  SessionID tab2_id;
  ASSERT_NE(window2_id.id(), window_id.id());

  service()->SetWindowType(window2_id, BrowserType::BROWSER);
  service()->SetWindowBounds(window2_id, window_bounds, false);

  TabNavigation nav1(0, GURL("http://google.com"), L"abc", "def",
                     PageTransition::QUALIFIER_MASK);
  TabNavigation nav2(0, GURL("http://google2.com"), L"abcd", "defg",
                     PageTransition::AUTO_BOOKMARK);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  helper_.PrepareTabInWindow(window2_id, tab2_id, 0, false);
  UpdateNavigation(window2_id, tab2_id, nav2, true);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(1, windows->size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());
  ASSERT_EQ(1, windows[0]->tabs.size());

  SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}
