// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/native_scroll_bar.h"

#include <math.h>
#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlframe.h>

#include "base/message_loop.h"
#include "chrome/views/hwnd_view.h"
#include "chrome/views/widget.h"

namespace views {

/////////////////////////////////////////////////////////////////////////////
//
// ScrollBarContainer
//
// Since windows scrollbar only send notifications to their parent hwnd, we
// use instance of this class to wrap native scrollbars.
//
/////////////////////////////////////////////////////////////////////////////
class ScrollBarContainer : public CWindowImpl<ScrollBarContainer,
                           CWindow,
                           CWinTraits<WS_CHILD>> {
 public:
  ScrollBarContainer(ScrollBar* parent) : parent_(parent),
                                          scrollbar_(NULL) {
    Create(parent->GetWidget()->GetNativeView());
    ::ShowWindow(m_hWnd, SW_SHOW);
  }

  virtual ~ScrollBarContainer() {
  }

  DECLARE_FRAME_WND_CLASS(L"ChromeViewsScrollBarContainer", NULL);
  BEGIN_MSG_MAP(ScrollBarContainer);
    MSG_WM_CREATE(OnCreate);
    MSG_WM_ERASEBKGND(OnEraseBkgnd);
    MSG_WM_PAINT(OnPaint);
    MSG_WM_SIZE(OnSize);
    MSG_WM_HSCROLL(OnHorizScroll);
    MSG_WM_VSCROLL(OnVertScroll);
  END_MSG_MAP();

  HWND GetScrollBarHWND() {
    return scrollbar_;
  }

  // Invoked when the scrollwheel is used
  void ScrollWithOffset(int o) {
    SCROLLINFO si;
    si.cbSize = sizeof(si);
    si.fMask = SIF_POS;
    ::GetScrollInfo(scrollbar_, SB_CTL, &si);
    int pos = si.nPos - o;

    if (pos < parent_->GetMinPosition())
      pos = parent_->GetMinPosition();
    else if (pos > parent_->GetMaxPosition())
      pos = parent_->GetMaxPosition();

    ScrollBarController* sbc = parent_->GetController();
    sbc->ScrollToPosition(parent_, pos);

    si.nPos = pos;
    si.fMask = SIF_POS;
    ::SetScrollInfo(scrollbar_, SB_CTL, &si, TRUE);
  }

 private:

  LRESULT OnCreate(LPCREATESTRUCT create_struct) {
    scrollbar_ = CreateWindow(L"SCROLLBAR",
                              L"",
                              WS_CHILD | (parent_->IsHorizontal() ?
                                          SBS_HORZ : SBS_VERT),
                              0,
                              0,
                              parent_->width(),
                              parent_->height(),
                              m_hWnd,
                              NULL,
                              NULL,
                              NULL);
    ::ShowWindow(scrollbar_, SW_SHOW);
    return 1;
  }

  LRESULT OnEraseBkgnd(HDC dc) {
    return 1;
  }

  void OnPaint(HDC ignore) {
    PAINTSTRUCT ps;
    HDC dc = ::BeginPaint(*this, &ps);
    ::EndPaint(*this, &ps);
  }

  void OnSize(int type, const CSize& sz) {
    ::SetWindowPos(scrollbar_,
                   0,
                   0,
                   0,
                   sz.cx,
                   sz.cy,
                   SWP_DEFERERASE |
                   SWP_NOACTIVATE |
                   SWP_NOCOPYBITS |
                   SWP_NOOWNERZORDER |
                   SWP_NOSENDCHANGING |
                   SWP_NOZORDER);
  }

  void OnScroll(int code, HWND source, bool is_horizontal) {
    int pos;

    if (code == SB_ENDSCROLL) {
      return;
    }

    // If we receive an event from the scrollbar, make the view
    // component focused so we actually get mousewheel events.
    if (source != NULL) {
      Widget* widget = parent_->GetWidget();
      if (widget && widget->GetNativeView() != GetFocus()) {
        parent_->RequestFocus();
      }
    }

    SCROLLINFO si;
    si.cbSize = sizeof(si);
    si.fMask = SIF_POS | SIF_TRACKPOS;
    ::GetScrollInfo(scrollbar_, SB_CTL, &si);
    pos = si.nPos;

    ScrollBarController* sbc = parent_->GetController();

    switch (code) {
      case SB_BOTTOM: // case SB_RIGHT:
        pos = parent_->GetMaxPosition();
        break;
      case SB_TOP:  // case SB_LEFT:
        pos = parent_->GetMinPosition();
        break;
      case SB_LINEDOWN:  //  case SB_LINERIGHT:
        pos += sbc->GetScrollIncrement(parent_, false, true);
        pos = std::min(parent_->GetMaxPosition(), pos);
        break;
      case SB_LINEUP:  //  case SB_LINELEFT:
        pos -= sbc->GetScrollIncrement(parent_, false, false);
        pos = std::max(parent_->GetMinPosition(), pos);
        break;
      case SB_PAGEDOWN:  //  case SB_PAGERIGHT:
        pos += sbc->GetScrollIncrement(parent_, true, true);
        pos = std::min(parent_->GetMaxPosition(), pos);
        break;
      case SB_PAGEUP:  // case SB_PAGELEFT:
        pos -= sbc->GetScrollIncrement(parent_, true, false);
        pos = std::max(parent_->GetMinPosition(), pos);
        break;
      case SB_THUMBPOSITION:
      case SB_THUMBTRACK:
        pos = si.nTrackPos;
        if (pos < parent_->GetMinPosition())
          pos = parent_->GetMinPosition();
        else if (pos > parent_->GetMaxPosition())
          pos = parent_->GetMaxPosition();
        break;
      default:
        break;
    }

    sbc->ScrollToPosition(parent_, pos);

    si.nPos = pos;
    si.fMask = SIF_POS;
    ::SetScrollInfo(scrollbar_, SB_CTL, &si, TRUE);

    // Note: the system scrollbar modal loop doesn't give a chance
    // to our message_loop so we need to call DidProcessMessage()
    // manually.
    //
    // Sadly, we don't know what message has been processed. We may
    // want to remove the message from DidProcessMessage()
    MSG dummy;
    dummy.hwnd = NULL;
    dummy.message = 0;
    MessageLoopForUI::current()->DidProcessMessage(dummy);
  }

  // note: always ignore 2nd param as it is 16 bits
  void OnHorizScroll(int n_sb_code, int ignore, HWND source) {
    OnScroll(n_sb_code, source, true);
  }

  // note: always ignore 2nd param as it is 16 bits
  void OnVertScroll(int n_sb_code, int ignore, HWND source) {
    OnScroll(n_sb_code, source, false);
  }



  ScrollBar* parent_;
  HWND scrollbar_;
};

NativeScrollBar::NativeScrollBar(bool is_horiz)
  : sb_view_(NULL),
    sb_container_(NULL),
    ScrollBar(is_horiz) {
}

NativeScrollBar::~NativeScrollBar() {
  if (sb_container_) {
    // We always destroy the scrollbar container explicitly to cover all
    // cases including when the container is no longer connected to a
    // widget tree.
    ::DestroyWindow(*sb_container_);
    delete sb_container_;
  }
}

void NativeScrollBar::ViewHierarchyChanged(bool is_add, View *parent,
                                           View *child) {
  Widget* widget;
  if (is_add && (widget = GetWidget()) && !sb_view_) {
    sb_view_ = new HWNDView();
    AddChildView(sb_view_);
    sb_container_ = new ScrollBarContainer(this);
    sb_view_->Attach(*sb_container_);
    Layout();
  }
}

void NativeScrollBar::Layout() {
  if (sb_view_)
    sb_view_->SetBounds(GetLocalBounds(true));
}

gfx::Size NativeScrollBar::GetPreferredSize() {
  if (IsHorizontal())
    return gfx::Size(0, GetLayoutSize());
  return gfx::Size(GetLayoutSize(), 0);
}

void NativeScrollBar::Update(int viewport_size,
                             int content_size,
                             int current_pos) {
  ScrollBar::Update(viewport_size, content_size, current_pos);
  if (!sb_container_)
    return;

  if (content_size < 0)
    content_size = 0;

  if (current_pos < 0)
    current_pos = 0;

  if (current_pos > content_size)
    current_pos = content_size;

  SCROLLINFO si;
  si.cbSize = sizeof(si);
  si.fMask = SIF_DISABLENOSCROLL | SIF_POS | SIF_RANGE | SIF_PAGE;
  si.nMin = 0;
  si.nMax = content_size;
  si.nPos = current_pos;
  si.nPage = viewport_size;
  ::SetScrollInfo(sb_container_->GetScrollBarHWND(),
                  SB_CTL,
                  &si,
                  TRUE);
}

int NativeScrollBar::GetLayoutSize() const {
  return ::GetSystemMetrics(IsHorizontal() ? SM_CYHSCROLL : SM_CYVSCROLL);
}

int NativeScrollBar::GetPosition() const {
  SCROLLINFO si;
  si.cbSize = sizeof(si);
  si.fMask = SIF_POS;
  GetScrollInfo(sb_container_->GetScrollBarHWND(), SB_CTL, &si);
  return si.nPos;
}

bool NativeScrollBar::OnMouseWheel(const MouseWheelEvent& e) {
  if (!sb_container_) {
    return false;
  }

  sb_container_->ScrollWithOffset(e.GetOffset());
  return true;
}

bool NativeScrollBar::OnKeyPressed(const KeyEvent& event) {
  if (!sb_container_) {
    return false;
  }
  int code = -1;
  switch(event.GetCharacter()) {
    case VK_UP:
      if (!IsHorizontal())
        code = SB_LINEUP;
      break;
    case VK_PRIOR:
      code = SB_PAGEUP;
      break;
    case VK_NEXT:
      code = SB_PAGEDOWN;
      break;
    case VK_DOWN:
      if (!IsHorizontal())
        code = SB_LINEDOWN;
      break;
    case VK_HOME:
      code = SB_TOP;
      break;
    case VK_END:
      code = SB_BOTTOM;
      break;
    case VK_LEFT:
      if (IsHorizontal())
        code = SB_LINELEFT;
      break;
    case VK_RIGHT:
      if (IsHorizontal())
        code = SB_LINERIGHT;
      break;
  }
  if (code != -1) {
    ::SendMessage(*sb_container_,
                  IsHorizontal() ? WM_HSCROLL : WM_VSCROLL,
                  MAKELONG(static_cast<WORD>(code), 0), 0L);
    return true;
  }
  return false;
}

//static
int NativeScrollBar::GetHorizontalScrollBarHeight() {
  return ::GetSystemMetrics(SM_CYHSCROLL);
}

//static
int NativeScrollBar::GetVerticalScrollBarWidth() {
  return ::GetSystemMetrics(SM_CXVSCROLL);
}

}  // namespace views
