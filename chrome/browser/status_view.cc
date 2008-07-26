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

#include "chrome/browser/status_view.h"

const int StatusView::kLayoutPadding = 5;
const int StatusView::kButtonWidth = 200;
const int StatusView::kButtonHeight = 30;

StatusView::StatusView(TabContentsType type) : TabContents(type) {
}

StatusView::~StatusView() {
  for (size_t i = 0; i < buttons_.size(); ++i)
    delete buttons_[i].button;
}

void StatusView::CreateView(HWND parent_hwnd,
                            const gfx::Rect& initial_bounds) {
  Create(parent_hwnd);
}

LRESULT StatusView::OnCreate(LPCREATESTRUCT create_struct) {
  CRect rect(kLayoutPadding, kButtonHeight + kLayoutPadding * 2, 200, 200);
  OnCreate(rect);
  return 0;
}

void StatusView::OnSize(WPARAM wParam, const CSize& size) {
  int start_x = kLayoutPadding;
  int start_y = kButtonHeight + kLayoutPadding * 2;
  int end_x = size.cx - kLayoutPadding;
  int end_y = size.cy - kLayoutPadding;
  CRect rect(start_x, start_y, end_x, end_y);
  OnSize(rect);
}

LRESULT StatusView::OnEraseBkgnd(HDC hdc) {
  HBRUSH brush = GetSysColorBrush(COLOR_3DFACE);
  HGDIOBJ old_brush = SelectObject(hdc, brush);

  RECT rc;
  GetClientRect(&rc);
  FillRect(hdc, &rc, brush);

  SelectObject(hdc, old_brush);
  return 1;
}

void StatusView::CreateButton(int id, const wchar_t* title) {
  int button_count = static_cast<int>(buttons_.size());
  int width_offset =
      kLayoutPadding + button_count * (kButtonWidth + kLayoutPadding);
  CRect rect(0, 0, kButtonWidth, kButtonHeight);
  rect.OffsetRect(width_offset, kLayoutPadding);
  ButtonInfo bi;
  bi.button = new CButton();
  bi.id = id;
  bi.button->Create(m_hWnd, rect, NULL, WS_CHILD | WS_VISIBLE, 0, bi.id);
  bi.button->SetWindowText(title);
  buttons_.push_back(bi);
}

void StatusView::SetButtonText(int id, const wchar_t* title) {
  for (size_t i = 0; i < buttons_.size(); ++i) {
    if (buttons_[i].id == id) {
      buttons_[i].button->SetWindowText(title);
      return;
    }
  }

  DLOG(INFO) << "No button with id " << id << " to set title " << title;
}
