// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/path.h"
#include "chrome/views/background.h"
#include "chrome/views/event.h"
#include "chrome/views/root_view.h"
#include "chrome/views/view.h"
#include "chrome/views/window.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ViewTest : public testing::Test {
 public:
  ViewTest() {
    OleInitialize(NULL);
  }

  ~ViewTest() {
    OleUninitialize();
  }
 
 private:
  MessageLoopForUI message_loop_;
};

// Paints the RootView.
void PaintRootView(ChromeViews::RootView* root, bool empty_paint) {
  if (!empty_paint) {
    root->PaintNow();
  } else {
    // User isn't logged in, so that PaintNow will generate an empty rectangle.
    // Invoke paint directly.
    CRect paint_rect = root->GetScheduledPaintRect();
    ChromeCanvas canvas(paint_rect.Width(), paint_rect.Height(), true);
    canvas.TranslateInt(-paint_rect.left, -paint_rect.top);
    canvas.ClipRectInt(0, 0, paint_rect.Width(), paint_rect.Height());
    root->ProcessPaint(&canvas);
  }
}

/*
typedef CWinTraits<WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS> CVTWTraits;

// A trivial window implementation that tracks whether or not it has been
// painted. This is used by the painting test to determine if paint will result
// in an empty region.
class EmptyWindow : public CWindowImpl<EmptyWindow,
                                       CWindow,
                                       CVTWTraits> {
 public:
  DECLARE_FRAME_WND_CLASS(L"Chrome_ChromeViewsEmptyWindow", 0)

  BEGIN_MSG_MAP_EX(EmptyWindow)
    MSG_WM_PAINT(OnPaint)
  END_MSG_MAP()

  EmptyWindow::EmptyWindow(const CRect& bounds) : empty_paint_(false) {
    Create(NULL, static_cast<RECT>(bounds));
    ShowWindow(SW_SHOW);
  }

  EmptyWindow::~EmptyWindow() {
    ShowWindow(SW_HIDE);
    DestroyWindow();
  }

  void EmptyWindow::OnPaint(HDC dc) {
    PAINTSTRUCT ps;
    HDC paint_dc = BeginPaint(&ps);
    if (!empty_paint_ && (ps.rcPaint.top - ps.rcPaint.bottom) == 0 &&
        (ps.rcPaint.right - ps.rcPaint.left) == 0) {
      empty_paint_ = true;
    }
    EndPaint(&ps);
  }

  bool empty_paint() {
    return empty_paint_;
  }

 private:
  bool empty_paint_;

  DISALLOW_EVIL_CONSTRUCTORS(EmptyWindow);
};
*/
}

using namespace ChromeViews;

////////////////////////////////////////////////////////////////////////////////
//
// A view subclass for testing purpose
//
////////////////////////////////////////////////////////////////////////////////
class TestView : public View {
 public:
  TestView() {
  }

  virtual ~TestView() {}

  // Reset all test state
  void Reset() {
    did_change_bounds_ = false;
    child_added_ = false;
    child_removed_ = false;
    last_mouse_event_type_ = 0;
    location_.x = 0;
    location_.y = 0;
    last_clip_.setEmpty();
  }

  virtual void DidChangeBounds(const CRect& previous, const CRect& current);
  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);
  virtual bool OnMousePressed(const MouseEvent& event);
  virtual bool OnMouseDragged(const MouseEvent& event);
  virtual void OnMouseReleased(const MouseEvent& event, bool canceled);
  virtual void Paint(ChromeCanvas* canvas);

  // DidChangeBounds test
  bool did_change_bounds_;
  CRect previous_bounds_;
  CRect new_bounds_;

  // AddRemoveNotifications test
  bool child_added_;
  bool child_removed_;
  View* parent_;
  View* child_;

  // MouseEvent
  int last_mouse_event_type_;
  CPoint location_;

  // Painting
  SkRect last_clip_;
};

////////////////////////////////////////////////////////////////////////////////
// DidChangeBounds
////////////////////////////////////////////////////////////////////////////////

void TestView::DidChangeBounds(const CRect& previous, const CRect& current) {
  did_change_bounds_ = true;
  previous_bounds_ = previous;
  new_bounds_ = current;
}

TEST_F(ViewTest, DidChangeBounds) {
  TestView* v = new TestView();

  CRect prev_rect(0, 0, 200, 200);
  CRect new_rect(100, 100, 250, 250);

  v->SetBounds(prev_rect);
  v->Reset();

  v->SetBounds(new_rect);
  EXPECT_EQ(v->did_change_bounds_, true);
  EXPECT_EQ(v->previous_bounds_, prev_rect);
  EXPECT_EQ(v->new_bounds_, new_rect);

  EXPECT_EQ(v->bounds(), gfx::Rect(new_rect));
  delete v;
}


////////////////////////////////////////////////////////////////////////////////
// AddRemoveNotifications
////////////////////////////////////////////////////////////////////////////////

void TestView::ViewHierarchyChanged(bool is_add, View *parent, View *child) {
  if (is_add) {
    child_added_ = true;
  } else {
    child_removed_ = true;
  }
  parent_ = parent;
  child_ = child;
}

TEST_F(ViewTest, AddRemoveNotifications) {
  TestView* v1 = new TestView();
  v1->SetBounds(0, 0, 300, 300);

  TestView* v2 = new TestView();
  v2->SetBounds(0, 0, 300, 300);

  TestView* v3 = new TestView();
  v3->SetBounds(0, 0, 300, 300);

  // Add a child. Make sure both v2 and v3 receive the right
  // notification
  v2->Reset();
  v3->Reset();
  v2->AddChildView(v3);
  EXPECT_EQ(v2->child_added_, true);
  EXPECT_EQ(v2->parent_, v2);
  EXPECT_EQ(v2->child_, v3);

  EXPECT_EQ(v3->child_added_, true);
  EXPECT_EQ(v3->parent_, v2);
  EXPECT_EQ(v3->child_, v3);

  // Add v2 and transitively v3 to v1. Make sure that all views
  // received the right notification

  v1->Reset();
  v2->Reset();
  v3->Reset();
  v1->AddChildView(v2);

  EXPECT_EQ(v1->child_added_, true);
  EXPECT_EQ(v1->child_, v2);
  EXPECT_EQ(v1->parent_, v1);

  EXPECT_EQ(v2->child_added_, true);
  EXPECT_EQ(v2->child_, v2);
  EXPECT_EQ(v2->parent_, v1);

  EXPECT_EQ(v3->child_added_, true);
  EXPECT_EQ(v3->child_, v2);
  EXPECT_EQ(v3->parent_, v1);

  // Remove v2. Make sure all views received the right notification
  v1->Reset();
  v2->Reset();
  v3->Reset();
  v1->RemoveChildView(v2);

  EXPECT_EQ(v1->child_removed_, true);
  EXPECT_EQ(v1->parent_, v1);
  EXPECT_EQ(v1->child_, v2);

  EXPECT_EQ(v2->child_removed_, true);
  EXPECT_EQ(v2->parent_, v1);
  EXPECT_EQ(v2->child_, v2);

  EXPECT_EQ(v3->child_removed_, true);
  EXPECT_EQ(v3->parent_, v1);
  EXPECT_EQ(v3->child_, v3);

  // Clean-up
  delete v1;
  delete v2;  // This also deletes v3 (child of v2).
}

////////////////////////////////////////////////////////////////////////////////
// MouseEvent
////////////////////////////////////////////////////////////////////////////////

bool TestView::OnMousePressed(const MouseEvent& event) {
  last_mouse_event_type_ = event.GetType();
  location_.x = event.x();
  location_.y = event.y();
  return true;
}

bool TestView::OnMouseDragged(const MouseEvent& event) {
  last_mouse_event_type_ = event.GetType();
  location_.x = event.x();
  location_.y = event.y();
  return true;
}

void TestView::OnMouseReleased(const MouseEvent& event, bool canceled) {
  last_mouse_event_type_ = event.GetType();
  location_.x = event.x();
  location_.y = event.y();
}

TEST_F(ViewTest, MouseEvent) {
  TestView* v1 = new TestView();
  v1->SetBounds(0, 0, 300, 300);

  TestView* v2 = new TestView();
  v2->SetBounds (100, 100, 100, 100);

  ChromeViews::HWNDViewContainer window;
  window.set_delete_on_destroy(false);
  window.set_window_style(WS_OVERLAPPEDWINDOW);
  window.Init(NULL, gfx::Rect(50, 50, 650, 650), false);
  RootView* root = window.GetRootView();

  root->AddChildView(v1);
  v1->AddChildView(v2);

  v1->Reset();
  v2->Reset();

  MouseEvent pressed(Event::ET_MOUSE_PRESSED,
                     110,
                     120,
                     Event::EF_LEFT_BUTTON_DOWN);
  root->OnMousePressed(pressed);
  EXPECT_EQ(v2->last_mouse_event_type_, Event::ET_MOUSE_PRESSED);
  EXPECT_EQ(v2->location_.x, 10);
  EXPECT_EQ(v2->location_.y, 20);
  // Make sure v1 did not receive the event
  EXPECT_EQ(v1->last_mouse_event_type_, 0);

  // Drag event out of bounds. Should still go to v2
  v1->Reset();
  v2->Reset();
  MouseEvent dragged(Event::ET_MOUSE_DRAGGED,
                     50,
                     40,
                     Event::EF_LEFT_BUTTON_DOWN);
  root->OnMouseDragged(dragged);
  EXPECT_EQ(v2->last_mouse_event_type_, Event::ET_MOUSE_DRAGGED);
  EXPECT_EQ(v2->location_.x, -50);
  EXPECT_EQ(v2->location_.y, -60);
  // Make sure v1 did not receive the event
  EXPECT_EQ(v1->last_mouse_event_type_, 0);

  // Releasted event out of bounds. Should still go to v2
  v1->Reset();
  v2->Reset();
  MouseEvent released(Event::ET_MOUSE_RELEASED, 0, 0, 0);
  root->OnMouseDragged(released);
  EXPECT_EQ(v2->last_mouse_event_type_, Event::ET_MOUSE_RELEASED);
  EXPECT_EQ(v2->location_.x, -100);
  EXPECT_EQ(v2->location_.y, -100);
  // Make sure v1 did not receive the event
  EXPECT_EQ(v1->last_mouse_event_type_, 0);

  window.CloseNow();
}

////////////////////////////////////////////////////////////////////////////////
// Painting
////////////////////////////////////////////////////////////////////////////////

void TestView::Paint(ChromeCanvas* canvas) {
  canvas->getClipBounds(&last_clip_);
}

void CheckRect(const SkRect& check_rect, const SkRect& target_rect) {
  EXPECT_EQ(target_rect.fLeft, check_rect.fLeft);
  EXPECT_EQ(target_rect.fRight, check_rect.fRight);
  EXPECT_EQ(target_rect.fTop, check_rect.fTop);
  EXPECT_EQ(target_rect.fBottom, check_rect.fBottom);
}

/* This test is disabled because it is flakey on some systems.
TEST_F(ViewTest, Painting) {
  // Determine if InvalidateRect generates an empty paint rectangle.
  EmptyWindow paint_window(CRect(50, 50, 650, 650));
  paint_window.RedrawWindow(CRect(0, 0, 600, 600), NULL,
                            RDW_UPDATENOW | RDW_INVALIDATE | RDW_ALLCHILDREN);
  bool empty_paint = paint_window.empty_paint();

  ChromeViews::HWNDViewContainer window;
  window.set_delete_on_destroy(false);
  window.set_window_style(WS_OVERLAPPEDWINDOW);
  window.Init(NULL, gfx::Rect(50, 50, 650, 650), NULL);
  RootView* root = window.GetRootView();

  TestView* v1 = new TestView();
  v1->SetBounds(0, 0, 650, 650);
  root->AddChildView(v1);

  TestView* v2 = new TestView();
  v2->SetBounds(10, 10, 80, 80);
  v1->AddChildView(v2);

  TestView* v3 = new TestView();
  v3->SetBounds(10, 10, 60, 60);
  v2->AddChildView(v3);

  TestView* v4 = new TestView();
  v4->SetBounds(10, 200, 100, 100);
  v1->AddChildView(v4);

  // Make sure to paint current rects
  PaintRootView(root, empty_paint);


  v1->Reset();
  v2->Reset();
  v3->Reset();
  v4->Reset();
  v3->SchedulePaint(10, 10, 10, 10);
  PaintRootView(root, empty_paint);

  SkRect tmp_rect;

  tmp_rect.set(SkIntToScalar(10),
               SkIntToScalar(10),
               SkIntToScalar(20),
               SkIntToScalar(20));
  CheckRect(v3->last_clip_, tmp_rect);

  tmp_rect.set(SkIntToScalar(20),
               SkIntToScalar(20),
               SkIntToScalar(30),
               SkIntToScalar(30));
  CheckRect(v2->last_clip_, tmp_rect);

  tmp_rect.set(SkIntToScalar(30),
               SkIntToScalar(30),
               SkIntToScalar(40),
               SkIntToScalar(40));
  CheckRect(v1->last_clip_, tmp_rect);

  // Make sure v4 was not painted
  tmp_rect.setEmpty();
  CheckRect(v4->last_clip_, tmp_rect);

  window.DestroyWindow();
}
*/
typedef std::vector<View*> ViewList;

class RemoveViewObserver : public NotificationObserver {
public:
  RemoveViewObserver() { }

  void Observe(NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
      ASSERT_TRUE(type == NOTIFY_VIEW_REMOVED);
      removed_views_.push_back(Source<ChromeViews::View>(source).ptr());
  }

  bool WasRemoved(ChromeViews::View* view) {
    return std::find(removed_views_.begin(), removed_views_.end(), view) !=
        removed_views_.end();
  }

  ViewList removed_views_;

};

TEST_F(ViewTest, RemoveNotification) {
  scoped_ptr<RemoveViewObserver> observer(new RemoveViewObserver);

  NotificationService::current()->AddObserver(
      observer.get(), NOTIFY_VIEW_REMOVED, NotificationService::AllSources());

  ChromeViews::HWNDViewContainer* window = new ChromeViews::HWNDViewContainer;
  ChromeViews::RootView* root_view = window->GetRootView();

  View* v1 = new View;
  root_view->AddChildView(v1);
  View* v11 = new View;
  v1->AddChildView(v11);
  View* v111 = new View;
  v11->AddChildView(v111);
  View* v112 = new View;
  v11->AddChildView(v112);
  View* v113 = new View;
  v11->AddChildView(v113);
  View* v1131 = new View;
  v113->AddChildView(v1131);
  View* v12 = new View;
  v1->AddChildView(v12);

  View* v2 = new View;
  root_view->AddChildView(v2);
  View* v21 = new View;
  v2->AddChildView(v21);
  View* v211 = new View;
  v21->AddChildView(v211);

  // Try removing a leaf view.
  v21->RemoveChildView(v211);
  EXPECT_EQ(1, observer->removed_views_.size());
  EXPECT_TRUE(observer->WasRemoved(v211));
  delete v211;  // We won't use this one anymore.

  // Now try removing a view with a hierarchy of depth 1.
  observer->removed_views_.clear();
  v11->RemoveChildView(v113);
  EXPECT_EQ(observer->removed_views_.size(), 2);
  EXPECT_TRUE(observer->WasRemoved(v113) && observer->WasRemoved(v1131));
  delete v113;  // We won't use this one anymore.

  // Now remove even more.
  observer->removed_views_.clear();
  root_view->RemoveChildView(v1);
  EXPECT_EQ(observer->removed_views_.size(), 5);
  EXPECT_TRUE(observer->WasRemoved(v1) &&
              observer->WasRemoved(v11) && observer->WasRemoved(v12) &&
              observer->WasRemoved(v111)  && observer->WasRemoved(v112));

  // Put v1 back for more tests.
  root_view->AddChildView(v1);
  observer->removed_views_.clear();

  // Now delete the root view (deleting the window will trigger a delete of the
  // RootView) and make sure we are notified that the views were removed.
  delete window;
  EXPECT_EQ(observer->removed_views_.size(), 7);
  EXPECT_TRUE(observer->WasRemoved(v1) && observer->WasRemoved(v2) &&
              observer->WasRemoved(v11) && observer->WasRemoved(v12) &&
              observer->WasRemoved(v21) &&
              observer->WasRemoved(v111)  && observer->WasRemoved(v112));

  NotificationService::current()->RemoveObserver(observer.get(),
      NOTIFY_VIEW_REMOVED, NotificationService::AllSources());
}

namespace {
class HitTestView : public ChromeViews::View {
 public:
  explicit HitTestView(bool has_hittest_mask)
      : has_hittest_mask_(has_hittest_mask) {
  }
  virtual ~HitTestView() {}

 protected:
  // Overridden from ChromeViews::View:
  virtual bool HasHitTestMask() const {
    return has_hittest_mask_;
  }
  virtual void GetHitTestMask(gfx::Path* mask) const {
    DCHECK(has_hittest_mask_);
    DCHECK(mask);

    SkScalar w = SkIntToScalar(width());
    SkScalar h = SkIntToScalar(height());

    // Create a triangular mask within the bounds of this View.
    mask->moveTo(w / 2, 0);
    mask->lineTo(w, h);
    mask->lineTo(0, h);
    mask->close();
  }

 private:
  bool has_hittest_mask_;

  DISALLOW_COPY_AND_ASSIGN(HitTestView);
};

POINT ConvertPointToView(ChromeViews::View* view, const POINT& p) {
  gfx::Point tmp(p);
  ChromeViews::View::ConvertPointToView(view->GetRootView(), view, &tmp);
  return tmp.ToPOINT();
}
}

TEST_F(ViewTest, HitTestMasks) {
  ChromeViews::HWNDViewContainer window;
  ChromeViews::RootView* root_view = window.GetRootView();
  root_view->SetBounds(0, 0, 500, 500);

  gfx::Rect v1_bounds = gfx::Rect(0, 0, 100, 100);
  HitTestView* v1 = new HitTestView(false);
  v1->SetBounds(v1_bounds.ToRECT());
  root_view->AddChildView(v1);

  gfx::Rect v2_bounds = gfx::Rect(105, 0, 100, 100);
  HitTestView* v2 = new HitTestView(true);
  v2->SetBounds(v2_bounds.ToRECT());
  root_view->AddChildView(v2);

  POINT v1_centerpoint = v1_bounds.CenterPoint().ToPOINT();
  POINT v2_centerpoint = v2_bounds.CenterPoint().ToPOINT();
  POINT v1_origin = v1_bounds.origin().ToPOINT();
  POINT v2_origin = v2_bounds.origin().ToPOINT();

  // Test HitTest
  EXPECT_EQ(true, v1->HitTest(ConvertPointToView(v1, v1_centerpoint)));
  EXPECT_EQ(true, v2->HitTest(ConvertPointToView(v2, v2_centerpoint)));

  EXPECT_EQ(true, v1->HitTest(ConvertPointToView(v1, v1_origin)));
  EXPECT_EQ(false, v2->HitTest(ConvertPointToView(v2, v2_origin)));

  // Test GetViewForPoint
  EXPECT_EQ(v1, root_view->GetViewForPoint(v1_centerpoint));
  EXPECT_EQ(v2, root_view->GetViewForPoint(v2_centerpoint));
  EXPECT_EQ(v1, root_view->GetViewForPoint(v1_origin));
  EXPECT_EQ(root_view, root_view->GetViewForPoint(v2_origin));
}
