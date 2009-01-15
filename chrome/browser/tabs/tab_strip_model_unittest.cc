// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/dock_info.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_factory.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/tabs/tab_strip_model_order_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/stl_util-inl.h"
#include "testing/gtest/include/gtest/gtest.h"

const TabContentsType kHTTPTabContentsType =
    static_cast<TabContentsType>(TAB_CONTENTS_NUM_TYPES + 1);
const TabContentsType kReplacementContentsType =
    static_cast<TabContentsType>(kHTTPTabContentsType + 1);

// Since you can't just instantiate a TabContents, and some of its methods
// are protected, we subclass TabContents with our own testing dummy which
// knows how to drive the base class' NavigationController as URLs are
// loaded.
class TabStripModelTestTabContents : public TabContents {
 public:
  TabStripModelTestTabContents(const TabContentsType type)
      : TabContents(type) {
  }
};


// This constructs our fake TabContents.
class TabStripModelTestTabContentsFactory : public TabContentsFactory {
 public:
  virtual TabContents* CreateInstance() {
    return new TabStripModelTestTabContents(kHTTPTabContentsType);
  }

  virtual bool CanHandleURL(const GURL& url) {
    return url.scheme() == "http";
  }
};

TabStripModelTestTabContentsFactory factory;

class TabStripModelTest : public testing::Test {
 public:
  // Overridden from testing::Test
  virtual void SetUp() {
    TabContents::RegisterFactory(kHTTPTabContentsType, &factory);

    // Name a subdirectory of the temp directory.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
    file_util::AppendToPath(&test_dir_, L"TabStripModelTest");

    // Create a fresh, empty copy of this directory.
    file_util::Delete(test_dir_, true);
    CreateDirectory(test_dir_.c_str(), NULL);

    profile_path_ = test_dir_;
    file_util::AppendToPath(&profile_path_, L"New Profile");

    profile_ = ProfileManager::CreateProfile(profile_path_,
        L"New Profile", L"new-profile", L"");
    ASSERT_TRUE(profile_);
    pm_.AddProfile(profile_);
  }

  virtual void TearDown() {
    TabContents::RegisterFactory(kHTTPTabContentsType, NULL);

    // Removes a profile from the set of currently-loaded profiles.
    pm_.RemoveProfileByPath(profile_path_);

    // Clean up test directory
    ASSERT_TRUE(file_util::Delete(test_dir_, true));
    ASSERT_FALSE(file_util::PathExists(test_dir_));

    // Flush the message loop to make Purify happy.
    message_loop_.RunAllPending();
  }

 protected:
  TabContents* CreateTabContents() {
    TabStripModelTestTabContents* contents =
        new TabStripModelTestTabContents(kHTTPTabContentsType);
    contents->SetupController(profile_);
    return contents;
  }
  TabContents* CreateReplacementContents() {
    TabStripModelTestTabContents* contents =
        new TabStripModelTestTabContents(kReplacementContentsType);
    contents->SetupController(profile_);
    return contents;
  }
  TabContents* CreateNewTabTabContents() {
    TabStripModelTestTabContents* contents =
        new TabStripModelTestTabContents(TAB_CONTENTS_NEW_TAB_UI);
    contents->SetupController(profile_);
    return contents;
  }

  // Forwards a URL "load" request through to our dummy TabContents
  // implementation.
  void LoadURL(TabContents* contents, const std::wstring& url) {
    contents->controller()->LoadURL(GURL(url), GURL(), PageTransition::LINK);
  }

  void GoBack(TabContents* contents) {
    contents->controller()->GoBack();
  }

  void GoForward(TabContents* contents) {
    contents->controller()->GoForward();
  }

  void SwitchTabTo(TabContents* contents) {
    contents->DidBecomeSelected();
  }

 Profile* profile_;

 private:
  MessageLoopForUI message_loop_;
  std::wstring test_dir_;
  std::wstring profile_path_;
  ProfileManager pm_;
};

class MockTabStripModelObserver : public TabStripModelObserver {
 public:
  MockTabStripModelObserver() : empty_(true) {}
  ~MockTabStripModelObserver() {
    STLDeleteContainerPointers(states_.begin(), states_.end());
  }

  enum TabStripModelObserverAction {
    INSERT,
    CLOSE,
    DETACH,
    SELECT,
    MOVE,
    CHANGE
  };

  struct State {
    State(TabContents* a_dst_contents,
          int a_dst_index,
          TabStripModelObserverAction a_action)
        : src_contents(NULL),
          dst_contents(a_dst_contents),
          src_index(-1),
          dst_index(a_dst_index),
          action(a_action),
          user_gesture(false),
          foreground(false) {
    }

    TabContents* src_contents;
    TabContents* dst_contents;
    int src_index;
    int dst_index;
    bool user_gesture;
    bool foreground;
    TabStripModelObserverAction action;
  };

  int GetStateCount() const {
    return static_cast<int>(states_.size());
  }

  State* GetStateAt(int index) const {
    DCHECK(index >= 0 && index < GetStateCount());
    return states_.at(index);
  }

  bool StateEquals(int index, const State& state) {
    State* s = GetStateAt(index);
    EXPECT_EQ(s->src_contents, state.src_contents);
    EXPECT_EQ(s->dst_contents, state.dst_contents);
    EXPECT_EQ(s->src_index, state.src_index);
    EXPECT_EQ(s->dst_index, state.dst_index);
    EXPECT_EQ(s->user_gesture, state.user_gesture);
    EXPECT_EQ(s->foreground, state.foreground);
    EXPECT_EQ(s->action, state.action);
    return (s->src_contents == state.src_contents &&
            s->dst_contents == state.dst_contents &&
            s->src_index == state.src_index &&
            s->dst_index == state.dst_index &&
            s->user_gesture == state.user_gesture &&
            s->foreground == state.foreground &&
            s->action == state.action);
  }

  // TabStripModelObserver implementation:
  virtual void TabInsertedAt(TabContents* contents,
                             int index,
                             bool foreground) {
    empty_ = false;
    State* s = new State(contents, index, INSERT);
    s->foreground = foreground;
    states_.push_back(s);
  }
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* new_contents,
                             int index,
                             bool user_gesture) {
    State* s = new State(new_contents, index, SELECT);
    s->src_contents = old_contents;
    s->user_gesture = user_gesture;
    states_.push_back(s);
  }
  virtual void TabMoved(
      TabContents* contents, int from_index, int to_index) {
    State* s = new State(contents, to_index, MOVE);
    s->src_index = from_index;
    states_.push_back(s);
  }

  virtual void TabClosingAt(TabContents* contents, int index) {
    states_.push_back(new State(contents, index, CLOSE));
  }
  virtual void TabDetachedAt(TabContents* contents, int index) {
    states_.push_back(new State(contents, index, DETACH));
  }
  virtual void TabChangedAt(TabContents* contents, int index) {
    states_.push_back(new State(contents, index, CHANGE));
  }
  virtual void TabStripEmpty() {
    empty_ = true;
  }

  void ClearStates() {
    STLDeleteContainerPointers(states_.begin(), states_.end());
    states_.clear();
  }

  bool empty() const { return empty_; }

 private:
  std::vector<State*> states_;

  bool empty_;

  DISALLOW_EVIL_CONSTRUCTORS(MockTabStripModelObserver);
};

TEST_F(TabStripModelTest, TestBasicAPI) {
  TabStripModel tabstrip(NULL, profile_);
  MockTabStripModelObserver observer;
  tabstrip.AddObserver(&observer);

  EXPECT_TRUE(tabstrip.empty());

  typedef MockTabStripModelObserver::State State;

  TabContents* contents1 = CreateTabContents();

  // Note! The ordering of these tests is important, each subsequent test
  // builds on the state established in the previous. This is important if you
  // ever insert tests rather than append.

  // Test AppendTabContents, ContainsIndex
  {
    EXPECT_FALSE(tabstrip.ContainsIndex(0));
    tabstrip.AppendTabContents(contents1, true);
    EXPECT_TRUE(tabstrip.ContainsIndex(0));
    EXPECT_EQ(1, tabstrip.count());
    EXPECT_EQ(2, observer.GetStateCount());
    State s1(contents1, 0, MockTabStripModelObserver::INSERT);
    s1.foreground = true;
    EXPECT_TRUE(observer.StateEquals(0, s1));
    State s2(contents1, 0, MockTabStripModelObserver::SELECT);
    s2.src_contents = NULL;
    EXPECT_TRUE(observer.StateEquals(1, s2));
    observer.ClearStates();
  }

  // Test InsertTabContentsAt, foreground tab.
  TabContents* contents2 = CreateTabContents();
  {
    tabstrip.InsertTabContentsAt(1, contents2, true, false);

    EXPECT_EQ(2, tabstrip.count());
    EXPECT_EQ(2, observer.GetStateCount());
    State s1(contents2, 1, MockTabStripModelObserver::INSERT);
    s1.foreground = true;
    EXPECT_TRUE(observer.StateEquals(0, s1));
    State s2(contents2, 1, MockTabStripModelObserver::SELECT);
    s2.src_contents = contents1;
    EXPECT_TRUE(observer.StateEquals(1, s2));
    observer.ClearStates();
  }

  // Test InsertTabContentsAt, background tab.
  TabContents* contents3 = CreateTabContents();
  {
    tabstrip.InsertTabContentsAt(2, contents3, false, false);

    EXPECT_EQ(3, tabstrip.count());
    EXPECT_EQ(1, observer.GetStateCount());
    State s1(contents3, 2, MockTabStripModelObserver::INSERT);
    s1.foreground = false;
    EXPECT_TRUE(observer.StateEquals(0, s1));
    observer.ClearStates();
  }

  // Test SelectTabContentsAt
  {
    tabstrip.SelectTabContentsAt(2, true);
    EXPECT_EQ(1, observer.GetStateCount());
    State s1(contents3, 2, MockTabStripModelObserver::SELECT);
    s1.src_contents = contents2;
    s1.user_gesture = true;
    EXPECT_TRUE(observer.StateEquals(0, s1));
    observer.ClearStates();
  }

  // Test ReplaceTabContentsAt, replacing the selected index
  TabContents* replacement_contents3 = CreateReplacementContents();
  {
    tabstrip.ReplaceTabContentsAt(2, replacement_contents3);
    // ReplaceTabContentsAt doesn't delete the source.  It depends on
    // NavigationCollector, which is not part of this test.
    contents3->Destroy();

    EXPECT_EQ(2, observer.GetStateCount());
    State s1(replacement_contents3, 2, MockTabStripModelObserver::CHANGE);
    EXPECT_TRUE(observer.StateEquals(0, s1));
    State s2(replacement_contents3, 2, MockTabStripModelObserver::SELECT);
    s2.src_contents = contents3;
    s2.user_gesture = false;
    EXPECT_TRUE(observer.StateEquals(1, s2));
    observer.ClearStates();
  }

  // Test ReplaceTabContentsAt, replacing NOT the selected index
  TabContents* replacement_contents2 = CreateReplacementContents();
  {
    tabstrip.ReplaceTabContentsAt(1, replacement_contents2);
    // ReplaceTabContentsAt doesn't delete the source.  It depends on
    // NavigationCollector, which is not part of this test.
    contents2->Destroy();

    EXPECT_EQ(1, observer.GetStateCount());
    State s1(replacement_contents2, 1, MockTabStripModelObserver::CHANGE);
    EXPECT_TRUE(observer.StateEquals(0, s1));
    observer.ClearStates();
  }

  // Test DetachTabContentsAt
  {
    // Detach
    TabContents* detached = tabstrip.DetachTabContentsAt(2);
    // ... and append again because we want this for later.
    tabstrip.AppendTabContents(detached, true);
    EXPECT_EQ(4, observer.GetStateCount());
    State s1(detached, 2, MockTabStripModelObserver::DETACH);
    EXPECT_TRUE(observer.StateEquals(0, s1));
    State s2(replacement_contents2, 1, MockTabStripModelObserver::SELECT);
    s2.src_contents = replacement_contents3;
    s2.user_gesture = false;
    EXPECT_TRUE(observer.StateEquals(1, s2));
    State s3(detached, 2, MockTabStripModelObserver::INSERT);
    s3.foreground = true;
    EXPECT_TRUE(observer.StateEquals(2, s3));
    State s4(detached, 2, MockTabStripModelObserver::SELECT);
    s4.src_contents = replacement_contents2;
    s4.user_gesture = false;
    EXPECT_TRUE(observer.StateEquals(3, s4));
    observer.ClearStates();
  }

  // Test CloseTabContentsAt
  {
    tabstrip.CloseTabContentsAt(2);
    EXPECT_EQ(2, tabstrip.count());

    EXPECT_EQ(3, observer.GetStateCount());
    State s1(replacement_contents3, 2, MockTabStripModelObserver::CLOSE);
    EXPECT_TRUE(observer.StateEquals(0, s1));
    State s2(replacement_contents3, 2, MockTabStripModelObserver::DETACH);
    EXPECT_TRUE(observer.StateEquals(1, s2));
    State s3(replacement_contents2, 1, MockTabStripModelObserver::SELECT);
    s3.src_contents = replacement_contents3;
    s3.user_gesture = false;
    EXPECT_TRUE(observer.StateEquals(2, s3));
    observer.ClearStates();
  }

  // Test MoveTabContentsAt
  {
    tabstrip.MoveTabContentsAt(1, 0);

    EXPECT_EQ(1, observer.GetStateCount());
    State s1(replacement_contents2, 0, MockTabStripModelObserver::MOVE);
    s1.src_index = 1;
    EXPECT_TRUE(observer.StateEquals(0, s1));
    observer.ClearStates();
  }

  // Test Getters
  {
    EXPECT_EQ(replacement_contents2, tabstrip.GetSelectedTabContents());
    EXPECT_EQ(replacement_contents2, tabstrip.GetTabContentsAt(0));
    EXPECT_EQ(contents1, tabstrip.GetTabContentsAt(1));
    EXPECT_EQ(0, tabstrip.GetIndexOfTabContents(replacement_contents2));
    EXPECT_EQ(1, tabstrip.GetIndexOfTabContents(contents1));
    EXPECT_EQ(0, tabstrip.GetIndexOfController(
        replacement_contents2->controller()));
    EXPECT_EQ(1, tabstrip.GetIndexOfController(contents1->controller()));
  }

  // Test UpdateTabContentsStateAt
  {
    tabstrip.UpdateTabContentsStateAt(0);
    EXPECT_EQ(1, observer.GetStateCount());
    State s1(replacement_contents2, 0, MockTabStripModelObserver::CHANGE);
    EXPECT_TRUE(observer.StateEquals(0, s1));
    observer.ClearStates();
  }

  // Test SelectNextTab, SelectPreviousTab, SelectLastTab
  {
    // Make sure the second of the two tabs is selected first...
    tabstrip.SelectTabContentsAt(1, true);
    tabstrip.SelectPreviousTab();
    EXPECT_EQ(0, tabstrip.selected_index());
    tabstrip.SelectLastTab();
    EXPECT_EQ(1, tabstrip.selected_index());
    tabstrip.SelectNextTab();
    EXPECT_EQ(0, tabstrip.selected_index());
  }

  // Test CloseSelectedTab
  {
    tabstrip.CloseSelectedTab();
    // |CloseSelectedTab| calls CloseTabContentsAt, we already tested that, now
    // just verify that the count and selected index have changed
    // appropriately...
    EXPECT_EQ(1, tabstrip.count());
    EXPECT_EQ(0, tabstrip.selected_index());
  }

  tabstrip.CloseAllTabs();
  // TabStripModel should now be empty.
  EXPECT_TRUE(tabstrip.empty());

  // Opener methods are tested below...

  tabstrip.RemoveObserver(&observer);
}

TEST_F(TabStripModelTest, TestBasicOpenerAPI) {
  TabStripModel tabstrip(NULL, profile_);
  EXPECT_TRUE(tabstrip.empty());

  // This is a basic test of opener functionality. opener_contents is created
  // as the first tab in the strip and then we create 5 other tabs in the
  // background with opener_contents set as their opener.

  TabContents* opener_contents = CreateTabContents();
  NavigationController* opener = opener_contents->controller();
  tabstrip.AppendTabContents(opener_contents, true);
  TabContents* contents1 = CreateTabContents();
  TabContents* contents2 = CreateTabContents();
  TabContents* contents3 = CreateTabContents();
  TabContents* contents4 = CreateTabContents();
  TabContents* contents5 = CreateTabContents();

  // We use |InsertTabContentsAt| here instead of AppendTabContents so that
  // openership relationships are preserved.
  tabstrip.InsertTabContentsAt(tabstrip.count(), contents1, false, true);
  tabstrip.InsertTabContentsAt(tabstrip.count(), contents2, false, true);
  tabstrip.InsertTabContentsAt(tabstrip.count(), contents3, false, true);
  tabstrip.InsertTabContentsAt(tabstrip.count(), contents4, false, true);
  tabstrip.InsertTabContentsAt(tabstrip.count(), contents5, false, true);

  // All the tabs should have the same opener.
  for (int i = 1; i < tabstrip.count(); ++i)
    EXPECT_EQ(opener, tabstrip.GetOpenerOfTabContentsAt(i));

  // If there is a next adjacent item, then the index should be of that item.
  EXPECT_EQ(2, tabstrip.GetIndexOfNextTabContentsOpenedBy(opener, 1, false));
  // If the last tab in the group is closed, the preceding tab in the same
  // group should be selected.
  EXPECT_EQ(4, tabstrip.GetIndexOfNextTabContentsOpenedBy(opener, 5, false));

  // Tests the method that finds the last tab opened by the same opener in the
  // strip (this is the insertion index for the next background tab for the
  // specified opener).
  EXPECT_EQ(5, tabstrip.GetIndexOfLastTabContentsOpenedBy(opener, 1));

  // For a tab that has opened no other tabs, the return value should always be
  // -1...
  NavigationController* o1 = contents1->controller();
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextTabContentsOpenedBy(o1, 3, false));
  EXPECT_EQ(-1, tabstrip.GetIndexOfLastTabContentsOpenedBy(o1, 3));

  // ForgetAllOpeners should destroy all opener relationships.
  tabstrip.ForgetAllOpeners();
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextTabContentsOpenedBy(opener, 1, false));
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextTabContentsOpenedBy(opener, 5, false));
  EXPECT_EQ(-1, tabstrip.GetIndexOfLastTabContentsOpenedBy(opener, 1));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

static int GetInsertionIndex(TabStripModel* tabstrip, TabContents* contents) {
  return tabstrip->order_controller()->DetermineInsertionIndex(
      contents, PageTransition::LINK, false);
}

static void InsertTabContentses(TabStripModel* tabstrip,
                                TabContents* contents1,
                                TabContents* contents2,
                                TabContents* contents3) {
  tabstrip->InsertTabContentsAt(GetInsertionIndex(tabstrip, contents1),
                                contents1, false, true);
  tabstrip->InsertTabContentsAt(GetInsertionIndex(tabstrip, contents2),
                                contents2, false, true);
  tabstrip->InsertTabContentsAt(GetInsertionIndex(tabstrip, contents3),
                                contents3, false, true);
}

// Tests opening background tabs.
TEST_F(TabStripModelTest, TestLTRInsertionOptions) {
  TabStripModel tabstrip(NULL, profile_);
  EXPECT_TRUE(tabstrip.empty());

  TabContents* opener_contents = CreateTabContents();
  tabstrip.AppendTabContents(opener_contents, true);

  TabContents* contents1 = CreateTabContents();
  TabContents* contents2 = CreateTabContents();
  TabContents* contents3 = CreateTabContents();

  // Test LTR
  InsertTabContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(contents1, tabstrip.GetTabContentsAt(1));
  EXPECT_EQ(contents2, tabstrip.GetTabContentsAt(2));
  EXPECT_EQ(contents3, tabstrip.GetTabContentsAt(3));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// This test constructs a tabstrip, and then simulates loading several tabs in
// the background from link clicks on the first tab. Then it simulates opening
// a new tab from the first tab in the foreground via a link click, verifies
// that this tab is opened adjacent to the opener, then closes it.
// Finally it tests that a tab opened for some non-link purpose openes at the
// end of the strip, not bundled to any existing context.
TEST_F(TabStripModelTest, TestInsertionIndexDetermination) {
  TabStripModel tabstrip(NULL, profile_);
  EXPECT_TRUE(tabstrip.empty());

  TabContents* opener_contents = CreateTabContents();
  NavigationController* opener = opener_contents->controller();
  tabstrip.AppendTabContents(opener_contents, true);

  // Open some other random unrelated tab in the background to monkey with our
  // insertion index.
  TabContents* other_contents = CreateTabContents();
  tabstrip.AppendTabContents(other_contents, false);

  TabContents* contents1 = CreateTabContents();
  TabContents* contents2 = CreateTabContents();
  TabContents* contents3 = CreateTabContents();

  // Start by testing LTR
  InsertTabContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(opener_contents, tabstrip.GetTabContentsAt(0));
  EXPECT_EQ(contents1, tabstrip.GetTabContentsAt(1));
  EXPECT_EQ(contents2, tabstrip.GetTabContentsAt(2));
  EXPECT_EQ(contents3, tabstrip.GetTabContentsAt(3));
  EXPECT_EQ(other_contents, tabstrip.GetTabContentsAt(4));

  // The opener API should work...
  EXPECT_EQ(3, tabstrip.GetIndexOfNextTabContentsOpenedBy(opener, 2, false));
  EXPECT_EQ(2, tabstrip.GetIndexOfNextTabContentsOpenedBy(opener, 3, false));
  EXPECT_EQ(3, tabstrip.GetIndexOfLastTabContentsOpenedBy(opener, 1));

  // Now open a foreground tab from a link. It should be opened adjacent to the
  // opener tab.
  TabContents* fg_link_contents = CreateTabContents();
  int insert_index = tabstrip.order_controller()->DetermineInsertionIndex(
      fg_link_contents, PageTransition::LINK, true);
  EXPECT_EQ(1, insert_index);
  tabstrip.InsertTabContentsAt(insert_index, fg_link_contents, true, true);
  EXPECT_EQ(1, tabstrip.selected_index());
  EXPECT_EQ(fg_link_contents, tabstrip.GetSelectedTabContents());

  // Now close this contents. The selection should move to the opener contents.
  tabstrip.CloseSelectedTab();
  EXPECT_EQ(0, tabstrip.selected_index());

  // Now open a new empty tab. It should open at the end of the strip.
  TabContents* fg_nonlink_contents = CreateTabContents();
  insert_index = tabstrip.order_controller()->DetermineInsertionIndex(
      fg_nonlink_contents, PageTransition::AUTO_BOOKMARK, true);
  EXPECT_EQ(tabstrip.count(), insert_index);
  // We break the opener relationship...
  tabstrip.InsertTabContentsAt(insert_index, fg_nonlink_contents, false, false);
  // Now select it, so that user_gesture == true causes the opener relationship
  // to be forgotten...
  tabstrip.SelectTabContentsAt(tabstrip.count() - 1, true);
  EXPECT_EQ(tabstrip.count() - 1, tabstrip.selected_index());
  EXPECT_EQ(fg_nonlink_contents, tabstrip.GetSelectedTabContents());

  // Verify that all opener relationships are forgotten.
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextTabContentsOpenedBy(opener, 2, false));
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextTabContentsOpenedBy(opener, 3, false));
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextTabContentsOpenedBy(opener, 3, false));
  EXPECT_EQ(-1, tabstrip.GetIndexOfLastTabContentsOpenedBy(opener, 1));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests that selection is shifted to the correct tab when a tab is closed.
// If a tab is in the background when it is closed, the selection does not
// change.
// If a tab is in the foreground (selected),
//   If that tab does not have an opener, selection shifts to the right.
//   If the tab has an opener,
//     The next tab (scanning LTR) in the entire strip that has the same opener
//     is selected
//     If there are no other tabs that have the same opener,
//       The opener is selected
//
TEST_F(TabStripModelTest, TestSelectOnClose) {
  TabStripModel tabstrip(NULL, profile_);
  EXPECT_TRUE(tabstrip.empty());

  TabContents* opener_contents = CreateTabContents();
  NavigationController* opener = opener_contents->controller();
  tabstrip.AppendTabContents(opener_contents, true);

  TabContents* contents1 = CreateTabContents();
  TabContents* contents2 = CreateTabContents();
  TabContents* contents3 = CreateTabContents();

  // Note that we use Detach instead of Close throughout this test to avoid
  // having to keep reconstructing these TabContentses.

  // First test that closing tabs that are in the background doesn't adjust the
  // current selection.
  InsertTabContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(0, tabstrip.selected_index());

  tabstrip.DetachTabContentsAt(1);
  EXPECT_EQ(0, tabstrip.selected_index());

  for (int i = tabstrip.count() - 1; i >= 1; --i)
    tabstrip.DetachTabContentsAt(i);

  // Now test that when a tab doesn't have an opener, selection shifts to the
  // right when the tab is closed.
  InsertTabContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(0, tabstrip.selected_index());

  tabstrip.ForgetAllOpeners();
  tabstrip.SelectTabContentsAt(1, true);
  EXPECT_EQ(1, tabstrip.selected_index());
  tabstrip.DetachTabContentsAt(1);
  EXPECT_EQ(1, tabstrip.selected_index());
  tabstrip.DetachTabContentsAt(1);
  EXPECT_EQ(1, tabstrip.selected_index());
  tabstrip.DetachTabContentsAt(1);
  EXPECT_EQ(0, tabstrip.selected_index());

  for (int i = tabstrip.count() - 1; i >= 1; --i)
    tabstrip.DetachTabContentsAt(i);

  // Now test that when a tab does have an opener, it selects the next tab
  // opened by the same opener scanning LTR when it is closed.
  InsertTabContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(0, tabstrip.selected_index());
  tabstrip.SelectTabContentsAt(2, false);
  EXPECT_EQ(2, tabstrip.selected_index());
  tabstrip.CloseTabContentsAt(2);
  EXPECT_EQ(2, tabstrip.selected_index());
  tabstrip.CloseTabContentsAt(2);
  EXPECT_EQ(1, tabstrip.selected_index());
  tabstrip.CloseTabContentsAt(1);
  EXPECT_EQ(0, tabstrip.selected_index());

  // Finally test that when a tab has no "siblings" that the opener is
  // selected.
  TabContents* other_contents = CreateTabContents();
  tabstrip.InsertTabContentsAt(1, other_contents, false, false);
  EXPECT_EQ(2, tabstrip.count());
  TabContents* opened_contents = CreateTabContents();
  tabstrip.InsertTabContentsAt(2, opened_contents, true, true);
  EXPECT_EQ(2, tabstrip.selected_index());
  tabstrip.CloseTabContentsAt(2);
  EXPECT_EQ(0, tabstrip.selected_index());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests the following context menu commands:
//  - Close Tab
//  - Close Other Tabs
//  - Close Tabs To Right
//  - Close Tabs Opened By
TEST_F(TabStripModelTest, TestContextMenuCloseCommands) {
  TabStripModel tabstrip(NULL, profile_);
  EXPECT_TRUE(tabstrip.empty());

  TabContents* opener_contents = CreateTabContents();
  NavigationController* opener = opener_contents->controller();
  tabstrip.AppendTabContents(opener_contents, true);

  TabContents* contents1 = CreateTabContents();
  TabContents* contents2 = CreateTabContents();
  TabContents* contents3 = CreateTabContents();

  InsertTabContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(0, tabstrip.selected_index());

  tabstrip.ExecuteContextMenuCommand(2, TabStripModel::CommandCloseTab);
  EXPECT_EQ(3, tabstrip.count());

  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseTabsToRight);
  EXPECT_EQ(1, tabstrip.count());
  EXPECT_EQ(opener_contents, tabstrip.GetSelectedTabContents());

  TabContents* dummy_contents = CreateTabContents();
  tabstrip.AppendTabContents(dummy_contents, false);

  contents1 = CreateTabContents();
  contents2 = CreateTabContents();
  contents3 = CreateTabContents();
  InsertTabContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(5, tabstrip.count());

  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseTabsOpenedBy);
  EXPECT_EQ(2, tabstrip.count());
  EXPECT_EQ(dummy_contents, tabstrip.GetTabContentsAt(1));

  contents1 = CreateTabContents();
  contents2 = CreateTabContents();
  contents3 = CreateTabContents();
  InsertTabContentses(&tabstrip, contents1, contents2, contents3);
  EXPECT_EQ(5, tabstrip.count());

  int dummy_index = tabstrip.count() - 1;
  tabstrip.SelectTabContentsAt(dummy_index, true);
  EXPECT_EQ(dummy_contents, tabstrip.GetSelectedTabContents());

  tabstrip.ExecuteContextMenuCommand(dummy_index,
                                     TabStripModel::CommandCloseOtherTabs);
  EXPECT_EQ(1, tabstrip.count());
  EXPECT_EQ(dummy_contents, tabstrip.GetSelectedTabContents());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests whether or not TabContentses are inserted in the correct position
// using this "smart" function with a simulated middle click action on a series
// of links on the home page.
TEST_F(TabStripModelTest, AddTabContents_MiddleClickLinksAndClose) {
  TabStripModel tabstrip(NULL, profile_);
  EXPECT_TRUE(tabstrip.empty());

  // Open the Home Page
  TabContents* homepage_contents = CreateTabContents();
  tabstrip.AddTabContents(
      homepage_contents, -1, PageTransition::AUTO_BOOKMARK, true);

  // Open some other tab, by user typing.
  TabContents* typed_page_contents = CreateTabContents();
  tabstrip.AddTabContents(
      typed_page_contents, -1, PageTransition::TYPED, true);

  EXPECT_EQ(2, tabstrip.count());

  // Re-select the home page.
  tabstrip.SelectTabContentsAt(0, true);

  // Open a bunch of tabs by simulating middle clicking on links on the home
  // page.
  TabContents* middle_click_contents1 = CreateTabContents();
  tabstrip.AddTabContents(
    middle_click_contents1, -1, PageTransition::LINK, false);
  TabContents* middle_click_contents2 = CreateTabContents();
  tabstrip.AddTabContents(
    middle_click_contents2, -1, PageTransition::LINK, false);
  TabContents* middle_click_contents3 = CreateTabContents();
  tabstrip.AddTabContents(
    middle_click_contents3, -1, PageTransition::LINK, false);

  EXPECT_EQ(5, tabstrip.count());

  EXPECT_EQ(homepage_contents, tabstrip.GetTabContentsAt(0));
  EXPECT_EQ(middle_click_contents1, tabstrip.GetTabContentsAt(1));
  EXPECT_EQ(middle_click_contents2, tabstrip.GetTabContentsAt(2));
  EXPECT_EQ(middle_click_contents3, tabstrip.GetTabContentsAt(3));
  EXPECT_EQ(typed_page_contents, tabstrip.GetTabContentsAt(4));

  // Now simulate seleting a tab in the middle of the group of tabs opened from
  // the home page and start closing them. Each TabContents in the group should
  // be closed, right to left. This test is constructed to start at the middle
  // TabContents in the group to make sure the cursor wraps around to the first
  // TabContents in the group before closing the opener or any other
  // TabContents.
  tabstrip.SelectTabContentsAt(2, true);
  tabstrip.CloseSelectedTab();
  EXPECT_EQ(middle_click_contents3, tabstrip.GetSelectedTabContents());
  tabstrip.CloseSelectedTab();
  EXPECT_EQ(middle_click_contents1, tabstrip.GetSelectedTabContents());
  tabstrip.CloseSelectedTab();
  EXPECT_EQ(homepage_contents, tabstrip.GetSelectedTabContents());
  tabstrip.CloseSelectedTab();
  EXPECT_EQ(typed_page_contents, tabstrip.GetSelectedTabContents());

  EXPECT_EQ(1, tabstrip.count());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests whether or not a TabContents created by a left click on a link that
// opens a new tab is inserted correctly adjacent to the tab that spawned it.
TEST_F(TabStripModelTest, AddTabContents_LeftClickPopup) {
  TabStripModel tabstrip(NULL, profile_);
  EXPECT_TRUE(tabstrip.empty());

  // Open the Home Page
  TabContents* homepage_contents = CreateTabContents();
  tabstrip.AddTabContents(
    homepage_contents, -1, PageTransition::AUTO_BOOKMARK, true);

  // Open some other tab, by user typing.
  TabContents* typed_page_contents = CreateTabContents();
  tabstrip.AddTabContents(
    typed_page_contents, -1, PageTransition::TYPED, true);

  EXPECT_EQ(2, tabstrip.count());

  // Re-select the home page.
  tabstrip.SelectTabContentsAt(0, true);

  // Open a tab by simulating a left click on a link that opens in a new tab.
  TabContents* left_click_contents = CreateTabContents();
  tabstrip.AddTabContents(left_click_contents, -1, PageTransition::LINK, true);

  // Verify the state meets our expectations.
  EXPECT_EQ(3, tabstrip.count());
  EXPECT_EQ(homepage_contents, tabstrip.GetTabContentsAt(0));
  EXPECT_EQ(left_click_contents, tabstrip.GetTabContentsAt(1));
  EXPECT_EQ(typed_page_contents, tabstrip.GetTabContentsAt(2));

  // The newly created tab should be selected.
  EXPECT_EQ(left_click_contents, tabstrip.GetSelectedTabContents());

  // After closing the selected tab, the selection should move to the left, to
  // the opener.
  tabstrip.CloseSelectedTab();
  EXPECT_EQ(homepage_contents, tabstrip.GetSelectedTabContents());

  EXPECT_EQ(2, tabstrip.count());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests whether or not new tabs that should split context (typed pages,
// generated urls, also blank tabs) open at the end of the tabstrip instead of
// in the middle.
TEST_F(TabStripModelTest, AddTabContents_CreateNewBlankTab) {
  TabStripModel tabstrip(NULL, profile_);
  EXPECT_TRUE(tabstrip.empty());

  // Open the Home Page
  TabContents* homepage_contents = CreateTabContents();
  tabstrip.AddTabContents(
    homepage_contents, -1, PageTransition::AUTO_BOOKMARK, true);

  // Open some other tab, by user typing.
  TabContents* typed_page_contents = CreateTabContents();
  tabstrip.AddTabContents(
    typed_page_contents, -1, PageTransition::TYPED, true);

  EXPECT_EQ(2, tabstrip.count());

  // Re-select the home page.
  tabstrip.SelectTabContentsAt(0, true);

  // Open a new blank tab in the foreground.
  TabContents* new_blank_contents = CreateTabContents();
  tabstrip.AddTabContents(new_blank_contents, -1, PageTransition::TYPED, true);

  // Verify the state of the tabstrip.
  EXPECT_EQ(3, tabstrip.count());
  EXPECT_EQ(homepage_contents, tabstrip.GetTabContentsAt(0));
  EXPECT_EQ(typed_page_contents, tabstrip.GetTabContentsAt(1));
  EXPECT_EQ(new_blank_contents, tabstrip.GetTabContentsAt(2));

  // Now open a couple more blank tabs in the background.
  TabContents* background_blank_contents1 = CreateTabContents();
  tabstrip.AddTabContents(
      background_blank_contents1, -1, PageTransition::TYPED, false);
  TabContents* background_blank_contents2 = CreateTabContents();
  tabstrip.AddTabContents(
      background_blank_contents2, -1, PageTransition::GENERATED, false);
  EXPECT_EQ(5, tabstrip.count());
  EXPECT_EQ(homepage_contents, tabstrip.GetTabContentsAt(0));
  EXPECT_EQ(typed_page_contents, tabstrip.GetTabContentsAt(1));
  EXPECT_EQ(new_blank_contents, tabstrip.GetTabContentsAt(2));
  EXPECT_EQ(background_blank_contents1, tabstrip.GetTabContentsAt(3));
  EXPECT_EQ(background_blank_contents2, tabstrip.GetTabContentsAt(4));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests whether opener state is correctly forgotten when the user switches
// context.
TEST_F(TabStripModelTest, AddTabContents_ForgetOpeners) {
  TabStripModel tabstrip(NULL, profile_);
  EXPECT_TRUE(tabstrip.empty());

  // Open the Home Page
  TabContents* homepage_contents = CreateTabContents();
  tabstrip.AddTabContents(
    homepage_contents, -1, PageTransition::AUTO_BOOKMARK, true);

  // Open some other tab, by user typing.
  TabContents* typed_page_contents = CreateTabContents();
  tabstrip.AddTabContents(
    typed_page_contents, -1, PageTransition::TYPED, true);

  EXPECT_EQ(2, tabstrip.count());

  // Re-select the home page.
  tabstrip.SelectTabContentsAt(0, true);

  // Open a bunch of tabs by simulating middle clicking on links on the home
  // page.
  TabContents* middle_click_contents1 = CreateTabContents();
  tabstrip.AddTabContents(
    middle_click_contents1, -1, PageTransition::LINK, false);
  TabContents* middle_click_contents2 = CreateTabContents();
  tabstrip.AddTabContents(
    middle_click_contents2, -1, PageTransition::LINK, false);
  TabContents* middle_click_contents3 = CreateTabContents();
  tabstrip.AddTabContents(
    middle_click_contents3, -1, PageTransition::LINK, false);

  // Break out of the context by selecting a tab in a different context.
  EXPECT_EQ(typed_page_contents, tabstrip.GetTabContentsAt(4));
  tabstrip.SelectLastTab();
  EXPECT_EQ(typed_page_contents, tabstrip.GetSelectedTabContents());

  // Step back into the context by selecting a tab inside it.
  tabstrip.SelectTabContentsAt(2, true);
  EXPECT_EQ(middle_click_contents2, tabstrip.GetSelectedTabContents());

  // Now test that closing tabs selects to the right until there are no more,
  // then to the left, as if there were no context (context has been
  // successfully forgotten).
  tabstrip.CloseSelectedTab();
  EXPECT_EQ(middle_click_contents3, tabstrip.GetSelectedTabContents());
  tabstrip.CloseSelectedTab();
  EXPECT_EQ(typed_page_contents, tabstrip.GetSelectedTabContents());
  tabstrip.CloseSelectedTab();
  EXPECT_EQ(middle_click_contents1, tabstrip.GetSelectedTabContents());
  tabstrip.CloseSelectedTab();
  EXPECT_EQ(homepage_contents, tabstrip.GetSelectedTabContents());

  EXPECT_EQ(1, tabstrip.count());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

class TabStripDummyDelegate : public TabStripModelDelegate {
 public:
   explicit TabStripDummyDelegate(TabContents* dummy)
      : dummy_contents_(dummy) {}
  virtual ~TabStripDummyDelegate() {}

  // Overridden from TabStripModelDelegate:
  virtual GURL GetBlankTabURL() const { return NewTabUIURL(); }
  virtual void CreateNewStripWithContents(TabContents* contents,
                                          const gfx::Rect& window_bounds,
                                          const DockInfo& dock_info) {}
  virtual int GetDragActions() const { return 0; }
  virtual TabContents* CreateTabContentsForURL(
      const GURL& url,
      const GURL& referrer,
      Profile* profile,
      PageTransition::Type transition,
      bool defer_load,
      SiteInstance* instance) const {
    if (url == NewTabUIURL())
      return dummy_contents_;
    return NULL;
  }
  virtual bool CanDuplicateContentsAt(int index) { return false; }
  virtual void DuplicateContentsAt(int index) {}
  virtual void CloseFrameAfterDragSession() {}

 private:
  // A dummy TabContents we give to callers that expect us to actually build a
  // Destinations tab for them.
  TabContents* dummy_contents_;

  DISALLOW_EVIL_CONSTRUCTORS(TabStripDummyDelegate);
};

// Added for http://b/issue?id=958960
TEST_F(TabStripModelTest, AppendContentsReselectionTest) {
  TabContents* fake_destinations_tab = CreateTabContents();
  TabStripDummyDelegate delegate(fake_destinations_tab);
  TabStripModel tabstrip(&delegate, profile_);
  EXPECT_TRUE(tabstrip.empty());

  // Open the Home Page
  TabContents* homepage_contents = CreateTabContents();
  tabstrip.AddTabContents(
    homepage_contents, -1, PageTransition::AUTO_BOOKMARK, true);

  // Open some other tab, by user typing.
  TabContents* typed_page_contents = CreateTabContents();
  tabstrip.AddTabContents(
    typed_page_contents, -1, PageTransition::TYPED, false);

  // The selected tab should still be the first.
  EXPECT_EQ(0, tabstrip.selected_index());

  // Now simulate a link click that opens a new tab (by virtue of target=_blank)
  // and make sure the right tab gets selected when the new tab is closed.
  TabContents* target_blank_contents = CreateTabContents();
  tabstrip.AppendTabContents(target_blank_contents, true);
  EXPECT_EQ(2, tabstrip.selected_index());
  tabstrip.CloseTabContentsAt(2);
  EXPECT_EQ(0, tabstrip.selected_index());

  // Now open a blank tab...
  // (see also AddTabContents_NewTabAtEndOfStripInheritsGroup for an
  // explanation of this behavior)
  tabstrip.AddBlankTab(true);
  EXPECT_EQ(2, tabstrip.selected_index());
  tabstrip.CloseTabContentsAt(2);
  EXPECT_EQ(0, tabstrip.selected_index());

  // clean up after ourselves
  tabstrip.CloseAllTabs();
}

// Added for http://b/issue?id=1027661
TEST_F(TabStripModelTest, ReselectionConsidersChildrenTest) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel strip(&delegate, profile_);

  // Open page A
  TabContents* page_a_contents = CreateTabContents();
  strip.AddTabContents(
      page_a_contents, -1, PageTransition::AUTO_BOOKMARK, true);

  // Simulate middle click to open page A.A and A.B
  TabContents* page_a_a_contents = CreateTabContents();
  strip.AddTabContents(page_a_a_contents, -1, PageTransition::LINK, false);
  TabContents* page_a_b_contents = CreateTabContents();
  strip.AddTabContents(page_a_b_contents, -1, PageTransition::LINK, false);

  // Select page A.A
  strip.SelectTabContentsAt(1, true);
  EXPECT_EQ(page_a_a_contents, strip.GetSelectedTabContents());

  // Simulate a middle click to open page A.A.A
  TabContents* page_a_a_a_contents = CreateTabContents();
  strip.AddTabContents(page_a_a_a_contents, -1, PageTransition::LINK, false);

  EXPECT_EQ(page_a_a_a_contents, strip.GetTabContentsAt(2));

  // Close page A.A
  strip.CloseTabContentsAt(strip.selected_index());

  // Page A.A.A should be selected, NOT A.B
  EXPECT_EQ(page_a_a_a_contents, strip.GetSelectedTabContents());

  // Close page A.A.A
  strip.CloseTabContentsAt(strip.selected_index());

  // Page A.B should be selected
  EXPECT_EQ(page_a_b_contents, strip.GetSelectedTabContents());

  // Close page A.B
  strip.CloseTabContentsAt(strip.selected_index());

  // Page A should be selected
  EXPECT_EQ(page_a_contents, strip.GetSelectedTabContents());

  // Clean up.
  strip.CloseAllTabs();
}

TEST_F(TabStripModelTest, AddTabContents_NewTabAtEndOfStripInheritsGroup) {
  TabStripDummyDelegate delegate(NULL);
  TabStripModel strip(&delegate, profile_);

  // Open page A
  TabContents* page_a_contents = CreateTabContents();
  strip.AddTabContents(page_a_contents, -1, PageTransition::START_PAGE, true);

  // Open pages B, C and D in the background from links on page A...
  TabContents* page_b_contents = CreateTabContents();
  TabContents* page_c_contents = CreateTabContents();
  TabContents* page_d_contents = CreateTabContents();
  strip.AddTabContents(page_b_contents, -1, PageTransition::LINK, false);
  strip.AddTabContents(page_c_contents, -1, PageTransition::LINK, false);
  strip.AddTabContents(page_d_contents, -1, PageTransition::LINK, false);

  // Switch to page B's tab.
  strip.SelectTabContentsAt(1, true);

  // Open a New Tab at the end of the strip (simulate Ctrl+T)
  TabContents* new_tab_contents = CreateNewTabTabContents();
  strip.AddTabContents(new_tab_contents, -1, PageTransition::TYPED, true);

  EXPECT_EQ(4, strip.GetIndexOfTabContents(new_tab_contents));
  EXPECT_EQ(4, strip.selected_index());

  // Close the New Tab that was just opened. We should be returned to page B's
  // Tab...
  strip.CloseTabContentsAt(4);

  EXPECT_EQ(1, strip.selected_index());

  // Open a non-New Tab tab at the end of the strip, with a TYPED transition.
  // This is like typing a URL in the address bar and pressing Alt+Enter. The
  // behavior should be the same as above.
  TabContents* page_e_contents = CreateTabContents();
  strip.AddTabContents(page_e_contents, -1, PageTransition::TYPED, true);

  EXPECT_EQ(4, strip.GetIndexOfTabContents(page_e_contents));
  EXPECT_EQ(4, strip.selected_index());

  // Close the Tab. Selection should shift back to page B's Tab.
  strip.CloseTabContentsAt(4);

  EXPECT_EQ(1, strip.selected_index());

  // Open a non-New Tab tab at the end of the strip, with some other
  // transition. This is like right clicking on a bookmark and choosing "Open
  // in New Tab". No opener relationship should be preserved between this Tab
  // and the one that was active when the gesture was performed.
  TabContents* page_f_contents = CreateTabContents();
  strip.AddTabContents(page_f_contents, -1, PageTransition::AUTO_BOOKMARK,
                       true);

  EXPECT_EQ(4, strip.GetIndexOfTabContents(page_f_contents));
  EXPECT_EQ(4, strip.selected_index());

  // Close the Tab. The next-adjacent should be selected.
  strip.CloseTabContentsAt(4);

  EXPECT_EQ(3, strip.selected_index());

  // Clean up.
  strip.CloseAllTabs();
}

