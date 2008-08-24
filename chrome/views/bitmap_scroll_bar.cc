// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/bitmap_scroll_bar.h"

#include "base/message_loop.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/menu.h"
#include "chrome/views/scroll_view.h"
#include "chrome/views/view_container.h"
#include "skia/include/SkBitmap.h"

#include "generated_resources.h"

namespace ChromeViews {

namespace {

// The distance the mouse can be dragged outside the bounds of the thumb during
// dragging before the scrollbar will snap back to its regular position.
static const int kScrollThumbDragOutSnap = 100;

///////////////////////////////////////////////////////////////////////////////
//
// AutorepeatButton
//
//  A button that activates on mouse pressed rather than released, and that
//  continues to fire the clicked action as the mouse button remains pressed
//  down on the button.
//
///////////////////////////////////////////////////////////////////////////////
class AutorepeatButton : public Button {
 public:
  AutorepeatButton()
      : repeater_(NewCallback<AutorepeatButton>(this,
          &AutorepeatButton::NotifyClick)) {
  }
  virtual ~AutorepeatButton() {}

 protected:
  virtual bool OnMousePressed(const MouseEvent& event) {
    Button::NotifyClick(event.GetFlags());
    repeater_.Start();
    return true;
  }

  virtual void OnMouseReleased(const MouseEvent& event, bool canceled) {
    repeater_.Stop();
    View::OnMouseReleased(event, canceled);
  }

 private:
  void NotifyClick() {
    BaseButton::NotifyClick(0);
  }

  // The repeat controller that we use to repeatedly click the button when the
  // mouse button is down.
  RepeatController repeater_;

  DISALLOW_EVIL_CONSTRUCTORS(AutorepeatButton);
};

///////////////////////////////////////////////////////////////////////////////
//
// BitmapScrollBarThumb
//
//  A view that acts as the thumb in the scroll bar track that the user can
//  drag to scroll the associated contents view within the viewport.
//
///////////////////////////////////////////////////////////////////////////////
class BitmapScrollBarThumb : public View {
 public:
  explicit BitmapScrollBarThumb(BitmapScrollBar* scroll_bar)
      : scroll_bar_(scroll_bar),
        drag_start_position_(-1),
        mouse_offset_(-1),
        state_(BaseButton::BS_NORMAL) {
  }
  virtual ~BitmapScrollBarThumb() { }

  // Sets the size (width or height) of the thumb to the specified value.
  void SetSize(int size) {
    // Make sure the thumb is never sized smaller than its minimum possible
    // display size.
    CSize prefsize;
    GetPreferredSize(&prefsize);
    size = std::max(size,
                    static_cast<int>(scroll_bar_->IsHorizontal() ?
                        prefsize.cx : prefsize.cy));
    CRect bounds;
    GetBounds(&bounds);
    if (scroll_bar_->IsHorizontal()) {
      bounds.right = bounds.left + size;
    } else {
      bounds.bottom = bounds.top + size;
    }
    SetBounds(bounds);
  }

  // Retrieves the size (width or height) of the thumb.
  int GetSize() const {
    CRect bounds;
    GetBounds(&bounds);
    if (scroll_bar_->IsHorizontal())
      return bounds.Width();
    return bounds.Height();
  }

  // Sets the position of the thumb on the x or y axis.
  void SetPosition(int position) {
    CRect bounds;
    GetBounds(&bounds);
    gfx::Rect track_bounds = scroll_bar_->GetTrackBounds();
    if (scroll_bar_->IsHorizontal()) {
      bounds.MoveToX(track_bounds.x() + position);
    } else {
      bounds.MoveToY(track_bounds.y() + position);
    }
    SetBounds(bounds);
  }

  // Gets the position of the thumb on the x or y axis.
  int GetPosition() const {
    CRect bounds;
    GetBounds(&bounds);
    gfx::Rect track_bounds = scroll_bar_->GetTrackBounds();
    if (scroll_bar_->IsHorizontal())
      return bounds.left - track_bounds.x();
    return bounds.top - track_bounds.y();
  }

  // View overrides:
  virtual void GetPreferredSize(CSize* preferred_size) {
    DCHECK(preferred_size);
    preferred_size->cx = background_bitmap()->width();
    preferred_size->cy = start_cap_bitmap()->height() +
        end_cap_bitmap()->height() + grippy_bitmap()->height();
  }

 protected:
  // View overrides:
  virtual void Paint(ChromeCanvas* canvas) {
    canvas->DrawBitmapInt(*start_cap_bitmap(), 0, 0);
    int top_cap_height = start_cap_bitmap()->height();
    int bottom_cap_height = end_cap_bitmap()->height();
    int thumb_body_height = GetHeight() - top_cap_height - bottom_cap_height;
    canvas->TileImageInt(*background_bitmap(), 0, top_cap_height,
                         background_bitmap()->width(), thumb_body_height);
    canvas->DrawBitmapInt(*end_cap_bitmap(), 0,
                          GetHeight() - bottom_cap_height);

    // Paint the grippy over the track.
    int grippy_x = (GetWidth() - grippy_bitmap()->width()) / 2;
    int grippy_y = (thumb_body_height - grippy_bitmap()->height()) / 2;
    canvas->DrawBitmapInt(*grippy_bitmap(), grippy_x, grippy_y);
  }

  virtual void OnMouseEntered(const MouseEvent& event) {
    SetState(BaseButton::BS_HOT);
  }

  virtual void OnMouseExited(const MouseEvent& event) {
    SetState(BaseButton::BS_NORMAL);
  }

  virtual bool OnMousePressed(const MouseEvent& event) {
    mouse_offset_ = scroll_bar_->IsHorizontal() ? event.GetX() : event.GetY();
    drag_start_position_ = GetPosition();
    SetState(BaseButton::BS_PUSHED);
    return true;
  }

  virtual bool OnMouseDragged(const MouseEvent& event) {
    // If the user moves the mouse more than |kScrollThumbDragOutSnap| outside
    // the bounds of the thumb, the scrollbar will snap the scroll back to the
    // point it was at before the drag began.
    if (scroll_bar_->IsHorizontal()) {
      if ((event.GetY() < GetY() - kScrollThumbDragOutSnap) ||
          (event.GetY() > (GetY() + GetHeight() + kScrollThumbDragOutSnap))) {
        scroll_bar_->ScrollToThumbPosition(drag_start_position_, false);
        return true;
      }
    } else {
      if ((event.GetX() < GetX() - kScrollThumbDragOutSnap) ||
          (event.GetX() > (GetX() + GetWidth() + kScrollThumbDragOutSnap))) {
        scroll_bar_->ScrollToThumbPosition(drag_start_position_, false);
        return true;
      }
    }
    if (scroll_bar_->IsHorizontal()) {
      int thumb_x = event.GetX() - mouse_offset_;
      scroll_bar_->ScrollToThumbPosition(GetX() + thumb_x, false);
    } else {
      int thumb_y = event.GetY() - mouse_offset_;
      scroll_bar_->ScrollToThumbPosition(GetY() + thumb_y, false);
    }
    return true;
  }

  virtual void OnMouseReleased(const MouseEvent& event,
                               bool canceled) {
    SetState(BaseButton::BS_HOT);
    View::OnMouseReleased(event, canceled);
  }

 private:
  // Returns the bitmap rendered at the start of the thumb.
  SkBitmap* start_cap_bitmap() const {
    return scroll_bar_->images_[BitmapScrollBar::THUMB_START_CAP][state_];
  }

  // Returns the bitmap rendered at the end of the thumb.
  SkBitmap* end_cap_bitmap() const {
    return scroll_bar_->images_[BitmapScrollBar::THUMB_END_CAP][state_];
  }

  // Returns the bitmap that is tiled in the background of the thumb between
  // the start and the end caps.
  SkBitmap* background_bitmap() const {
    return scroll_bar_->images_[BitmapScrollBar::THUMB_MIDDLE][state_];
  }

  // Returns the bitmap that is rendered in the middle of the thumb
  // transparently over the background bitmap.
  SkBitmap* grippy_bitmap() const {
    return scroll_bar_->images_[BitmapScrollBar::THUMB_GRIPPY][BaseButton::BS_NORMAL];
  }

  // Update our state and schedule a repaint when the mouse moves over us.
  void SetState(BaseButton::ButtonState state) {
    state_ = state;
    SchedulePaint();
  }

  // The BitmapScrollBar that owns us.
  BitmapScrollBar* scroll_bar_;

  int drag_start_position_;

  // The position of the mouse on the scroll axis relative to the top of this
  // View when the drag started.
  int mouse_offset_;

  // The current state of the thumb button.
  BaseButton::ButtonState state_;

  DISALLOW_EVIL_CONSTRUCTORS(BitmapScrollBarThumb);
};

} // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// BitmapScrollBar, public:

BitmapScrollBar::BitmapScrollBar(bool horizontal, bool show_scroll_buttons)
    : contents_size_(0),
      contents_scroll_offset_(0),
      prev_button_(new AutorepeatButton),
      next_button_(new AutorepeatButton),
      thumb_(new BitmapScrollBarThumb(this)),
      thumb_track_state_(BaseButton::BS_NORMAL),
      last_scroll_amount_(SCROLL_NONE),
      repeater_(NewCallback<BitmapScrollBar>(this,
          &BitmapScrollBar::TrackClicked)),
      context_menu_mouse_position_(0),
      show_scroll_buttons_(show_scroll_buttons),
      ScrollBar(horizontal) {
  if (!show_scroll_buttons_) {
    prev_button_->SetVisible(false);
    next_button_->SetVisible(false);
  }
  prev_button_->SetListener(this, -1);
  next_button_->SetListener(this, -1);

  AddChildView(prev_button_);
  AddChildView(next_button_);
  AddChildView(thumb_);

  SetContextMenuController(this);
  prev_button_->SetContextMenuController(this);
  next_button_->SetContextMenuController(this);
  thumb_->SetContextMenuController(this);
}

gfx::Rect BitmapScrollBar::GetTrackBounds() const {
  CSize prefsize;
  prev_button_->GetPreferredSize(&prefsize);
  if (IsHorizontal()) {
    if (!show_scroll_buttons_)
      prefsize.cx = 0;
    int new_width = std::max(0,
                             static_cast<int>(GetWidth() - (prefsize.cx * 2)));
    gfx::Rect track_bounds(prefsize.cx, 0, new_width, prefsize.cy);
    return track_bounds;
  }
  if (!show_scroll_buttons_)
    prefsize.cy = 0;
  gfx::Rect track_bounds(0, prefsize.cy, prefsize.cx,
                         std::max(0l, GetHeight() - (prefsize.cy * 2)));
  return track_bounds;
}

void BitmapScrollBar::SetImage(ScrollBarPart part,
                               BaseButton::ButtonState state,
                               SkBitmap* bitmap) {
  DCHECK(part < PART_COUNT);
  DCHECK(state < BaseButton::kButtonStateCount);
  switch (part) {
    case PREV_BUTTON:
      prev_button_->SetImage(state, bitmap);
      break;
    case NEXT_BUTTON:
      next_button_->SetImage(state, bitmap);
      break;
    case THUMB_START_CAP:
    case THUMB_MIDDLE:
    case THUMB_END_CAP:
    case THUMB_GRIPPY:
    case THUMB_TRACK:
      images_[part][state] = bitmap;
      break;
  }
}

void BitmapScrollBar::ScrollByAmount(ScrollAmount amount) {
  ScrollBarController* controller = GetController();
  int offset = contents_scroll_offset_;
  switch (amount) {
    case SCROLL_START:
      offset = GetMinPosition();
      break;
    case SCROLL_END:
      offset = GetMaxPosition();
      break;
    case SCROLL_PREV_LINE:
      offset -= controller->GetScrollIncrement(this, false, false);
      offset = std::max(GetMinPosition(), offset);
      break;
    case SCROLL_NEXT_LINE:
      offset += controller->GetScrollIncrement(this, false, true);
      offset = std::min(GetMaxPosition(), offset);
      break;
    case SCROLL_PREV_PAGE:
      offset -= controller->GetScrollIncrement(this, true, false);
      offset = std::max(GetMinPosition(), offset);
      break;
    case SCROLL_NEXT_PAGE:
      offset += controller->GetScrollIncrement(this, true, true);
      offset = std::min(GetMaxPosition(), offset);
      break;
  }
  contents_scroll_offset_ = offset;
  ScrollContentsToOffset();
}

void BitmapScrollBar::ScrollToThumbPosition(int thumb_position,
                                            bool scroll_to_middle) {
  contents_scroll_offset_ =
      CalculateContentsOffset(thumb_position, scroll_to_middle);
  if (contents_scroll_offset_ < GetMinPosition()) {
    contents_scroll_offset_ = GetMinPosition();
  } else if (contents_scroll_offset_ > GetMaxPosition()) {
    contents_scroll_offset_ = GetMaxPosition();
  }
  ScrollContentsToOffset();
  SchedulePaint();
}

void BitmapScrollBar::ScrollByContentsOffset(int contents_offset) {
  contents_scroll_offset_ -= contents_offset;
  if (contents_scroll_offset_ < GetMinPosition()) {
    contents_scroll_offset_ = GetMinPosition();
  } else if (contents_scroll_offset_ > GetMaxPosition()) {
    contents_scroll_offset_ = GetMaxPosition();
  }
  ScrollContentsToOffset();
}

void BitmapScrollBar::TrackClicked() {
  if (last_scroll_amount_ != SCROLL_NONE) {
    CRect thumb_bounds;
    thumb_->GetBounds(&thumb_bounds);
    ScrollByAmount(last_scroll_amount_);
  }
}

///////////////////////////////////////////////////////////////////////////////
// BitmapScrollBar, View implementation:

void BitmapScrollBar::GetPreferredSize(CSize* preferred_size) {
  DCHECK(preferred_size);

  // In this case, we're returning the desired width of the scrollbar and its
  // minimum allowable height.
  CSize button_prefsize;
  prev_button_->GetPreferredSize(&button_prefsize);
  preferred_size->cx = button_prefsize.cx;
  preferred_size->cy = button_prefsize.cy * 2;
}

void BitmapScrollBar::Paint(ChromeCanvas* canvas) {
  // Paint the track.
  gfx::Rect track_bounds = GetTrackBounds();
  canvas->TileImageInt(*images_[THUMB_TRACK][thumb_track_state_],
                       track_bounds.x(), track_bounds.y(),
                       track_bounds.width(), track_bounds.height());
}

void BitmapScrollBar::Layout() {
  // Size and place the two scroll buttons.
  if (show_scroll_buttons_) {
    CSize prefsize;
    prev_button_->GetPreferredSize(&prefsize);
    prev_button_->SetBounds(0, 0, prefsize.cx, prefsize.cy);
    next_button_->GetPreferredSize(&prefsize);
    if (IsHorizontal()) {
      next_button_->SetBounds(GetWidth() - prefsize.cx, 0, prefsize.cx,
                              prefsize.cy);
    } else {
      next_button_->SetBounds(0, GetHeight() - prefsize.cy, prefsize.cx,
                              prefsize.cy);
    }
  } else {
    prev_button_->SetBounds(0, 0, 0, 0);
    next_button_->SetBounds(0, 0, 0, 0);
  }

  // Size and place the thumb
  CSize thumb_prefsize;
  thumb_->GetPreferredSize(&thumb_prefsize);
  gfx::Rect track_bounds = GetTrackBounds();

  // Preserve the height/width of the thumb (depending on orientation) as set
  // by the last call to |Update|, but coerce the width/height to be the
  // appropriate value for the bitmaps provided.
  CRect bounds;
  thumb_->GetBounds(&bounds);
  if (IsHorizontal()) {
    thumb_->SetBounds(bounds.left, bounds.top, bounds.Width(),
                      thumb_prefsize.cy);
  } else {
    thumb_->SetBounds(bounds.left, bounds.top, thumb_prefsize.cx,
                      bounds.Height());
  }

  // Hide the thumb if the track isn't tall enough to display even a tiny
  // thumb. The user can only use the mousewheel, scroll buttons or keyboard
  // in this scenario.
  if ((IsHorizontal() && (track_bounds.width() < thumb_prefsize.cx)) ||
      (!IsHorizontal() && (track_bounds.height() < thumb_prefsize.cy))) {
    thumb_->SetVisible(false);
  } else if (!thumb_->IsVisible()) {
    thumb_->SetVisible(true);
  }
}

void BitmapScrollBar::DidChangeBounds(const CRect& previous,
                                      const CRect& current) {
  Layout();
}

bool BitmapScrollBar::OnMousePressed(const MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton()) {
    SetThumbTrackState(BaseButton::BS_PUSHED);
    CRect thumb_bounds;
    thumb_->GetBounds(&thumb_bounds);
    if (IsHorizontal()) {
      if (event.GetX() < thumb_bounds.left) {
        last_scroll_amount_ = SCROLL_PREV_PAGE;
      } else if (event.GetX() > thumb_bounds.right) {
        last_scroll_amount_ = SCROLL_NEXT_PAGE;
      }
    } else {
      if (event.GetY() < thumb_bounds.top) {
        last_scroll_amount_ = SCROLL_PREV_PAGE;
      } else if (event.GetY() > thumb_bounds.bottom) {
        last_scroll_amount_ = SCROLL_NEXT_PAGE;
      }
    }
    TrackClicked();
    repeater_.Start();
  }
  return true;
}

void BitmapScrollBar::OnMouseReleased(const MouseEvent& event, bool canceled) {
  SetThumbTrackState(BaseButton::BS_NORMAL);
  repeater_.Stop();
  View::OnMouseReleased(event, canceled);
}

bool BitmapScrollBar::OnMouseWheel(const MouseWheelEvent& event) {
  ScrollByContentsOffset(event.GetOffset());
  return true;
}

bool BitmapScrollBar::OnKeyPressed(const KeyEvent& event) {
  ScrollAmount amount = SCROLL_NONE;
  switch(event.GetCharacter()) {
    case VK_UP:
      if (!IsHorizontal())
        amount = SCROLL_PREV_LINE;
      break;
    case VK_DOWN:
      if (!IsHorizontal())
        amount = SCROLL_NEXT_LINE;
      break;
    case VK_LEFT:
      if (IsHorizontal())
        amount = SCROLL_PREV_LINE;
      break;
    case VK_RIGHT:
      if (IsHorizontal())
        amount = SCROLL_NEXT_LINE;
      break;
    case VK_PRIOR:
      amount = SCROLL_PREV_PAGE;
      break;
    case VK_NEXT:
      amount = SCROLL_NEXT_PAGE;
      break;
    case VK_HOME:
      amount = SCROLL_START;
      break;
    case VK_END:
      amount = SCROLL_END;
      break;
  }
  if (amount != SCROLL_NONE) {
    ScrollByAmount(amount);
    return true;
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// BitmapScrollBar, ContextMenuController implementation:

enum ScrollBarContextMenuCommands {
  ScrollBarContextMenuCommand_ScrollHere = 1,
  ScrollBarContextMenuCommand_ScrollStart,
  ScrollBarContextMenuCommand_ScrollEnd,
  ScrollBarContextMenuCommand_ScrollPageUp,
  ScrollBarContextMenuCommand_ScrollPageDown,
  ScrollBarContextMenuCommand_ScrollPrev,
  ScrollBarContextMenuCommand_ScrollNext
};

void BitmapScrollBar::ShowContextMenu(View* source, int x, int y,
                                      bool is_mouse_gesture) {
  ViewContainer* vc = GetViewContainer();
  CRect vc_bounds;
  vc->GetBounds(&vc_bounds, true);
  CPoint temp_pt(x - vc_bounds.left, y - vc_bounds.top);
  View::ConvertPointFromViewContainer(this, &temp_pt);
  context_menu_mouse_position_ = IsHorizontal() ? temp_pt.x : temp_pt.y;

  Menu menu(this, Menu::TOPLEFT, GetViewContainer()->GetHWND());
  menu.AppendDelegateMenuItem(ScrollBarContextMenuCommand_ScrollHere);
  menu.AppendSeparator();
  menu.AppendDelegateMenuItem(ScrollBarContextMenuCommand_ScrollStart);
  menu.AppendDelegateMenuItem(ScrollBarContextMenuCommand_ScrollEnd);
  menu.AppendSeparator();
  menu.AppendDelegateMenuItem(ScrollBarContextMenuCommand_ScrollPageUp);
  menu.AppendDelegateMenuItem(ScrollBarContextMenuCommand_ScrollPageDown);
  menu.AppendSeparator();
  menu.AppendDelegateMenuItem(ScrollBarContextMenuCommand_ScrollPrev);
  menu.AppendDelegateMenuItem(ScrollBarContextMenuCommand_ScrollNext);
  menu.RunMenuAt(x, y);
}

///////////////////////////////////////////////////////////////////////////////
// BitmapScrollBar, Menu::Delegate implementation:

std::wstring BitmapScrollBar::GetLabel(int id) const {
  switch (id) {
    case ScrollBarContextMenuCommand_ScrollHere:
      return l10n_util::GetString(IDS_SCROLLBAR_CXMENU_SCROLLHERE);
    case ScrollBarContextMenuCommand_ScrollStart:
      if (IsHorizontal())
        return l10n_util::GetString(IDS_SCROLLBAR_CXMENU_SCROLLLEFTEDGE);
      return l10n_util::GetString(IDS_SCROLLBAR_CXMENU_SCROLLHOME);
    case ScrollBarContextMenuCommand_ScrollEnd:
      if (IsHorizontal())
        return l10n_util::GetString(IDS_SCROLLBAR_CXMENU_SCROLLRIGHTEDGE);
      return l10n_util::GetString(IDS_SCROLLBAR_CXMENU_SCROLLEND);
    case ScrollBarContextMenuCommand_ScrollPageUp:
      if (IsHorizontal())
        return l10n_util::GetString(IDS_SCROLLBAR_CXMENU_SCROLLPAGEUP);
      return l10n_util::GetString(IDS_SCROLLBAR_CXMENU_SCROLLPAGEUP);
    case ScrollBarContextMenuCommand_ScrollPageDown:
      if (IsHorizontal())
        return l10n_util::GetString(IDS_SCROLLBAR_CXMENU_SCROLLPAGEDOWN);
      return l10n_util::GetString(IDS_SCROLLBAR_CXMENU_SCROLLPAGEDOWN);
    case ScrollBarContextMenuCommand_ScrollPrev:
      if (IsHorizontal())
        return l10n_util::GetString(IDS_SCROLLBAR_CXMENU_SCROLLLEFT);
      return l10n_util::GetString(IDS_SCROLLBAR_CXMENU_SCROLLUP);
    case ScrollBarContextMenuCommand_ScrollNext:
      if (IsHorizontal())
        return l10n_util::GetString(IDS_SCROLLBAR_CXMENU_SCROLLRIGHT);
      return l10n_util::GetString(IDS_SCROLLBAR_CXMENU_SCROLLDOWN);
  }
  NOTREACHED() << "Invalid BitmapScrollBar Context Menu command!";
  return L"";
}

bool BitmapScrollBar::IsCommandEnabled(int id) const {
  switch (id) {
    case ScrollBarContextMenuCommand_ScrollPageUp:
    case ScrollBarContextMenuCommand_ScrollPageDown:
      return !IsHorizontal();
  }
  return true;
}

void BitmapScrollBar::ExecuteCommand(int id) {
  switch (id) {
    case ScrollBarContextMenuCommand_ScrollHere:
      ScrollToThumbPosition(context_menu_mouse_position_, true);
      break;
    case ScrollBarContextMenuCommand_ScrollStart:
      ScrollByAmount(SCROLL_START);
      break;
    case ScrollBarContextMenuCommand_ScrollEnd:
      ScrollByAmount(SCROLL_END);
      break;
    case ScrollBarContextMenuCommand_ScrollPageUp:
      ScrollByAmount(SCROLL_PREV_PAGE);
      break;
    case ScrollBarContextMenuCommand_ScrollPageDown:
      ScrollByAmount(SCROLL_NEXT_PAGE);
      break;
    case ScrollBarContextMenuCommand_ScrollPrev:
      ScrollByAmount(SCROLL_PREV_LINE);
      break;
    case ScrollBarContextMenuCommand_ScrollNext:
      ScrollByAmount(SCROLL_NEXT_LINE);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// BitmapScrollBar, BaseButton::ButtonListener implementation:

void BitmapScrollBar::ButtonPressed(BaseButton* sender) {
  if (sender == prev_button_) {
    ScrollByAmount(SCROLL_PREV_LINE);
  } else if (sender == next_button_) {
    ScrollByAmount(SCROLL_NEXT_LINE);
  }
}

///////////////////////////////////////////////////////////////////////////////
// BitmapScrollBar, ScrollBar implementation:

void BitmapScrollBar::Update(int viewport_size, int content_size,
                             int contents_scroll_offset) {
  ScrollBar::Update(viewport_size, content_size, contents_scroll_offset);

  // Make sure contents_size is always > 0 to avoid divide by zero errors in
  // calculations throughout this code.
  contents_size_ = std::max(1, content_size);

  if (content_size < 0)
    content_size = 0;
  if (contents_scroll_offset < 0)
    contents_scroll_offset = 0;
  if (contents_scroll_offset > content_size)
    contents_scroll_offset = content_size;

  // Thumb Height and Thumb Pos.
  // The height of the thumb is the ratio of the Viewport height to the
  // content size multiplied by the height of the thumb track.
  double ratio = static_cast<double>(viewport_size) / contents_size_;
  int thumb_size = static_cast<int>(ratio * GetTrackSize());
  thumb_->SetSize(thumb_size);

  int thumb_position = CalculateThumbPosition(contents_scroll_offset);
  thumb_->SetPosition(thumb_position);
}

int BitmapScrollBar::GetLayoutSize() const {
  CSize prefsize;
  prev_button_->GetPreferredSize(&prefsize);
  return IsHorizontal() ? prefsize.cy : prefsize.cx;
}

int BitmapScrollBar::GetPosition() const {
  return thumb_->GetPosition();
}

///////////////////////////////////////////////////////////////////////////////
// BitmapScrollBar, private:

void BitmapScrollBar::ScrollContentsToOffset() {
  GetController()->ScrollToPosition(this, contents_scroll_offset_);
  thumb_->SetPosition(CalculateThumbPosition(contents_scroll_offset_));
}

int BitmapScrollBar::GetTrackSize() const {
  gfx::Rect track_bounds = GetTrackBounds();
  return IsHorizontal() ? track_bounds.width() : track_bounds.height();
}

int BitmapScrollBar::CalculateThumbPosition(int contents_scroll_offset) const {
  return (contents_scroll_offset * GetTrackSize()) / contents_size_;
}

int BitmapScrollBar::CalculateContentsOffset(int thumb_position,
                                             bool scroll_to_middle) const {
  if (scroll_to_middle)
    thumb_position = thumb_position - (thumb_->GetSize() / 2);
  return (thumb_position * contents_size_) / GetTrackSize();
}

void BitmapScrollBar::SetThumbTrackState(BaseButton::ButtonState state) {
  thumb_track_state_ = state;
  SchedulePaint();
}

}

