// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Disabled right now as this won't work on BuildBots right now as this test
// require the box it runs on to be unlocked (and no screen-savers).
// The test actually simulates mouse and key events, so if the screen is locked,
// the events don't go to the Chrome window.
#include "testing/gtest/include/gtest/gtest.h"

#include "base/gfx/rect.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/background.h"
#include "chrome/views/border.h"
#include "chrome/views/controls/button/checkbox.h"
#include "chrome/views/controls/button/native_button.h"
#include "chrome/views/controls/button/radio_button.h"
#include "chrome/views/controls/label.h"
#include "chrome/views/controls/link.h"
#include "chrome/views/controls/scroll_view.h"
#include "chrome/views/controls/tabbed_pane.h"
#include "chrome/views/controls/text_field.h"
#include "chrome/views/widget/accelerator_handler.h"
#include "chrome/views/widget/root_view.h"
#include "chrome/views/widget/widget_win.h"
#include "chrome/views/window/window.h"
#include "chrome/views/window/window_delegate.h"
#include "SkColor.h"


namespace {

static const int kWindowWidth = 600;
static const int kWindowHeight = 500;

static int count = 1;

static const int kTopCheckBoxID = count++;  // 1
static const int kLeftContainerID = count++;
static const int kAppleLabelID = count++;
static const int kAppleTextFieldID = count++;
static const int kOrangeLabelID = count++;  // 5
static const int kOrangeTextFieldID = count++;
static const int kBananaLabelID = count++;
static const int kBananaTextFieldID = count++;
static const int kKiwiLabelID = count++;
static const int kKiwiTextFieldID = count++; // 10
static const int kFruitButtonID = count++;
static const int kFruitCheckBoxID = count++;

static const int kRightContainerID = count++;
static const int kAsparagusButtonID = count++;
static const int kBroccoliButtonID = count++;  // 15
static const int kCauliflowerButtonID = count++;

static const int kInnerContainerID = count++;
static const int kScrollViewID = count++;
static const int kScrollContentViewID = count++;
static const int kRosettaLinkID = count++;  // 20
static const int kStupeurEtTremblementLinkID = count++;
static const int kDinerGameLinkID = count++;
static const int kRidiculeLinkID = count++;
static const int kClosetLinkID = count++;
static const int kVisitingLinkID = count++;  // 25
static const int kAmelieLinkID = count++;
static const int kJoyeuxNoelLinkID = count++;
static const int kCampingLinkID = count++;
static const int kBriceDeNiceLinkID = count++;
static const int kTaxiLinkID = count++;  // 30
static const int kAsterixLinkID = count++;

static const int kOKButtonID = count++;
static const int kCancelButtonID = count++;
static const int kHelpButtonID = count++;

static const int kStyleContainerID = count++;  // 35
static const int kBoldCheckBoxID = count++;
static const int kItalicCheckBoxID = count++;
static const int kUnderlinedCheckBoxID = count++;

static const int kSearchContainerID = count++;
static const int kSearchTextFieldID = count++;  // 40
static const int kSearchButtonID = count++;
static const int kHelpLinkID = count++;

static const int kThumbnailContainerID = count++;
static const int kThumbnailStarID = count++;
static const int kThumbnailSuperStarID = count++;

class FocusManagerTest;

// BorderView is a NativeControl that creates a tab control as its child and
// takes a View to add as the child of the tab control. The tab control is used
// to give a nice background for the view. At some point we'll have a real
// wrapper for TabControl, and this can be nuked in favor of it.
// Taken from keyword_editor_view.cc.
// It is interesting in our test as it is a native control containing another
// RootView.
class BorderView : public views::NativeControl {
 public:
  explicit BorderView(View* child) : child_(child) {
    DCHECK(child);
    SetFocusable(false);
  }

  virtual ~BorderView() {}

  virtual HWND CreateNativeControl(HWND parent_container) {
    // Create the tab control.
    HWND tab_control = ::CreateWindowEx(GetAdditionalExStyle(),
                                        WC_TABCONTROL,
                                        L"",
                                        WS_CHILD,
                                        0, 0, width(), height(),
                                        parent_container, NULL, NULL, NULL);
    // Create the view container which is a child of the TabControl.
    widget_ = new views::WidgetWin();
    widget_->Init(tab_control, gfx::Rect(), false);
    widget_->SetContentsView(child_);
    widget_->SetFocusTraversableParentView(this);
    ResizeContents(tab_control);
    return tab_control;
  }

  virtual LRESULT OnNotify(int w_param, LPNMHDR l_param) {
    return 0;
  }

  virtual void Layout() {
    NativeControl::Layout();
    ResizeContents(GetNativeControlHWND());
  }

  virtual views::RootView* GetContentsRootView() {
    return widget_->GetRootView();
  }

  virtual views::FocusTraversable* GetFocusTraversable() {
    return widget_;
  }

  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child) {
    NativeControl::ViewHierarchyChanged(is_add, parent, child);

    if (child == this && is_add) {
      // We have been added to a view hierarchy, update the FocusTraversable
      // parent.
      widget_->SetFocusTraversableParent(GetRootView());
    }
  }

private:
  void ResizeContents(HWND tab_control) {
    DCHECK(tab_control);
    CRect content_bounds;
    if (!GetClientRect(tab_control, &content_bounds))
      return;
    TabCtrl_AdjustRect(tab_control, FALSE, &content_bounds);
    widget_->MoveWindow(content_bounds.left, content_bounds.top,
      content_bounds.Width(), content_bounds.Height(),
      TRUE);
  }

  View* child_;
  views::WidgetWin* widget_;

  DISALLOW_EVIL_CONSTRUCTORS(BorderView);
};

class TestViewWindow : public views::WidgetWin {
 public:
  explicit TestViewWindow(FocusManagerTest* test);
  ~TestViewWindow() { }

  void Init();

  views::View* contents() const { return contents_; }


  // Return the ID of the component that currently has the focus.
  int GetFocusedComponentID();

  // Simulate pressing the tab button in the window.
  void PressTab(bool shift_pressed, bool ctrl_pressed);

  views::RootView* GetContentsRootView() const {
    return contents_->GetRootView();
  }

  views::RootView* GetStyleRootView() const {
    return style_tab_->GetContentsRootView();
  }

  views::RootView* GetSearchRootView() const {
    return search_border_view_->GetContentsRootView();
  }

 private:
  views::View* contents_;

  views::TabbedPane* style_tab_;
  BorderView* search_border_view_;

  FocusManagerTest* test_;

  DISALLOW_EVIL_CONSTRUCTORS(TestViewWindow);
};

class FocusManagerTest : public testing::Test {
 public:
  TestViewWindow* GetWindow();
  ~FocusManagerTest();

 protected:
  FocusManagerTest();

  virtual void SetUp();
  virtual void TearDown();

  MessageLoopForUI message_loop_;
  TestViewWindow* test_window_;
};

////////////////////////////////////////////////////////////////////////////////
// TestViewWindow
////////////////////////////////////////////////////////////////////////////////

TestViewWindow::TestViewWindow(FocusManagerTest* test)
    : test_(test),
      contents_(NULL),
      style_tab_(NULL),
      search_border_view_(NULL) {
}

// Initializes and shows the window with the contents view.
void TestViewWindow::Init() {
  gfx::Rect bounds(0, 0, 600, 460);
  contents_ = new views::View();
  contents_->set_background(
      views::Background::CreateSolidBackground(255, 255, 255));

  WidgetWin::Init(NULL, bounds, true);
  SetContentsView(contents_);

  views::Checkbox* cb =
      new views::Checkbox(L"This is a checkbox");
  contents_->AddChildView(cb);
  // In this fast paced world, who really has time for non hard-coded layout?
  cb->SetBounds(10, 10, 200, 20);
  cb->SetID(kTopCheckBoxID);

  views::View* left_container = new views::View();
  left_container->set_border(
      views::Border::CreateSolidBorder(1, SK_ColorBLACK));
  left_container->set_background(
      views::Background::CreateSolidBackground(240, 240, 240));
  left_container->SetID(kLeftContainerID);
  contents_->AddChildView(left_container);
  left_container->SetBounds(10, 35, 250, 200);

  int label_x = 5;
  int label_width = 50;
  int label_height = 15;
  int text_field_width = 150;
  int y = 10;
  int gap_between_labels = 10;

  views::Label* label = new views::Label(L"Apple:");
  label->SetID(kAppleLabelID);
  left_container->AddChildView(label);
  label->SetBounds(label_x, y, label_width, label_height);

  views::TextField* text_field = new views::TextField();
  text_field->SetID(kAppleTextFieldID);
  left_container->AddChildView(text_field);
  text_field->SetBounds(label_x + label_width + 5, y,
                        text_field_width, label_height);

  y += label_height + gap_between_labels;

  label = new views::Label(L"Orange:");
  label->SetID(kOrangeLabelID);
  left_container->AddChildView(label);
  label->SetBounds(label_x, y, label_width, label_height);

  text_field = new views::TextField();
  text_field->SetID(kOrangeTextFieldID);
  left_container->AddChildView(text_field);
  text_field->SetBounds(label_x + label_width + 5, y,
                        text_field_width, label_height);

  y += label_height + gap_between_labels;

  label = new views::Label(L"Banana:");
  label->SetID(kBananaLabelID);
  left_container->AddChildView(label);
  label->SetBounds(label_x, y, label_width, label_height);

  text_field = new views::TextField();
  text_field->SetID(kBananaTextFieldID);
  left_container->AddChildView(text_field);
  text_field->SetBounds(label_x + label_width + 5, y,
                        text_field_width, label_height);

  y += label_height + gap_between_labels;

  label = new views::Label(L"Kiwi:");
  label->SetID(kKiwiLabelID);
  left_container->AddChildView(label);
  label->SetBounds(label_x, y, label_width, label_height);

  text_field = new views::TextField();
  text_field->SetID(kKiwiTextFieldID);
  left_container->AddChildView(text_field);
  text_field->SetBounds(label_x + label_width + 5, y,
                        text_field_width, label_height);

  y += label_height + gap_between_labels;

  views::NativeButton* button = new views::NativeButton(NULL, L"Click me");
  button->SetBounds(label_x, y + 10, 50, 20);
  button->SetID(kFruitButtonID);
  left_container->AddChildView(button);
  y += 40;

  cb =  new views::Checkbox(L"This is another check box");
  cb->SetBounds(label_x + label_width + 5, y, 100, 20);
  cb->SetID(kFruitCheckBoxID);
  left_container->AddChildView(cb);

  views::View* right_container = new views::View();
  right_container->set_border(
      views::Border::CreateSolidBorder(1, SK_ColorBLACK));
  right_container->set_background(
      views::Background::CreateSolidBackground(240, 240, 240));
  right_container->SetID(kRightContainerID);
  contents_->AddChildView(right_container);
  right_container->SetBounds(270, 35, 300, 200);

  y = 10;
  int radio_button_height = 15;
  int gap_between_radio_buttons = 10;
  views::View* radio_button =
      new views::RadioButton(L"Asparagus", 1);
  radio_button->SetID(kAsparagusButtonID);
  right_container->AddChildView(radio_button);
  radio_button->SetBounds(5, y, 70, radio_button_height);
  radio_button->SetGroup(1);
  y += radio_button_height + gap_between_radio_buttons;
  radio_button = new views::RadioButton(L"Broccoli", 1);
  radio_button->SetID(kBroccoliButtonID);
  right_container->AddChildView(radio_button);
  radio_button->SetBounds(5, y, 70, radio_button_height);
  radio_button->SetGroup(1);
  y += radio_button_height + gap_between_radio_buttons;
  radio_button = new views::RadioButton(L"Cauliflower", 1);
  radio_button->SetID(kCauliflowerButtonID);
  right_container->AddChildView(radio_button);
  radio_button->SetBounds(5, y, 70, radio_button_height);
  radio_button->SetGroup(1);
  y += radio_button_height + gap_between_radio_buttons;

  views::View* inner_container = new views::View();
  inner_container->set_border(
      views::Border::CreateSolidBorder(1, SK_ColorBLACK));
  inner_container->set_background(
      views::Background::CreateSolidBackground(230, 230, 230));
  inner_container->SetID(kInnerContainerID);
  right_container->AddChildView(inner_container);
  inner_container->SetBounds(100, 10, 150, 180);

  views::ScrollView* scroll_view = new views::ScrollView();
  scroll_view->SetID(kScrollViewID);
  inner_container->AddChildView(scroll_view);
  scroll_view->SetBounds(1, 1, 148, 178);

  views::View* scroll_content = new views::View();
  scroll_content->SetBounds(0, 0, 200, 200);
  scroll_content->set_background(
      views::Background::CreateSolidBackground(200, 200, 200));
  scroll_view->SetContents(scroll_content);

  static const wchar_t* const kTitles[] = {
      L"Rosetta", L"Stupeur et tremblement", L"The diner game",
      L"Ridicule", L"Le placard", L"Les Visiteurs", L"Amelie",
      L"Joyeux Noel", L"Camping", L"Brice de Nice",
      L"Taxi", L"Asterix"
  };

  static const int kIDs[] = {
      kRosettaLinkID, kStupeurEtTremblementLinkID, kDinerGameLinkID,
      kRidiculeLinkID, kClosetLinkID, kVisitingLinkID, kAmelieLinkID,
      kJoyeuxNoelLinkID, kCampingLinkID, kBriceDeNiceLinkID,
      kTaxiLinkID, kAsterixLinkID
  };

  DCHECK(arraysize(kTitles) == arraysize(kIDs));

  y = 5;
  for (int i = 0; i < arraysize(kTitles); ++i) {
    views::Link* link = new views::Link(kTitles[i]);
    link->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    link->SetID(kIDs[i]);
    scroll_content->AddChildView(link);
    link->SetBounds(5, y, 300, 15);
    y += 15;
  }

  y = 250;
  int width = 50;
  button = new views::NativeButton(NULL, L"OK");
  button->SetID(kOKButtonID);

  contents_->AddChildView(button);
  button->SetBounds(150, y, width, 20);

  button = new views::NativeButton(NULL, L"Cancel");
  button->SetID(kCancelButtonID);
  contents_->AddChildView(button);
  button->SetBounds(250, y, width, 20);

  button = new views::NativeButton(NULL, L"Help");
  button->SetID(kHelpButtonID);
  contents_->AddChildView(button);
  button->SetBounds(350, y, width, 20);

  y += 40;

  // Left bottom box with style checkboxes.
  views::View* contents = new views::View();
  contents->set_background(
      views::Background::CreateSolidBackground(SK_ColorWHITE));
  cb = new views::Checkbox(L"Bold");
  contents->AddChildView(cb);
  cb->SetBounds(10, 10, 50, 20);
  cb->SetID(kBoldCheckBoxID);

  cb = new views::Checkbox(L"Italic");
  contents->AddChildView(cb);
  cb->SetBounds(70, 10, 50, 20);
  cb->SetID(kItalicCheckBoxID);

  cb = new views::Checkbox(L"Underlined");
  contents->AddChildView(cb);
  cb->SetBounds(130, 10, 70, 20);
  cb->SetID(kUnderlinedCheckBoxID);

  style_tab_ = new views::TabbedPane();
  style_tab_->SetID(kStyleContainerID);
  contents_->AddChildView(style_tab_);
  style_tab_->SetBounds(10, y, 210, 50);
  style_tab_->AddTab(L"Style", contents);
  style_tab_->AddTab(L"Other", new views::View());

  // Right bottom box with search.
  contents = new views::View();
  contents->set_background(
      views::Background::CreateSolidBackground(SK_ColorWHITE));
  text_field = new views::TextField();
  contents->AddChildView(text_field);
  text_field->SetBounds(10, 10, 100, 20);
  text_field->SetID(kSearchTextFieldID);

  button = new views::NativeButton(NULL, L"Search");
  contents->AddChildView(button);
  button->SetBounds(115, 10, 50, 20);
  button->SetID(kSearchButtonID);

  views::Link* link = new views::Link(L"Help");
  link->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  link->SetID(kHelpLinkID);
  contents->AddChildView(link);
  link->SetBounds(170, 10, 30, 15);

  search_border_view_ = new BorderView(contents);
  search_border_view_->SetID(kSearchContainerID);

  contents_->AddChildView(search_border_view_);
  search_border_view_->SetBounds(300, y, 200, 50);

  y += 60;

  contents = new views::View();
  contents->SetFocusable(true);
  contents->set_background(
      views::Background::CreateSolidBackground(SK_ColorBLUE));
  contents->SetID(kThumbnailContainerID);
  button = new views::NativeButton(NULL, L"Star");
  contents->AddChildView(button);
  button->SetBounds(5, 5, 50, 20);
  button->SetID(kThumbnailStarID);
  button = new views::NativeButton(NULL, L"SuperStar");
  contents->AddChildView(button);
  button->SetBounds(60, 5, 100, 20);
  button->SetID(kThumbnailSuperStarID);

  contents_->AddChildView(contents);
  contents->SetBounds(200, y, 200, 50);
}

////////////////////////////////////////////////////////////////////////////////
// FocusManagerTest
////////////////////////////////////////////////////////////////////////////////

FocusManagerTest::FocusManagerTest() {
}

FocusManagerTest::~FocusManagerTest() {
}

TestViewWindow* FocusManagerTest::GetWindow() {
  return test_window_;
}

void FocusManagerTest::SetUp() {
  OleInitialize(NULL);
  test_window_ = new TestViewWindow(this);
  test_window_->Init();
  ShowWindow(test_window_->GetNativeView(), SW_SHOW);
}

void FocusManagerTest::TearDown() {
  test_window_->CloseNow();

  // Flush the message loop to make Purify happy.
  message_loop_.RunAllPending();
  OleUninitialize();
}

////////////////////////////////////////////////////////////////////////////////
// The tests
////////////////////////////////////////////////////////////////////////////////


TEST_F(FocusManagerTest, NormalTraversal) {
  const int kTraversalIDs[] = { kTopCheckBoxID,  kAppleTextFieldID,
      kOrangeTextFieldID, kBananaTextFieldID, kKiwiTextFieldID,
      kFruitButtonID, kFruitCheckBoxID, kAsparagusButtonID, kRosettaLinkID,
      kStupeurEtTremblementLinkID,
      kDinerGameLinkID, kRidiculeLinkID, kClosetLinkID, kVisitingLinkID,
      kAmelieLinkID, kJoyeuxNoelLinkID, kCampingLinkID, kBriceDeNiceLinkID,
      kTaxiLinkID, kAsterixLinkID, kOKButtonID, kCancelButtonID, kHelpButtonID,
      kStyleContainerID, kBoldCheckBoxID, kItalicCheckBoxID,
      kUnderlinedCheckBoxID, kSearchTextFieldID, kSearchButtonID, kHelpLinkID,
      kThumbnailContainerID, kThumbnailStarID, kThumbnailSuperStarID };

  // Uncomment the following line if you want to test manually the UI of this
  // test.
  // MessageLoop::current()->Run(new views::AcceleratorHandler());

  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManager(test_window_->GetNativeView());
  // Let's traverse the whole focus hierarchy (several times, to make sure it
  // loops OK).
  focus_manager->SetFocusedView(NULL);
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < arraysize(kTraversalIDs); j++) {
      focus_manager->AdvanceFocus(false);
      views::View* focused_view = focus_manager->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->GetID());
    }
  }

  // Focus the 1st item.
  views::RootView* root_view = test_window_->GetContentsRootView();
  focus_manager->SetFocusedView(root_view->GetViewByID(kTraversalIDs[0]));

  /* BROKEN because of bug #1153276.  The reverse order of traversal in Tabbed
     Panes is broken (we go to the tab before going to the content
  // Let's traverse in reverse order.
  for (int i = 0; i < 3; ++i) {
    for (int j = arraysize(kTraversalIDs) - 1; j >= 0; --j) {
      focus_manager->AdvanceFocus(true);
      views::View* focused_view = focus_manager->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->GetID());
    }
  }
  */
}

TEST_F(FocusManagerTest, TraversalWithNonEnabledViews) {
  const int kMainContentsDisabledIDs[] = {
      kBananaTextFieldID, kFruitCheckBoxID, kAsparagusButtonID,
      kCauliflowerButtonID, kClosetLinkID, kVisitingLinkID, kBriceDeNiceLinkID,
      kTaxiLinkID, kAsterixLinkID, kHelpButtonID };

  const int kStyleContentsDisabledIDs[] = { kBoldCheckBoxID };

  const int kSearchContentsDisabledIDs[] = { kSearchTextFieldID, kHelpLinkID };

  const int kTraversalIDs[] = { kTopCheckBoxID,  kAppleTextFieldID,
      kOrangeTextFieldID, kKiwiTextFieldID, kFruitButtonID, kBroccoliButtonID,
      kRosettaLinkID, kStupeurEtTremblementLinkID, kDinerGameLinkID,
      kRidiculeLinkID, kAmelieLinkID, kJoyeuxNoelLinkID, kCampingLinkID,
      kOKButtonID, kCancelButtonID, kStyleContainerID,
      kItalicCheckBoxID, kUnderlinedCheckBoxID, kSearchButtonID,
      kThumbnailContainerID, kThumbnailStarID, kThumbnailSuperStarID };

  // Let's disable some views.
  views::RootView* root_view = test_window_->GetContentsRootView();
  for (int i = 0; i < arraysize(kMainContentsDisabledIDs); i++) {
    views::View* v = root_view->GetViewByID(kMainContentsDisabledIDs[i]);
    ASSERT_TRUE(v != NULL);
    if (v)
      v->SetEnabled(false);
  }
  root_view = test_window_->GetStyleRootView();
  for (int i = 0; i < arraysize(kStyleContentsDisabledIDs); i++) {
    views::View* v = root_view->GetViewByID(kStyleContentsDisabledIDs[i]);
    ASSERT_TRUE(v != NULL);
    if (v)
      v->SetEnabled(false);
  }
  root_view = test_window_->GetSearchRootView();
  for (int i = 0; i < arraysize(kSearchContentsDisabledIDs); i++) {
    views::View* v =
        root_view->GetViewByID(kSearchContentsDisabledIDs[i]);
    ASSERT_TRUE(v != NULL);
    if (v)
      v->SetEnabled(false);
  }


  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManager(test_window_->GetNativeView());
  views::View* focused_view;
  // Let's do one traversal (several times, to make sure it loops ok).
  for (int i = 0; i < 3;++i) {
    for (int j = 0; j < arraysize(kTraversalIDs); j++) {
      focus_manager->AdvanceFocus(false);
      focused_view = focus_manager->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->GetID());
    }
  }

  // Focus the 1st item.
  focus_manager->AdvanceFocus(false);
  focused_view = focus_manager->GetFocusedView();
  EXPECT_TRUE(focused_view != NULL);
  if (focused_view)
    EXPECT_EQ(kTraversalIDs[0], focused_view->GetID());

  // Same thing in reverse.
  /* BROKEN because of bug #1153276.  The reverse order of traversal in Tabbed
  Panes is broken (we go to the tab before going to the content

  for (int i = 0; i < 3; ++i) {
    for (int j = arraysize(kTraversalIDs) - 1; j >= 0; --j) {
      focus_manager->AdvanceFocus(true);
      focused_view = focus_manager->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->GetID());
    }
  }
  */
}

}
