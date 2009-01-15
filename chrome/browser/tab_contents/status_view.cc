// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/status_view.h"

const int StatusView::kLayoutPadding = 5;
const int StatusView::kButtonWidth = 200;
const int StatusView::kButtonHeight = 30;

StatusView::StatusView(TabContentsType type) : TabContents(type) {
}

StatusView::~StatusView() {
  for (size_t i = 0; i < buttons_.size(); ++i)
    delete buttons_[i].button;
}

void StatusView::CreateView() {
  Create(GetDesktopWindow());
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

