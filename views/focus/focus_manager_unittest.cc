// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Disabled right now as this won't work on BuildBots right now as this test
// require the box it runs on to be unlocked (and no screen-savers).
// The test actually simulates mouse and key events, so if the screen is locked,
// the events don't go to the Chrome window.
#include "testing/gtest/include/gtest/gtest.h"

#include "app/resource_bundle.h"
#include "base/gfx/rect.h"
#include "base/string_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "views/background.h"
#include "views/border.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/button/native_button.h"
#include "views/controls/button/radio_button.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/combobox/native_combobox_wrapper.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/controls/scroll_view.h"
#include "views/controls/tabbed_pane.h"
#include "views/controls/textfield/textfield.h"
#include "views/widget/accelerator_handler.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_win.h"
#include "views/window/window_delegate.h"
#include "views/window/window_win.h"

namespace views {

static const int kWindowWidth = 600;
static const int kWindowHeight = 500;

static int count = 1;

static const int kTopCheckBoxID = count++;  // 1
static const int kLeftContainerID = count++;
static const int kAppleLabelID = count++;
static const int kAppleTextfieldID = count++;
static const int kOrangeLabelID = count++;  // 5
static const int kOrangeTextfieldID = count++;
static const int kBananaLabelID = count++;
static const int kBananaTextfieldID = count++;
static const int kKiwiLabelID = count++;
static const int kKiwiTextfieldID = count++; // 10
static const int kFruitButtonID = count++;
static const int kFruitCheckBoxID = count++;
static const int kComboboxID = count++;

static const int kRightContainerID = count++;
static const int kAsparagusButtonID = count++;  // 15
static const int kBroccoliButtonID = count++;
static const int kCauliflowerButtonID = count++;

static const int kInnerContainerID = count++;
static const int kScrollViewID = count++;
static const int kScrollContentViewID = count++;  // 20
static const int kRosettaLinkID = count++;
static const int kStupeurEtTremblementLinkID = count++;
static const int kDinerGameLinkID = count++;
static const int kRidiculeLinkID = count++;
static const int kClosetLinkID = count++;  // 25
static const int kVisitingLinkID = count++;
static const int kAmelieLinkID = count++;
static const int kJoyeuxNoelLinkID = count++;
static const int kCampingLinkID = count++;
static const int kBriceDeNiceLinkID = count++;  // 30
static const int kTaxiLinkID = count++;
static const int kAsterixLinkID = count++;

static const int kOKButtonID = count++;
static const int kCancelButtonID = count++;
static const int kHelpButtonID = count++;  // 35

static const int kStyleContainerID = count++;
static const int kBoldCheckBoxID = count++;
static const int kItalicCheckBoxID = count++;
static const int kUnderlinedCheckBoxID = count++;
static const int kStyleHelpLinkID = count++;  // 40
static const int kStyleTextEditID = count++;

static const int kSearchContainerID = count++;
static const int kSearchTextfieldID = count++;
static const int kSearchButtonID = count++;
static const int kHelpLinkID = count++;  // 45

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
class BorderView : public NativeControl {
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
    widget_ = new WidgetWin();
    widget_->Init(tab_control, gfx::Rect());
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

  virtual RootView* GetContentsRootView() {
    return widget_->GetRootView();
  }

  virtual FocusTraversable* GetFocusTraversable() {
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
  WidgetWin* widget_;

  DISALLOW_COPY_AND_ASSIGN(BorderView);
};

class DummyComboboxModel : public Combobox::Model {
 public:
  virtual int GetItemCount(Combobox* source) { return 10; }

  virtual std::wstring GetItemAt(Combobox* source, int index) {
    return L"Item " + IntToWString(index);
  }
};

class FocusManagerTest : public testing::Test, public WindowDelegate {
 public:
  FocusManagerTest()
      : window_(NULL),
        focus_change_listener_(NULL),
        content_view_(NULL) {
    OleInitialize(NULL);
  }

  ~FocusManagerTest() {
    OleUninitialize();
  }

  virtual void SetUp() {
    window_ = static_cast<WindowWin*>(
        Window::CreateChromeWindow(NULL, bounds(), this));
    InitContentView();
    window_->Show();
  }

  virtual void TearDown() {
    if (focus_change_listener_)
      GetFocusManager()->RemoveFocusChangeListener(focus_change_listener_);
    window_->CloseNow();

    // Flush the message loop to make Purify happy.
    message_loop()->RunAllPending();
  }

  FocusManager* GetFocusManager() {
    return FocusManager::GetFocusManagerForNativeView(
        window_->GetNativeWindow());
  }

  // WindowDelegate Implementation.
  virtual View* GetContentsView() {
    if (!content_view_)
      content_view_ = new View();
    return content_view_;
  }

  virtual void InitContentView() {
  }

 protected:
  virtual gfx::Rect bounds() {
    return gfx::Rect(0, 0, 500, 500);
  }

  // Mocks activating/deactivating the window.
  void SimulateActivateWindow() {
    ::SendMessage(window_->GetNativeWindow(), WM_ACTIVATE, WA_ACTIVE, NULL);
  }
  void SimulateDeactivateWindow() {
    ::SendMessage(window_->GetNativeWindow(), WM_ACTIVATE, WA_INACTIVE, NULL);
  }

  MessageLoopForUI* message_loop() { return &message_loop_; }

  WindowWin* window_;
  View* content_view_;

  void AddFocusChangeListener(FocusChangeListener* listener) {
    ASSERT_FALSE(focus_change_listener_);
    focus_change_listener_ = listener;
    GetFocusManager()->AddFocusChangeListener(listener);
  }

 private:
  FocusChangeListener* focus_change_listener_;
  MessageLoopForUI message_loop_;

  DISALLOW_COPY_AND_ASSIGN(FocusManagerTest);
};

class FocusTraversalTest : public FocusManagerTest {
 public:
  ~FocusTraversalTest();

  virtual void InitContentView();

 protected:
  FocusTraversalTest();

  virtual gfx::Rect bounds() {
    return gfx::Rect(0, 0, 600, 460);
  }

  View* FindViewByID(int id) {
    View* view = GetContentsView()->GetViewByID(id);
    if (view)
      return view;
    view = style_tab_->GetContentsRootView()->GetViewByID(id);
    if (view)
      return view;
    view = search_border_view_->GetContentsRootView()->GetViewByID(id);
    if (view)
      return view;
    return NULL;
  }

 private:
  TabbedPane* style_tab_;
  BorderView* search_border_view_;
  DummyComboboxModel combobox_model_;

  DISALLOW_COPY_AND_ASSIGN(FocusTraversalTest);
};

////////////////////////////////////////////////////////////////////////////////
// FocusTraversalTest
////////////////////////////////////////////////////////////////////////////////

FocusTraversalTest::FocusTraversalTest()
  : style_tab_(NULL),
    search_border_view_(NULL) {
}

FocusTraversalTest::~FocusTraversalTest() {
}

void FocusTraversalTest::InitContentView() {
  content_view_->set_background(
      Background::CreateSolidBackground(255, 255, 255));

  Checkbox* cb = new Checkbox(L"This is a checkbox");
  content_view_->AddChildView(cb);
  // In this fast paced world, who really has time for non hard-coded layout?
  cb->SetBounds(10, 10, 200, 20);
  cb->SetID(kTopCheckBoxID);

  View* left_container = new View();
  left_container->set_border(Border::CreateSolidBorder(1, SK_ColorBLACK));
  left_container->set_background(
      Background::CreateSolidBackground(240, 240, 240));
  left_container->SetID(kLeftContainerID);
  content_view_->AddChildView(left_container);
  left_container->SetBounds(10, 35, 250, 200);

  int label_x = 5;
  int label_width = 50;
  int label_height = 15;
  int text_field_width = 150;
  int y = 10;
  int gap_between_labels = 10;

  Label* label = new Label(L"Apple:");
  label->SetID(kAppleLabelID);
  left_container->AddChildView(label);
  label->SetBounds(label_x, y, label_width, label_height);

  Textfield* text_field = new Textfield();
  text_field->SetID(kAppleTextfieldID);
  left_container->AddChildView(text_field);
  text_field->SetBounds(label_x + label_width + 5, y,
                        text_field_width, label_height);

  y += label_height + gap_between_labels;

  label = new Label(L"Orange:");
  label->SetID(kOrangeLabelID);
  left_container->AddChildView(label);
  label->SetBounds(label_x, y, label_width, label_height);

  text_field = new Textfield();
  text_field->SetID(kOrangeTextfieldID);
  left_container->AddChildView(text_field);
  text_field->SetBounds(label_x + label_width + 5, y,
                        text_field_width, label_height);

  y += label_height + gap_between_labels;

  label = new Label(L"Banana:");
  label->SetID(kBananaLabelID);
  left_container->AddChildView(label);
  label->SetBounds(label_x, y, label_width, label_height);

  text_field = new Textfield();
  text_field->SetID(kBananaTextfieldID);
  left_container->AddChildView(text_field);
  text_field->SetBounds(label_x + label_width + 5, y,
                        text_field_width, label_height);

  y += label_height + gap_between_labels;

  label = new Label(L"Kiwi:");
  label->SetID(kKiwiLabelID);
  left_container->AddChildView(label);
  label->SetBounds(label_x, y, label_width, label_height);

  text_field = new Textfield();
  text_field->SetID(kKiwiTextfieldID);
  left_container->AddChildView(text_field);
  text_field->SetBounds(label_x + label_width + 5, y,
                        text_field_width, label_height);

  y += label_height + gap_between_labels;

  NativeButton* button = new NativeButton(NULL, L"Click me");
  button->SetBounds(label_x, y + 10, 50, 20);
  button->SetID(kFruitButtonID);
  left_container->AddChildView(button);
  y += 40;

  cb =  new Checkbox(L"This is another check box");
  cb->SetBounds(label_x + label_width + 5, y, 150, 20);
  cb->SetID(kFruitCheckBoxID);
  left_container->AddChildView(cb);
  y += 20;

  Combobox* combobox =  new Combobox(&combobox_model_);
  combobox->SetBounds(label_x + label_width + 5, y, 150, 20);
  combobox->SetID(kComboboxID);
  left_container->AddChildView(combobox);

  View* right_container = new View();
  right_container->set_border(Border::CreateSolidBorder(1, SK_ColorBLACK));
  right_container->set_background(
      Background::CreateSolidBackground(240, 240, 240));
  right_container->SetID(kRightContainerID);
  content_view_->AddChildView(right_container);
  right_container->SetBounds(270, 35, 300, 200);

  y = 10;
  int radio_button_height = 15;
  int gap_between_radio_buttons = 10;
  View* radio_button = new RadioButton(L"Asparagus", 1);
  radio_button->SetID(kAsparagusButtonID);
  right_container->AddChildView(radio_button);
  radio_button->SetBounds(5, y, 70, radio_button_height);
  radio_button->SetGroup(1);
  y += radio_button_height + gap_between_radio_buttons;
  radio_button = new RadioButton(L"Broccoli", 1);
  radio_button->SetID(kBroccoliButtonID);
  right_container->AddChildView(radio_button);
  radio_button->SetBounds(5, y, 70, radio_button_height);
  radio_button->SetGroup(1);
  y += radio_button_height + gap_between_radio_buttons;
  radio_button = new RadioButton(L"Cauliflower", 1);
  radio_button->SetID(kCauliflowerButtonID);
  right_container->AddChildView(radio_button);
  radio_button->SetBounds(5, y, 70, radio_button_height);
  radio_button->SetGroup(1);
  y += radio_button_height + gap_between_radio_buttons;

  View* inner_container = new View();
  inner_container->set_border(Border::CreateSolidBorder(1, SK_ColorBLACK));
  inner_container->set_background(
      Background::CreateSolidBackground(230, 230, 230));
  inner_container->SetID(kInnerContainerID);
  right_container->AddChildView(inner_container);
  inner_container->SetBounds(100, 10, 150, 180);

  ScrollView* scroll_view = new ScrollView();
  scroll_view->SetID(kScrollViewID);
  inner_container->AddChildView(scroll_view);
  scroll_view->SetBounds(1, 1, 148, 178);

  View* scroll_content = new View();
  scroll_content->SetBounds(0, 0, 200, 200);
  scroll_content->set_background(
      Background::CreateSolidBackground(200, 200, 200));
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
    Link* link = new Link(kTitles[i]);
    link->SetHorizontalAlignment(Label::ALIGN_LEFT);
    link->SetID(kIDs[i]);
    scroll_content->AddChildView(link);
    link->SetBounds(5, y, 300, 15);
    y += 15;
  }

  y = 250;
  int width = 50;
  button = new NativeButton(NULL, L"OK");
  button->SetID(kOKButtonID);

  content_view_->AddChildView(button);
  button->SetBounds(150, y, width, 20);

  button = new NativeButton(NULL, L"Cancel");
  button->SetID(kCancelButtonID);
  content_view_->AddChildView(button);
  button->SetBounds(250, y, width, 20);

  button = new NativeButton(NULL, L"Help");
  button->SetID(kHelpButtonID);
  content_view_->AddChildView(button);
  button->SetBounds(350, y, width, 20);

  y += 40;

  // Left bottom box with style checkboxes.
  View* contents = new View();
  contents->set_background(Background::CreateSolidBackground(SK_ColorWHITE));
  cb = new Checkbox(L"Bold");
  contents->AddChildView(cb);
  cb->SetBounds(10, 10, 50, 20);
  cb->SetID(kBoldCheckBoxID);

  cb = new Checkbox(L"Italic");
  contents->AddChildView(cb);
  cb->SetBounds(70, 10, 50, 20);
  cb->SetID(kItalicCheckBoxID);

  cb = new Checkbox(L"Underlined");
  contents->AddChildView(cb);
  cb->SetBounds(130, 10, 70, 20);
  cb->SetID(kUnderlinedCheckBoxID);

  Link* link = new Link(L"Help");
  contents->AddChildView(link);
  link->SetBounds(10, 35, 70, 10);
  link->SetID(kStyleHelpLinkID);

  text_field = new Textfield();
  contents->AddChildView(text_field);
  text_field->SetBounds(10, 50, 100, 20);
  text_field->SetID(kStyleTextEditID);

  style_tab_ = new TabbedPane();
  style_tab_->SetID(kStyleContainerID);
  content_view_->AddChildView(style_tab_);
  style_tab_->SetBounds(10, y, 210, 100);
  style_tab_->AddTab(L"Style", contents);
  style_tab_->AddTab(L"Other", new View());

  // Right bottom box with search.
  contents = new View();
  contents->set_background(Background::CreateSolidBackground(SK_ColorWHITE));
  text_field = new Textfield();
  contents->AddChildView(text_field);
  text_field->SetBounds(10, 10, 100, 20);
  text_field->SetID(kSearchTextfieldID);

  button = new NativeButton(NULL, L"Search");
  contents->AddChildView(button);
  button->SetBounds(115, 10, 50, 20);
  button->SetID(kSearchButtonID);

  link = new Link(L"Help");
  link->SetHorizontalAlignment(Label::ALIGN_LEFT);
  link->SetID(kHelpLinkID);
  contents->AddChildView(link);
  link->SetBounds(170, 20, 30, 15);

  search_border_view_ = new BorderView(contents);
  search_border_view_->SetID(kSearchContainerID);

  content_view_->AddChildView(search_border_view_);
  search_border_view_->SetBounds(300, y, 200, 50);

  y += 60;

  contents = new View();
  contents->SetFocusable(true);
  contents->set_background(Background::CreateSolidBackground(SK_ColorBLUE));
  contents->SetID(kThumbnailContainerID);
  button = new NativeButton(NULL, L"Star");
  contents->AddChildView(button);
  button->SetBounds(5, 5, 50, 20);
  button->SetID(kThumbnailStarID);
  button = new NativeButton(NULL, L"SuperStar");
  contents->AddChildView(button);
  button->SetBounds(60, 5, 100, 20);
  button->SetID(kThumbnailSuperStarID);

  content_view_->AddChildView(contents);
  contents->SetBounds(250, y, 200, 50);
}

////////////////////////////////////////////////////////////////////////////////
// The tests
////////////////////////////////////////////////////////////////////////////////

enum FocusTestEventType {
  WILL_GAIN_FOCUS = 0,
  DID_GAIN_FOCUS,
  WILL_LOSE_FOCUS
};

struct FocusTestEvent {
  FocusTestEvent(FocusTestEventType type, int view_id)
      : type(type),
        view_id(view_id) {
  }

  FocusTestEventType type;
  int view_id;
};

class SimpleTestView : public View {
 public:
  SimpleTestView(std::vector<FocusTestEvent>* event_list, int view_id)
      : event_list_(event_list) {
    SetFocusable(true);
    SetID(view_id);
  }

  virtual void WillGainFocus() {
    event_list_->push_back(FocusTestEvent(WILL_GAIN_FOCUS, GetID()));
  }

  virtual void DidGainFocus() {
    event_list_->push_back(FocusTestEvent(DID_GAIN_FOCUS, GetID()));
  }

  virtual void WillLoseFocus() {
    event_list_->push_back(FocusTestEvent(WILL_LOSE_FOCUS, GetID()));
  }

 private:
  std::vector<FocusTestEvent>* event_list_;
};

// Tests that the appropriate Focus related methods are called when a View
// gets/loses focus.
TEST_F(FocusManagerTest, ViewFocusCallbacks) {
  std::vector<FocusTestEvent> event_list;
  const int kView1ID = 1;
  const int kView2ID = 2;

  SimpleTestView* view1 = new SimpleTestView(&event_list, kView1ID);
  SimpleTestView* view2 = new SimpleTestView(&event_list, kView2ID);
  content_view_->AddChildView(view1);
  content_view_->AddChildView(view2);

  view1->RequestFocus();
  ASSERT_EQ(2, event_list.size());
  EXPECT_EQ(WILL_GAIN_FOCUS, event_list[0].type);
  EXPECT_EQ(kView1ID, event_list[0].view_id);
  EXPECT_EQ(DID_GAIN_FOCUS, event_list[1].type);
  EXPECT_EQ(kView1ID, event_list[0].view_id);

  event_list.clear();
  view2->RequestFocus();
  ASSERT_EQ(3, event_list.size());
  EXPECT_EQ(WILL_LOSE_FOCUS, event_list[0].type);
  EXPECT_EQ(kView1ID, event_list[0].view_id);
  EXPECT_EQ(WILL_GAIN_FOCUS, event_list[1].type);
  EXPECT_EQ(kView2ID, event_list[1].view_id);
  EXPECT_EQ(DID_GAIN_FOCUS, event_list[2].type);
  EXPECT_EQ(kView2ID, event_list[2].view_id);

  event_list.clear();
  GetFocusManager()->ClearFocus();
  ASSERT_EQ(1, event_list.size());
  EXPECT_EQ(WILL_LOSE_FOCUS, event_list[0].type);
  EXPECT_EQ(kView2ID, event_list[0].view_id);
}

typedef std::pair<View*, View*> ViewPair;
class TestFocusChangeListener : public FocusChangeListener {
 public:
  virtual void FocusWillChange(View* focused_before, View* focused_now) {
    focus_changes_.push_back(ViewPair(focused_before, focused_now));
  }

  const std::vector<ViewPair>& focus_changes() const { return focus_changes_; }
  void ClearFocusChanges() { focus_changes_.clear(); }

 private:
  // A vector of which views lost/gained focus.
  std::vector<ViewPair> focus_changes_;
};

TEST_F(FocusManagerTest, FocusChangeListener) {
  View* view1 = new View();
  view1->SetFocusable(true);
  View* view2 = new View();
  view2->SetFocusable(true);
  content_view_->AddChildView(view1);
  content_view_->AddChildView(view2);

  TestFocusChangeListener listener;
  AddFocusChangeListener(&listener);

  view1->RequestFocus();
  ASSERT_EQ(1, listener.focus_changes().size());
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(NULL, view1));
  listener.ClearFocusChanges();

  view2->RequestFocus();
  ASSERT_EQ(1, listener.focus_changes().size());
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(view1, view2));
  listener.ClearFocusChanges();

  GetFocusManager()->ClearFocus();
  ASSERT_EQ(1, listener.focus_changes().size());
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(view2, NULL));
}

class TestNativeButton : public NativeButton {
 public:
  explicit TestNativeButton(const std::wstring& text)
      : NativeButton(NULL, text) {
  };
  virtual HWND TestGetNativeControlHWND() {
    return native_wrapper_->GetTestingHandle();
  }
};

class TestCheckbox : public Checkbox {
 public:
  explicit TestCheckbox(const std::wstring& text) : Checkbox(text) {
  };
  virtual HWND TestGetNativeControlHWND() {
    return native_wrapper_->GetTestingHandle();
  }
};

class TestRadioButton : public RadioButton {
 public:
  explicit TestRadioButton(const std::wstring& text) : RadioButton(text, 1) {
  };
  virtual HWND TestGetNativeControlHWND() {
    return native_wrapper_->GetTestingHandle();
  }
};

class TestTextfield : public Textfield {
 public:
  TestTextfield() { }
  virtual HWND TestGetNativeComponent() {
    return native_wrapper_->GetTestingHandle();
  }
};

class TestCombobox : public Combobox, public Combobox::Model {
 public:
  TestCombobox() : Combobox(this) { }
  virtual HWND TestGetNativeComponent() {
    return native_wrapper_->GetTestingHandle();
  }
  virtual int GetItemCount(Combobox* source) {
    return 10;
  }
  virtual std::wstring GetItemAt(Combobox* source, int index) {
    return L"Hello combo";
  }
};

class TestTabbedPane : public TabbedPane {
 public:
  TestTabbedPane() { }
  virtual HWND TestGetNativeControlHWND() {
    return GetNativeControlHWND();
  }
};

// Tests that NativeControls do set the focus View appropriately on the
// FocusManager.
TEST_F(FocusManagerTest, FocusNativeControls) {
  TestNativeButton* button = new TestNativeButton(L"Press me");
  TestCheckbox* checkbox = new TestCheckbox(L"Checkbox");
  TestRadioButton* radio_button = new TestRadioButton(L"RadioButton");
  TestTextfield* textfield = new TestTextfield();
  TestCombobox* combobox = new TestCombobox();
  TestTabbedPane* tabbed_pane = new TestTabbedPane();
  TestNativeButton* tab_button = new TestNativeButton(L"tab button");

  content_view_->AddChildView(button);
  content_view_->AddChildView(checkbox);
  content_view_->AddChildView(radio_button);
  content_view_->AddChildView(textfield);
  content_view_->AddChildView(combobox);
  content_view_->AddChildView(tabbed_pane);
  tabbed_pane->AddTab(L"Awesome tab", tab_button);

  // Simulate the native HWNDs getting the native focus.
  ::SendMessage(button->TestGetNativeControlHWND(), WM_SETFOCUS, NULL, NULL);
  EXPECT_EQ(button, GetFocusManager()->GetFocusedView());

  ::SendMessage(checkbox->TestGetNativeControlHWND(), WM_SETFOCUS, NULL, NULL);
  EXPECT_EQ(checkbox, GetFocusManager()->GetFocusedView());

  ::SendMessage(radio_button->TestGetNativeControlHWND(), WM_SETFOCUS,
                NULL, NULL);
  EXPECT_EQ(radio_button, GetFocusManager()->GetFocusedView());

  ::SendMessage(textfield->TestGetNativeComponent(), WM_SETFOCUS, NULL, NULL);
  EXPECT_EQ(textfield, GetFocusManager()->GetFocusedView());

  ::SendMessage(combobox->TestGetNativeComponent(), WM_SETFOCUS, NULL, NULL);
  EXPECT_EQ(combobox, GetFocusManager()->GetFocusedView());

  ::SendMessage(tabbed_pane->TestGetNativeControlHWND(), WM_SETFOCUS,
                NULL, NULL);
  EXPECT_EQ(tabbed_pane, GetFocusManager()->GetFocusedView());

  ::SendMessage(tab_button->TestGetNativeControlHWND(), WM_SETFOCUS,
                NULL, NULL);
  EXPECT_EQ(tab_button, GetFocusManager()->GetFocusedView());
}

// Test that when activating/deactivating the top window, the focus is stored/
// restored properly.
TEST_F(FocusManagerTest, FocusStoreRestore) {
  NativeButton* button = new NativeButton(NULL, L"Press me");
  View* view = new View();
  view->SetFocusable(true);

  content_view_->AddChildView(button);
  content_view_->AddChildView(view);

  TestFocusChangeListener listener;
  AddFocusChangeListener(&listener);

  view->RequestFocus();

  // Deacivate the window, it should store its focus.
  SimulateDeactivateWindow();
  EXPECT_EQ(NULL, GetFocusManager()->GetFocusedView());
  ASSERT_EQ(2, listener.focus_changes().size());
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(NULL, view));
  EXPECT_TRUE(listener.focus_changes()[1] == ViewPair(view, NULL));
  listener.ClearFocusChanges();

  // Reactivate, focus should come-back to the previously focused view.
  SimulateActivateWindow();
  EXPECT_EQ(view, GetFocusManager()->GetFocusedView());
  ASSERT_EQ(1, listener.focus_changes().size());
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(NULL, view));
  listener.ClearFocusChanges();

  // Same test with a NativeControl.
  button->RequestFocus();
  SimulateDeactivateWindow();
  EXPECT_EQ(NULL, GetFocusManager()->GetFocusedView());
  ASSERT_EQ(2, listener.focus_changes().size());
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(view, button));
  EXPECT_TRUE(listener.focus_changes()[1] == ViewPair(button, NULL));
  listener.ClearFocusChanges();

  SimulateActivateWindow();
  EXPECT_EQ(button, GetFocusManager()->GetFocusedView());
  ASSERT_EQ(1, listener.focus_changes().size());
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(NULL, button));
  listener.ClearFocusChanges();

  /*
  // Now test that while the window is inactive we can change the focused view
  // (we do that in several places).
  SimulateDeactivateWindow();
  // TODO: would have to mock the window being inactive (with a TestWidgetWin
  //       that would return false on IsActive()).
  GetFocusManager()->SetFocusedView(view);
  ::SendMessage(window_->GetNativeWindow(), WM_ACTIVATE, WA_ACTIVE, NULL);

  EXPECT_EQ(view, GetFocusManager()->GetFocusedView());
  ASSERT_EQ(2, listener.focus_changes().size());
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(button, NULL));
  EXPECT_TRUE(listener.focus_changes()[1] == ViewPair(NULL, view));
  */
}

TEST_F(FocusManagerTest, ContainsView) {
  View* view = new View();
  scoped_ptr<View> detached_view(new View());
  TabbedPane* tabbed_pane = new TabbedPane();
  TabbedPane* nested_tabbed_pane = new TabbedPane();
  NativeButton* tab_button = new NativeButton(NULL, L"tab button");

  content_view_->AddChildView(view);
  content_view_->AddChildView(tabbed_pane);
  // Adding a View inside a TabbedPane to test the case of nested root view.

  tabbed_pane->AddTab(L"Awesome tab", nested_tabbed_pane);
  nested_tabbed_pane->AddTab(L"Awesomer tab", tab_button);

  EXPECT_TRUE(GetFocusManager()->ContainsView(view));
  EXPECT_TRUE(GetFocusManager()->ContainsView(tabbed_pane));
  EXPECT_TRUE(GetFocusManager()->ContainsView(nested_tabbed_pane));
  EXPECT_TRUE(GetFocusManager()->ContainsView(tab_button));
  EXPECT_FALSE(GetFocusManager()->ContainsView(detached_view.get()));
}

TEST_F(FocusTraversalTest, NormalTraversal) {
  const int kTraversalIDs[] = { kTopCheckBoxID,  kAppleTextfieldID,
      kOrangeTextfieldID, kBananaTextfieldID, kKiwiTextfieldID,
      kFruitButtonID, kFruitCheckBoxID, kComboboxID, kAsparagusButtonID,
      kRosettaLinkID, kStupeurEtTremblementLinkID,
      kDinerGameLinkID, kRidiculeLinkID, kClosetLinkID, kVisitingLinkID,
      kAmelieLinkID, kJoyeuxNoelLinkID, kCampingLinkID, kBriceDeNiceLinkID,
      kTaxiLinkID, kAsterixLinkID, kOKButtonID, kCancelButtonID, kHelpButtonID,
      kStyleContainerID, kBoldCheckBoxID, kItalicCheckBoxID,
      kUnderlinedCheckBoxID, kStyleHelpLinkID, kStyleTextEditID,
      kSearchTextfieldID, kSearchButtonID, kHelpLinkID,
      kThumbnailContainerID, kThumbnailStarID, kThumbnailSuperStarID };

  // Uncomment the following line if you want to test manually the UI of this
  // test.
  // MessageLoopForUI::current()->Run(new AcceleratorHandler());

  // Let's traverse the whole focus hierarchy (several times, to make sure it
  // loops OK).
  GetFocusManager()->SetFocusedView(NULL);
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < arraysize(kTraversalIDs); j++) {
      GetFocusManager()->AdvanceFocus(false);
      View* focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->GetID());
    }
  }

  // Let's traverse in reverse order.
  GetFocusManager()->SetFocusedView(NULL);
  for (int i = 0; i < 3; ++i) {
    for (int j = arraysize(kTraversalIDs) - 1; j >= 0; --j) {
      GetFocusManager()->AdvanceFocus(true);
      View* focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->GetID());
    }
  }
}

TEST_F(FocusTraversalTest, TraversalWithNonEnabledViews) {
  const int kDisabledIDs[] = {
      kBananaTextfieldID, kFruitCheckBoxID, kComboboxID, kAsparagusButtonID,
      kCauliflowerButtonID, kClosetLinkID, kVisitingLinkID, kBriceDeNiceLinkID,
      kTaxiLinkID, kAsterixLinkID, kHelpButtonID, kBoldCheckBoxID,
      kSearchTextfieldID, kHelpLinkID };

  const int kTraversalIDs[] = { kTopCheckBoxID,  kAppleTextfieldID,
      kOrangeTextfieldID, kKiwiTextfieldID, kFruitButtonID, kBroccoliButtonID,
      kRosettaLinkID, kStupeurEtTremblementLinkID, kDinerGameLinkID,
      kRidiculeLinkID, kAmelieLinkID, kJoyeuxNoelLinkID, kCampingLinkID,
      kOKButtonID, kCancelButtonID, kStyleContainerID, kItalicCheckBoxID,
      kUnderlinedCheckBoxID, kStyleHelpLinkID, kStyleTextEditID,
      kSearchButtonID, kThumbnailContainerID, kThumbnailStarID,
      kThumbnailSuperStarID };

  // Let's disable some views.
  for (int i = 0; i < arraysize(kDisabledIDs); i++) {
    View* v = FindViewByID(kDisabledIDs[i]);
    ASSERT_TRUE(v != NULL);
    if (v)
      v->SetEnabled(false);
  }

  // Uncomment the following line if you want to test manually the UI of this
  // test.
  // MessageLoopForUI::current()->Run(new AcceleratorHandler());

  View* focused_view;
  // Let's do one traversal (several times, to make sure it loops ok).
  GetFocusManager()->SetFocusedView(NULL);
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < arraysize(kTraversalIDs); j++) {
      GetFocusManager()->AdvanceFocus(false);
      focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->GetID());
    }
  }

  // Same thing in reverse.
  GetFocusManager()->SetFocusedView(NULL);
  for (int i = 0; i < 3; ++i) {
    for (int j = arraysize(kTraversalIDs) - 1; j >= 0; --j) {
      GetFocusManager()->AdvanceFocus(true);
      focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->GetID());
    }
  }
}

// Counts accelerator calls.
class TestAcceleratorTarget : public AcceleratorTarget {
 public:
  explicit TestAcceleratorTarget(bool process_accelerator)
      : accelerator_count_(0), process_accelerator_(process_accelerator) {}

  virtual bool AcceleratorPressed(const Accelerator& accelerator) {
    ++accelerator_count_;
    return process_accelerator_;
  }

  int accelerator_count() const { return accelerator_count_; }

 private:
  int accelerator_count_;  // number of times that the accelerator is activated
  bool process_accelerator_;  // return value of AcceleratorPressed

  DISALLOW_COPY_AND_ASSIGN(TestAcceleratorTarget);
};

TEST_F(FocusManagerTest, CallsNormalAcceleratorTarget) {
  FocusManager* focus_manager = GetFocusManager();
  Accelerator return_accelerator(VK_RETURN, false, false, false);
  Accelerator escape_accelerator(VK_ESCAPE, false, false, false);

  TestAcceleratorTarget return_target(true);
  TestAcceleratorTarget escape_target(true);
  EXPECT_EQ(return_target.accelerator_count(), 0);
  EXPECT_EQ(escape_target.accelerator_count(), 0);
  EXPECT_EQ(NULL,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));
  EXPECT_EQ(NULL,
            focus_manager->GetCurrentTargetForAccelerator(escape_accelerator));

  // Register targets.
  focus_manager->RegisterAccelerator(return_accelerator, &return_target);
  focus_manager->RegisterAccelerator(escape_accelerator, &escape_target);

  // Checks if the correct target is registered.
  EXPECT_EQ(&return_target,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));
  EXPECT_EQ(&escape_target,
            focus_manager->GetCurrentTargetForAccelerator(escape_accelerator));

  // Hitting the return key.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 1);
  EXPECT_EQ(escape_target.accelerator_count(), 0);

  // Hitting the escape key.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 1);
  EXPECT_EQ(escape_target.accelerator_count(), 1);

  // Register another target for the return key.
  TestAcceleratorTarget return_target2(true);
  EXPECT_EQ(return_target2.accelerator_count(), 0);
  focus_manager->RegisterAccelerator(return_accelerator, &return_target2);
  EXPECT_EQ(&return_target2,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));

  // Hitting the return key; return_target2 has the priority.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 1);
  EXPECT_EQ(return_target2.accelerator_count(), 1);

  // Register a target that does not process the accelerator event.
  TestAcceleratorTarget return_target3(false);
  EXPECT_EQ(return_target3.accelerator_count(), 0);
  focus_manager->RegisterAccelerator(return_accelerator, &return_target3);
  EXPECT_EQ(&return_target3,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));

  // Hitting the return key.
  // Since the event handler of return_target3 returns false, return_target2
  // should be called too.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 1);
  EXPECT_EQ(return_target2.accelerator_count(), 2);
  EXPECT_EQ(return_target3.accelerator_count(), 1);

  // Unregister return_target2.
  focus_manager->UnregisterAccelerator(return_accelerator, &return_target2);
  EXPECT_EQ(&return_target3,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));

  // Hitting the return key. return_target3 and return_target should be called.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 2);
  EXPECT_EQ(return_target2.accelerator_count(), 2);
  EXPECT_EQ(return_target3.accelerator_count(), 2);

  // Unregister targets.
  focus_manager->UnregisterAccelerator(return_accelerator, &return_target);
  focus_manager->UnregisterAccelerator(return_accelerator, &return_target3);
  focus_manager->UnregisterAccelerator(escape_accelerator, &escape_target);

  // Now there is no target registered.
  EXPECT_EQ(NULL,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));
  EXPECT_EQ(NULL,
            focus_manager->GetCurrentTargetForAccelerator(escape_accelerator));

  // Hitting the return key and the escape key. Nothing should happen.
  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 2);
  EXPECT_EQ(return_target2.accelerator_count(), 2);
  EXPECT_EQ(return_target3.accelerator_count(), 2);
  EXPECT_FALSE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(escape_target.accelerator_count(), 1);
}

// Unregisters itself when its accelerator is invoked.
class SelfUnregisteringAcceleratorTarget : public AcceleratorTarget {
 public:
  SelfUnregisteringAcceleratorTarget(Accelerator accelerator,
                                     FocusManager* focus_manager)
      : accelerator_(accelerator),
        focus_manager_(focus_manager),
        accelerator_count_(0) {
  }

  virtual bool AcceleratorPressed(const Accelerator& accelerator) {
    ++accelerator_count_;
    focus_manager_->UnregisterAccelerator(accelerator, this);
    return true;
  }

  int accelerator_count() const { return accelerator_count_; }

 private:
  Accelerator accelerator_;
  FocusManager* focus_manager_;
  int accelerator_count_;

  DISALLOW_COPY_AND_ASSIGN(SelfUnregisteringAcceleratorTarget);
};

TEST_F(FocusManagerTest, CallsSelfDeletingAcceleratorTarget) {
  FocusManager* focus_manager = GetFocusManager();
  Accelerator return_accelerator(VK_RETURN, false, false, false);
  SelfUnregisteringAcceleratorTarget target(return_accelerator, focus_manager);
  EXPECT_EQ(target.accelerator_count(), 0);
  EXPECT_EQ(NULL,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));

  // Register the target.
  focus_manager->RegisterAccelerator(return_accelerator, &target);
  EXPECT_EQ(&target,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));

  // Hitting the return key. The target will be unregistered.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(target.accelerator_count(), 1);
  EXPECT_EQ(NULL,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));

  // Hitting the return key again; nothing should happen.
  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(target.accelerator_count(), 1);
}

}  // namespace views
