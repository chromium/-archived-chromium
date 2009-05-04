// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/html_dialog_view.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/views/widget/root_view.h"
#include "chrome/views/window/window.h"

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView, public:

HtmlDialogView::HtmlDialogView(Profile* profile,
                               HtmlDialogUIDelegate* delegate)
    : DOMView(),
      profile_(profile),
      delegate_(delegate) {
  DCHECK(profile);
}

HtmlDialogView::~HtmlDialogView() {
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView, views::View implementation:

gfx::Size HtmlDialogView::GetPreferredSize() {
  gfx::Size out;
  delegate_->GetDialogSize(&out);
  return out;
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView, views::WindowDelegate implementation:

bool HtmlDialogView::CanResize() const {
  return true;
}

bool HtmlDialogView::IsModal() const {
  return delegate_->IsDialogModal();
}

std::wstring HtmlDialogView::GetWindowTitle() const {
  return delegate_->GetDialogTitle();
}

void HtmlDialogView::WindowClosing() {
  // If we still have a delegate that means we haven't notified it of the
  // dialog closing. This happens if the user clicks the Close button on the
  // dialog.
  if (delegate_)
    OnDialogClosed("");
}

views::View* HtmlDialogView::GetContentsView() {
  return this;
}

views::View* HtmlDialogView::GetInitiallyFocusedView() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogUIDelegate implementation:

bool HtmlDialogView::IsDialogModal() const {
  return IsModal();
}

std::wstring HtmlDialogView::GetDialogTitle() const {
  return GetWindowTitle();
}

GURL HtmlDialogView::GetDialogContentURL() const {
  return delegate_->GetDialogContentURL();
}

void HtmlDialogView::GetDialogSize(gfx::Size* size) const {
  delegate_->GetDialogSize(size);
}

std::string HtmlDialogView::GetDialogArgs() const {
  return delegate_->GetDialogArgs();
}

void HtmlDialogView::OnDialogClosed(const std::string& json_retval) {
  delegate_->OnDialogClosed(json_retval);
  delegate_ = NULL;  // We will not communicate further with the delegate.
  window()->Close();
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView:

void HtmlDialogView::InitDialog() {
  // Now Init the DOMView. This view runs in its own process to render the html.
  DOMView::Init(profile_, NULL);

  // Set the delegate. This must be done before loading the page. See
  // the comment above HtmlDialogUI in its header file for why.
  HtmlDialogUI::GetPropertyAccessor().SetProperty(tab_contents_->property_bag(),
                                                  this);

  DOMView::LoadURL(delegate_->GetDialogContentURL());
}
